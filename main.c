#include "raylib.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_ITEMS 100
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

// Core data structure for the inventory
typedef struct {
    int id;
    char name[50];
    int quantity;
    float price;
    bool isActive;
} InventoryItem;

// Application state management
typedef enum { 
    SCREEN_MENU, 
    SCREEN_VIEW, 
    SCREEN_ADD,
    SCREEN_EDIT 
} AppScreen;

// --- DATABASE FUNCTIONS ---
void SaveInventory(InventoryItem inventory[], int count) {
    FILE *file = fopen("inventory_db.csv", "w");
    if (file == NULL) return;
    for (int i = 0; i < count; i++) {
        fprintf(file, "%d,%s,%d,%.2f,%d\n", 
                inventory[i].id, inventory[i].name, 
                inventory[i].quantity, inventory[i].price, 
                inventory[i].isActive ? 1 : 0);
    }
    fclose(file);
}

int LoadInventory(InventoryItem inventory[]) {
    FILE *file = fopen("inventory_db.csv", "r");
    if (file == NULL) return 0;
    
    int count = 0;
    int activeFlag = 0;
    
    // Parses comma-separated values, allowing spaces in the name
    while (count < MAX_ITEMS && fscanf(file, "%d,%49[^,],%d,%f,%d\n", 
            &inventory[count].id, 
            inventory[count].name, 
            &inventory[count].quantity, 
            &inventory[count].price, 
            &activeFlag) == 5) {
        
        inventory[count].isActive = (activeFlag == 1);
        count++;
    }
    fclose(file);
    return count;
}

