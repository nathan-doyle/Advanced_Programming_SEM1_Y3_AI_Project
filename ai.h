#ifndef AI_H
#define AI_H

#include "inventory.h"

#define UPKEEP_BILL_INTERVAL 8.0f

typedef struct
{
    float simTime;
    float simSpeed;
    float totalRevenue;
    float totalOrderSpend;
    float totalUpkeepPaid;
    float netProfit;
    int totalUnitsSold;
    int missedSalesCount;
    float upkeepTimer;
    float lastStaffBill;
    float lastUtilityBill;
    float lastStorageBill;
    float lastTotalBill;
    float customerSatisfaction;
    float avgPriceMarkupPct;
    float baseFairPrice[MAX_ITEMS];
    float aiPriceMult[MAX_ITEMS];
    char aiLastActionLog[160];
} AiEconomyState;

void InitAiEconomy(InventoryItem inventory[], int *itemCount);
AiEconomyState *GetAiEconomyState(void);
float GetSatisfactionDemandMultiplier(void);
float GetItemPriceElasticity(int idx);
void AdjustSkuPriceMultiplier(int skuIdx, float delta, InventoryItem *sku, float sampleTime);
void ResetSkuFairPrice(int skuIdx, InventoryItem *sku, bool preservePriceRatio);
void UpdateAiAndEconomyStep(float dt, InventoryItem inventory[], int *itemCount, OrderRecord orders[], int *orderCount);

#endif