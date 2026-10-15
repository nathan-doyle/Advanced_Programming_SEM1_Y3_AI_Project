#include "arena.h"
#include "events.h"
#include "ai.h"
#include "ui.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define MAX_ANIM_CUSTOMERS 32
#define MAX_FLOATING_POPUPS 28

#define FORKLIFT_HOME_X 456.0f
#define FORKLIFT_HOME_Y 762.0f

typedef enum
{
    CUST_INACTIVE = 0,
    CUST_ENTERING,
    CUST_TO_SHELF,
    CUST_PICKING,
    CUST_TO_QUEUE,
    CUST_SCANNING,
    CUST_EXITING,
    CUST_ANGRY_EXIT
} CustomerAnimState;

typedef struct
{
    bool active;
    CustomerAnimState state;
    Vector2 pos;
    Vector2 target;
    int skuIndex;
    int tillIndex;
    float stateTimer;
    float walkPhase;
    float speed;
    float saleAmount;
    float markupPct;
    int unitsBought;
    Color shirtColor;
    Color hairColor;
    Color itemColor;
    bool hasItem;
} CustomerAgent;

typedef enum
{
    FORK_IDLE = 0,
    FORK_TO_TRUCK,
    FORK_PICKING_PALLET,
    FORK_TO_RACK,
    FORK_DROPPING_PALLET
} ForkliftState;

typedef struct
{
    ForkliftState state;
    Vector2 pos;
    Vector2 target;
    float stateTimer;
    float forkElevation;
    bool hasPallet;
    int targetRackIdx;
    int palletsRemaining;
    float wheelPhase;
    bool facingRight;
} ForkliftAgent;

typedef struct
{
    bool active;
    int orderId;
    int supplierId;
    char supplierName[40];
    bool isDamaged;
    float dockTimer;
    float maxDockTime;
    float truckYOffset;
    int totalUnits;
} DockedTruckState;

typedef struct
{
    Vector2 pos;
    int targetBay;
    float scanTimer;
    float hoverPhase;
} AiDroneAgent;

typedef struct
{
    bool active;
    Vector2 pos;
    char text[52];
    Color color;
    float timer;
    float maxTime;
} FloatingPopup;

static CustomerAgent customers[MAX_ANIM_CUSTOMERS];
static ForkliftAgent forklift;
static DockedTruckState dockedTruck;
static AiDroneAgent aiDrone = {{720.0f, 360.0f}, 0, 0.0f, 0.0f};
static FloatingPopup popups[MAX_FLOATING_POPUPS];

static float tillScanLaserTimer[3] = {0.0f, 0.0f, 0.0f};
static float conveyorBeltOffset = 0.0f;
static float ambientCustomerTimer = 0.0f;

static bool MoveTowardsVec2(Vector2 *pos, Vector2 target, float speed, float dt)
{
    float dx = target.x - pos->x;
    float dy = target.y - pos->y;
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist <= speed * dt || dist < 2.0f)
    {
        *pos = target;
        return true;
    }
    pos->x += (dx / dist) * speed * dt;
    pos->y += (dy / dist) * speed * dt;
    return false;
}

Color GetSkuBoxColor(int idx)
{
    Color pal[12] = {
        (Color){25, 235, 245, 255},  // Neon Cyan
        (Color){195, 125, 255, 255}, // Electric Purple
        (Color){255, 185, 45, 255},  // Amber Gold
        (Color){50, 240, 150, 255},  // Emerald Green
        (Color){255, 95, 145, 255},  // Coral Rose
        (Color){95, 180, 255, 255},  // Azure Blue
        (Color){255, 230, 75, 255},  // Solar Yellow
        (Color){65, 235, 200, 255},  // Aqua Mint
        (Color){255, 135, 90, 255},  // Sunset Tangerine
        (Color){160, 235, 90, 255},  // Lime Volt
        (Color){240, 120, 230, 255}, // Magenta Orchid
        (Color){130, 155, 255, 255}  // Periwinkle Indigo
    };
    return pal[abs(idx) % 12];
}

void InitArena(void)
{
    for (int i = 0; i < MAX_ANIM_CUSTOMERS; i++)
        customers[i].active = false;
    for (int i = 0; i < MAX_FLOATING_POPUPS; i++)
        popups[i].active = false;
    for (int i = 0; i < 3; i++)
        tillScanLaserTimer[i] = 0.0f;

    conveyorBeltOffset = 0.0f;
    ambientCustomerTimer = 0.0f;

    forklift.state = FORK_IDLE;
    forklift.pos = (Vector2){FORKLIFT_HOME_X, FORKLIFT_HOME_Y};
    forklift.target = forklift.pos;
    forklift.forkElevation = 0.0f;
    forklift.hasPallet = false;
    forklift.palletsRemaining = 0;
    forklift.facingRight = true;

    dockedTruck.active = false;
    aiDrone = (AiDroneAgent){{720.0f, 360.0f}, 0, 0.0f, 0.0f};
}

void SpawnFloatingPopup(float x, float y, const char *txt, Color col)
{
    for (int i = 0; i < MAX_FLOATING_POPUPS; i++)
    {
        if (!popups[i].active)
        {
            popups[i].active = true;
            popups[i].pos = (Vector2){x, y};
            strncpy(popups[i].text, txt, 51);
            popups[i].text[51] = '\0';
            popups[i].color = col;
            popups[i].maxTime = 2.1f;
            popups[i].timer = 2.1f;
            return;
        }
    }
}

void SpawnShopperAgent(int skuIdx, int units, float saleEur, float markupPct, bool stockoutAngry)
{
    ChaosEventState *ev = GetEventsState();
    for (int c = 0; c < MAX_ANIM_CUSTOMERS; c++)
    {
        if (!customers[c].active)
        {
            Color shirts[6] = {
                (Color){75, 165, 255, 255},
                (Color){245, 100, 125, 255},
                (Color){65, 220, 155, 255},
                (Color){245, 195, 65, 255},
                (Color){180, 120, 250, 255},
                (Color){255, 145, 75, 255}};
            Color hairs[4] = {
                (Color){45, 35, 30, 255},
                (Color){110, 75, 45, 255},
                (Color){210, 175, 95, 255},
                (Color){75, 80, 95, 255}};
            customers[c].active = true;
            customers[c].state = CUST_ENTERING;
            customers[c].pos = (Vector2){1405.0f + (float)GetRandomValue(-45, 45), 298.0f};
            customers[c].target = (Vector2){1345.0f + (float)GetRandomValue(-40, 40), 365.0f};
            customers[c].skuIndex = skuIdx;
            customers[c].tillIndex = GetRandomValue(0, 2);
            customers[c].stateTimer = 0.0f;
            customers[c].walkPhase = (float)GetRandomValue(0, 100) * 0.1f;
            customers[c].speed = (ev->viralSurgeTimer > 0.0f ? 225.0f : 155.0f) + (float)GetRandomValue(-18, 25);
            customers[c].saleAmount = saleEur;
            customers[c].markupPct = markupPct;
            customers[c].unitsBought = units;
            customers[c].shirtColor = shirts[GetRandomValue(0, 5)];
            customers[c].hairColor = hairs[GetRandomValue(0, 3)];
            customers[c].itemColor = GetSkuBoxColor(skuIdx);
            customers[c].hasItem = !stockoutAngry;
            return;
        }
    }
}

