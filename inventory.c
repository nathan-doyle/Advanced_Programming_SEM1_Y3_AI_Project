#include "inventory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void SaveInventory(InventoryItem inventory[], int count)
{
    FILE *file = fopen("inventory_db.csv", "w");
    if (file == NULL)
        return;
    for (int i = 0; i < count; i++)
    {
        fprintf(file, "%d,%s,%d,%.2f,%.2f,%d,%d,%d,%.2f,%d\n",
                inventory[i].id, inventory[i].name,
                inventory[i].quantity, inventory[i].price,
                inventory[i].orderCost, inventory[i].maxStock,
                inventory[i].minStock, inventory[i].inTransit,
                inventory[i].baseDemandRate,
                inventory[i].isActive ? 1 : 0);
    }
    fclose(file);
}

int LoadInventory(InventoryItem inventory[])
{
    FILE *file = fopen("inventory_db.csv", "r");
    if (file == NULL)
        return 0;

    int count = 0;
    int activeFlag = 0;

    while (count < MAX_ITEMS && fscanf(file, "%d,%49[^,],%d,%f,%f,%d,%d,%d,%f,%d\n",
                                       &inventory[count].id, inventory[count].name,
                                       &inventory[count].quantity, &inventory[count].price,
                                       &inventory[count].orderCost, &inventory[count].maxStock,
                                       &inventory[count].minStock, &inventory[count].inTransit,
                                       &inventory[count].baseDemandRate,
                                       &activeFlag) == 10)
    {

        inventory[count].isActive = (activeFlag == 1);
        inventory[count].inTransit = 0;
        inventory[count].saleTimer = 0.0f;
        if (inventory[count].baseDemandRate <= 0.05f)
            inventory[count].baseDemandRate = 0.28f;
        count++;
    }
    fclose(file);
    return count;
}

void SeedDefaultInventory(InventoryItem inventory[], int *count)
{
    *count = 6;
    inventory[0] = (InventoryItem){1, "RTX-4080 Core", 12, 516.6f, 410.0f, 30, 10, 0, 0.24f, 0.0f, true};
    inventory[1] = (InventoryItem){2, "DDR5-64GB Kit", 14, 138.6f, 110.0f, 36, 12, 0, 0.32f, 0.0f, true};
    inventory[2] = (InventoryItem){3, "NVMe 2TB Gen5", 15, 103.3f, 82.0f, 40, 13, 0, 0.36f, 0.0f, true};
    inventory[3] = (InventoryItem){4, "850W Gold PSU", 12, 94.5f, 75.0f, 32, 10, 0, 0.28f, 0.0f, true};
    inventory[4] = (InventoryItem){5, "AM5 Logic Board", 11, 186.5f, 148.0f, 28, 9, 0, 0.26f, 0.0f, true};
    inventory[5] = (InventoryItem){6, "Liquid Cooler", 14, 80.6f, 64.0f, 36, 12, 0, 0.32f, 0.0f, true};
    SaveInventory(inventory, *count);
}

int GetActiveSkuCount(const InventoryItem inventory[])
{
    int count = 0;
    for (int i = 0; i < MAX_ITEMS; i++)
    {
        if (inventory[i].isActive)
            count++;
    }
    if (count > MAX_STORE_SKUS)
        count = MAX_STORE_SKUS;
    return count;
}

int GetActiveSlotForInventoryIndex(const InventoryItem inventory[], int invIdx)
{
    int slot = 0;
    for (int i = 0; i < MAX_ITEMS; i++)
    {
        if (inventory[i].isActive)
        {
            if (i == invIdx)
                return (slot < MAX_STORE_SKUS) ? slot : (slot % MAX_STORE_SKUS);
            slot++;
        }
    }
    return 0;
}

int PickRandomActiveInventoryIndex(const InventoryItem inventory[])
{
    int activeIndices[MAX_STORE_SKUS];
    int count = 0;
    for (int i = 0; i < MAX_ITEMS && count < MAX_STORE_SKUS; i++)
    {
        if (inventory[i].isActive)
        {
            activeIndices[count++] = i;
        }
    }
    if (count == 0)
        return 0;
    return activeIndices[GetRandomValue(0, count - 1)];
}

Rectangle GetShelfBayRect(int activeSlot, int totalActiveSkus)
{
    if (activeSlot < 0)
        activeSlot = 0;
    if (activeSlot >= MAX_STORE_SKUS)
        activeSlot %= MAX_STORE_SKUS;

    if (totalActiveSkus <= 6)
    {
        int rows = (totalActiveSkus > 0) ? totalActiveSkus : 6;
        float availH = 576.0f;
        float stepY = availH / (float)rows;
        float bayH = stepY - 12.0f;
        if (bayH > 86.0f)
            bayH = 86.0f;
        float sy = 326.0f + activeSlot * stepY;
        return (Rectangle){766.0f, sy, 152.0f, bayH};
    }
    else
    {
        int col = activeSlot / 6;
        int row = activeSlot % 6;
        float sy = 326.0f + row * 96.0f;
        float sx = (col == 0) ? 744.0f : 926.0f;
        return (Rectangle){sx, sy, 128.0f, 84.0f};
    }
}