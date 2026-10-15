#include "ai.h"
#include "suppliers.h"
#include "events.h"
#include "telemetry.h"
#include "arena.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static AiEconomyState aiState;
static float aiDecisionTimer = 0.0f;
static float demandAccumulator[MAX_ITEMS] = {0.0f};

AiEconomyState *GetAiEconomyState(void)
{
    return &aiState;
}

float GetSatisfactionDemandMultiplier(void)
{
    if (aiState.customerSatisfaction >= 85.0f)
    {
        return 1.08f;
    }
    else if (aiState.customerSatisfaction >= 65.0f)
    {
        return 0.88f + ((aiState.customerSatisfaction - 65.0f) / 20.0f) * 0.20f;
    }
    else if (aiState.customerSatisfaction >= 40.0f)
    {
        return 0.52f + ((aiState.customerSatisfaction - 40.0f) / 25.0f) * 0.36f;
    }
    else
    {
        return 0.22f + (aiState.customerSatisfaction / 40.0f) * 0.30f;
    }
}

float GetItemPriceElasticity(int idx)
{
    if (idx < 0 || idx >= MAX_ITEMS)
        return 1.0f;
    float mult = aiState.aiPriceMult[idx];
    if (mult <= 0.01f)
        mult = 1.0f;
    float elasticity = 1.0f - (mult - 1.0f) * 2.25f;
    if (elasticity < 0.28f)
        elasticity = 0.28f;
    if (elasticity > 1.32f)
        elasticity = 1.32f;
    return elasticity;
}

void AdjustSkuPriceMultiplier(int skuIdx, float delta, InventoryItem *sku, float sampleTime)
{
    if (skuIdx < 0 || skuIdx >= MAX_ITEMS || sku == NULL)
        return;
    aiState.aiPriceMult[skuIdx] += delta;
    if (aiState.aiPriceMult[skuIdx] < 0.88f)
        aiState.aiPriceMult[skuIdx] = 0.88f;
    if (aiState.aiPriceMult[skuIdx] > 1.18f)
        aiState.aiPriceMult[skuIdx] = 1.18f;
    sku->price = aiState.baseFairPrice[skuIdx] * aiState.aiPriceMult[skuIdx];
    RecordSkuTelemetrySample(skuIdx, sku, sampleTime);
}

void ResetSkuFairPrice(int skuIdx, InventoryItem *sku, bool preservePriceRatio)
{
    if (skuIdx < 0 || skuIdx >= MAX_ITEMS || sku == NULL)
        return;
    aiState.baseFairPrice[skuIdx] = sku->orderCost * BASE_FAIR_MARGIN_MULT;
    if (preservePriceRatio && aiState.baseFairPrice[skuIdx] > 1.0f)
    {
        aiState.aiPriceMult[skuIdx] = sku->price / aiState.baseFairPrice[skuIdx];
        if (aiState.aiPriceMult[skuIdx] < 0.88f)
            aiState.aiPriceMult[skuIdx] = 0.88f;
        if (aiState.aiPriceMult[skuIdx] > 1.18f)
            aiState.aiPriceMult[skuIdx] = 1.18f;
    }
    else
    {
        aiState.aiPriceMult[skuIdx] = 1.0f;
    }
    sku->price = aiState.baseFairPrice[skuIdx] * aiState.aiPriceMult[skuIdx];
}

