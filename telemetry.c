#include "telemetry.h"
#include "suppliers.h"
#include "events.h"
#include "ai.h"
#include <string.h>
#include <math.h>

static ProductTelemetry skuTelemetry[MAX_ITEMS];
static float graphSampleTimer = 0.0f;

void ClearSkuTelemetry(int skuIdx)
{
    if (skuIdx >= 0 && skuIdx < MAX_ITEMS)
    {
        memset(&skuTelemetry[skuIdx], 0, sizeof(ProductTelemetry));
    }
}

ProductTelemetry *GetProductTelemetry(int skuIdx)
{
    if (skuIdx < 0 || skuIdx >= MAX_ITEMS)
        return &skuTelemetry[0];
    return &skuTelemetry[skuIdx];
}

void RecordSkuSaleWindow(int skuIdx, int sold, float saleRev, float saleProf)
{
    if (skuIdx < 0 || skuIdx >= MAX_ITEMS)
        return;
    skuTelemetry[skuIdx].unitsSoldInWindow += sold;
    skuTelemetry[skuIdx].totalUnitsSoldSku += sold;
    skuTelemetry[skuIdx].totalRevenueSku += saleRev;
    skuTelemetry[skuIdx].totalProfitSku += saleProf;
}

void RecordSkuTelemetrySample(int i, const InventoryItem *item, float sampleTime)
{
    if (i < 0 || i >= MAX_ITEMS)
        return;
    ProductTelemetry *t = &skuTelemetry[i];
    ChaosEventState *ev = GetEventsState();

    float effPrice = (item->saleTimer > 0.0f) ? (item->price * 0.88f) : item->price;
    float effCost = GetEffectiveUnitCost(item, ev->subsidyTimer);

    float elast = GetItemPriceElasticity(i);
    float vel = item->baseDemandRate * GetSatisfactionDemandMultiplier() * elast *
                (item->saleTimer > 0.0f ? 2.2f : 1.0f) * (ev->viralSurgeTimer > 0.0f ? 2.5f : 1.0f);
    if (item->quantity == 0)
        vel = 0.0f;

    float volSample = (float)t->unitsSoldInWindow + (vel * GRAPH_SAMPLE_INTERVAL);
    t->unitsSoldInWindow = 0;

    float unitMargin = effPrice - effCost;
    float profitSample = unitMargin * (volSample > 0.25f ? volSample : 0.45f);

    if (t->count < GRAPH_HISTORY_LEN)
    {
        int idx = t->count++;
        t->price[idx] = effPrice;
        t->cost[idx] = effCost;
        t->salesVol[idx] = volSample;
        t->profit[idx] = profitSample;
        t->timeStamp[idx] = sampleTime;
    }
    else
    {
        for (int k = 0; k < GRAPH_HISTORY_LEN - 1; k++)
        {
            t->price[k] = t->price[k + 1];
            t->cost[k] = t->cost[k + 1];
            t->salesVol[k] = t->salesVol[k + 1];
            t->profit[k] = t->profit[k + 1];
            t->timeStamp[k] = t->timeStamp[k + 1];
        }
        t->price[GRAPH_HISTORY_LEN - 1] = effPrice;
        t->cost[GRAPH_HISTORY_LEN - 1] = effCost;
        t->salesVol[GRAPH_HISTORY_LEN - 1] = volSample;
        t->profit[GRAPH_HISTORY_LEN - 1] = profitSample;
        t->timeStamp[GRAPH_HISTORY_LEN - 1] = sampleTime;
    }
}

void InitTelemetry(InventoryItem inventory[])
{
    graphSampleTimer = 0.0f;
    AiEconomyState *ai = GetAiEconomyState();

    for (int i = 0; i < MAX_ITEMS; i++)
    {
        ClearSkuTelemetry(i);
        if (inventory[i].isActive)
        {
            for (int k = 0; k < 10; k++)
            {
                float wave = sinf((float)(k + i) * 0.7f) * 0.02f;
                ai->aiPriceMult[i] = 1.0f + wave;
                inventory[i].price = ai->baseFairPrice[i] * ai->aiPriceMult[i];
                skuTelemetry[i].unitsSoldInWindow = (k % 3 == 0) ? 1 : 0;
                RecordSkuTelemetrySample(i, &inventory[i], (float)(k - 10) * GRAPH_SAMPLE_INTERVAL);
            }
            ai->aiPriceMult[i] = 1.0f;
            inventory[i].price = ai->baseFairPrice[i];
        }
    }
}

void UpdateTelemetrySampler(float dt, float simTime, const InventoryItem inventory[])
{
    graphSampleTimer += dt;
    if (graphSampleTimer >= GRAPH_SAMPLE_INTERVAL)
    {
        graphSampleTimer -= GRAPH_SAMPLE_INTERVAL;
        for (int i = 0; i < MAX_ITEMS; i++)
        {
            if (inventory[i].isActive)
            {
                RecordSkuTelemetrySample(i, &inventory[i], simTime);
            }
        }
    }
}