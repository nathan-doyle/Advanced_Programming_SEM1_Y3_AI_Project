#include "raylib.h"
#include "inventory.h"
#include "ui.h"
#include <stdlib.h>
#include <time.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "NDAL Inventory Management - Autonomous AI Supply Chain Simulation");
    SetTargetFPS(60);
    SetRandomSeed((unsigned int)time(NULL));

    InventoryItem inventory[MAX_ITEMS] = {0};
    OrderRecord orders[MAX_ORDERS] = {0};

    int itemCount = LoadInventory(inventory);
    int orderCount = 0;

    InitSimulation(inventory, &itemCount);

    RenderTexture2D target = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);

    while (!WindowShouldClose())
    {
        if (IsKeyPressed(KEY_F11))
            ToggleFullscreen();

        BeginDrawing();
        ClearBackground(BLACK);

        BeginTextureMode(target);
        UpdateAndDrawUI(inventory, &itemCount, orders, &orderCount);
        EndTextureMode();

        float scale = MIN((float)GetScreenWidth() / SCREEN_WIDTH, (float)GetScreenHeight() / SCREEN_HEIGHT);
        DrawTexturePro(target.texture,
                       (Rectangle){0.0f, 0.0f, (float)target.texture.width, (float)-target.texture.height},
                       (Rectangle){(GetScreenWidth() - ((float)SCREEN_WIDTH * scale)) * 0.5f,
                                   (GetScreenHeight() - ((float)SCREEN_HEIGHT * scale)) * 0.5f,
                                   (float)SCREEN_WIDTH * scale, (float)SCREEN_HEIGHT * scale},
                       (Vector2){0, 0}, 0.0f, WHITE);
        EndDrawing();
    }

    UnloadRenderTexture(target);
    CloseWindow();
    return 0;
}