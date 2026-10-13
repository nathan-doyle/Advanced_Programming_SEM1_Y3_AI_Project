#include "raylib.h"
#include "inventory.h"
#include "ui.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

int main(void)
{
    // 1. Initialization
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Inventory Management System");
    SetTargetFPS(60);

    InventoryItem inventory[MAX_ITEMS] = {0};
    OrderRecord orders[MAX_ORDERS] = {0};

    // Load databases on startup
    int itemCount = LoadInventory(inventory);
    int orderCount = LoadOrders(orders);

    // Create a virtual canvas to maintain the 800x600 resolution
    RenderTexture2D target = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);

    // 2. Main Game Loop
    while (!WindowShouldClose())
    {
        if (IsKeyPressed(KEY_F11))
            ToggleFullscreen();

        BeginDrawing();
        ClearBackground(BLACK);

        // Draw UI to the virtual canvas
        BeginTextureMode(target);
        UpdateAndDrawUI(inventory, &itemCount, orders, &orderCount);
        EndTextureMode();

        // Calculate aspect-ratio scale for the current window size
        float scale = MIN((float)GetScreenWidth() / SCREEN_WIDTH, (float)GetScreenHeight() / SCREEN_HEIGHT);

        // Draw the scaled canvas centered on the screen
        DrawTexturePro(target.texture,
                       (Rectangle){0.0f, 0.0f, (float)target.texture.width, (float)-target.texture.height},
                       (Rectangle){(GetScreenWidth() - ((float)SCREEN_WIDTH * scale)) * 0.5f, (GetScreenHeight() - ((float)SCREEN_HEIGHT * scale)) * 0.5f,
                                   (float)SCREEN_WIDTH * scale, (float)SCREEN_HEIGHT * scale},
                       (Vector2){0, 0}, 0.0f, WHITE);
        EndDrawing();
    }

    // 3. De-Initialization
    UnloadRenderTexture(target);
    CloseWindow();
    return 0;
}