void TriggerDockUnloading(OrderRecord *ord, int totalUnits, const InventoryItem inventory[])
{
    dockedTruck.active = true;
    dockedTruck.orderId = ord->orderId;
    dockedTruck.supplierId = ord->supplierId;
    strncpy(dockedTruck.supplierName, ord->supplierName, 39);
    dockedTruck.supplierName[39] = '\0';
    dockedTruck.isDamaged = ord->isDamaged;
    dockedTruck.maxDockTime = 9.0f;
    dockedTruck.dockTimer = 9.0f;
    dockedTruck.truckYOffset = -80.0f;
    dockedTruck.totalUnits = totalUnits;

    int activeSkus = GetActiveSkuCount(inventory);
    if (activeSkus < 1)
        activeSkus = 1;

    forklift.palletsRemaining = (ord->itemCount > 0) ? (ord->itemCount + 1) : 2;
    if (forklift.palletsRemaining > 4)
        forklift.palletsRemaining = 4;
    forklift.state = FORK_TO_TRUCK;
    forklift.targetRackIdx = GetRandomValue(0, activeSkus - 1);
}

void UpdateArenaAnimations(float dt, const InventoryItem inventory[])
{
    if (dt <= 0.0f)
        return;

    conveyorBeltOffset = fmodf(conveyorBeltOffset + dt * 48.0f, 18.0f);
    int totalActiveSkus = GetActiveSkuCount(inventory);
    AiEconomyState *ai = GetAiEconomyState();

    // Ambient shopper spawning
    ambientCustomerTimer += dt;
    if (ambientCustomerTimer >= 2.8f && totalActiveSkus > 0)
    {
        ambientCustomerTimer = 0.0f;
        int activeCount = 0;
        for (int c = 0; c < MAX_ANIM_CUSTOMERS; c++)
            if (customers[c].active)
                activeCount++;
        if (activeCount < 5)
        {
            int rSku = PickRandomActiveInventoryIndex(inventory);
            float mPct = (ai->aiPriceMult[rSku] - 1.0f) * 100.0f;
            SpawnShopperAgent(rSku, 1, inventory[rSku].price, mPct, inventory[rSku].quantity == 0);
        }
    }

    // Update Checkout Laser Timers
    for (int i = 0; i < 3; i++)
    {
        if (tillScanLaserTimer[i] > 0.0f)
        {
            tillScanLaserTimer[i] -= dt;
            if (tillScanLaserTimer[i] < 0.0f)
                tillScanLaserTimer[i] = 0.0f;
        }
    }

    // Update Customer Agents
    for (int c = 0; c < MAX_ANIM_CUSTOMERS; c++)
    {
        if (!customers[c].active)
            continue;
        customers[c].walkPhase += dt * 11.0f;

        int activeSlot = GetActiveSlotForInventoryIndex(inventory, customers[c].skuIndex);
        Rectangle targetBay = GetShelfBayRect(activeSlot, totalActiveSkus);
        float shelfPickX = targetBay.x + targetBay.width + 16.0f;
        float shelfPickY = targetBay.y + targetBay.height * 0.5f;
        float tillY = 420.0f + (customers[c].tillIndex % 3) * 175.0f;

        switch (customers[c].state)
        {
        case CUST_ENTERING:
        {
            if (MoveTowardsVec2(&customers[c].pos, customers[c].target, customers[c].speed, dt))
            {
                customers[c].state = CUST_TO_SHELF;
                customers[c].target = (Vector2){shelfPickX + (float)GetRandomValue(0, 10), shelfPickY};
            }
            break;
        }
        case CUST_TO_SHELF:
        {
            if (MoveTowardsVec2(&customers[c].pos, customers[c].target, customers[c].speed, dt))
            {
                customers[c].state = CUST_PICKING;
                customers[c].stateTimer = 0.60f;
            }
            break;
        }
        case CUST_PICKING:
        {
            customers[c].stateTimer -= dt;
            if (customers[c].stateTimer <= 0.0f)
            {
                if (!customers[c].hasItem)
                {
                    SpawnFloatingPopup(customers[c].pos.x - 10, customers[c].pos.y - 28, "OUT OF STOCK! -4.5% SAT", (Color){255, 80, 100, 255});
                    customers[c].state = CUST_ANGRY_EXIT;
                    customers[c].target = (Vector2){1435.0f, 298.0f};
                }
                else
                {
                    customers[c].state = CUST_TO_QUEUE;
                    customers[c].target = (Vector2){1265.0f + (float)GetRandomValue(-18, 12), tillY + (float)GetRandomValue(-12, 12)};
                }
            }
            break;
        }
        case CUST_TO_QUEUE:
        {
            if (MoveTowardsVec2(&customers[c].pos, customers[c].target, customers[c].speed, dt))
            {
                customers[c].state = CUST_SCANNING;
                customers[c].stateTimer = 0.90f;
                tillScanLaserTimer[customers[c].tillIndex % 3] = 0.90f;
            }
            break;
        }
        case CUST_SCANNING:
        {
            customers[c].stateTimer -= dt;
            tillScanLaserTimer[customers[c].tillIndex % 3] = 0.25f;
            if (customers[c].stateTimer <= 0.0f)
            {
                if (customers[c].markupPct > 6.0f)
                {
                    SpawnFloatingPopup(1310.0f, tillY - 34.0f, TextFormat("+EUR %.0f (PRICEY! -SAT)", customers[c].saleAmount), (Color){255, 195, 45, 255});
                }
                else if (customers[c].markupPct < -3.0f)
                {
                    SpawnFloatingPopup(1310.0f, tillY - 34.0f, TextFormat("+EUR %.0f (DEAL! +SAT)", customers[c].saleAmount), (Color){55, 250, 155, 255});
                }
                else
                {
                    SpawnFloatingPopup(1320.0f, tillY - 34.0f, TextFormat("+EUR %.0f", customers[c].saleAmount), (Color){55, 250, 155, 255});
                }
                customers[c].state = CUST_EXITING;
                customers[c].target = (Vector2){1435.0f, 298.0f};
            }
            break;
        }
        case CUST_EXITING:
        case CUST_ANGRY_EXIT:
        {
            if (MoveTowardsVec2(&customers[c].pos, customers[c].target, customers[c].speed * 1.15f, dt))
            {
                customers[c].active = false;
            }
            break;
        }
        default:
            break;
        }
    }

    // Update Docked Truck & Forklift
    if (dockedTruck.active)
    {
        dockedTruck.dockTimer -= dt;
        if (dockedTruck.truckYOffset < 0.0f)
        {
            dockedTruck.truckYOffset += dt * 85.0f;
            if (dockedTruck.truckYOffset > 0.0f)
                dockedTruck.truckYOffset = 0.0f;
        }
        if (dockedTruck.dockTimer <= 0.0f)
        {
            dockedTruck.active = false;
        }
    }

    if (forklift.state != FORK_IDLE)
    {
        forklift.wheelPhase += dt * 14.0f;
    }
    switch (forklift.state)
    {
    case FORK_IDLE:
    {
        Vector2 homePos = {FORKLIFT_HOME_X, FORKLIFT_HOME_Y};
        MoveTowardsVec2(&forklift.pos, homePos, 125.0f, dt);
        forklift.facingRight = true;
        if (dockedTruck.active && forklift.palletsRemaining > 0)
        {
            forklift.state = FORK_TO_TRUCK;
        }
        break;
    }
    case FORK_TO_TRUCK:
    {
        Vector2 dockPickPos = {485.0f, 395.0f};
        forklift.facingRight = (dockPickPos.x >= forklift.pos.x);
        if (MoveTowardsVec2(&forklift.pos, dockPickPos, 185.0f, dt))
        {
            forklift.state = FORK_PICKING_PALLET;
            forklift.stateTimer = 0.55f;
        }
        break;
    }
    case FORK_PICKING_PALLET:
    {
        forklift.stateTimer -= dt;
        forklift.forkElevation = 1.0f - (forklift.stateTimer / 0.55f);
        if (forklift.forkElevation > 1.0f)
            forklift.forkElevation = 1.0f;
        if (forklift.stateTimer <= 0.0f)
        {
            forklift.hasPallet = true;
            forklift.state = FORK_TO_RACK;
            int maxSlot = (totalActiveSkus > 0) ? totalActiveSkus : 6;
            forklift.targetRackIdx = GetRandomValue(0, maxSlot - 1);
        }
        break;
    }
    case FORK_TO_RACK:
    {
        Rectangle rackBay = GetShelfBayRect(forklift.targetRackIdx % 6, totalActiveSkus);
        Vector2 rackDropPos = {rackBay.x - 42.0f, rackBay.y + rackBay.height * 0.5f};
        forklift.facingRight = true;
        if (MoveTowardsVec2(&forklift.pos, rackDropPos, 175.0f, dt))
        {
            forklift.state = FORK_DROPPING_PALLET;
            forklift.stateTimer = 0.50f;
        }
        break;
    }
    case FORK_DROPPING_PALLET:
    {
        forklift.stateTimer -= dt;
        forklift.forkElevation = (forklift.stateTimer / 0.50f);
        if (forklift.forkElevation < 0.0f)
            forklift.forkElevation = 0.0f;
        if (forklift.stateTimer <= 0.0f)
        {
            forklift.hasPallet = false;
            forklift.palletsRemaining--;
            SpawnFloatingPopup(forklift.pos.x - 20.0f, forklift.pos.y - 28.0f, "+PALLET STOCKED", (Color){35, 245, 255, 255});
            if (dockedTruck.active && forklift.palletsRemaining > 0)
            {
                forklift.state = FORK_TO_TRUCK;
            }
            else
            {
                forklift.state = FORK_IDLE;
            }
        }
        break;
    }
    default:
        break;
    }

    // Update Autonomous AI Drone
    aiDrone.hoverPhase += dt * 5.0f;
    int maxDroneSlots = (totalActiveSkus > 0) ? totalActiveSkus : 6;
    if (aiDrone.targetBay >= maxDroneSlots)
        aiDrone.targetBay = 0;
    Rectangle droneBay = GetShelfBayRect(aiDrone.targetBay, totalActiveSkus);
    Vector2 droneTarget = {droneBay.x - 22.0f, droneBay.y + droneBay.height * 0.5f};
    if (MoveTowardsVec2(&aiDrone.pos, droneTarget, 125.0f, dt))
    {
        aiDrone.scanTimer += dt;
        if (aiDrone.scanTimer >= 1.2f)
        {
            aiDrone.scanTimer = 0.0f;
            aiDrone.targetBay = (aiDrone.targetBay + 1) % maxDroneSlots;
        }
    }

    // Update Floating Popups
    for (int p = 0; p < MAX_FLOATING_POPUPS; p++)
    {
        if (popups[p].active)
        {
            popups[p].timer -= dt;
            popups[p].pos.y -= dt * 36.0f;
            if (popups[p].timer <= 0.0f)
                popups[p].active = false;
        }
    }
}

