#ifndef TELEMETRY_H
#define TELEMETRY_H

#include "inventory.h"

#define GRAPH_HISTORY_LEN 40
#define GRAPH_SAMPLE_INTERVAL 1.2f

typedef struct
{
    float price[GRAPH_HISTORY_LEN];
    float cost[GRAPH_HISTORY_LEN];
    float salesVol[GRAPH_HISTORY_LEN];
    float profit[GRAPH_HISTORY_LEN];
    float timeStamp[GRAPH_HISTORY_LEN];
    int count;
    int unitsSoldInWindow;
    int totalUnitsSoldSku;
    float totalRevenueSku;
    float totalProfitSku;
} ProductTelemetry;

void InitTelemetry(InventoryItem inventory[]);
void ClearSkuTelemetry(int skuIdx);
ProductTelemetry *GetProductTelemetry(int skuIdx);
void RecordSkuSaleWindow(int skuIdx, int sold, float saleRev, float saleProf);
void RecordSkuTelemetrySample(int i, const InventoryItem *item, float sampleTime);
void UpdateTelemetrySampler(float dt, float simTime, const InventoryItem inventory[]);

#endif