void InitAiEconomy(InventoryItem inventory[], int *itemCount)
{
    aiState.simTime = 0.0f;
    aiState.simSpeed = 1.0f;
    aiState.totalRevenue = 0.0f;
    aiState.totalOrderSpend = 0.0f;
    aiState.totalUpkeepPaid = 0.0f;
    aiState.netProfit = 0.0f;
    aiState.totalUnitsSold = 0;
    aiState.missedSalesCount = 0;
    aiState.upkeepTimer = 0.0f;
    aiState.lastStaffBill = 165.0f;
    aiState.lastUtilityBill = 115.0f;
    aiState.lastStorageBill = 72.0f;
    aiState.lastTotalBill = 352.0f;
    aiState.customerSatisfaction = 74.0f;
    aiState.avgPriceMarkupPct = 0.0f;
    aiDecisionTimer = 0.0f;
    strcpy(aiState.aiLastActionLog, "NDAL AI online. Balancing lean stock, heavy upkeep & price elasticity...");

    for (int i = 0; i < MAX_ITEMS; i++)
    {
        demandAccumulator[i] = 0.0f;
    }

    int activeSeen = 0;
    for (int i = 0; i < MAX_ITEMS; i++)
    {
        if (inventory[i].isActive)
        {
            activeSeen++;
            if (activeSeen > MAX_STORE_SKUS)
            {
                inventory[i].isActive = false;
                continue;
            }
            if (inventory[i].orderCost <= 1.0f)
                inventory[i].orderCost = 70.0f;
            if (inventory[i].maxStock > 46)
            {
                inventory[i].maxStock = 26 + (i % 4) * 5;
            }
            if (inventory[i].maxStock < 16)
                inventory[i].maxStock = 24;
            inventory[i].minStock = (int)(inventory[i].maxStock * 0.33f);
            if (inventory[i].minStock < 6)
                inventory[i].minStock = 6;

            int targetStartQty = inventory[i].minStock + 3;
            if (inventory[i].quantity > targetStartQty + 4 || inventory[i].quantity <= 0)
            {
                inventory[i].quantity = targetStartQty;
            }

            aiState.baseFairPrice[i] = inventory[i].orderCost * BASE_FAIR_MARGIN_MULT;
            aiState.aiPriceMult[i] = 1.00f;
            inventory[i].price = aiState.baseFairPrice[i];

            if (inventory[i].baseDemandRate < 0.20f || inventory[i].baseDemandRate > 0.40f)
            {
                inventory[i].baseDemandRate = 0.24f + (float)(i % 4) * 0.04f;
            }
        }
        else
        {
            aiState.baseFairPrice[i] = 100.0f;
            aiState.aiPriceMult[i] = 1.00f;
        }
    }
    SaveInventory(inventory, *itemCount);
}