int main(void) {
    // 1. Initialization
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Inventory Management System");
    SetTargetFPS(60);

    AppScreen currentScreen = SCREEN_MENU;
    InventoryItem inventory[MAX_ITEMS] = {0};
    
    // Load database on startup instead of using dummy data
    int itemCount = LoadInventory(inventory);
    int editTargetIndex = -1; 

    // Custom Dark Theme Colors
    Color bgDark = (Color){ 20, 20, 24, 255 };
    Color panelDark = (Color){ 40, 40, 45, 255 };
    Color panelHover = (Color){ 60, 60, 65, 255 };
    Color textYellow = (Color){ 255, 255, 200, 255 };
    Color textDim = (Color){ 180, 180, 150, 255 };
    Color highlightCyan = (Color){ 0, 210, 210, 255 };
    Color highlightPurple = (Color){ 150, 80, 220, 255 };

    // UI Definitions - Menu & View
    Rectangle viewBtn = { SCREEN_WIDTH/2 - 150, 200, 300, 50 };
    Rectangle addBtn = { SCREEN_WIDTH/2 - 150, 280, 300, 50 };
    Rectangle backBtn = { 20, 20, 100, 40 };

    // UI Definitions - Add/Edit Screen Inputs
    Rectangle nameBox = { 250, 150, 300, 40 };
    Rectangle qtyBox = { 250, 220, 300, 40 };
    Rectangle priceBox = { 250, 290, 300, 40 };
    Rectangle saveBtn = { 250, 360, 200, 40 };

    int activeInput = 0; 
    char nameInput[50] = "\0";
    char qtyInput[10] = "\0";
    char priceInput[10] = "\0";
    int nameCount = 0, qtyCount = 0, priceCount = 0;

    // 2. Main Game Loop
    while (!WindowShouldClose()) {
        // --- UPDATE LOGIC ---
        Vector2 mousePoint = GetMousePosition();

        if (currentScreen == SCREEN_ADD || currentScreen == SCREEN_EDIT) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (CheckCollisionPointRec(mousePoint, nameBox)) activeInput = 0;
                else if (CheckCollisionPointRec(mousePoint, qtyBox)) activeInput = 1;
                else if (CheckCollisionPointRec(mousePoint, priceBox)) activeInput = 2;
            }

            int key = GetCharPressed();
            while (key > 0) {
                if ((key >= 32) && (key <= 125)) {
                    if (activeInput == 0 && nameCount < 49) {
                        nameInput[nameCount] = (char)key;
                        nameInput[nameCount + 1] = '\0';
                        nameCount++;
                    } else if (activeInput == 1 && qtyCount < 9 && key >= '0' && key <= '9') { 
                        qtyInput[qtyCount] = (char)key;
                        qtyInput[qtyCount + 1] = '\0';
                        qtyCount++;
                    } else if (activeInput == 2 && priceCount < 9 && ((key >= '0' && key <= '9') || key == '.')) { 
                        priceInput[priceCount] = (char)key;
                        priceInput[priceCount + 1] = '\0';
                        priceCount++;
                    }
                }
                key = GetCharPressed();  
            }

            if (IsKeyPressed(KEY_BACKSPACE)) {
                if (activeInput == 0 && nameCount > 0) {
                    nameCount--;
                    nameInput[nameCount] = '\0';
                } else if (activeInput == 1 && qtyCount > 0) {
                    qtyCount--;
                    qtyInput[qtyCount] = '\0';
                } else if (activeInput == 2 && priceCount > 0) {
                    priceCount--;
                    priceInput[priceCount] = '\0';
                }
            }

            if (CheckCollisionPointRec(mousePoint, saveBtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (nameCount > 0) {
                    if (currentScreen == SCREEN_ADD && itemCount < MAX_ITEMS) {
                        inventory[itemCount].id = itemCount + 1;
                        strcpy(inventory[itemCount].name, nameInput);
                        inventory[itemCount].quantity = atoi(qtyInput);
                        inventory[itemCount].price = (float)atof(priceInput);
                        inventory[itemCount].isActive = true;
                        itemCount++;
                    } else if (currentScreen == SCREEN_EDIT && editTargetIndex >= 0) {
                        strcpy(inventory[editTargetIndex].name, nameInput);
                        inventory[editTargetIndex].quantity = atoi(qtyInput);
                        inventory[editTargetIndex].price = (float)atof(priceInput);
                    }

                    // Save to database immediately upon creation/edit
                    SaveInventory(inventory, itemCount);

                    nameInput[0] = '\0'; qtyInput[0] = '\0'; priceInput[0] = '\0';
                    nameCount = 0; qtyCount = 0; priceCount = 0;
                    currentScreen = SCREEN_MENU;
                }
            }
        }

        // --- DRAWING ---
        BeginDrawing();
        ClearBackground(bgDark);

        switch(currentScreen) {
            case SCREEN_MENU: {
                DrawText("INVENTORY DASHBOARD", SCREEN_WIDTH/2 - MeasureText("INVENTORY DASHBOARD", 30)/2, 80, 30, textYellow);

                bool hoverView = CheckCollisionPointRec(mousePoint, viewBtn);
                DrawRectangleRec(viewBtn, hoverView ? panelHover : panelDark);
                DrawRectangleLinesEx(viewBtn, 2, hoverView ? highlightCyan : panelDark);
                DrawText("View Inventory", viewBtn.x + 60, viewBtn.y + 15, 20, textYellow);
                if (hoverView && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) currentScreen = SCREEN_VIEW;

                bool hoverAdd = CheckCollisionPointRec(mousePoint, addBtn);
                DrawRectangleRec(addBtn, hoverAdd ? panelHover : panelDark);
                DrawRectangleLinesEx(addBtn, 2, hoverAdd ? highlightPurple : panelDark);
                DrawText("Add New Item", addBtn.x + 70, addBtn.y + 15, 20, textYellow);
                if (hoverAdd && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) currentScreen = SCREEN_ADD;
                break;
            }

            case SCREEN_VIEW: {
                DrawText("CURRENT INVENTORY", SCREEN_WIDTH/2 - MeasureText("CURRENT INVENTORY", 30)/2, 25, 30, textYellow);
                
                bool hoverBack = CheckCollisionPointRec(mousePoint, backBtn);
                DrawRectangleRec(backBtn, hoverBack ? panelHover : panelDark);
                DrawRectangleLinesEx(backBtn, 2, hoverBack ? highlightCyan : panelDark);
                DrawText("BACK", backBtn.x + 25, backBtn.y + 10, 20, textYellow);
                if (hoverBack && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) currentScreen = SCREEN_MENU;

                int yOffset = 100;
                DrawText("ID", 100, yOffset, 20, textDim);
                DrawText("Name", 150, yOffset, 20, textDim);
                DrawText("Qty", 380, yOffset, 20, textDim);
                DrawText("Price", 460, yOffset, 20, textDim);
                yOffset += 40;
                
                for (int i = 0; i < MAX_ITEMS; i++) {
                    if (inventory[i].isActive) {
                        char idStr[10], qtyStr[10], priceStr[20];
                        sprintf(idStr, "%d", inventory[i].id);
                        sprintf(qtyStr, "%d", inventory[i].quantity);
                        sprintf(priceStr, "$%.2f", inventory[i].price);

                        DrawText(idStr, 100, yOffset + 5, 20, textYellow);
                        DrawText(inventory[i].name, 150, yOffset + 5, 20, textYellow);
                        DrawText(qtyStr, 380, yOffset + 5, 20, textYellow);
                        DrawText(priceStr, 460, yOffset + 5, 20, textYellow);

                        Rectangle editBtn = { 560, yOffset, 80, 30 };
                        bool hoverEdit = CheckCollisionPointRec(mousePoint, editBtn);
                        DrawRectangleRec(editBtn, hoverEdit ? panelHover : panelDark);
                        DrawRectangleLinesEx(editBtn, 2, highlightCyan);
                        DrawText("EDIT", editBtn.x + 22, editBtn.y + 8, 15, hoverEdit ? highlightCyan : textDim);

                        if (hoverEdit && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                            currentScreen = SCREEN_EDIT;
                            editTargetIndex = i;
                            strcpy(nameInput, inventory[i].name);
                            nameCount = strlen(nameInput);
                            sprintf(qtyInput, "%d", inventory[i].quantity);
                            qtyCount = strlen(qtyInput);
                            sprintf(priceInput, "%.2f", inventory[i].price);
                            priceCount = strlen(priceInput);
                        }

                        Rectangle deleteBtn = { 650, yOffset, 80, 30 };
                        bool hoverDelete = CheckCollisionPointRec(mousePoint, deleteBtn);
                        DrawRectangleRec(deleteBtn, hoverDelete ? panelHover : panelDark);
                        DrawRectangleLinesEx(deleteBtn, 2, highlightPurple);
                        DrawText("DELETE", deleteBtn.x + 12, deleteBtn.y + 8, 15, hoverDelete ? highlightPurple : textDim);

                        if (hoverDelete && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                            inventory[i].isActive = false; 
                            SaveInventory(inventory, itemCount); // Save instantly on delete
                        }

                        yOffset += 45;
                    }
                }
                break;
            }

            case SCREEN_ADD:
            case SCREEN_EDIT: {
                const char* titleText = currentScreen == SCREEN_ADD ? "ADD NEW ITEM" : "EDIT ITEM";
                DrawText(titleText, SCREEN_WIDTH/2 - MeasureText(titleText, 30)/2, 25, 30, textYellow);
                
                bool hoverBack = CheckCollisionPointRec(mousePoint, backBtn);
                DrawRectangleRec(backBtn, hoverBack ? panelHover : panelDark);
                DrawRectangleLinesEx(backBtn, 2, hoverBack ? highlightCyan : panelDark);
                DrawText("BACK", backBtn.x + 25, backBtn.y + 10, 20, textYellow);
                if (hoverBack && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    nameInput[0] = '\0'; qtyInput[0] = '\0'; priceInput[0] = '\0';
                    nameCount = 0; qtyCount = 0; priceCount = 0;
                    currentScreen = SCREEN_MENU;
                }

                DrawText("Name:", 150, 160, 20, textDim);
                DrawRectangleRec(nameBox, panelDark);
                DrawRectangleLinesEx(nameBox, 2, activeInput == 0 ? highlightCyan : panelHover);
                DrawText(nameInput, nameBox.x + 5, nameBox.y + 10, 20, textYellow);

                DrawText("Qty:", 150, 230, 20, textDim);
                DrawRectangleRec(qtyBox, panelDark);
                DrawRectangleLinesEx(qtyBox, 2, activeInput == 1 ? highlightCyan : panelHover);
                DrawText(qtyInput, qtyBox.x + 5, qtyBox.y + 10, 20, textYellow);

                DrawText("Price:", 150, 300, 20, textDim);
                DrawRectangleRec(priceBox, panelDark);
                DrawRectangleLinesEx(priceBox, 2, activeInput == 2 ? highlightCyan : panelHover);
                DrawText(priceInput, priceBox.x + 5, priceBox.y + 10, 20, textYellow);

                bool hoverSave = CheckCollisionPointRec(mousePoint, saveBtn);
                DrawRectangleRec(saveBtn, hoverSave ? highlightCyan : highlightPurple);
                
                const char* btnText = currentScreen == SCREEN_ADD ? "SAVE ITEM" : "UPDATE ITEM";
                int textW = MeasureText(btnText, 20);
                DrawText(btnText, saveBtn.x + (saveBtn.width - textW)/2, saveBtn.y + 10, 20, hoverSave ? bgDark : textYellow);

                break;
            }
        }

        EndDrawing();
    }

    // 3. De-Initialization
    CloseWindow();
    return 0;
}