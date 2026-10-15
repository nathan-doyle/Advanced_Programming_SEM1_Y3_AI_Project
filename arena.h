#ifndef ARENA_H
#define ARENA_H

#include <stdbool.h>
#include "raylib.h"
#include "inventory.h"

void InitArena(void);
Color GetSkuBoxColor(int idx);
void SpawnFloatingPopup(float x, float y, const char *txt, Color col);
void SpawnShopperAgent(int skuIdx, int units, float saleEur, float markupPct, bool stockoutAngry);
void TriggerDockUnloading(OrderRecord *ord, int totalUnits, const InventoryItem inventory[]);
void UpdateArenaAnimations(float dt, const InventoryItem inventory[]);
void DrawCentralDigitalTwinArena(Rectangle arena, InventoryItem inventory[], int itemCount, OrderRecord orders[], int orderCount);

#endif