#include "ui.h"
#include "raylib.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Shared UI State Variables
static AppScreen currentScreen = SCREEN_MENU;
static int editTargetIndex = -1;
static int activeInput = 0;
static char nameInput[50] = "\0";
static char qtyInput[10] = "\0";
static char priceInput[10] = "\0";
static char costInput[10] = "\0";
static char maxInput[10] = "\0";
static char minInput[10] = "\0";
static int nameCount = 0, qtyCount = 0, priceCount = 0, costCount = 0, maxCount = 0, minCount = 0;

static int scrollIndex = 0;
static int scrollLowStock = 0;
static int scrollAllStock = 0;
static int scrollHistory = 0;

static BasketItem currentBasket[MAX_BASKET_ITEMS];
static int basketCount = 0;
static int customOrderQty[MAX_ITEMS] = {0};

// Tracks which slider is actively being dragged
static int activeSliderIndex = -1;

// Helper: Safely add items to basket capping at MaxStock
static void AddToBasket(int itemId, int qtyToAdd, InventoryItem inventory[])
{
    if (qtyToAdd <= 0)
        return;

    int invIdx = -1;
    for (int i = 0; i < MAX_ITEMS; i++)
        if (inventory[i].id == itemId)
            invIdx = i;
    if (invIdx == -1)
        return;

    int existingIdx = -1;
    for (int i = 0; i < basketCount; i++)
        if (currentBasket[i].itemId == itemId)
            existingIdx = i;

    int currentInBasket = (existingIdx != -1) ? currentBasket[existingIdx].orderQuantity : 0;
    int maxAllowed = inventory[invIdx].maxStock - (inventory[invIdx].quantity + inventory[invIdx].inTransit);

    int allowedToAdd = qtyToAdd;
    if (currentInBasket + allowedToAdd > maxAllowed)
    {
        allowedToAdd = maxAllowed - currentInBasket;
    }

    if (allowedToAdd > 0)
    {
        if (existingIdx != -1)
        {
            currentBasket[existingIdx].orderQuantity += allowedToAdd;
        }
        else if (basketCount < MAX_BASKET_ITEMS)
        {
            currentBasket[basketCount].itemId = itemId;
            currentBasket[basketCount].orderQuantity = allowedToAdd;
            basketCount++;
        }
    }
}

