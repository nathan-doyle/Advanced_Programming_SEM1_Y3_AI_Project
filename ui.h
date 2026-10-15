#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include "raylib.h"
#include "inventory.h"

typedef enum
{
    SCREEN_MENU,         // Live Simulation Telemetry & Digital Twin Arena
    SCREEN_VIEW,         // Inventory Catalog, Analytics Graph & Flash Sales
    SCREEN_ORDER,        // Suppliers Matrix & AI Brain Configuration
    SCREEN_EVENTS,       // Director Chaos Panel & Add/Edit SKU
    SCREEN_ORDER_HISTORY // Live Automated Delivery Progress Bars
} AppScreen;

// Shared Crisp Typography & Card Helpers (used by UI and Arena renderers)
int MeasureTextCrisp(const char *text, int fontSize);
void DrawTextCrisp(const char *text, int x, int y, int fontSize, Color color);
void DrawTextTruncated(const char *text, int x, int y, int fontSize, int maxWidth, Color color);
void DrawTextCenteredInBox(Rectangle box, const char *text, int fontSize, Color color);
void DrawDashboardCard(Rectangle rect, Color bgColor, Color borderColor, float roundness, bool isHovered);
void DrawPillBadge(Rectangle rect, const char *text, Color accentColor, Color bgDark);

void InitSimulation(InventoryItem inventory[], int *itemCount);
void UpdateAndDrawUI(InventoryItem inventory[], int *itemCount, OrderRecord orders[], int *orderCount);

#endif