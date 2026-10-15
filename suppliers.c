#include "suppliers.h"
#include "events.h"
#include "arena.h"
#include "raylib.h"

static Supplier suppliers[MAX_SUPPLIERS];

void InitSuppliers(void)
{
    suppliers[0] = (Supplier){1, "SwiftLogix Express", 1.18f, 95.0f, 8.0f, false, 0.0f, 1.0f, 0.0f};
    suppliers[1] = (Supplier){2, "PrimeCore Direct", 1.04f, 55.0f, 15.0f, false, 0.0f, 1.0f, 0.0f};
    suppliers[2] = (Supplier){3, "EuroBulk Wholesale", 0.94f, 30.0f, 26.0f, false, 0.0f, 1.0f, 0.0f};
}

Supplier *GetSuppliers(void)
{
    return suppliers;
}

void UpdateSupplierTimers(float dt)
{
    for (int s = 0; s < MAX_SUPPLIERS; s++)
    {
        if (suppliers[s].shortageTimer > 0.0f)
        {
            suppliers[s].shortageTimer -= dt;
            if (suppliers[s].shortageTimer <= 0.0f)
            {
                suppliers[s].shortageTimer = 0.0f;
                suppliers[s].hasShortage = false;
                PushNotification(TextFormat("%s supply restored!", suppliers[s].name), 1);
            }
        }
        if (suppliers[s].priceSpikeTimer > 0.0f)
        {
            suppliers[s].priceSpikeTimer -= dt;
            if (suppliers[s].priceSpikeTimer <= 0.0f)
            {
                suppliers[s].priceSpikeTimer = 0.0f;
                suppliers[s].priceSpikeMult = 1.0f;
                PushNotification(TextFormat("%s prices normalized.", suppliers[s].name), 0);
            }
        }
    }
}

float GetEffectiveUnitCost(const InventoryItem *item, float subsidyTimer)
{
    float subMult = (subsidyTimer > 0.0f) ? 0.80f : 1.0f;
    float avgSupSpike = 1.05f;
    int activeSups = 0;
    float sumMult = 0.0f;
    for (int s = 0; s < MAX_SUPPLIERS; s++)
    {
        if (!suppliers[s].hasShortage)
        {
            sumMult += suppliers[s].costMultiplier * suppliers[s].priceSpikeMult;
            activeSups++;
        }
    }
    if (activeSups > 0)
        avgSupSpike = sumMult / (float)activeSups;
    return item->orderCost * avgSupSpike * subMult;
}

void UpdateActiveOrders(float dt, float weatherDelayTimer, InventoryItem inventory[], int itemCount, OrderRecord orders[], int orderCount)
{
    float transitSpeed = (weatherDelayTimer > 0.0f) ? 0.45f : 1.0f;
    for (int o = 0; o < orderCount; o++)
    {
        if (orders[o].isActive && orders[o].status != STATUS_DELIVERED)
        {
            orders[o].elapsedTime += dt * transitSpeed;
            float progress = orders[o].totalDeliveryTime > 0.0f ? (orders[o].elapsedTime / orders[o].totalDeliveryTime) : 1.0f;

            if (progress < 0.20f)
                orders[o].status = STATUS_PROCESSING;
            else if (progress < 0.72f)
                orders[o].status = STATUS_IN_TRANSIT;
            else if (progress < 1.00f)
                orders[o].status = STATUS_OUT_FOR_DELIVERY;
            else
            {
                orders[o].elapsedTime = orders[o].totalDeliveryTime;
                orders[o].status = STATUS_DELIVERED;

                int totalArrived = 0;
                int totalLost = 0;
                for (int b = 0; b < orders[o].itemCount; b++)
                {
                    for (int i = 0; i < MAX_ITEMS; i++)
                    {
                        if (inventory[i].isActive && inventory[i].id == orders[o].items[b].itemId)
                        {
                            int orderedQty = orders[o].items[b].orderQuantity;
                            inventory[i].inTransit -= orderedQty;
                            if (inventory[i].inTransit < 0)
                                inventory[i].inTransit = 0;

                            int receivedQty = orderedQty;
                            if (orders[o].isDamaged)
                            {
                                receivedQty = (int)(orderedQty * 0.60f);
                                totalLost += (orderedQty - receivedQty);
                            }
                            inventory[i].quantity += receivedQty;
                            if (inventory[i].quantity > inventory[i].maxStock)
                            {
                                inventory[i].quantity = inventory[i].maxStock;
                            }
                            totalArrived += receivedQty;
                        }
                    }
                }
                SaveInventory(inventory, itemCount);
                TriggerDockUnloading(&orders[o], totalArrived, inventory);

                if (orders[o].isDamaged)
                {
                    PushNotification(TextFormat("Order #%d arrived DAMAGED! +%d units (%d lost)", orders[o].orderId, totalArrived, totalLost), 2);
                    SpawnFloatingPopup(480.0f, 345.0f, TextFormat("DAMAGED! -%d UNITS", totalLost), (Color){255, 80, 100, 255});
                }
                else
                {
                    PushNotification(TextFormat("Order #%d DELIVERED from %s (+%d units)", orders[o].orderId, orders[o].supplierName, totalArrived), 1);
                    SpawnFloatingPopup(480.0f, 345.0f, TextFormat("+%d UNITS ARRIVED", totalArrived), (Color){55, 250, 155, 255});
                }
            }
        }
    }
}