void UpdateAndDrawUI(InventoryItem inventory[], int *itemCount, OrderRecord orders[], int *orderCount)
{
    Color bgDark = (Color){20, 20, 24, 255};
    Color panelDark = (Color){40, 40, 45, 255};
    Color panelHover = (Color){60, 60, 65, 255};
    Color textYellow = (Color){255, 255, 200, 255};
    Color textDim = (Color){180, 180, 150, 255};
    Color highlightCyan = (Color){0, 210, 210, 255};
    Color highlightPurple = (Color){150, 80, 220, 255};
    Color barRed = (Color){230, 41, 55, 255};
    Color textGreen = (Color){0, 220, 0, 255};

    Rectangle backBtn = {20, 20, 100, 40};

    float scale = ((float)GetScreenWidth() / SCREEN_WIDTH < (float)GetScreenHeight() / SCREEN_HEIGHT) ? ((float)GetScreenWidth() / SCREEN_WIDTH) : ((float)GetScreenHeight() / SCREEN_HEIGHT);
    float offsetX = (GetScreenWidth() - (SCREEN_WIDTH * scale)) * 0.5f;
    float offsetY = (GetScreenHeight() - (SCREEN_HEIGHT * scale)) * 0.5f;

    Vector2 physicalMouse = GetMousePosition();
    Vector2 mousePoint = {(physicalMouse.x - offsetX) / scale, (physicalMouse.y - offsetY) / scale};
    int wheel = (int)GetMouseWheelMove();

    ClearBackground(bgDark);

    switch (currentScreen)
    {
    case SCREEN_MENU:
    {
        DrawText("INVENTORY DASHBOARD", SCREEN_WIDTH / 2 - MeasureText("INVENTORY DASHBOARD", 30) / 2, 60, 30, textYellow);

        Rectangle viewBtn = {SCREEN_WIDTH / 2 - 150, 140, 300, 50};
        Rectangle addBtn = {SCREEN_WIDTH / 2 - 150, 210, 300, 50};
        Rectangle orderBtn = {SCREEN_WIDTH / 2 - 150, 280, 300, 50};
        Rectangle histBtn = {SCREEN_WIDTH / 2 - 150, 350, 300, 50};

        bool hView = CheckCollisionPointRec(mousePoint, viewBtn);
        DrawRectangleRec(viewBtn, hView ? panelHover : panelDark);
        DrawRectangleLinesEx(viewBtn, 2, hView ? highlightCyan : panelDark);
        DrawText("View Inventory", viewBtn.x + 60, viewBtn.y + 15, 20, textYellow);
        if (hView && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            currentScreen = SCREEN_VIEW;

        bool hAdd = CheckCollisionPointRec(mousePoint, addBtn);
        DrawRectangleRec(addBtn, hAdd ? panelHover : panelDark);
        DrawRectangleLinesEx(addBtn, 2, hAdd ? highlightPurple : panelDark);
        DrawText("Add New Item", addBtn.x + 70, addBtn.y + 15, 20, textYellow);
        if (hAdd && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            currentScreen = SCREEN_ADD;

        bool hOrder = CheckCollisionPointRec(mousePoint, orderBtn);
        DrawRectangleRec(orderBtn, hOrder ? panelHover : panelDark);
        DrawRectangleLinesEx(orderBtn, 2, hOrder ? highlightCyan : panelDark);
        DrawText("Order Stock", orderBtn.x + 80, orderBtn.y + 15, 20, textYellow);
        if (hOrder && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            currentScreen = SCREEN_ORDER;

        bool hHist = CheckCollisionPointRec(mousePoint, histBtn);
        DrawRectangleRec(histBtn, hHist ? panelHover : panelDark);
        DrawRectangleLinesEx(histBtn, 2, hHist ? highlightPurple : panelDark);
        DrawText("Order History", histBtn.x + 75, histBtn.y + 15, 20, textYellow);
        if (hHist && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            currentScreen = SCREEN_ORDER_HISTORY;
        break;
    }

    case SCREEN_VIEW:
    {
        DrawText("CURRENT INVENTORY", SCREEN_WIDTH / 2 - MeasureText("CURRENT INVENTORY", 30) / 2, 25, 30, textYellow);

        bool hoverBack = CheckCollisionPointRec(mousePoint, backBtn);
        DrawRectangleRec(backBtn, hoverBack ? panelHover : panelDark);
        DrawRectangleLinesEx(backBtn, 2, hoverBack ? highlightCyan : panelDark);
        DrawText("BACK", backBtn.x + 25, backBtn.y + 10, 20, textYellow);
        if (hoverBack && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            currentScreen = SCREEN_MENU;
            scrollIndex = 0;
        }

        int totalActive = 0;
        for (int i = 0; i < MAX_ITEMS; i++)
            if (inventory[i].isActive)
                totalActive++;

        if (wheel != 0)
            scrollIndex -= wheel;
        if (scrollIndex > totalActive - 9)
            scrollIndex = totalActive - 9;
        if (scrollIndex < 0)
            scrollIndex = 0;

        int yOffset = 90;

        // ADJUSTED: Re-spaced columns to prevent "Qty(In Transit)" from hitting "Price"[cite: 9]
        DrawText("ID", 20, yOffset, 20, textDim);
        DrawText("Name", 70, yOffset, 20, textDim);
        DrawText("Qty(In Transit)", 230, yOffset, 20, textDim);
        DrawText("Price", 420, yOffset, 20, textDim);
        DrawText("Status", 540, yOffset, 20, textDim);
        yOffset += 40;

        int activeCount = 0;
        int drawnCount = 0;

        for (int i = 0; i < MAX_ITEMS; i++)
        {
            if (inventory[i].isActive)
            {
                if (activeCount >= scrollIndex && drawnCount < 9)
                {

                    DrawText(TextFormat("%d", inventory[i].id), 20, yOffset + 5, 20, textYellow);
                    DrawText(inventory[i].name, 70, yOffset + 5, 20, textYellow);
                    DrawText(TextFormat("%d (+%d)", inventory[i].quantity, inventory[i].inTransit), 230, yOffset + 5, 20, textYellow);
                    DrawText(TextFormat("EUR %.2f", inventory[i].price), 420, yOffset + 5, 20, textYellow);

                    int max = inventory[i].maxStock > 0 ? inventory[i].maxStock : 1;
                    float fillRatio = (float)inventory[i].quantity / max;
                    if (fillRatio > 1.0f)
                        fillRatio = 1.0f;

                    Color barColor = inventory[i].quantity <= inventory[i].minStock ? barRed : (fillRatio < 0.70f ? highlightPurple : highlightCyan);
                    DrawRectangle(540, yOffset + 5, 80 * fillRatio, 20, barColor);
                    DrawRectangleLines(540, yOffset + 5, 80, 20, textDim);

                    Rectangle editBtn = {635, yOffset, 70, 30};
                    bool hEdit = CheckCollisionPointRec(mousePoint, editBtn);
                    DrawRectangleRec(editBtn, hEdit ? panelHover : panelDark);
                    DrawRectangleLinesEx(editBtn, 2, highlightCyan);
                    DrawText("EDIT", editBtn.x + 15, editBtn.y + 8, 15, hEdit ? highlightCyan : textDim);
                    if (hEdit && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                    {
                        currentScreen = SCREEN_EDIT;
                        editTargetIndex = i;
                        strcpy(nameInput, inventory[i].name);
                        nameCount = strlen(nameInput);
                        sprintf(qtyInput, "%d", inventory[i].quantity);
                        qtyCount = strlen(qtyInput);
                        sprintf(priceInput, "%.2f", inventory[i].price);
                        priceCount = strlen(priceInput);
                        sprintf(costInput, "%.2f", inventory[i].orderCost);
                        costCount = strlen(costInput);
                        sprintf(maxInput, "%d", inventory[i].maxStock);
                        maxCount = strlen(maxInput);
                        sprintf(minInput, "%d", inventory[i].minStock);
                        minCount = strlen(minInput);
                    }

                    Rectangle deleteBtn = {715, yOffset, 75, 30};
                    bool hDel = CheckCollisionPointRec(mousePoint, deleteBtn);
                    DrawRectangleRec(deleteBtn, hDel ? panelHover : panelDark);
                    DrawRectangleLinesEx(deleteBtn, 2, highlightPurple);
                    DrawText("DELETE", deleteBtn.x + 12, deleteBtn.y + 8, 15, hDel ? highlightPurple : textDim);
                    if (hDel && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                    {
                        inventory[i].isActive = false;
                        SaveInventory(inventory, *itemCount);
                    }

                    yOffset += 45;
                    drawnCount++;
                }
                activeCount++;
            }
        }
        break;
    }

    case SCREEN_ADD:
    case SCREEN_EDIT:
    {
        const char *titleText = currentScreen == SCREEN_ADD ? "ADD NEW ITEM" : "EDIT ITEM";
        DrawText(titleText, SCREEN_WIDTH / 2 - MeasureText(titleText, 30) / 2, 25, 30, textYellow);

        bool hoverBack = CheckCollisionPointRec(mousePoint, backBtn);
        DrawRectangleRec(backBtn, hoverBack ? panelHover : panelDark);
        DrawRectangleLinesEx(backBtn, 2, hoverBack ? highlightCyan : panelDark);
        DrawText("BACK", backBtn.x + 25, backBtn.y + 10, 20, textYellow);

        if (hoverBack && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            nameInput[0] = '\0';
            qtyInput[0] = '\0';
            priceInput[0] = '\0';
            costInput[0] = '\0';
            maxInput[0] = '\0';
            minInput[0] = '\0';
            nameCount = 0;
            qtyCount = 0;
            priceCount = 0;
            costCount = 0;
            maxCount = 0;
            minCount = 0;
            currentScreen = SCREEN_MENU;
        }

        Rectangle nameBox = {280, 100, 300, 40};
        Rectangle qtyBox = {280, 150, 300, 40};
        Rectangle priceBox = {280, 200, 300, 40};
        Rectangle costBox = {280, 250, 300, 40};
        Rectangle maxBox = {280, 300, 300, 40};
        Rectangle minBox = {280, 350, 300, 40};
        Rectangle saveBtn = {280, 420, 200, 40};

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            if (CheckCollisionPointRec(mousePoint, nameBox))
                activeInput = 0;
            else if (CheckCollisionPointRec(mousePoint, qtyBox))
                activeInput = 1;
            else if (CheckCollisionPointRec(mousePoint, priceBox))
                activeInput = 2;
            else if (CheckCollisionPointRec(mousePoint, costBox))
                activeInput = 3;
            else if (CheckCollisionPointRec(mousePoint, maxBox))
                activeInput = 4;
            else if (CheckCollisionPointRec(mousePoint, minBox))
                activeInput = 5;
        }

        int key = GetCharPressed();
        while (key > 0)
        {
            if ((key >= 32) && (key <= 125))
            {
                if (activeInput == 0 && nameCount < 49)
                {
                    nameInput[nameCount] = (char)key;
                    nameInput[nameCount + 1] = '\0';
                    nameCount++;
                }
                else if (activeInput == 1 && qtyCount < 9 && key >= '0' && key <= '9')
                {
                    qtyInput[qtyCount] = (char)key;
                    qtyInput[qtyCount + 1] = '\0';
                    qtyCount++;
                }
                else if (activeInput == 2 && priceCount < 9 && ((key >= '0' && key <= '9') || key == '.'))
                {
                    priceInput[priceCount] = (char)key;
                    priceInput[priceCount + 1] = '\0';
                    priceCount++;
                }
                else if (activeInput == 3 && costCount < 9 && ((key >= '0' && key <= '9') || key == '.'))
                {
                    costInput[costCount] = (char)key;
                    costInput[costCount + 1] = '\0';
                    costCount++;
                }
                else if (activeInput == 4 && maxCount < 9 && key >= '0' && key <= '9')
                {
                    maxInput[maxCount] = (char)key;
                    maxInput[maxCount + 1] = '\0';
                    maxCount++;
                }
                else if (activeInput == 5 && minCount < 9 && key >= '0' && key <= '9')
                {
                    minInput[minCount] = (char)key;
                    minInput[minCount + 1] = '\0';
                    minCount++;
                }
            }
            key = GetCharPressed();
        }

        if (IsKeyPressed(KEY_BACKSPACE))
        {
            if (activeInput == 0 && nameCount > 0)
            {
                nameCount--;
                nameInput[nameCount] = '\0';
            }
            else if (activeInput == 1 && qtyCount > 0)
            {
                qtyCount--;
                qtyInput[qtyCount] = '\0';
            }
            else if (activeInput == 2 && priceCount > 0)
            {
                priceCount--;
                priceInput[priceCount] = '\0';
            }
            else if (activeInput == 3 && costCount > 0)
            {
                costCount--;
                costInput[costCount] = '\0';
            }
            else if (activeInput == 4 && maxCount > 0)
            {
                maxCount--;
                maxInput[maxCount] = '\0';
            }
            else if (activeInput == 5 && minCount > 0)
            {
                minCount--;
                minInput[minCount] = '\0';
            }
        }

        DrawText("Name:", 120, 110, 20, textDim);
        DrawRectangleRec(nameBox, panelDark);
        DrawRectangleLinesEx(nameBox, 2, activeInput == 0 ? highlightCyan : panelHover);
        DrawText(nameInput, nameBox.x + 5, nameBox.y + 10, 20, textYellow);
        DrawText("Qty:", 120, 160, 20, textDim);
        DrawRectangleRec(qtyBox, panelDark);
        DrawRectangleLinesEx(qtyBox, 2, activeInput == 1 ? highlightCyan : panelHover);
        DrawText(qtyInput, qtyBox.x + 5, qtyBox.y + 10, 20, textYellow);
        DrawText("Sell Price:", 120, 210, 20, textDim);
        DrawRectangleRec(priceBox, panelDark);
        DrawRectangleLinesEx(priceBox, 2, activeInput == 2 ? highlightCyan : panelHover);
        DrawText(priceInput, priceBox.x + 5, priceBox.y + 10, 20, textYellow);
        DrawText("Order Cost:", 120, 260, 20, textDim);
        DrawRectangleRec(costBox, panelDark);
        DrawRectangleLinesEx(costBox, 2, activeInput == 3 ? highlightCyan : panelHover);
        DrawText(costInput, costBox.x + 5, costBox.y + 10, 20, textYellow);
        DrawText("Max:", 120, 310, 20, textDim);
        DrawRectangleRec(maxBox, panelDark);
        DrawRectangleLinesEx(maxBox, 2, activeInput == 4 ? highlightCyan : panelHover);
        DrawText(maxInput, maxBox.x + 5, maxBox.y + 10, 20, textYellow);
        DrawText("Min:", 120, 360, 20, textDim);
        DrawRectangleRec(minBox, panelDark);
        DrawRectangleLinesEx(minBox, 2, activeInput == 5 ? highlightCyan : panelHover);
        DrawText(minInput, minBox.x + 5, minBox.y + 10, 20, textYellow);

        bool hSave = CheckCollisionPointRec(mousePoint, saveBtn);
        DrawRectangleRec(saveBtn, hSave ? highlightCyan : highlightPurple);
        DrawText(currentScreen == SCREEN_ADD ? "SAVE ITEM" : "UPDATE ITEM", saveBtn.x + 40, saveBtn.y + 10, 20, hSave ? bgDark : textYellow);

        if (hSave && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && nameCount > 0)
        {
            if (currentScreen == SCREEN_ADD && *itemCount < MAX_ITEMS)
            {
                inventory[*itemCount].id = *itemCount + 1;
                strcpy(inventory[*itemCount].name, nameInput);
                inventory[*itemCount].quantity = atoi(qtyInput);
                inventory[*itemCount].price = (float)atof(priceInput);
                inventory[*itemCount].orderCost = (float)atof(costInput);
                inventory[*itemCount].maxStock = atoi(maxInput);
                inventory[*itemCount].minStock = atoi(minInput);
                inventory[*itemCount].inTransit = 0;
                inventory[*itemCount].isActive = true;
                (*itemCount)++;
            }
            else if (currentScreen == SCREEN_EDIT && editTargetIndex >= 0)
            {
                strcpy(inventory[editTargetIndex].name, nameInput);
                inventory[editTargetIndex].quantity = atoi(qtyInput);
                inventory[editTargetIndex].price = (float)atof(priceInput);
                inventory[editTargetIndex].orderCost = (float)atof(costInput);
                inventory[editTargetIndex].maxStock = atoi(maxInput);
                inventory[editTargetIndex].minStock = atoi(minInput);
            }
            SaveInventory(inventory, *itemCount);
            nameInput[0] = '\0';
            qtyInput[0] = '\0';
            priceInput[0] = '\0';
            costInput[0] = '\0';
            maxInput[0] = '\0';
            minInput[0] = '\0';
            nameCount = 0;
            qtyCount = 0;
            priceCount = 0;
            costCount = 0;
            maxCount = 0;
            minCount = 0;
            currentScreen = SCREEN_MENU;
        }
        break;
    }

    case SCREEN_ORDER:
    {
        DrawText("ORDER STOCK", SCREEN_WIDTH / 2 - MeasureText("ORDER STOCK", 30) / 2, 15, 30, textYellow);

        bool hoverBack = CheckCollisionPointRec(mousePoint, backBtn);
        DrawRectangleRec(backBtn, hoverBack ? panelHover : panelDark);
        DrawRectangleLinesEx(backBtn, 2, hoverBack ? highlightCyan : panelDark);
        DrawText("BACK", backBtn.x + 25, backBtn.y + 10, 20, textYellow);
        if (hoverBack && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            currentScreen = SCREEN_MENU;
        }

        Rectangle leftPanel = {20, 60, 360, 400};
        Rectangle rightPanel = {400, 60, 380, 400};

        if (wheel != 0)
        {
            if (CheckCollisionPointRec(mousePoint, leftPanel))
                scrollLowStock -= wheel;
            if (CheckCollisionPointRec(mousePoint, rightPanel))
                scrollAllStock -= wheel;
        }
        if (scrollLowStock < 0)
            scrollLowStock = 0;
        if (scrollAllStock < 0)
            scrollAllStock = 0;

        DrawText("LOW STOCK (<= MIN)", 30, 70, 20, barRed);
        DrawText("ALL PRODUCTS", 410, 70, 20, highlightCyan);

        // Left Panel (Low Stock)
        int drawnLeft = 0, activeLeft = 0;
        for (int i = 0; i < MAX_ITEMS; i++)
        {
            if (inventory[i].isActive && inventory[i].quantity <= inventory[i].minStock)
            {
                if (activeLeft >= scrollLowStock && drawnLeft < 7)
                {
                    int y = 110 + drawnLeft * 50;
                    DrawText(inventory[i].name, 30, y, 20, textYellow);
                    DrawText(TextFormat("%d/%d", inventory[i].quantity, inventory[i].minStock), 150, y, 20, barRed);

                    int inBasket = 0;
                    for (int b = 0; b < basketCount; b++)
                        if (currentBasket[b].itemId == inventory[i].id)
                            inBasket = currentBasket[b].orderQuantity;
                    int maxAllowed = inventory[i].maxStock - (inventory[i].quantity + inventory[i].inTransit + inBasket);

                    Rectangle btn = {240, y - 5, 120, 30};
                    bool hBtn = CheckCollisionPointRec(mousePoint, btn);
                    DrawRectangleRec(btn, hBtn ? panelHover : panelDark);
                    DrawRectangleLinesEx(btn, 1, maxAllowed > 0 ? highlightCyan : textDim);
                    DrawText(TextFormat("Add %d", maxAllowed), btn.x + 10, btn.y + 5, 20, maxAllowed > 0 ? textYellow : textDim);

                    if (hBtn && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && maxAllowed > 0)
                        AddToBasket(inventory[i].id, maxAllowed, inventory);
                    drawnLeft++;
                }
                activeLeft++;
            }
        }

        // Right Panel (All Products Custom Order)
        int drawnRight = 0, activeRight = 0;
        for (int i = 0; i < MAX_ITEMS; i++)
        {
            if (inventory[i].isActive)
            {
                if (activeRight >= scrollAllStock && drawnRight < 7)
                {
                    int y = 110 + drawnRight * 50;
                    DrawText(inventory[i].name, 410, y, 20, textYellow);

                    int inBasket = 0;
                    for (int b = 0; b < basketCount; b++)
                        if (currentBasket[b].itemId == inventory[i].id)
                            inBasket = currentBasket[b].orderQuantity;
                    int maxAllowed = inventory[i].maxStock - (inventory[i].quantity + inventory[i].inTransit + inBasket);

                    if (customOrderQty[i] > maxAllowed)
                        customOrderQty[i] = maxAllowed;

                    DrawText("0", 510, y, 20, textDim);
                    DrawText(TextFormat("%d", maxAllowed), 655, y, 20, textDim);

                    Rectangle track = {535, y + 5, 100, 10};
                    DrawRectangleRec(track, panelDark);
                    float fill = maxAllowed > 0 ? (float)customOrderQty[i] / maxAllowed : 0.0f;
                    Rectangle handle = {track.x + (fill * track.width) - 5, track.y - 5, 10, 20};
                    DrawRectangleRec(handle, highlightPurple);

                    Rectangle hitBox = {track.x, track.y - 10, track.width, 30};

                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePoint, hitBox) && maxAllowed > 0)
                    {
                        activeSliderIndex = i;
                    }

                    if (activeSliderIndex == i)
                    {
                        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) || !IsMouseButtonDown(MOUSE_BUTTON_LEFT))
                        {
                            activeSliderIndex = -1;
                        }
                        else
                        {
                            float pct = (mousePoint.x - track.x) / track.width;
                            if (pct < 0.0f)
                                pct = 0.0f;
                            if (pct > 1.0f)
                                pct = 1.0f;
                            customOrderQty[i] = (int)(pct * maxAllowed);

                            fill = (float)customOrderQty[i] / maxAllowed;
                            int floatingX = track.x + (fill * track.width) - MeasureText(TextFormat("%d", customOrderQty[i]), 20) / 2;
                            DrawText(TextFormat("%d", customOrderQty[i]), floatingX, y - 20, 20, highlightCyan);
                        }
                    }

                    Rectangle addBtn = {690, y - 5, 80, 30};
                    bool hAdd = CheckCollisionPointRec(mousePoint, addBtn);
                    DrawRectangleRec(addBtn, hAdd ? panelHover : panelDark);
                    DrawRectangleLinesEx(addBtn, 1, customOrderQty[i] > 0 ? highlightCyan : textDim);
                    DrawText("ADD", addBtn.x + 20, addBtn.y + 5, 20, customOrderQty[i] > 0 ? textYellow : textDim);

                    if (hAdd && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && customOrderQty[i] > 0)
                    {
                        AddToBasket(inventory[i].id, customOrderQty[i], inventory);
                        customOrderQty[i] = 0;
                    }
                    drawnRight++;
                }
                activeRight++;
            }
        }

        // Bottom Basket Summary
        DrawRectangle(0, 480, 800, 120, panelDark);
        DrawLine(0, 480, 800, 480, highlightCyan);

        float basketTotal = 25.0f;
        float basketRevenue = 0.0f;
        int totalUnits = 0;

        for (int i = 0; i < basketCount; i++)
        {
            int idx = -1;
            for (int j = 0; j < MAX_ITEMS; j++)
                if (inventory[j].id == currentBasket[i].itemId)
                    idx = j;
            if (idx != -1)
            {
                basketTotal += currentBasket[i].orderQuantity * inventory[idx].orderCost;
                basketRevenue += currentBasket[i].orderQuantity * inventory[idx].price;
                totalUnits += currentBasket[i].orderQuantity;
            }
        }
        float basketProfit = basketRevenue - basketTotal;

        DrawText("BASKET SUMMARY", 20, 490, 20, highlightCyan);

        int summaryTextWidth = MeasureText("BASKET SUMMARY", 20);
        Rectangle clearBtn = {20 + summaryTextWidth + 20, 485, 80, 30};
        bool hClear = CheckCollisionPointRec(mousePoint, clearBtn);
        DrawRectangleRec(clearBtn, hClear ? panelHover : bgDark);
        DrawRectangleLinesEx(clearBtn, 1, barRed);
        DrawText("Clear", clearBtn.x + 15, clearBtn.y + 5, 20, hClear ? textYellow : textDim);
        if (hClear && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            basketCount = 0;

        DrawText(TextFormat("Total Items: %d", totalUnits), 20, 520, 20, textYellow);
        DrawText("Base Fee: EUR 25.00", 20, 550, 20, textDim);

        DrawText(TextFormat("Total Order Cost: EUR %.2f", basketTotal), 260, 520, 20, textYellow);
        Color profitColor = basketProfit >= 0 ? textGreen : barRed;
        DrawText(TextFormat("Est. Profit: EUR %.2f", basketProfit), 260, 550, 20, profitColor);

        Rectangle checkoutBtn = {580, 510, 180, 50};
        bool hCheck = CheckCollisionPointRec(mousePoint, checkoutBtn);
        DrawRectangleRec(checkoutBtn, hCheck && basketCount > 0 ? highlightPurple : bgDark);
        DrawRectangleLinesEx(checkoutBtn, 2, basketCount > 0 ? highlightPurple : textDim);
        DrawText("CHECKOUT", checkoutBtn.x + 35, checkoutBtn.y + 15, 20, basketCount > 0 ? textYellow : textDim);

        if (hCheck && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && basketCount > 0 && *orderCount < MAX_ORDERS)
        {
            orders[*orderCount].orderId = *orderCount + 1;
            orders[*orderCount].itemCount = basketCount;
            orders[*orderCount].totalCost = basketTotal;
            orders[*orderCount].estimatedProfit = basketProfit;
            orders[*orderCount].status = STATUS_IN_TRANSIT;
            orders[*orderCount].isActive = true;

            for (int b = 0; b < basketCount; b++)
            {
                orders[*orderCount].items[b] = currentBasket[b];
                for (int i = 0; i < MAX_ITEMS; i++)
                {
                    if (inventory[i].id == currentBasket[b].itemId)
                    {
                        inventory[i].inTransit += currentBasket[b].orderQuantity;
                    }
                }
            }
            (*orderCount)++;
            SaveOrders(orders, *orderCount);
            SaveInventory(inventory, *itemCount);
            basketCount = 0;
            currentScreen = SCREEN_ORDER_HISTORY;
        }
        break;
    }

    case SCREEN_ORDER_HISTORY:
    {
        DrawText("ORDER HISTORY", SCREEN_WIDTH / 2 - MeasureText("ORDER HISTORY", 30) / 2, 25, 30, textYellow);

        bool hoverBack = CheckCollisionPointRec(mousePoint, backBtn);
        DrawRectangleRec(backBtn, hoverBack ? panelHover : panelDark);
        DrawRectangleLinesEx(backBtn, 2, hoverBack ? highlightCyan : panelDark);
        DrawText("BACK", backBtn.x + 25, backBtn.y + 10, 20, textYellow);
        if (hoverBack && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            currentScreen = SCREEN_MENU;

        if (wheel != 0)
            scrollHistory -= wheel;
        if (scrollHistory > *orderCount - 9)
            scrollHistory = *orderCount - 9;
        if (scrollHistory < 0)
            scrollHistory = 0;

        int yOffset = 100;

        DrawText("OrderID", 30, yOffset, 20, textDim);
        DrawText("Items", 120, yOffset, 20, textDim);
        DrawText("Cost", 190, yOffset, 20, textDim);
        DrawText("Profit", 350, yOffset, 20, textDim);
        DrawText("Status", 500, yOffset, 20, textDim);
        yOffset += 40;

        for (int i = 0; i < *orderCount; i++)
        {
            if (i >= scrollHistory && i < scrollHistory + 9)
            {
                DrawText(TextFormat("#%d", orders[i].orderId), 30, yOffset, 20, textYellow);
                DrawText(TextFormat("%d", orders[i].itemCount), 120, yOffset, 20, textYellow);
                DrawText(TextFormat("EUR %.2f", orders[i].totalCost), 190, yOffset, 20, textYellow);

                Color profCol = orders[i].estimatedProfit >= 0 ? textGreen : barRed;
                DrawText(TextFormat("EUR %.2f", orders[i].estimatedProfit), 350, yOffset, 20, profCol);

                const char *statusText = orders[i].status == STATUS_DELIVERED ? "Delivered" : "In Transit";
                Color statCol = orders[i].status == STATUS_DELIVERED ? textDim : highlightCyan;
                DrawText(statusText, 500, yOffset, 20, statCol);

                if (orders[i].status == STATUS_IN_TRANSIT)
                {
                    Rectangle btn = {620, yOffset - 5, 160, 30};
                    bool hover = CheckCollisionPointRec(mousePoint, btn);
                    DrawRectangleRec(btn, hover ? panelHover : panelDark);
                    DrawRectangleLinesEx(btn, 1, highlightPurple);
                    DrawText("MARK DELIVERED", btn.x + 10, btn.y + 5, 15, hover ? textYellow : textDim);

                    if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                    {
                        orders[i].status = STATUS_DELIVERED;
                        for (int b = 0; b < orders[i].itemCount; b++)
                        {
                            for (int j = 0; j < MAX_ITEMS; j++)
                            {
                                if (inventory[j].id == orders[i].items[b].itemId)
                                {
                                    inventory[j].inTransit -= orders[i].items[b].orderQuantity;
                                    inventory[j].quantity += orders[i].items[b].orderQuantity;
                                }
                            }
                        }
                        SaveOrders(orders, *orderCount);
                        SaveInventory(inventory, *itemCount);
                    }
                }
                yOffset += 45;
            }
        }
        break;
    }
    }
}