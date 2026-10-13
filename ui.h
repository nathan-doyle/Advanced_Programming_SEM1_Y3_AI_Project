#ifndef UI_H
#define UI_H

#include "inventory.h"

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

typedef enum
{
    SCREEN_MENU,
    SCREEN_VIEW,
    SCREEN_ADD,
    SCREEN_EDIT,
    SCREEN_ORDER,
    SCREEN_ORDER_HISTORY
} AppScreen;

void UpdateAndDrawUI(InventoryItem inventory[], int *itemCount, OrderRecord orders[], int *orderCount);

#endif