void DrawCentralDigitalTwinArena(Rectangle arena, InventoryItem inventory[], int itemCount, OrderRecord orders[], int orderCount)
{
    (void)itemCount;
    Color bgArena = (Color){16, 19, 28, 255};
    Color roadAsphalt = (Color){24, 28, 38, 255};
    Color warehouseFloor = (Color){30, 34, 46, 255};
    Color storeFloorA = (Color){24, 29, 42, 255};
    Color storeFloorB = (Color){29, 35, 50, 255};
    Color textYellow = (Color){255, 255, 242, 255};
    Color textDim = (Color){205, 218, 238, 255};
    Color highlightCyan = (Color){35, 245, 255, 255};
    Color highlightPurple = (Color){205, 135, 255, 255};
    Color barRed = (Color){255, 80, 100, 255};
    Color textGreen = (Color){55, 250, 155, 255};
    Color warnOrange = (Color){255, 195, 45, 255};
    Color safetyYellow = (Color){250, 205, 35, 255};
    Color gridLine = (Color){52, 60, 84, 255};

    ChaosEventState *ev = GetEventsState();
    AiEconomyState *ai = GetAiEconomyState();
    int totalActiveSkus = GetActiveSkuCount(inventory);
    bool twoAisleMode = (totalActiveSkus > 6);

    DrawDashboardCard(arena, bgArena, highlightCyan, 0.02f, false);

    // 1. TOP MULTI-LANE LOGISTICS HIGHWAY & LANDSCAPING
    Rectangle roadRect = {arena.x + 6, arena.y + 44, arena.width - 12, 120};
    DrawRectangleRec(roadRect, roadAsphalt);

    Rectangle vergeRect = {roadRect.x, roadRect.y, roadRect.width, 22};
    DrawRectangleRec(vergeRect, (Color){18, 34, 32, 255});
    for (int lx = (int)roadRect.x + 55; lx < (int)(roadRect.x + roadRect.width - 40); lx += 150)
    {
        DrawCircle(lx - 35, (int)roadRect.y + 11, 8.0f, (Color){28, 78, 54, 255});
        DrawCircle(lx - 33, (int)roadRect.y + 9, 5.0f, (Color){42, 115, 76, 255});
        DrawCircle(lx, (int)roadRect.y + 16, 26.0f, (Color){255, 220, 120, 22});
        DrawRectangle(lx - 3, (int)roadRect.y + 4, 6, 12, (Color){90, 100, 120, 255});
        DrawCircle(lx, (int)roadRect.y + 14, 4.0f, textYellow);
    }

    DrawLine((int)roadRect.x, (int)roadRect.y + 22, (int)(roadRect.x + roadRect.width), (int)roadRect.y + 22, gridLine);
    DrawLineEx((Vector2){roadRect.x, roadRect.y + roadRect.height}, (Vector2){roadRect.x + roadRect.width, roadRect.y + roadRect.height}, 2.5f, warnOrange);

    for (int rx = (int)roadRect.x + 24; rx < (int)(roadRect.x + roadRect.width - 40); rx += 64)
    {
        DrawRectangle(rx, (int)roadRect.y + 70, 34, 4, (Color){245, 205, 55, 190});
    }
    DrawTextCrisp("NDAL SUPPLIER LOGISTICS CORRIDOR", (int)roadRect.x + 790, (int)roadRect.y + 27, 15, textDim);

    for (int o = 0; o < orderCount; o++)
    {
        if (orders[o].isActive && orders[o].status != STATUS_DELIVERED)
        {
            float prog = orders[o].totalDeliveryTime > 0.0f ? (orders[o].elapsedTime / orders[o].totalDeliveryTime) : 1.0f;
            if (prog > 1.0f)
                prog = 1.0f;

            float tx = 1460.0f - prog * 960.0f;
            float ty = roadRect.y + 34.0f + (o % 2) * 42.0f;

            Color supCol = (orders[o].supplierId == 1) ? highlightCyan : ((orders[o].supplierId == 2) ? highlightPurple : textGreen);
            if (orders[o].isDamaged)
                supCol = barRed;

            DrawTriangle((Vector2){tx - 24, ty + 6}, (Vector2){tx - 78, ty - 6}, (Vector2){tx - 78, ty + 18}, (Color){255, 255, 190, 35});
            DrawTriangle((Vector2){tx - 24, ty + 24}, (Vector2){tx - 78, ty + 14}, (Vector2){tx - 78, ty + 36}, (Color){255, 255, 190, 35});

            DrawRectangleRounded((Rectangle){tx - 22, ty + 5, 114, 30}, 0.2f, 6, (Color){0, 0, 0, 110});
            DrawRectangleRounded((Rectangle){tx, ty, 88, 28}, 0.18f, 6, supCol);
            DrawRectangleRounded((Rectangle){tx + 3, ty + 3, 82, 22}, 0.15f, 4, (Color){18, 22, 32, 215});
            DrawRectangleRounded((Rectangle){tx + 6, ty + 18, 76 * prog, 4}, 0.5f, 4, supCol);

            DrawRectangleRounded((Rectangle){tx - 23, ty + 2, 23, 24}, 0.3f, 6, (Color){225, 232, 242, 255});
            DrawRectangle((int)tx - 19, (int)ty + 5, 9, 18, (Color){28, 38, 56, 255});
            DrawCircle((int)tx - 24, (int)ty + 6, 3.0f, textYellow);
            DrawCircle((int)tx - 24, (int)ty + 22, 3.0f, textYellow);

            DrawCircle((int)tx - 12, (int)ty + 28, 5.0f, (Color){15, 16, 20, 255});
            DrawCircle((int)tx + 16, (int)ty + 28, 5.0f, (Color){15, 16, 20, 255});
            DrawCircle((int)tx + 62, (int)ty + 28, 5.0f, (Color){15, 16, 20, 255});
            DrawCircle((int)tx + 76, (int)ty + 28, 5.0f, (Color){15, 16, 20, 255});

            DrawTextTruncated(TextFormat("#%d %.0f%%", orders[o].orderId, prog * 100.0f), (int)tx + 8, (int)ty + 4, 14, 74, textYellow);
        }
    }

    // 2. WAREHOUSE FLOOR, DOCK BAY 01 & PAINTED SAFETY LANES
    float whWidth = twoAisleMode ? 378.0f : 400.0f;
    Rectangle whRect = {arena.x + 6, arena.y + 164, whWidth, arena.height - 170};
    DrawRectangleRec(whRect, warehouseFloor);

    if (ev->shrinkageAlarmTimer > 0.0f && ((int)(ai->simTime * 8.0f) % 2 == 0))
    {
        DrawRectangleRec(whRect, (Color){255, 80, 100, 50});
    }

    for (int wy = (int)whRect.y + 70; wy < (int)(whRect.y + whRect.height - 120); wy += 75)
    {
        DrawLine((int)whRect.x + 10, wy, (int)(whRect.x + whRect.width - 10), wy, (Color){45, 50, 66, 160});
    }
    for (int wx = (int)whRect.x + 90; wx < (int)(whRect.x + whRect.width - 20); wx += 95)
    {
        DrawLine(wx, (int)whRect.y + 10, wx, (int)(whRect.y + whRect.height - 120), (Color){45, 50, 66, 120});
    }

    DrawLineEx((Vector2){445, 360}, (Vector2){445, 705}, 2.0f, (Color){250, 205, 35, 85});
    DrawLineEx((Vector2){515, 360}, (Vector2){515, 705}, 2.0f, (Color){250, 205, 35, 85});
    DrawLineEx((Vector2){515, 520}, (Vector2){whRect.x + whRect.width - 12, 520}, 2.0f, (Color){250, 205, 35, 85});
    DrawTextCrisp("FORKLIFT LANE", 530, 496, 13, (Color){250, 205, 35, 160});

    Rectangle dockBay = {whRect.x + 52, whRect.y - 4, 156, 54};
    DrawRectangleRec(dockBay, (Color){20, 23, 34, 255});
    DrawRectangleLinesEx(dockBay, 2.5f, dockedTruck.active ? warnOrange : gridLine);
    DrawTextCrisp("LOADING DOCK BAY 01", (int)dockBay.x + 12, (int)dockBay.y + 8, 13, dockedTruck.active ? warnOrange : textDim);

    DrawCircle((int)(dockBay.x + dockBay.width + 18), (int)dockBay.y + 24, 8.0f, (Color){15, 18, 26, 255});
    DrawCircle((int)(dockBay.x + dockBay.width + 18), (int)dockBay.y + 24, 5.5f, dockedTruck.active ? barRed : textGreen);

    for (int hx = (int)dockBay.x + 6; hx < (int)(dockBay.x + dockBay.width - 8); hx += 16)
    {
        DrawLineEx((Vector2){(float)hx, dockBay.y + 42}, (Vector2){(float)hx + 8, dockBay.y + 52}, 3.0f, warnOrange);
    }

    if (dockedTruck.active)
    {
        float dty = dockBay.y - 54.0f + dockedTruck.truckYOffset;
        Color tCol = (dockedTruck.supplierId == 1) ? highlightCyan : ((dockedTruck.supplierId == 2) ? highlightPurple : textGreen);
        if (dockedTruck.isDamaged)
            tCol = barRed;

        DrawRectangleRounded((Rectangle){dockBay.x + 24, dty - 32, 104, 32}, 0.3f, 6, (Color){215, 222, 235, 255});
        DrawRectangleRounded((Rectangle){dockBay.x + 14, dty, 124, 92}, 0.1f, 6, tCol);
        DrawRectangle((int)dockBay.x + 22, (int)dty + 8, 108, 78, (Color){22, 25, 36, 255});

        for (int p = 0; p < forklift.palletsRemaining; p++)
        {
            int px = (int)dockBay.x + 30 + (p % 2) * 48;
            int py = (int)dty + 16 + (p / 2) * 32;
            DrawRectangle(px, py, 38, 24, (Color){205, 145, 70, 255});
            DrawRectangleLines(px, py, 38, 24, textYellow);
            DrawRectangle(px + 5, py + 4, 28, 16, GetSkuBoxColor(p));
        }

        if (((int)(ai->simTime * 6.0f)) % 2 == 0)
        {
            DrawCircle((int)dockBay.x + 14, (int)dty + 92, 6.5f, warnOrange);
            DrawCircle((int)dockBay.x + 138, (int)dty + 92, 6.5f, warnOrange);
        }
        DrawTextCenteredInBox((Rectangle){dockBay.x + 24, dty - 28, 104, 24}, "UNLOADING CARGO", 12, (Color){15, 18, 26, 255});
    }

    // DEDICATED YELLOW-LINED FORKLIFT PARKING BAY [P-01]
    Rectangle parkBay = {whRect.x + 20, whRect.y + whRect.height - 218, 168, 92};
    DrawRectangleRec(parkBay, (Color){22, 26, 36, 255});
    DrawRectangleLinesEx(parkBay, 3.0f, safetyYellow);

    for (int hx = (int)parkBay.x + 8; hx < (int)(parkBay.x + parkBay.width - 10); hx += 16)
    {
        DrawLineEx((Vector2){(float)hx, parkBay.y + 3},
                   (Vector2){(float)hx + 8, parkBay.y + 14},
                   2.2f, (Color){250, 205, 35, 195});
    }

    DrawLineEx((Vector2){parkBay.x + 22, parkBay.y + 26}, (Vector2){parkBay.x + 115, parkBay.y + 26}, 2.0f, (Color){250, 205, 35, 130});
    DrawLineEx((Vector2){parkBay.x + 22, parkBay.y + 68}, (Vector2){parkBay.x + 115, parkBay.y + 68}, 2.0f, (Color){250, 205, 35, 130});
    DrawRectangle((int)parkBay.x + 8, (int)parkBay.y + 32, 8, 28, (Color){60, 70, 90, 255});
    DrawCircle((int)parkBay.x + 12, (int)parkBay.y + 46, 3.5f, forklift.state == FORK_IDLE ? textGreen : warnOrange);
    DrawTextCrisp("BAY P-01", (int)(parkBay.x + parkBay.width - 74), (int)(parkBay.y + parkBay.height - 22), 14, safetyYellow);

    // SEPARATED WAREHOUSE TELEMETRY CARD
    Rectangle forkPad = {whRect.x + 12, whRect.y + whRect.height - 110, whWidth - 24, 96};
    DrawDashboardCard(forkPad, (Color){18, 22, 32, 245}, (Color){255, 195, 45, 130}, 0.15f, false);
    DrawTextTruncated(TextFormat("FORKLIFT 01: %s", forklift.state == FORK_IDLE ? "PARKED [BAY P-01]" : "ACTIVE UNLOADING"),
                      (int)forkPad.x + 14, (int)forkPad.y + 12, 15, (int)forkPad.width - 24, forklift.state == FORK_IDLE ? highlightCyan : warnOrange);
    DrawTextTruncated(TextFormat("Holding Tax: EUR %.0f / 8s Cycle", ai->lastStorageBill),
                      (int)forkPad.x + 14, (int)forkPad.y + 38, 15, (int)forkPad.width - 24, warnOrange);
    DrawTextTruncated(TextFormat("Power Bill:  EUR %.0f / 8s Cycle", ai->lastUtilityBill),
                      (int)forkPad.x + 14, (int)forkPad.y + 64, 15, (int)forkPad.width - 24, ev->energySpikeTimer > 0 ? barRed : textDim);

    // 3. RETAIL STORE FLOOR & ADAPTIVE 1-COLUMN / 2-COLUMN STORE AISLES
    Rectangle storeRect = {whRect.x + whRect.width, arena.y + 164, arena.width - 12 - whRect.width, arena.height - 170};
    DrawRectangleRec(storeRect, storeFloorA);

    int tileSize = 44;
    for (int ty = (int)storeRect.y; ty < (int)(storeRect.y + storeRect.height - 8); ty += tileSize)
    {
        for (int tx = (int)storeRect.x; tx < (int)(storeRect.x + storeRect.width - 8); tx += tileSize)
        {
            if (((tx / tileSize) + (ty / tileSize)) % 2 == 0)
            {
                DrawRectangle(tx, ty, tileSize, tileSize, storeFloorB);
            }
        }
    }

    if (ev->energySpikeTimer > 0.0f && ((int)(ai->simTime * 10.0f) % 3 == 0))
    {
        DrawRectangleRec(storeRect, (Color){255, 195, 45, 24});
    }

    if (!twoAisleMode)
    {
        Rectangle rackZone = {756.0f, arena.y + 164, 172.0f, arena.height - 170};
        DrawRectangleRec(rackZone, (Color){20, 24, 34, 255});
        DrawLineEx((Vector2){rackZone.x, rackZone.y}, (Vector2){rackZone.x, rackZone.y + rackZone.height}, 2.0f, gridLine);
        DrawLineEx((Vector2){rackZone.x + rackZone.width, rackZone.y}, (Vector2){rackZone.x + rackZone.width, rackZone.y + rackZone.height}, 2.0f, gridLine);
        DrawTextCenteredInBox((Rectangle){rackZone.x, rackZone.y + 8, rackZone.width, 24},
                              TextFormat("AISLE 1 (%d/%d SKUs)", totalActiveSkus, MAX_STORE_SKUS), 14, highlightCyan);
    }
    else
    {
        Rectangle aisle1Zone = {736.0f, arena.y + 164, 144.0f, arena.height - 170};
        Rectangle aisle2Zone = {918.0f, arena.y + 164, 144.0f, arena.height - 170};
        DrawRectangleRec(aisle1Zone, (Color){20, 24, 34, 245});
        DrawRectangleRec(aisle2Zone, (Color){20, 24, 34, 245});
        DrawLineEx((Vector2){aisle1Zone.x, aisle1Zone.y}, (Vector2){aisle1Zone.x, aisle1Zone.y + aisle1Zone.height}, 2.0f, gridLine);
        DrawLineEx((Vector2){aisle1Zone.x + aisle1Zone.width, aisle1Zone.y}, (Vector2){aisle1Zone.x + aisle1Zone.width, aisle1Zone.y + aisle1Zone.height}, 2.0f, gridLine);
        DrawLineEx((Vector2){aisle2Zone.x, aisle2Zone.y}, (Vector2){aisle2Zone.x, aisle2Zone.y + aisle2Zone.height}, 2.0f, gridLine);
        DrawLineEx((Vector2){aisle2Zone.x + aisle2Zone.width, aisle2Zone.y}, (Vector2){aisle2Zone.x + aisle2Zone.width, aisle2Zone.y + aisle2Zone.height}, 2.0f, gridLine);

        DrawTextCenteredInBox((Rectangle){aisle1Zone.x, aisle1Zone.y + 8, aisle1Zone.width, 24}, "AISLE 1 (BAYS 1-6)", 13, highlightCyan);
        DrawTextCenteredInBox((Rectangle){aisle2Zone.x, aisle2Zone.y + 8, aisle2Zone.width, 24},
                              TextFormat("AISLE 2 (BAYS 7-%d)", totalActiveSkus), 13, highlightPurple);
    }

    int shelfDrawn = 0;
    for (int i = 0; i < MAX_ITEMS && shelfDrawn < MAX_STORE_SKUS; i++)
    {
        if (!inventory[i].isActive)
            continue;
        Rectangle shelfBay = GetShelfBayRect(shelfDrawn, totalActiveSkus);

        bool isLow = (inventory[i].quantity <= inventory[i].minStock);
        bool onSale = (inventory[i].saleTimer > 0.0f);
        float mPct = (ai->aiPriceMult[i] - 1.0f) * 100.0f;
        Color frameCol = onSale ? warnOrange : (isLow ? barRed : (mPct > 5.0f ? highlightPurple : gridLine));

        DrawRectangleRounded(shelfBay, 0.12f, 6, (Color){13, 16, 24, 255});
        DrawRectangle((int)shelfBay.x, (int)(shelfBay.y + shelfBay.height * 0.32f), (int)shelfBay.width, 3, (Color){235, 125, 30, 180});
        DrawRectangle((int)shelfBay.x, (int)(shelfBay.y + shelfBay.height * 0.66f), (int)shelfBay.width, 3, (Color){235, 125, 30, 180});
        DrawRectangleLinesEx(shelfBay, 2.0f, frameCol);

        int nameMaxW = (int)shelfBay.width - 48;
        DrawTextTruncated(inventory[i].name, (int)shelfBay.x + 6, (int)shelfBay.y + 4, 13, nameMaxW, textYellow);
        Color tagCol = (mPct > 4.0f) ? warnOrange : ((mPct < -2.0f) ? textGreen : highlightCyan);
        DrawTextTruncated(TextFormat("%+.0f%%", mPct), (int)(shelfBay.x + shelfBay.width - 42), (int)shelfBay.y + 4, 13, 40, tagCol);

        int maxS = inventory[i].maxStock > 0 ? inventory[i].maxStock : 30;
        float fill = (float)inventory[i].quantity / (float)maxS;
        if (fill > 1.0f)
            fill = 1.0f;

        int maxCols = twoAisleMode ? 5 : 6;
        int totalBoxSlots = maxCols * 2;
        int numBoxes = (int)ceilf(fill * (float)totalBoxSlots);
        if (inventory[i].quantity == 0)
            numBoxes = 0;

        Color boxCol = GetSkuBoxColor(i);
        int boxW = twoAisleMode ? 17 : 18;
        int boxStepX = 22;

        for (int b = 0; b < numBoxes; b++)
        {
            int col = b % maxCols;
            int row = b / maxCols;
            int bx = (int)shelfBay.x + 8 + col * boxStepX;
            int by = (int)(shelfBay.y + shelfBay.height - 24) - row * 28;
            DrawRectangle(bx, by, boxW, 19, boxCol);
            DrawRectangle(bx, by, boxW, 4, (Color){255, 255, 255, 105});
            DrawRectangleLines(bx, by, boxW, 19, (Color){15, 18, 26, 200});
        }

        if (onSale)
        {
            Rectangle saleTag = {shelfBay.x + shelfBay.width - 52, shelfBay.y + 26, 48, 18};
            DrawRectangleRounded(saleTag, 0.3f, 4, warnOrange);
            DrawTextCenteredInBox(saleTag, "SALE", 12, (Color){15, 16, 22, 255});
        }
        shelfDrawn++;
    }

    // 4. AUTONOMOUS AI INVENTORY DRONE & HIGH-DETAIL FORKLIFT
    {
        float dx = aiDrone.pos.x;
        float dy = aiDrone.pos.y + sinf(aiDrone.hoverPhase) * 4.5f;
        DrawTriangle((Vector2){dx, dy}, (Vector2){dx + 38, dy - 22}, (Vector2){dx + 38, dy + 22},
                     (Color){35, 245, 255, (unsigned char)(40 + 25 * sinf(ai->simTime * 8.0f))});
        DrawCircle((int)dx, (int)dy + 14, 10.0f, (Color){0, 0, 0, 90});
        DrawCircle((int)dx, (int)dy, 13.0f, (Color){30, 38, 54, 255});
        DrawCircleLines((int)dx, (int)dy, 13.0f, highlightCyan);
        DrawCircle((int)dx, (int)dy, 5.5f, highlightPurple);
        DrawCircle((int)dx + 4, (int)dy, 2.5f, highlightCyan);
    }

    {
        float fx = forklift.pos.x;
        float fy = forklift.pos.y;
        float dir = forklift.facingRight ? 1.0f : -1.0f;

        DrawEllipse((int)fx, (int)fy + 20, 30.0f, 11.0f, (Color){0, 0, 0, 130});

        float liftOffset = forklift.forkElevation * 10.0f;
        DrawLineEx((Vector2){fx + 16 * dir, fy - 8 - liftOffset}, (Vector2){fx + 44 * dir, fy - 8 - liftOffset}, 4.0f, (Color){185, 195, 210, 255});
        DrawLineEx((Vector2){fx + 16 * dir, fy + 8 - liftOffset}, (Vector2){fx + 44 * dir, fy + 8 - liftOffset}, 4.0f, (Color){185, 195, 210, 255});
        DrawRectangle((int)(fx + 14 * dir) - 3, (int)fy - 14, 6, 28, (Color){40, 45, 58, 255});

        if (forklift.hasPallet)
        {
            float px = forklift.facingRight ? (fx + 18.0f) : (fx - 46.0f);
            float py = fy - 18.0f - liftOffset;
            DrawRectangle((int)px, (int)py, 28, 32, (Color){205, 145, 70, 255});
            DrawRectangleLines((int)px, (int)py, 28, 32, textYellow);
            DrawRectangle((int)px + 4, (int)py + 4, 20, 24, GetSkuBoxColor(forklift.targetRackIdx));
            DrawLine((int)px + 4, (int)py + 16, (int)px + 24, (int)py + 16, textYellow);
        }

        Rectangle body = {fx - 24, fy - 18, 48, 36};
        DrawRectangleRounded(body, 0.35f, 8, (Color){250, 190, 25, 255});
        DrawRectangleRounded((Rectangle){fx - 15, fy - 13, 30, 26}, 0.25f, 6, (Color){26, 30, 42, 255});

        DrawCircle((int)fx, (int)fy, 8.5f, highlightCyan);
        DrawCircle((int)fx + (int)(2 * dir), (int)fy, 5.0f, (Color){255, 235, 150, 255});

        DrawRectangleRounded((Rectangle){fx - 20, fy - 23, 12, 6}, 0.3f, 4, (Color){15, 16, 20, 255});
        DrawRectangleRounded((Rectangle){fx + 8, fy - 23, 12, 6}, 0.3f, 4, (Color){15, 16, 20, 255});
        DrawRectangleRounded((Rectangle){fx - 20, fy + 17, 12, 6}, 0.3f, 4, (Color){15, 16, 20, 255});
        DrawRectangleRounded((Rectangle){fx + 8, fy + 17, 12, 6}, 0.3f, 4, (Color){15, 16, 20, 255});

        if (forklift.state != FORK_IDLE && ((int)(ai->simTime * 8.0f) % 2 == 0))
        {
            DrawCircle((int)(fx - 10 * dir), (int)fy - 8, 11.0f, (Color){255, 205, 0, 170});
            DrawCircle((int)(fx - 10 * dir), (int)fy - 8, 5.0f, textYellow);
        }
    }

    // 5. STORE ENTRANCE & PROMO BANNERS
    Rectangle doorFrame = {1300.0f, storeRect.y - 4, 185, 24};
    DrawRectangleRec(doorFrame, (Color){14, 22, 34, 255});
    DrawRectangleLinesEx(doorFrame, 2.0f, highlightCyan);
    DrawLine((int)(doorFrame.x + doorFrame.width * 0.5f), (int)doorFrame.y, (int)(doorFrame.x + doorFrame.width * 0.5f), (int)(doorFrame.y + doorFrame.height), highlightCyan);
    DrawTextCenteredInBox(doorFrame, "NDAL STORE ENTRANCE", 12, highlightCyan);

    DrawCircle(1280, (int)storeRect.y + 22, 12.0f, (Color){40, 125, 75, 255});
    DrawCircle(1505, (int)storeRect.y + 22, 12.0f, (Color){40, 125, 75, 255});

    float bannerX = twoAisleMode ? 1072.0f : 942.0f;
    int bannerMaxW = (int)(1290.0f - bannerX);

    bool anyFlashSale = false;
    for (int i = 0; i < MAX_ITEMS; i++)
        if (inventory[i].isActive && inventory[i].saleTimer > 0.0f)
            anyFlashSale = true;
    if (anyFlashSale || ev->viralSurgeTimer > 0.0f)
    {
        float pulse = 0.5f + 0.5f * sinf(ai->simTime * 6.0f);
        Color spotCol = (ev->viralSurgeTimer > 0.0f) ? (Color){255, 80, 100, (unsigned char)(32 + pulse * 38)}
                                                     : (Color){255, 195, 45, (unsigned char)(32 + pulse * 38)};
        DrawCircle(1140, (int)storeRect.y + 320, 135.0f, spotCol);
        const char *promoBanner = (ev->viralSurgeTimer > 0.0f) ? "VIRAL SURGE 2.5x!" : "FLASH SALE 2.2x!";
        DrawTextTruncated(promoBanner, (int)bannerX, (int)storeRect.y + 16, 17, bannerMaxW, ev->viralSurgeTimer > 0.0f ? barRed : warnOrange);
    }
    else
    {
        DrawTextTruncated(TextFormat("MARKUP: %+.1f%% | VEL: %.2fx", ai->avgPriceMarkupPct, GetSatisfactionDemandMultiplier()),
                          (int)bannerX, (int)storeRect.y + 16, 15, bannerMaxW, ai->avgPriceMarkupPct > 4.0f ? warnOrange : highlightCyan);
    }

    // 6. THREE MODERN CASHIER CHECKOUT LANES & CONVEYOR BELTS
    for (int t = 0; t < 3; t++)
    {
        float ty = 370.0f + t * 175.0f;
        float tx = 1325.0f;

        Rectangle queueMat = {tx - 92, ty + 24, 82, 54};
        DrawRectangleRounded(queueMat, 0.25f, 6, (Color){35, 245, 255, 32});
        DrawTextCenteredInBox(queueMat, "QUEUE HERE", 12, highlightCyan);

        Rectangle counter = {tx, ty, 132, 104};
        DrawDashboardCard(counter, (Color){34, 40, 56, 255}, tillScanLaserTimer[t] > 0.0f ? textGreen : (ev->wageSurgeTimer > 0.0f ? barRed : gridLine), 0.18f, false);

        Rectangle belt = {tx + 10, ty + 10, 34, 84};
        DrawRectangleRec(belt, (Color){16, 18, 24, 255});
        for (int by = (int)belt.y + (int)(conveyorBeltOffset) % 14; by < (int)(belt.y + belt.height - 2); by += 14)
        {
            DrawLine((int)belt.x + 3, by, (int)(belt.x + belt.width - 3), by, (Color){75, 84, 105, 255});
        }
        DrawRectangleLinesEx(belt, 1.0f, gridLine);

        Rectangle posTerm = {tx + 54, ty + 30, 34, 42};
        DrawRectangleRec(posTerm, (Color){18, 22, 32, 255});
        DrawRectangleLinesEx(posTerm, 1.5f, tillScanLaserTimer[t] > 0.0f ? textGreen : highlightCyan);
        DrawRectangle((int)posTerm.x + 5, (int)posTerm.y + 6, 24, 16, (Color){35, 200, 220, 100});

        float cx = tx + 108.0f;
        float cy = ty + 52.0f + sinf(ai->simTime * 3.2f + (float)t) * 2.5f;
        DrawCircle((int)cx, (int)cy + 4, 15.0f, (Color){0, 0, 0, 100});
        DrawCircle((int)cx, (int)cy, 14.0f, ev->wageSurgeTimer > 0.0f ? barRed : (Color){0, 185, 205, 255});
        DrawCircle((int)cx, (int)cy, 9.0f, (Color){245, 210, 180, 255});
        DrawCircle((int)cx + 2, (int)cy - 2, 7.5f, (Color){45, 35, 30, 255});

        DrawTextCrisp(ev->wageSurgeTimer > 0.0f ? "OVERTIME 1.85x!" : TextFormat("CHECKOUT LANE #%d", t + 1),
                      (int)tx + 6, (int)ty - 20, 14, ev->wageSurgeTimer > 0.0f ? barRed : textYellow);

        if (tillScanLaserTimer[t] > 0.0f)
        {
            DrawLineEx((Vector2){posTerm.x, posTerm.y + 21}, (Vector2){belt.x + 17, belt.y + 42}, 3.0f, barRed);
            DrawCircle((int)belt.x + 17, (int)belt.y + 42, 7.0f, (Color){255, 65, 65, 195});
            DrawTextCrisp("SCAN", (int)posTerm.x + 4, (int)posTerm.y + 24, 11, textGreen);
        }
    }

    DrawTextTruncated(TextFormat("Staff Payroll: EUR %.0f / 8s   |   %d Active Aisles   |   3 POS Lanes",
                                 ai->lastStaffBill, twoAisleMode ? 2 : 1),
                      (int)bannerX, (int)(storeRect.y + storeRect.height - 28), 15, 480, ev->wageSurgeTimer > 0 ? barRed : textDim);

    // 7. DETAILED 1080P ANIMATED CUSTOMER SHOPPERS
    for (int c = 0; c < MAX_ANIM_CUSTOMERS; c++)
    {
        if (!customers[c].active)
            continue;

        float bob = sinf(customers[c].walkPhase) * 3.0f;
        float cx = customers[c].pos.x;
        float cy = customers[c].pos.y + bob;

        DrawEllipse((int)cx, (int)cy + 10, 15.0f, 8.0f, (Color){0, 0, 0, 115});

        float stride = cosf(customers[c].walkPhase) * 5.0f;
        DrawCircle((int)(cx - 5 + stride), (int)cy + 8, 4.0f, (Color){30, 32, 40, 255});
        DrawCircle((int)(cx + 5 - stride), (int)cy + 8, 4.0f, (Color){30, 32, 40, 255});

        DrawCircle((int)cx, (int)cy, 13.5f, customers[c].shirtColor);
        DrawCircleLines((int)cx, (int)cy, 13.5f, (Color){255, 255, 255, 85});

        DrawCircle((int)cx, (int)cy - 2, 8.5f, (Color){245, 215, 185, 255});
        DrawCircle((int)cx, (int)cy - 4, 7.5f, customers[c].hairColor);

        if (customers[c].hasItem && (customers[c].state == CUST_TO_QUEUE || customers[c].state == CUST_SCANNING || customers[c].state == CUST_EXITING))
        {
            DrawRectangleRounded((Rectangle){cx + 9, cy - 8, 15, 15}, 0.2f, 4, (Color){40, 46, 62, 255});
            DrawRectangle((int)cx + 11, (int)cy - 6, 11, 11, customers[c].itemColor);
            DrawRectangleLines((int)cx + 11, (int)cy - 6, 11, 11, textYellow);
        }

        if (customers[c].state == CUST_ANGRY_EXIT)
        {
            DrawCircle((int)cx, (int)cy - 22, 10.0f, barRed);
            DrawTextCrisp("!", (int)cx - 3, (int)cy - 29, 16, textYellow);
        }
    }

    // 8. WEATHER STORM & SUBSIDY OVERLAY FX
    if (ev->weatherDelayTimer > 0.0f)
    {
        for (int r = 0; r < 65; r++)
        {
            float rx = arena.x + 20 + fmodf((float)(r * 61) + ai->simTime * 340.0f, arena.width - 40);
            float ry = arena.y + 44 + fmodf((float)(r * 37) + ai->simTime * 420.0f, 160.0f);
            DrawLineEx((Vector2){rx, ry}, (Vector2){rx - 14.0f, ry + 24.0f}, 2.0f, (Color){140, 205, 255, 185});
        }
        if (ev->lightningFlashAlpha > 0.01f)
        {
            DrawRectangleRec(roadRect, (Color){255, 255, 255, (unsigned char)(ev->lightningFlashAlpha * 185.0f)});
        }
        DrawPillBadge((Rectangle){arena.x + 280, arena.y + 52, 320, 28}, TextFormat("SEVERE STORM: -55%% TRANSIT SPEED (%.0fs)", ev->weatherDelayTimer), highlightPurple, bgArena);
    }

    if (ev->subsidyTimer > 0.0f)
    {
        DrawPillBadge((Rectangle){arena.x + 615, arena.y + 52, 290, 28}, TextFormat("GOV SUBSIDY: -20%% ORDER COST (%.0fs)", ev->subsidyTimer), textGreen, bgArena);
    }

    // 9. FLOATING REVENUE, UPKEEP & SATISFACTION POPUPS
    for (int p = 0; p < MAX_FLOATING_POPUPS; p++)
    {
        if (popups[p].active)
        {
            float alphaRatio = popups[p].timer / popups[p].maxTime;
            if (alphaRatio < 0.0f)
                alphaRatio = 0.0f;
            Color c = popups[p].color;
            c.a = (unsigned char)(alphaRatio * 255.0f);
            DrawTextCrisp(popups[p].text, (int)popups[p].pos.x, (int)popups[p].pos.y, 17, c);
        }
    }

    // 10. ARENA HEADER STATUS STRIP (y: 120..164)
    Rectangle arenaHdr = {arena.x + 6, arena.y + 6, arena.width - 12, 36};
    DrawRectangleRec(arenaHdr, (Color){12, 15, 22, 240});
    DrawTextTruncated("NDAL INVENTORY MANAGEMENT - LIVE DIGITAL TWIN", (int)arenaHdr.x + 16, (int)arenaHdr.y + 9, 17, 490, highlightCyan);
    DrawTextTruncated(TextFormat("Active SKUs: %d/%d (%s)  |  Markup: %+.1f%%  |  Next Bill: EUR %.0f in %.1fs",
                                 totalActiveSkus, MAX_STORE_SKUS, twoAisleMode ? "2 Aisles" : "1 Aisle",
                                 ai->avgPriceMarkupPct, ai->lastTotalBill, UPKEEP_BILL_INTERVAL - ai->upkeepTimer),
                      (int)arenaHdr.x + 515, (int)arenaHdr.y + 10, 15, 675, textYellow);
}