#ifndef INVENTORY_H
#define INVENTORY_H

#include <stdbool.h>

#define MAX_ITEMS 100
#define MAX_BASKET_ITEMS 50
#define MAX_ORDERS 100

// Updated Inventory Item
typedef struct
{
    int id;
    char name[50];
    int quantity;
    float price;     // Sell price
    float orderCost; // Buy cost
    int maxStock;
    int minStock;
    int inTransit; // Tracks items ordered but not yet delivered
    bool isActive;
} InventoryItem;

// Basket Item Structure
typedef struct
{
    int itemId;
    int orderQuantity;
} BasketItem;

// Order History Structure
typedef enum
{
    STATUS_PENDING,
    STATUS_IN_TRANSIT,
    STATUS_DELIVERED
} OrderStatus;

typedef struct
{
    int orderId;
    BasketItem items[MAX_BASKET_ITEMS];
    int itemCount;
    float totalCost; // Includes the EUR 25 flat fee
    float estimatedProfit;
    OrderStatus status;
    bool isActive;
} OrderRecord;

void SaveInventory(InventoryItem inventory[], int count);
int LoadInventory(InventoryItem inventory[]);
void SaveOrders(OrderRecord orders[], int count);
int LoadOrders(OrderRecord orders[]);

#endif