void UpdateAiAndEconomyStep(float dt, InventoryItem inventory[], int *itemCount, OrderRecord orders[], int *orderCount)
{
    aiState.simTime += dt;
    ChaosEventState *ev = GetEventsState();
    Supplier *suppliers = GetSuppliers();

    // 1. Heavy Upkeep & Overhead Billing Cycle (Every 8.0 seconds)
    int currentTotalUnits = 0;
    int emptySkus = 0;
    int lowSkus = 0;
    float sumMult = 0.0f;
    int activeSkuCount = 0;

    for (int i = 0; i < MAX_ITEMS; i++)
    {
        if (inventory[i].isActive)
        {
            currentTotalUnits += inventory[i].quantity;
            if (inventory[i].quantity == 0)
                emptySkus++;
            else if (inventory[i].quantity <= inventory[i].minStock)
                lowSkus++;
            if (aiState.baseFairPrice[i] <= 1.0f)
                aiState.baseFairPrice[i] = inventory[i].orderCost * BASE_FAIR_MARGIN_MULT;
            sumMult += aiState.aiPriceMult[i];
            activeSkuCount++;
        }
    }
    aiState.avgPriceMarkupPct = (activeSkuCount > 0) ? (((sumMult / (float)activeSkuCount) - 1.0f) * 100.0f) : 0.0f;

    if (emptySkus > 0)
    {
        aiState.customerSatisfaction -= (float)emptySkus * 0.85f * dt;
    }
    if (lowSkus > 2)
    {
        aiState.customerSatisfaction -= (float)(lowSkus - 2) * 0.15f * dt;
    }
    if (aiState.avgPriceMarkupPct > 3.0f)
    {
        aiState.customerSatisfaction -= ((aiState.avgPriceMarkupPct - 3.0f) * 0.12f) * dt;
    }
    else if (aiState.avgPriceMarkupPct < -2.0f && emptySkus == 0)
    {
        aiState.customerSatisfaction += 0.28f * dt;
    }
    if (aiState.customerSatisfaction > 100.0f)
        aiState.customerSatisfaction = 100.0f;
    if (aiState.customerSatisfaction < 0.0f)
        aiState.customerSatisfaction = 0.0f;

    aiState.lastStaffBill = 165.0f * (ev->wageSurgeTimer > 0.0f ? 1.85f : 1.0f);
    aiState.lastUtilityBill = 115.0f * (ev->energySpikeTimer > 0.0f ? 2.10f : 1.0f);
    aiState.lastStorageBill = (float)currentTotalUnits * 0.85f;
    aiState.lastTotalBill = aiState.lastStaffBill + aiState.lastUtilityBill + aiState.lastStorageBill;

    aiState.upkeepTimer += dt;
    if (aiState.upkeepTimer >= UPKEEP_BILL_INTERVAL)
    {
        aiState.upkeepTimer -= UPKEEP_BILL_INTERVAL;
        aiState.totalUpkeepPaid += aiState.lastTotalBill;
        aiState.netProfit = aiState.totalRevenue - aiState.totalOrderSpend - aiState.totalUpkeepPaid;

        PushNotification(TextFormat("UPKEEP PAID: -EUR %.0f (Staff:%.0f | Power:%.0f | Tax:%.0f)",
                                    aiState.lastTotalBill, aiState.lastStaffBill, aiState.lastUtilityBill, aiState.lastStorageBill),
                         0);
        SpawnFloatingPopup(980.0f, 860.0f, TextFormat("-EUR %.0f UPKEEP BILL", aiState.lastTotalBill), (Color){255, 195, 45, 255});
    }

    // 2. Simulate Customer Demand & Sales
    float satDemandMult = GetSatisfactionDemandMultiplier();

    for (int i = 0; i < MAX_ITEMS; i++)
    {
        if (!inventory[i].isActive)
            continue;

        if (inventory[i].saleTimer > 0.0f)
        {
            inventory[i].saleTimer -= dt;
            if (inventory[i].saleTimer < 0.0f)
                inventory[i].saleTimer = 0.0f;
        }

        float priceElasticity = GetItemPriceElasticity(i);
        float currentRate = inventory[i].baseDemandRate * satDemandMult * priceElasticity;
        if (inventory[i].saleTimer > 0.0f)
            currentRate *= 2.2f;
        if (ev->viralSurgeTimer > 0.0f)
            currentRate *= 2.5f;

        float jitter = 0.85f + ((float)(GetRandomValue(0, 30)) / 100.0f);
        demandAccumulator[i] += currentRate * jitter * dt;

        if (demandAccumulator[i] >= 1.0f)
        {
            int unitsWanted = (int)demandAccumulator[i];
            demandAccumulator[i] -= unitsWanted;

            if (inventory[i].quantity > 0)
            {
                int sold = (unitsWanted > inventory[i].quantity) ? inventory[i].quantity : unitsWanted;
                inventory[i].quantity -= sold;
                aiState.totalUnitsSold += sold;

                float effectiveSellPrice = (inventory[i].saleTimer > 0.0f) ? (inventory[i].price * 0.88f) : inventory[i].price;
                float saleRev = sold * effectiveSellPrice;
                float effCost = GetEffectiveUnitCost(&inventory[i], ev->subsidyTimer);
                float saleProf = sold * (effectiveSellPrice - effCost);

                aiState.totalRevenue += saleRev;
                aiState.netProfit = aiState.totalRevenue - aiState.totalOrderSpend - aiState.totalUpkeepPaid;

                RecordSkuSaleWindow(i, sold, saleRev, saleProf);

                float itemMarkupPct = (aiState.aiPriceMult[i] - 1.0f) * 100.0f;
                if (inventory[i].saleTimer > 0.0f || aiState.aiPriceMult[i] < 0.96f)
                {
                    aiState.customerSatisfaction += sold * 0.42f;
                }
                else if (aiState.aiPriceMult[i] <= 1.02f)
                {
                    aiState.customerSatisfaction += sold * 0.16f;
                }
                else
                {
                    float satPenalty = (aiState.aiPriceMult[i] - 1.015f) * 12.5f * (float)sold;
                    aiState.customerSatisfaction -= satPenalty;
                }
                if (aiState.customerSatisfaction > 100.0f)
                    aiState.customerSatisfaction = 100.0f;
                if (aiState.customerSatisfaction < 0.0f)
                    aiState.customerSatisfaction = 0.0f;

                SpawnShopperAgent(i, sold, saleRev, itemMarkupPct, false);

                if (sold < unitsWanted)
                {
                    int missed = unitsWanted - sold;
                    aiState.missedSalesCount += missed;
                    aiState.customerSatisfaction -= missed * 4.5f;
                    if (aiState.customerSatisfaction < 0.0f)
                        aiState.customerSatisfaction = 0.0f;
                    SpawnShopperAgent(i, missed, 0.0f, itemMarkupPct, true);
                }
            }
            else
            {
                aiState.missedSalesCount += unitsWanted;
                aiState.customerSatisfaction -= unitsWanted * 4.5f;
                if (aiState.customerSatisfaction < 0.0f)
                    aiState.customerSatisfaction = 0.0f;
                SpawnShopperAgent(i, unitsWanted, 0.0f, 0.0f, true);
            }
        }
    }

    // 3. Autonomous AI Dynamic Pricing & Inventory Manager Decision Loop (Every 1.5s)
    aiDecisionTimer += dt;
    if (aiDecisionTimer >= 1.5f)
    {
        aiDecisionTimer = 0.0f;

        int pricesRaised = 0;
        int pricesLowered = 0;

        for (int i = 0; i < MAX_ITEMS; i++)
        {
            if (!inventory[i].isActive)
                continue;
            if (aiState.baseFairPrice[i] <= 1.0f)
                aiState.baseFairPrice[i] = inventory[i].orderCost * BASE_FAIR_MARGIN_MULT;

            float targetMult = aiState.aiPriceMult[i];
            float stockRatio = (inventory[i].maxStock > 0) ? ((float)inventory[i].quantity / (float)inventory[i].maxStock) : 0.5f;

            if (aiState.customerSatisfaction > 84.0f && targetMult < 1.08f)
            {
                targetMult += 0.015f;
                pricesRaised++;
            }
            else if (aiState.customerSatisfaction >= 72.0f && aiState.netProfit < 0.0f && targetMult < 1.12f)
            {
                targetMult += 0.018f;
                pricesRaised++;
            }
            else if (aiState.customerSatisfaction < 68.0f)
            {
                targetMult -= 0.028f;
                pricesLowered++;
            }
            else if (aiState.customerSatisfaction < 76.0f && targetMult > 1.02f)
            {
                targetMult -= 0.015f;
                pricesLowered++;
            }

            if (ev->energySpikeTimer > 0.0f || ev->wageSurgeTimer > 0.0f)
            {
                if (aiState.customerSatisfaction >= 62.0f)
                    targetMult += 0.012f;
            }

            if (inventory[i].quantity > 0 && inventory[i].quantity <= inventory[i].minStock && aiState.customerSatisfaction >= 60.0f)
            {
                targetMult += 0.015f;
            }
            else if (stockRatio > 0.80f && targetMult > 0.94f)
            {
                targetMult -= 0.016f;
            }

            if (targetMult < 0.88f)
                targetMult = 0.88f;
            if (targetMult > 1.18f)
                targetMult = 1.18f;

            aiState.aiPriceMult[i] = targetMult;
            inventory[i].price = aiState.baseFairPrice[i] * aiState.aiPriceMult[i];
        }

        if (pricesRaised > pricesLowered)
        {
            snprintf(aiState.aiLastActionLog, sizeof(aiState.aiLastActionLog),
                     "NDAL AI PRICING: Nudged prices (Avg %+.1f%%) to cover EUR %.0f/8s upkeep [Sat: %.0f%%]",
                     aiState.avgPriceMarkupPct, aiState.lastTotalBill, aiState.customerSatisfaction);
        }
        else if (pricesLowered > pricesRaised)
        {
            snprintf(aiState.aiLastActionLog, sizeof(aiState.aiLastActionLog),
                     "NDAL AI PRICING: Trimmed prices (Avg %+.1f%%) to rebuild satisfaction [Sat: %.0f%%]",
                     aiState.avgPriceMarkupPct, aiState.customerSatisfaction);
        }

        int urgentItemsCount = 0;
        float lowestTimeToStockout = 999.0f;
        BasketItem aiBasket[MAX_BASKET_ITEMS];
        int aiBasketCount = 0;

        for (int i = 0; i < MAX_ITEMS; i++)
        {
            if (!inventory[i].isActive)
                continue;

            int effectiveStock = inventory[i].quantity + inventory[i].inTransit;
            float demandVel = inventory[i].baseDemandRate * satDemandMult * GetItemPriceElasticity(i) *
                              (inventory[i].saleTimer > 0.0f ? 2.2f : 1.0f) *
                              (ev->viralSurgeTimer > 0.0f ? 2.5f : 1.0f);
            float timeToOut = demandVel > 0.01f ? ((float)inventory[i].quantity / demandVel) : 999.0f;

            float bufferMult = 1.25f;
            if (inventory[i].saleTimer > 0.0f || ev->viralSurgeTimer > 0.0f)
                bufferMult = 1.55f;
            else if (aiState.customerSatisfaction < 65.0f)
                bufferMult = 1.40f;

            int triggerPoint = (int)ceilf(inventory[i].minStock * bufferMult);
            if (effectiveStock <= triggerPoint)
            {
                int hardCapQty = inventory[i].maxStock - effectiveStock;
                int targetCeiling = (inventory[i].saleTimer > 0.0f || ev->viralSurgeTimer > 0.0f)
                                        ? (int)(inventory[i].maxStock * 0.95f)
                                        : (int)(inventory[i].maxStock * 0.78f);
                int desiredQty = targetCeiling - effectiveStock;
                if (desiredQty < inventory[i].minStock)
                    desiredQty = hardCapQty;
                if (desiredQty > hardCapQty)
                    desiredQty = hardCapQty;

                if (desiredQty > 0 && aiBasketCount < MAX_BASKET_ITEMS)
                {
                    aiBasket[aiBasketCount].itemId = inventory[i].id;
                    aiBasket[aiBasketCount].orderQuantity = desiredQty;
                    aiBasketCount++;
                    urgentItemsCount++;
                    if (timeToOut < lowestTimeToStockout)
                        lowestTimeToStockout = timeToOut;
                }
            }
        }

        if (aiBasketCount > 0 && *orderCount < MAX_ORDERS)
        {
            int bestSupplierIdx = -1;
            float bestSupplierScore = -999999.0f;
            float bestOrderCost = 0.0f;
            float bestEstProfit = 0.0f;

            float subsidyMult = (ev->subsidyTimer > 0.0f) ? 0.80f : 1.0f;
            float urgencyWeight = (aiState.customerSatisfaction < 65.0f) ? 72.0f : 38.0f;

            for (int s = 0; s < MAX_SUPPLIERS; s++)
            {
                if (suppliers[s].hasShortage)
                    continue;

                float effDeliveryFee = suppliers[s].deliveryFee * suppliers[s].priceSpikeMult * subsidyMult;
                float effCostMult = suppliers[s].costMultiplier * suppliers[s].priceSpikeMult * subsidyMult;
                float effLeadTime = suppliers[s].deliveryTime * (ev->weatherDelayTimer > 0.0f ? 2.2f : 1.0f);

                float orderCost = effDeliveryFee;
                float expectedRev = 0.0f;

                for (int b = 0; b < aiBasketCount; b++)
                {
                    for (int i = 0; i < MAX_ITEMS; i++)
                    {
                        if (inventory[i].id == aiBasket[b].itemId)
                        {
                            orderCost += aiBasket[b].orderQuantity * (inventory[i].orderCost * effCostMult);
                            float sellP = (inventory[i].saleTimer > 0.0f) ? (inventory[i].price * 0.88f) : inventory[i].price;
                            expectedRev += aiBasket[b].orderQuantity * sellP;
                        }
                    }
                }

                float estProfit = expectedRev - orderCost;
                float stockoutPenalty = 0.0f;
                if (effLeadTime > lowestTimeToStockout)
                {
                    stockoutPenalty = (effLeadTime - lowestTimeToStockout) * urgencyWeight;
                }

                float score = estProfit - stockoutPenalty;
                if (score > bestSupplierScore)
                {
                    bestSupplierScore = score;
                    bestSupplierIdx = s;
                    bestOrderCost = orderCost;
                    bestEstProfit = estProfit;
                }
            }

            if (bestSupplierIdx != -1)
            {
                OrderRecord *newOrd = &orders[*orderCount];
                newOrd->orderId = *orderCount + 1;
                newOrd->supplierId = suppliers[bestSupplierIdx].id;
                strcpy(newOrd->supplierName, suppliers[bestSupplierIdx].name);
                newOrd->itemCount = aiBasketCount;
                newOrd->totalCost = bestOrderCost;
                newOrd->estimatedProfit = bestEstProfit;
                newOrd->elapsedTime = 0.0f;
                newOrd->totalDeliveryTime = suppliers[bestSupplierIdx].deliveryTime;
                newOrd->isDamaged = false;
                newOrd->status = STATUS_PROCESSING;
                newOrd->isActive = true;

                int totalOrderedUnits = 0;
                for (int b = 0; b < aiBasketCount; b++)
                {
                    newOrd->items[b] = aiBasket[b];
                    totalOrderedUnits += aiBasket[b].orderQuantity;
                    for (int i = 0; i < MAX_ITEMS; i++)
                    {
                        if (inventory[i].id == aiBasket[b].itemId)
                        {
                            inventory[i].inTransit += aiBasket[b].orderQuantity;
                        }
                    }
                }

                aiState.totalOrderSpend += bestOrderCost;
                aiState.netProfit = aiState.totalRevenue - aiState.totalOrderSpend - aiState.totalUpkeepPaid;
                (*orderCount)++;

                snprintf(aiState.aiLastActionLog, sizeof(aiState.aiLastActionLog),
                         "NDAL AI DISPATCH: Ordered %d u via %s [Cost: EUR %.0f | Est.Margin: %+.0f]",
                         totalOrderedUnits, suppliers[bestSupplierIdx].name,
                         bestOrderCost, bestEstProfit);

                PushNotification(TextFormat("AI Order #%d via %s (Cost: EUR %.0f | Margin: %+.0f)",
                                            newOrd->orderId, newOrd->supplierName, bestOrderCost, bestEstProfit),
                                 3);
                SpawnFloatingPopup(680.0f, 210.0f, TextFormat("ORDER #%d (-EUR %.0f)", newOrd->orderId, bestOrderCost), (Color){35, 245, 255, 255});
            }
            else
            {
                snprintf(aiState.aiLastActionLog, sizeof(aiState.aiLastActionLog),
                         "NDAL AI ALERT: %d SKUs low, ALL suppliers offline! Rationing stock via price!",
                         urgentItemsCount);
            }
        }
    }
    (void)itemCount;
}