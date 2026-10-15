#ifndef SUPPLIERS_H
#define SUPPLIERS_H

#include <stdbool.h>
#include "inventory.h"

#define MAX_SUPPLIERS 3

typedef struct
{
    int id;
    char name[40];
    float costMultiplier;
    float deliveryFee;
    float deliveryTime;
    bool hasShortage;
    float shortageTimer;
    float priceSpikeMult;
    float priceSpikeTimer;
} Supplier;

void InitSuppliers(void);
Supplier *GetSuppliers(void);
void UpdateSupplierTimers(float dt);
float GetEffectiveUnitCost(const InventoryItem *item, float subsidyTimer);
void UpdateActiveOrders(float dt, float weatherDelayTimer, InventoryItem inventory[], int itemCount, OrderRecord orders[], int orderCount);

#endif