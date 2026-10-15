#ifndef INVENTORY_H
#define INVENTORY_H

#include <stdbool.h>
#include "raylib.h"

#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080

#define MAX_ITEMS 100
#define MAX_STORE_SKUS 12
#define MAX_BASKET_ITEMS 50
#define MAX_ORDERS 100
#define BASE_FAIR_MARGIN_MULT 1.26f

typedef struct
{
    int id;
    char name[50];
    int quantity;
    float price;     // Retail sell price
    float orderCost; // Base wholesale unit cost
    int maxStock;
    int minStock;
    int inTransit;        // Units currently in transit
    float baseDemandRate; // Base units sold per second
    float saleTimer;      // > 0 if item is on Flash Sale
    bool isActive;
} InventoryItem;

typedef struct
{
    int itemId;
    int orderQuantity;
} BasketItem;

typedef enum
{
    STATUS_PROCESSING,
    STATUS_IN_TRANSIT,
    STATUS_OUT_FOR_DELIVERY,
    STATUS_DELIVERED
} OrderStatus;

typedef struct
{
    int orderId;
    int supplierId;
    char supplierName[40];
    BasketItem items[MAX_BASKET_ITEMS];
    int itemCount;
    float totalCost;
    float estimatedProfit;
    float elapsedTime;
    float totalDeliveryTime;
    bool isDamaged;
    OrderStatus status;
    bool isActive;
} OrderRecord;

void SaveInventory(InventoryItem inventory[], int count);
int LoadInventory(InventoryItem inventory[]);
void SeedDefaultInventory(InventoryItem inventory[], int *count);

int GetActiveSkuCount(const InventoryItem inventory[]);
int GetActiveSlotForInventoryIndex(const InventoryItem inventory[], int invIdx);
int PickRandomActiveInventoryIndex(const InventoryItem inventory[]);
Rectangle GetShelfBayRect(int activeSlot, int totalActiveSkus);

#endif