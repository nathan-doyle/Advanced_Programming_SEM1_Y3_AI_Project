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
#define MAX_AUDIT_LOGS 64

#ifndef BASE_FAIR_MARGIN_MULT
#define BASE_FAIR_MARGIN_MULT 1.42f
#endif

// Inventory Item with Barcode & Discrepancy Tracking
typedef struct
{
    int id;
    char name[50];
    char barcode[16]; // Unique EAN-13 simulated barcode
    int quantity;     // Actual physical shelf stock
    int expectedQty;  // System book stock (Discrepancy = quantity - expectedQty)
    float price;      // Retail sell price
    float orderCost;  // Base wholesale unit cost
    int maxStock;
    int minStock;
    int inTransit;        // Units currently in transit
    float baseDemandRate; // Base units sold per second
    float saleTimer;      // > 0 if item is on Flash Sale
    bool isActive;
} InventoryItem;

// Staff Discrepancy Audit Record
typedef struct
{
    int auditId;
    char staffName[32];
    char itemName[50];
    char barcode[16];
    int expectedQty;
    int countedQty;
    int discrepancy;       // countedQty - expectedQty
    float financialImpact; // discrepancy * orderCost
    char reason[32];
    float timestamp; // Simulation time when logged
} AuditRecord;

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

// Core Database & Shelf Helpers
void SaveInventory(InventoryItem inventory[], int count);
int LoadInventory(InventoryItem inventory[]);
void SeedDefaultInventory(InventoryItem inventory[], int *count);

int GetActiveSkuCount(const InventoryItem inventory[]);
int GetActiveSlotForInventoryIndex(const InventoryItem inventory[], int invIdx);
int PickRandomActiveInventoryIndex(const InventoryItem inventory[]);
Rectangle GetShelfBayRect(int activeSlot, int totalActiveSkus);

// Barcode, Search, Discrepancy Audit & Shopify Scraper Helpers
void GenerateSimulatedBarcode(int id, const char *name, char outBarcode[16]);
bool ItemMatchesSearch(const InventoryItem *item, const char *query);
int GetTotalDiscrepancyCount(const InventoryItem inventory[], int *outTotalUnitVariance);

AuditRecord *GetAuditLogs(int *outCount);
void LoadAuditLogs(void);
void SaveAuditLogs(void);
void RecordStaffAudit(const char *staffName, InventoryItem *item, const char *reason, float simTime);

int ScrapeOnlineStoreInventory(InventoryItem inventory[], int *itemCount, const char *customUrl, int presetIdx, char *outStatusMsg, int statusMaxLen);

#endif