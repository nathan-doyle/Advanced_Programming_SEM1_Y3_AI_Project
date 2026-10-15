#include "ui.h"
#include "suppliers.h"
#include "events.h"
#include "telemetry.h"
#include "ai.h"
#include "arena.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// =============================================================================
// PURE UI STATE VARIABLES
// =============================================================================
static AppScreen currentScreen = SCREEN_MENU;
static bool showItemFormModal = false;
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
static int scrollHistory = 0;
static bool fontFilterInitialized = false;

static bool catalogShowGraph = false;
static int selectedGraphSku = 0;
static bool showLinePrice = true;
static bool showLineCost = true;
static bool showLineSales = true;
static bool showLineProfit = true;

// =============================================================================
// HIGH-BRIGHTNESS, RAZOR-SHARP 1080P TYPOGRAPHY & RENDERING HELPERS
// =============================================================================

static float GetCrispSpacing(int fontSize)
{
    return (fontSize >= 22) ? 1.6f : ((fontSize >= 16) ? 1.3f : 1.0f);
}

int MeasureTextCrisp(const char *text, int fontSize)
{
    if (text == NULL || text[0] == '\0')
        return 0;
    Vector2 sz = MeasureTextEx(GetFontDefault(), text, (float)fontSize, GetCrispSpacing(fontSize));
    return (int)ceilf(sz.x) + 1;
}

void DrawTextCrisp(const char *text, int x, int y, int fontSize, Color color)
{
    if (text == NULL || text[0] == '\0')
        return;
    float sp = GetCrispSpacing(fontSize);
    Font f = GetFontDefault();

    Color shadowCol = (Color){0, 0, 0, (unsigned char)(color.a * 0.85f)};
    DrawTextEx(f, text, (Vector2){(float)x + 1.5f, (float)y + 1.5f}, (float)fontSize, sp, shadowCol);

    DrawTextEx(f, text, (Vector2){(float)x, (float)y}, (float)fontSize, sp, color);
    if (fontSize >= 13)
    {
        Color boldPass = color;
        boldPass.a = (unsigned char)(color.a * 0.55f);
        DrawTextEx(f, text, (Vector2){(float)x + 0.55f, (float)y}, (float)fontSize, sp, boldPass);
    }
}

void DrawTextTruncated(const char *text, int x, int y, int fontSize, int maxWidth, Color color)
{
    if (text == NULL || maxWidth <= 8)
        return;
    if (MeasureTextCrisp(text, fontSize) <= maxWidth)
    {
        DrawTextCrisp(text, x, y, fontSize, color);
        return;
    }
    char buf[180];
    strncpy(buf, text, 174);
    buf[174] = '\0';
    int len = (int)strlen(buf);
    while (len > 0)
    {
        buf[len] = '\0';
        char temp[184];
        snprintf(temp, sizeof(temp), "%s..", buf);
        if (MeasureTextCrisp(temp, fontSize) <= maxWidth)
        {
            DrawTextCrisp(temp, x, y, fontSize, color);
            return;
        }
        len--;
    }
}

void DrawTextCenteredInBox(Rectangle box, const char *text, int fontSize, Color color)
{
    if (text == NULL)
        return;
    int maxW = (int)box.width - 10;
    if (maxW <= 6)
        return;

    char buf[140];
    strncpy(buf, text, 134);
    buf[134] = '\0';

    if (MeasureTextCrisp(buf, fontSize) > maxW)
    {
        int len = (int)strlen(buf);
        while (len > 0)
        {
            buf[len] = '\0';
            char temp[144];
            snprintf(temp, sizeof(temp), "%s..", buf);
            if (MeasureTextCrisp(temp, fontSize) <= maxW)
            {
                strcpy(buf, temp);
                break;
            }
            len--;
        }
    }
    int tw = MeasureTextCrisp(buf, fontSize);
    int tx = (int)(box.x + (box.width - tw) * 0.5f);
    int ty = (int)(box.y + (box.height - fontSize) * 0.5f);
    DrawTextCrisp(buf, tx, ty, fontSize, color);
}

void DrawDashboardCard(Rectangle rect, Color bgColor, Color borderColor, float roundness, bool isHovered)
{
    DrawRectangleRounded((Rectangle){rect.x + 3, rect.y + 6, rect.width, rect.height}, roundness, 12, (Color){0, 0, 0, 95});
    DrawRectangleRounded((Rectangle){rect.x + 1, rect.y + 3, rect.width, rect.height}, roundness, 12, (Color){0, 0, 0, 65});

    float borderPad = isHovered ? 2.0f : 1.2f;
    Rectangle borderRect = {rect.x - borderPad, rect.y - borderPad, rect.width + borderPad * 2.0f, rect.height + borderPad * 2.0f};
    Color outline = isHovered ? borderColor : (Color){borderColor.r, borderColor.g, borderColor.b, 120};
    DrawRectangleRounded(borderRect, roundness, 12, outline);

    DrawRectangleRounded(rect, roundness, 12, bgColor);

    Rectangle topSheen = {rect.x + 2, rect.y + 2, rect.width - 4, rect.height * 0.42f};
    DrawRectangleRounded(topSheen, roundness, 8, (Color){255, 255, 255, isHovered ? 16 : 7});
}

void DrawPillBadge(Rectangle rect, const char *text, Color accentColor, Color bgDark)
{
    (void)bgDark;
    Color fill = (Color){accentColor.r, accentColor.g, accentColor.b, 50};
    Rectangle borderRect = {rect.x - 1.2f, rect.y - 1.2f, rect.width + 2.4f, rect.height + 2.4f};
    DrawRectangleRounded(borderRect, 0.55f, 8, (Color){accentColor.r, accentColor.g, accentColor.b, 200});
    DrawRectangleRounded(rect, 0.55f, 8, fill);
    DrawTextCenteredInBox(rect, text, (rect.height >= 24) ? 14 : 12, accentColor);
}

// =============================================================================
// MASTER SIMULATION INITIALIZER & STEP ROUTER
// =============================================================================

void InitSimulation(InventoryItem inventory[], int *itemCount)
{
    if (*itemCount == 0)
    {
        SeedDefaultInventory(inventory, itemCount);
    }
    InitSuppliers();
    InitEvents();
    InitAiEconomy(inventory, itemCount);
    InitTelemetry(inventory);
    InitArena();

    PushNotification("NDAL HARDCORE ECONOMY: Modular Architecture, Lean Shelves & Heavy Upkeep Active.", 0);
}

static void StepAllSimulationModules(float dt, InventoryItem inventory[], int *itemCount, OrderRecord orders[], int *orderCount)
{
    UpdateNotificationTimers(GetFrameTime());
    if (dt <= 0.0f)
        return;

    ChaosEventState *ev = GetEventsState();
    AiEconomyState *ai = GetAiEconomyState();

    UpdateEventsStep(dt, inventory, *itemCount, orders, *orderCount);
    UpdateSupplierTimers(dt);
    UpdateActiveOrders(dt, ev->weatherDelayTimer, inventory, *itemCount, orders, *orderCount);
    UpdateAiAndEconomyStep(dt, inventory, itemCount, orders, orderCount);
    UpdateTelemetrySampler(dt, ai->simTime, inventory);
    UpdateArenaAnimations(dt, inventory);
}

// =============================================================================
// 1080P PRODUCT CATALOGUE MULTI-LINE TELEMETRY GRAPH RENDERER
// =============================================================================

static void DrawProductAnalyticsGraph(Rectangle centerArena, Vector2 mousePoint, InventoryItem inventory[], int itemCount)
{
    Color bgDark = (Color){11, 13, 20, 255};
    Color cardDark = (Color){22, 26, 38, 255};
    Color cardHover = (Color){36, 42, 60, 255};
    Color inputDark = (Color){15, 18, 28, 255};
    Color textYellow = (Color){255, 255, 242, 255};
    Color textDim = (Color){205, 218, 238, 255};
    Color highlightCyan = (Color){35, 245, 255, 255};
    Color highlightPurple = (Color){205, 135, 255, 255};
    Color textGreen = (Color){55, 250, 155, 255};
    Color warnOrange = (Color){255, 195, 45, 255};
    Color gridLine = (Color){52, 60, 84, 255};

    if (selectedGraphSku < 0 || selectedGraphSku >= itemCount || !inventory[selectedGraphSku].isActive)
    {
        selectedGraphSku = 0;
        for (int i = 0; i < itemCount; i++)
        {
            if (inventory[i].isActive)
            {
                selectedGraphSku = i;
                break;
            }
        }
    }

    InventoryItem *sku = &inventory[selectedGraphSku];
    ProductTelemetry *t = GetProductTelemetry(selectedGraphSku);
    ChaosEventState *ev = GetEventsState();
    AiEconomyState *ai = GetAiEconomyState();

    float barY = centerArena.y + 68.0f;
    Rectangle prevBtn = {centerArena.x + 24, barY, 110, 38};
    Rectangle skuPill = {centerArena.x + 144, barY, 470, 38};
    Rectangle nextBtn = {centerArena.x + 624, barY, 110, 38};
    Rectangle pDownBtn = {centerArena.x + 754, barY, 210, 38};
    Rectangle pUpBtn = {centerArena.x + 976, barY, 216, 38};

    bool hPrev = CheckCollisionPointRec(mousePoint, prevBtn);
    bool hNext = CheckCollisionPointRec(mousePoint, nextBtn);
    bool hPDown = CheckCollisionPointRec(mousePoint, pDownBtn);
    bool hPUp = CheckCollisionPointRec(mousePoint, pUpBtn);

    DrawDashboardCard(prevBtn, hPrev ? cardHover : inputDark, highlightCyan, 0.3f, hPrev);
    DrawTextCenteredInBox(prevBtn, "< PREV SKU", 15, textYellow);
    if (hPrev && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && itemCount > 0)
    {
        for (int step = 1; step <= itemCount; step++)
        {
            int cand = (selectedGraphSku - step + itemCount) % itemCount;
            if (inventory[cand].isActive)
            {
                selectedGraphSku = cand;
                break;
            }
        }
    }

    DrawDashboardCard(skuPill, cardDark, highlightCyan, 0.3f, false);
    DrawTextCenteredInBox(skuPill, TextFormat("SKU #%d: %s  |  Stock: %d / %d (+%d Transit)", sku->id, sku->name, sku->quantity, sku->maxStock, sku->inTransit), 18, textYellow);

    DrawDashboardCard(nextBtn, hNext ? cardHover : inputDark, highlightCyan, 0.3f, hNext);
    DrawTextCenteredInBox(nextBtn, "NEXT SKU >", 15, textYellow);
    if (hNext && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && itemCount > 0)
    {
        for (int step = 1; step <= itemCount; step++)
        {
            int cand = (selectedGraphSku + step) % itemCount;
            if (inventory[cand].isActive)
            {
                selectedGraphSku = cand;
                break;
            }
        }
    }

    DrawDashboardCard(pDownBtn, hPDown ? textGreen : inputDark, textGreen, 0.3f, hPDown);
    DrawTextCenteredInBox(pDownBtn, "-4% MANUAL PRICE CUT", 15, hPDown ? bgDark : textGreen);
    if (hPDown && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        AdjustSkuPriceMultiplier(selectedGraphSku, -0.04f, sku, ai->simTime);
    }

    DrawDashboardCard(pUpBtn, hPUp ? warnOrange : inputDark, warnOrange, 0.3f, hPUp);
    DrawTextCenteredInBox(pUpBtn, "+4% MANUAL PRICE HIKE", 15, hPUp ? bgDark : warnOrange);
    if (hPUp && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        AdjustSkuPriceMultiplier(selectedGraphSku, +0.04f, sku, ai->simTime);
    }

    float curPrice = (sku->saleTimer > 0.0f) ? (sku->price * 0.88f) : sku->price;
    float curCost = GetEffectiveUnitCost(sku, ev->subsidyTimer);
    float curMargin = curPrice - curCost;
    float latestVol = (t->count > 0) ? t->salesVol[t->count - 1] : 0.0f;
    float mPct = (ai->aiPriceMult[selectedGraphSku] - 1.0f) * 100.0f;

    float legY = centerArena.y + 118.0f;
    Rectangle leg1 = {centerArena.x + 24, legY, 280, 64};
    Rectangle leg2 = {centerArena.x + 320, legY, 280, 64};
    Rectangle leg3 = {centerArena.x + 616, legY, 280, 64};
    Rectangle leg4 = {centerArena.x + 912, legY, 280, 64};

    bool hL1 = CheckCollisionPointRec(mousePoint, leg1);
    bool hL2 = CheckCollisionPointRec(mousePoint, leg2);
    bool hL3 = CheckCollisionPointRec(mousePoint, leg3);
    bool hL4 = CheckCollisionPointRec(mousePoint, leg4);

    if (hL1 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        showLinePrice = !showLinePrice;
    if (hL2 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        showLineCost = !showLineCost;
    if (hL3 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        showLineSales = !showLineSales;
    if (hL4 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        showLineProfit = !showLineProfit;

    DrawDashboardCard(leg1, showLinePrice ? cardDark : inputDark, showLinePrice ? highlightCyan : gridLine, 0.2f, hL1);
    DrawCircle((int)leg1.x + 18, (int)leg1.y + 20, 6.0f, showLinePrice ? highlightCyan : textDim);
    DrawTextTruncated("LIVE RETAIL PRICE (LEFT AXIS)", (int)leg1.x + 32, (int)leg1.y + 12, 13, 238, showLinePrice ? highlightCyan : textDim);
    DrawTextTruncated(TextFormat("EUR %.2f (%+.1f%% Markup)", curPrice, mPct), (int)leg1.x + 16, (int)leg1.y + 34, 18, 250, textYellow);

    DrawDashboardCard(leg2, showLineCost ? cardDark : inputDark, showLineCost ? warnOrange : gridLine, 0.2f, hL2);
    DrawCircle((int)leg2.x + 18, (int)leg2.y + 20, 6.0f, showLineCost ? warnOrange : textDim);
    DrawTextTruncated("EFFECTIVE UNIT COST (LEFT AXIS)", (int)leg2.x + 32, (int)leg2.y + 12, 13, 238, showLineCost ? warnOrange : textDim);
    DrawTextTruncated(TextFormat("EUR %.2f (Base: EUR %.0f)", curCost, sku->orderCost), (int)leg2.x + 16, (int)leg2.y + 34, 18, 250, textYellow);

    DrawDashboardCard(leg3, showLineSales ? cardDark : inputDark, showLineSales ? highlightPurple : gridLine, 0.2f, hL3);
    DrawCircle((int)leg3.x + 18, (int)leg3.y + 20, 6.0f, showLineSales ? highlightPurple : textDim);
    DrawTextTruncated("SALES VOLUME (RIGHT AXIS)", (int)leg3.x + 32, (int)leg3.y + 12, 13, 238, showLineSales ? highlightPurple : textDim);
    DrawTextTruncated(TextFormat("%d Sold  |  %.2f units/cycle", t->totalUnitsSoldSku, latestVol), (int)leg3.x + 16, (int)leg3.y + 34, 18, 250, textYellow);

    DrawDashboardCard(leg4, showLineProfit ? cardDark : inputDark, showLineProfit ? textGreen : gridLine, 0.2f, hL4);
    DrawCircle((int)leg4.x + 18, (int)leg4.y + 20, 6.0f, showLineProfit ? textGreen : textDim);
    DrawTextTruncated("REALIZED PROFIT (LEFT AXIS)", (int)leg4.x + 32, (int)leg4.y + 12, 13, 238, showLineProfit ? textGreen : textDim);
    DrawTextTruncated(TextFormat("+EUR %.1f/u  |  Total: +EUR %.0f", curMargin, t->totalProfitSku), (int)leg4.x + 16, (int)leg4.y + 34, 18, 250, textGreen);

    Rectangle graphCard = {centerArena.x + 24, centerArena.y + 196, 1168, 536};
    DrawDashboardCard(graphCard, inputDark, gridLine, 0.04f, false);

    Rectangle plotRect = {graphCard.x + 84, graphCard.y + 42, graphCard.width - 168, graphCard.height - 86};
    DrawRectangleRec(plotRect, (Color){10, 12, 18, 255});
    DrawRectangleLinesEx(plotRect, 1.5f, gridLine);

    float maxEur = curPrice * 1.25f;
    float minEur = 0.0f;
    float maxVol = 4.0f;

    for (int k = 0; k < t->count; k++)
    {
        if (t->price[k] > maxEur)
            maxEur = t->price[k];
        if (t->cost[k] > maxEur)
            maxEur = t->cost[k];
        if (t->profit[k] > maxEur)
            maxEur = t->profit[k];
        if (t->profit[k] < minEur)
            minEur = t->profit[k];
        if (t->salesVol[k] > maxVol)
            maxVol = t->salesVol[k];
    }
    maxEur *= 1.12f;
    if (maxEur < 50.0f)
        maxEur = 50.0f;
    maxVol *= 1.20f;
    if (maxVol < 3.0f)
        maxVol = 3.0f;

    int gridSteps = 5;
    for (int g = 0; g <= gridSteps; g++)
    {
        float ratio = (float)g / (float)gridSteps;
        float gy = plotRect.y + plotRect.height - ratio * plotRect.height;
        DrawLine((int)plotRect.x, (int)gy, (int)(plotRect.x + plotRect.width), (int)gy, (Color){36, 42, 58, 255});

        float eurVal = minEur + ratio * (maxEur - minEur);
        DrawTextTruncated(TextFormat("EUR %.0f", eurVal), (int)graphCard.x + 10, (int)gy - 8, 14, 70, highlightCyan);

        float volVal = ratio * maxVol;
        DrawTextTruncated(TextFormat("%.1f u", volVal), (int)(plotRect.x + plotRect.width + 10), (int)gy - 8, 14, 70, highlightPurple);
    }

    DrawTextCrisp("EUR SCALE", (int)graphCard.x + 10, (int)graphCard.y + 12, 14, highlightCyan);
    DrawTextCrisp("UNITS/CYCLE", (int)(plotRect.x + plotRect.width - 10), (int)graphCard.y + 12, 14, highlightPurple);
    DrawTextCrisp("CLICK CARDS ABOVE TO TOGGLE LINES  |  HOVER GRAPH TO INSPECT EXACT TIMESTAMPS", (int)plotRect.x + 140, (int)graphCard.y + 12, 14, textDim);

    if (t->count >= 2)
    {
        float xStep = plotRect.width / (float)(GRAPH_HISTORY_LEN - 1);
        int startOffset = GRAPH_HISTORY_LEN - t->count;

        for (int k = 0; k < t->count - 1; k++)
        {
            float x1 = plotRect.x + (float)(startOffset + k) * xStep;
            float x2 = plotRect.x + (float)(startOffset + k + 1) * xStep;

            if (showLineCost)
            {
                float y1 = plotRect.y + plotRect.height - ((t->cost[k] - minEur) / (maxEur - minEur)) * plotRect.height;
                float y2 = plotRect.y + plotRect.height - ((t->cost[k + 1] - minEur) / (maxEur - minEur)) * plotRect.height;
                DrawLineEx((Vector2){x1, y1}, (Vector2){x2, y2}, 3.0f, warnOrange);
            }
            if (showLineProfit)
            {
                float y1 = plotRect.y + plotRect.height - ((t->profit[k] - minEur) / (maxEur - minEur)) * plotRect.height;
                float y2 = plotRect.y + plotRect.height - ((t->profit[k + 1] - minEur) / (maxEur - minEur)) * plotRect.height;
                DrawLineEx((Vector2){x1, y1}, (Vector2){x2, y2}, 3.0f, textGreen);
            }
            if (showLineSales)
            {
                float y1 = plotRect.y + plotRect.height - (t->salesVol[k] / maxVol) * plotRect.height;
                float y2 = plotRect.y + plotRect.height - (t->salesVol[k + 1] / maxVol) * plotRect.height;
                DrawLineEx((Vector2){x1, y1}, (Vector2){x2, y2}, 3.2f, highlightPurple);
            }
            if (showLinePrice)
            {
                float y1 = plotRect.y + plotRect.height - ((t->price[k] - minEur) / (maxEur - minEur)) * plotRect.height;
                float y2 = plotRect.y + plotRect.height - ((t->price[k + 1] - minEur) / (maxEur - minEur)) * plotRect.height;
                DrawLineEx((Vector2){x1, y1}, (Vector2){x2, y2}, 3.8f, highlightCyan);
            }
        }

        for (int k = 0; k < t->count; k++)
        {
            float x = plotRect.x + (float)(startOffset + k) * xStep;
            if (showLineCost)
            {
                float y = plotRect.y + plotRect.height - ((t->cost[k] - minEur) / (maxEur - minEur)) * plotRect.height;
                DrawCircle((int)x, (int)y, 4.0f, warnOrange);
            }
            if (showLineProfit)
            {
                float y = plotRect.y + plotRect.height - ((t->profit[k] - minEur) / (maxEur - minEur)) * plotRect.height;
                DrawCircle((int)x, (int)y, 4.0f, textGreen);
            }
            if (showLineSales)
            {
                float y = plotRect.y + plotRect.height - (t->salesVol[k] / maxVol) * plotRect.height;
                DrawCircle((int)x, (int)y, 4.2f, highlightPurple);
            }
            if (showLinePrice)
            {
                float y = plotRect.y + plotRect.height - ((t->price[k] - minEur) / (maxEur - minEur)) * plotRect.height;
                DrawCircle((int)x, (int)y, 4.8f, highlightCyan);
            }
        }
    }

    DrawTextCrisp("OLDEST SAMPLE", (int)plotRect.x + 6, (int)(plotRect.y + plotRect.height + 12), 14, textDim);
    DrawTextCrisp("ROLLING TELEMETRY TIMELINE", (int)(plotRect.x + plotRect.width * 0.40f), (int)(plotRect.y + plotRect.height + 12), 14, textDim);
    DrawTextCrisp("LIVE (NOW)", (int)(plotRect.x + plotRect.width - 92), (int)(plotRect.y + plotRect.height + 12), 14, highlightCyan);

    int inspectIdx = t->count - 1;
    if (CheckCollisionPointRec(mousePoint, plotRect) && t->count > 0)
    {
        float xStep = plotRect.width / (float)(GRAPH_HISTORY_LEN - 1);
        int startOffset = GRAPH_HISTORY_LEN - t->count;
        int hoveredSlot = (int)roundf((mousePoint.x - plotRect.x) / xStep);
        int candIdx = hoveredSlot - startOffset;
        if (candIdx < 0)
            candIdx = 0;
        if (candIdx >= t->count)
            candIdx = t->count - 1;
        inspectIdx = candIdx;

        float cx = plotRect.x + (float)(startOffset + inspectIdx) * xStep;
        DrawLineEx((Vector2){cx, plotRect.y}, (Vector2){cx, plotRect.y + plotRect.height}, 2.0f, textYellow);
    }

    Rectangle inspectBar = {centerArena.x + 24, centerArena.y + 744, 1168, 54};
    DrawDashboardCard(inspectBar, cardDark, highlightCyan, 0.25f, false);
    if (inspectIdx >= 0 && inspectIdx < t->count)
    {
        char inspectText[180];
        snprintf(inspectText, sizeof(inspectText),
                 "INSPECTING T = %.1fs   |   Retail Price: EUR %.2f   |   Unit Cost: EUR %.2f   |   Sales Velocity: %.2f u/cycle   |   Cycle Profit: +EUR %.2f",
                 t->timeStamp[inspectIdx], t->price[inspectIdx], t->cost[inspectIdx],
                 t->salesVol[inspectIdx], t->profit[inspectIdx]);
        DrawTextCenteredInBox(inspectBar, inspectText, 18, textYellow);
    }
}

// =============================================================================
// MAIN 1920x1080 PERIMETER HUD & SCREEN ROUTER
// =============================================================================

void UpdateAndDrawUI(InventoryItem inventory[], int *itemCount, OrderRecord orders[], int *orderCount)
{
    if (!fontFilterInitialized)
    {
        SetTextureFilter(GetFontDefault().texture, TEXTURE_FILTER_POINT);
        fontFilterInitialized = true;
    }

    Color bgDark = (Color){10, 12, 18, 255};
    Color cardDark = (Color){20, 24, 35, 255};
    Color cardHover = (Color){34, 40, 58, 255};
    Color inputDark = (Color){14, 17, 26, 255};
    Color textYellow = (Color){255, 255, 242, 255};
    Color textDim = (Color){205, 218, 238, 255};
    Color highlightCyan = (Color){35, 245, 255, 255};
    Color highlightPurple = (Color){205, 135, 255, 255};
    Color barRed = (Color){255, 80, 100, 255};
    Color textGreen = (Color){55, 250, 155, 255};
    Color warnOrange = (Color){255, 195, 45, 255};
    Color gridLine = (Color){52, 60, 84, 255};

    float scale = ((float)GetScreenWidth() / SCREEN_WIDTH < (float)GetScreenHeight() / SCREEN_HEIGHT) ? ((float)GetScreenWidth() / SCREEN_WIDTH) : ((float)GetScreenHeight() / SCREEN_HEIGHT);
    float offsetX = (GetScreenWidth() - (SCREEN_WIDTH * scale)) * 0.5f;
    float offsetY = (GetScreenHeight() - (SCREEN_HEIGHT * scale)) * 0.5f;
    Vector2 physicalMouse = GetMousePosition();
    Vector2 mousePoint = {(physicalMouse.x - offsetX) / scale, (physicalMouse.y - offsetY) / scale};
    int wheel = (int)GetMouseWheelMove();

    AiEconomyState *ai = GetAiEconomyState();
    ChaosEventState *ev = GetEventsState();
    Supplier *suppliers = GetSuppliers();
    SimNotification *notifs = GetNotifications();

    StepAllSimulationModules(GetFrameTime() * ai->simSpeed, inventory, itemCount, orders, orderCount);

    ClearBackground(bgDark);

    // --- GLOBAL TELEMETRY CALCULATIONS ---
    int activeStock = GetActiveSkuCount(inventory);
    int lowStockCount = 0;
    int totalUnits = 0, totalMaxCapacity = 0, inTransitUnits = 0, pendingOrders = 0;

    for (int i = 0; i < MAX_ITEMS; i++)
    {
        if (inventory[i].isActive)
        {
            totalUnits += inventory[i].quantity;
            totalMaxCapacity += (inventory[i].maxStock > 0 ? inventory[i].maxStock : 30);
            if (inventory[i].quantity <= inventory[i].minStock)
                lowStockCount++;
        }
    }
    for (int i = 0; i < *orderCount; i++)
    {
        if (orders[i].status != STATUS_DELIVERED)
        {
            pendingOrders++;
            for (int b = 0; b < orders[i].itemCount; b++)
                inTransitUnits += orders[i].items[b].orderQuantity;
        }
    }

    Color satColor = (ai->customerSatisfaction >= 75.0f) ? textGreen : ((ai->customerSatisfaction >= 50.0f) ? warnOrange : barRed);
    const char *satMood = (ai->customerSatisfaction >= 82.0f) ? "DELIGHTED" : ((ai->customerSatisfaction >= 60.0f) ? "CONTENT" : ((ai->customerSatisfaction >= 35.0f) ? "ANNOYED" : "FURIOUS"));

    // =========================================================================
    // 1. TOP EDGE 1080P TELEMETRY BAR (x: 16..1904, y: 14..106, Height: 92px)
    // =========================================================================
    Rectangle topBar = {16, 14, 1888, 92};
    DrawDashboardCard(topBar, cardDark, gridLine, 0.18f, false);

    DrawCircle(54, 60, 24, highlightPurple);
    DrawCircle(54, 60, 18, highlightCyan);
    DrawTextCrisp("ND", 42, 51, 17, bgDark);
    DrawTextTruncated("NDAL INVENTORY", 86, 34, 21, 190, textYellow);
    DrawTextTruncated("Management System", 86, 62, 15, 190, highlightCyan);

    Rectangle clockBox = {284, 24, 268, 72};
    DrawDashboardCard(clockBox, inputDark, highlightCyan, 0.22f, false);
    int mins = (int)(ai->simTime / 60.0f);
    float secs = fmodf(ai->simTime, 60.0f);
    DrawTextCrisp("SIMULATION CLOCK", (int)clockBox.x + 14, (int)clockBox.y + 10, 13, textDim);
    DrawTextTruncated(TextFormat("%02d:%04.1f", mins, secs), (int)clockBox.x + 14, (int)clockBox.y + 30, 28, 120, highlightCyan);

    float speeds[4] = {0.0f, 1.0f, 2.0f, 4.0f};
    const char *spdLabels[4] = {"II", "1x", "2x", "4x"};
    for (int s = 0; s < 4; s++)
    {
        Rectangle sb = {clockBox.x + 132 + s * 32, clockBox.y + 24, 28, 34};
        bool hSb = CheckCollisionPointRec(mousePoint, sb);
        bool activeSpd = (fabsf(ai->simSpeed - speeds[s]) < 0.1f);
        DrawDashboardCard(sb, activeSpd ? highlightPurple : (hSb ? cardHover : cardDark), highlightPurple, 0.3f, hSb || activeSpd);
        DrawTextCenteredInBox(sb, spdLabels[s], 14, textYellow);
        if (hSb && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            ai->simSpeed = speeds[s];
    }

    Rectangle kpi1 = {568, 24, 324, 72};
    Rectangle kpi2 = {904, 24, 324, 72};
    Rectangle kpi3 = {1240, 24, 324, 72};
    Rectangle kpi4 = {1576, 24, 314, 72};

    DrawDashboardCard(kpi1, inputDark, ai->netProfit >= 0 ? textGreen : barRed, 0.22f, false);
    DrawTextTruncated("NET PROFIT (AFTER UPKEEP)", (int)kpi1.x + 16, (int)kpi1.y + 8, 13, 290, textDim);
    DrawTextTruncated(TextFormat("EUR %.0f", ai->netProfit), (int)kpi1.x + 16, (int)kpi1.y + 26, 24, 185, ai->netProfit >= 0 ? textGreen : barRed);
    DrawTextTruncated(TextFormat("Rev: EUR %.0f | Upkeep: -EUR %.0f", ai->totalRevenue, ai->totalUpkeepPaid), (int)kpi1.x + 16, (int)kpi1.y + 52, 14, 295, textYellow);

    DrawDashboardCard(kpi2, inputDark, ai->avgPriceMarkupPct > 4.0f ? warnOrange : highlightCyan, 0.22f, false);
    DrawTextTruncated(TextFormat("AI PRICE MARKUP (%+.1f%%)", ai->avgPriceMarkupPct), (int)kpi2.x + 16, (int)kpi2.y + 8, 13, 290, textDim);
    DrawTextTruncated(TextFormat("Bill: EUR %.0f / 8s", ai->lastTotalBill), (int)kpi2.x + 16, (int)kpi2.y + 26, 22, 290, (ev->energySpikeTimer > 0 || ev->wageSurgeTimer > 0) ? barRed : textYellow);
    float billProg = ai->upkeepTimer / UPKEEP_BILL_INTERVAL;
    DrawRectangleRounded((Rectangle){kpi2.x + 16, kpi2.y + 54, 292, 8}, 1.0f, 6, cardDark);
    DrawRectangleRounded((Rectangle){kpi2.x + 16, kpi2.y + 54, 292 * billProg, 8}, 1.0f, 6, warnOrange);

    DrawDashboardCard(kpi3, inputDark, satColor, 0.22f, false);
    DrawTextTruncated(TextFormat("CUSTOMER SATISFACTION [%s]", satMood), (int)kpi3.x + 16, (int)kpi3.y + 8, 13, 290, textDim);
    DrawTextTruncated(TextFormat("%.0f%%  (%.2fx Demand Vel)", ai->customerSatisfaction, GetSatisfactionDemandMultiplier()), (int)kpi3.x + 16, (int)kpi3.y + 26, 22, 290, satColor);
    DrawRectangleRounded((Rectangle){kpi3.x + 16, kpi3.y + 54, 292, 8}, 1.0f, 6, cardDark);
    DrawRectangleRounded((Rectangle){kpi3.x + 16, kpi3.y + 54, 292 * (ai->customerSatisfaction / 100.0f), 8}, 1.0f, 6, satColor);

    bool hk4 = CheckCollisionPointRec(mousePoint, kpi4);
    DrawDashboardCard(kpi4, hk4 ? cardHover : inputDark, lowStockCount > 0 ? barRed : highlightPurple, 0.22f, hk4);
    DrawTextTruncated(TextFormat("CATALOG (%d/%d SKUs) & TRANSIT", activeStock, MAX_STORE_SKUS), (int)kpi4.x + 16, (int)kpi4.y + 8, 13, 285, textDim);
    DrawTextTruncated(TextFormat("%d Low SKUs  |  +%d In Transit", lowStockCount, inTransitUnits), (int)kpi4.x + 16, (int)kpi4.y + 26, 20, 285, lowStockCount > 0 ? barRed : textYellow);
    DrawTextTruncated(TextFormat("Sold: %d units  |  Missed: %d", ai->totalUnitsSold, ai->missedSalesCount), (int)kpi4.x + 16, (int)kpi4.y + 52, 14, 285, textGreen);
    if (hk4 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        currentScreen = SCREEN_ORDER_HISTORY;

    // =========================================================================
    // 2. LEFT EDGE COMMAND & ADAPTIVE 1/2-COLUMN SHELF MONITOR (x: 16..336, y: 120..936)
    // =========================================================================
    Rectangle leftEdge = {16, 120, 320, 816};
    DrawDashboardCard(leftEdge, cardDark, gridLine, 0.03f, false);

    DrawTextCrisp("NDAL WORKSPACES", 32, 134, 15, highlightCyan);
    const char *navLabels[5] = {
        "Live Digital Twin Arena",
        "Product Catalog & Graph",
        "Supplier Network & AI",
        "Chaos Director Deck (8)",
        "Shipment Transit Log"};
    AppScreen navScreens[5] = {SCREEN_MENU, SCREEN_VIEW, SCREEN_ORDER, SCREEN_EVENTS, SCREEN_ORDER_HISTORY};

    for (int n = 0; n < 5; n++)
    {
        Rectangle navBtn = {30, 158 + n * 46, 292, 38};
        bool isCurrent = (currentScreen == navScreens[n]);
        bool isHover = CheckCollisionPointRec(mousePoint, navBtn);

        Color navBorder = isCurrent ? highlightCyan : (isHover ? highlightPurple : gridLine);
        Color navBg = isCurrent ? (Color){35, 245, 255, 38} : (isHover ? cardHover : inputDark);
        DrawDashboardCard(navBtn, navBg, navBorder, 0.25f, isHover || isCurrent);

        DrawRectangleRounded((Rectangle){navBtn.x + 6, navBtn.y + 8, 4, 22}, 0.5f, 4, isCurrent ? highlightCyan : (isHover ? highlightPurple : textDim));
        DrawTextTruncated(navLabels[n], (int)navBtn.x + 20, (int)navBtn.y + 10, 16, 260, isCurrent ? textYellow : textDim);

        if (isHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            currentScreen = navScreens[n];
            showItemFormModal = false;
            scrollIndex = 0;
        }
    }

    Rectangle autoBtn = {30, 394, 292, 38};
    bool hAuto = CheckCollisionPointRec(mousePoint, autoBtn);
    const char *autoLabels[4] = {
        "AUTO-CHAOS ENGINE: OFF",
        "AUTO-CHAOS: LOW (14% / 6.5s)",
        "AUTO-CHAOS: MED (25% / 6.5s)",
        "AUTO-CHAOS: HIGH (40% / 6.5s)"};
    Color autoCol = (ev->autoChaosMode == 0) ? textDim : ((ev->autoChaosMode == 1) ? highlightCyan : ((ev->autoChaosMode == 2) ? warnOrange : barRed));
    DrawDashboardCard(autoBtn, hAuto ? cardHover : inputDark, autoCol, 0.28f, hAuto);
    DrawTextCenteredInBox(autoBtn, autoLabels[ev->autoChaosMode], 15, autoCol);
    if (hAuto && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        ev->autoChaosMode = (ev->autoChaosMode + 1) % 4;
    }

    DrawLine(30, 444, 322, 444, gridLine);
    DrawTextTruncated(TextFormat("LIVE AI SHELF (%d/%d SKUs)", activeStock, MAX_STORE_SKUS), 32, 451, 15, 288, highlightCyan);
    DrawTextTruncated("L-Click: Flash Sale | R-Click: Graph", 32, 470, 13, 288, textDim);

    bool compactLeftGrid = (activeStock > 6);
    int leftSkusDrawn = 0;

    for (int i = 0; i < MAX_ITEMS && leftSkusDrawn < MAX_STORE_SKUS; i++)
    {
        if (!inventory[i].isActive)
            continue;

        Rectangle skuBox;
        if (!compactLeftGrid)
        {
            skuBox = (Rectangle){30, 492 + leftSkusDrawn * 72, 292, 64};
        }
        else
        {
            int col = leftSkusDrawn / 6;
            int row = leftSkusDrawn % 6;
            skuBox = (Rectangle){26 + col * 150, 492 + row * 72, 142, 64};
        }

        bool hSku = CheckCollisionPointRec(mousePoint, skuBox);
        bool onSale = (inventory[i].saleTimer > 0.0f);
        bool isLow = (inventory[i].quantity <= inventory[i].minStock);
        float mPct = (ai->aiPriceMult[i] - 1.0f) * 100.0f;

        Color sBorder = onSale ? warnOrange : (isLow ? barRed : gridLine);
        DrawDashboardCard(skuBox, hSku ? cardHover : inputDark, sBorder, 0.2f, hSku);

        Color pCol = (mPct > 4.0f) ? warnOrange : ((mPct < -2.0f) ? textGreen : highlightCyan);
        float dispP = onSale ? (inventory[i].price * 0.88f) : inventory[i].price;

        if (!compactLeftGrid)
        {
            DrawTextTruncated(inventory[i].name, (int)skuBox.x + 12, (int)skuBox.y + 8, 16, 150, onSale ? warnOrange : textYellow);
            DrawTextTruncated(TextFormat("EUR %.0f (%+.0f%%)", dispP, mPct), (int)skuBox.x + 168, (int)skuBox.y + 8, 14, 118, pCol);
            DrawTextTruncated(TextFormat("Stock: %d/%d (+%d Transit)", inventory[i].quantity, inventory[i].maxStock, inventory[i].inTransit),
                              (int)skuBox.x + 12, (int)skuBox.y + 28, 14, 268, isLow ? barRed : textDim);
        }
        else
        {
            DrawTextTruncated(inventory[i].name, (int)skuBox.x + 8, (int)skuBox.y + 6, 14, 126, onSale ? warnOrange : textYellow);
            DrawTextTruncated(TextFormat("%d/%d %+.0f%%", inventory[i].quantity, inventory[i].maxStock, mPct),
                              (int)skuBox.x + 8, (int)skuBox.y + 26, 13, 126, isLow ? barRed : pCol);
        }

        float r = inventory[i].maxStock > 0 ? (float)inventory[i].quantity / (float)inventory[i].maxStock : 0.0f;
        if (r > 1.0f)
            r = 1.0f;
        Rectangle barBg = {skuBox.x + 8, skuBox.y + 48, skuBox.width - 16, 8};
        DrawRectangleRounded(barBg, 1.0f, 4, cardDark);
        if (r > 0.02f)
        {
            DrawRectangleRounded((Rectangle){barBg.x, barBg.y, barBg.width * r, 8}, 1.0f, 4, onSale ? warnOrange : (isLow ? barRed : GetSkuBoxColor(i)));
        }

        if (hSku && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            inventory[i].saleTimer = 20.0f;
            RecordSkuTelemetrySample(i, &inventory[i], ai->simTime);
            PushNotification(TextFormat("FLASH SALE: %s (-12%% price, 2.2x demand)!", inventory[i].name), 2);
            Rectangle bay = GetShelfBayRect(leftSkusDrawn, activeStock);
            SpawnFloatingPopup(bay.x, bay.y, "FLASH SALE 2.2x!", warnOrange);
        }
        if (hSku && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
        {
            selectedGraphSku = i;
            catalogShowGraph = true;
            currentScreen = SCREEN_VIEW;
        }
        leftSkusDrawn++;
    }

    // =========================================================================
    // 3. RIGHT EDGE SUPPLIER RADAR & CHAOS DECK (x: 1584..1904, y: 120..936)
    // =========================================================================
    Rectangle rightEdge = {1584, 120, 320, 816};
    DrawDashboardCard(rightEdge, cardDark, gridLine, 0.03f, false);

    DrawTextCrisp("SUPPLIER NETWORK RADAR", 1600, 134, 15, highlightPurple);
    for (int s = 0; s < MAX_SUPPLIERS; s++)
    {
        Rectangle sBox = {1598, 158 + s * 78, 292, 70};
        bool hSup = CheckCollisionPointRec(mousePoint, sBox);
        Color sCol = suppliers[s].hasShortage ? barRed : (suppliers[s].priceSpikeTimer > 0 ? warnOrange : highlightCyan);
        DrawDashboardCard(sBox, hSup ? cardHover : inputDark, sCol, 0.2f, hSup);

        DrawTextTruncated(suppliers[s].name, (int)sBox.x + 12, (int)sBox.y + 8, 17, 195, textYellow);
        DrawPillBadge((Rectangle){sBox.x + 210, sBox.y + 8, 72, 22},
                      suppliers[s].hasShortage ? "OFFLINE" : (suppliers[s].priceSpikeTimer > 0 ? "+38% HIKE" : "ONLINE"),
                      sCol, bgDark);

        if (suppliers[s].hasShortage)
        {
            DrawTextTruncated(TextFormat("Supply Shortage: Restores in %.0fs", suppliers[s].shortageTimer), (int)sBox.x + 12, (int)sBox.y + 38, 14, 268, barRed);
        }
        else
        {
            float mult = suppliers[s].costMultiplier * suppliers[s].priceSpikeMult * (ev->subsidyTimer > 0 ? 0.80f : 1.0f);
            DrawTextTruncated(TextFormat("Rate: %.2fx Cost  |  Fee: EUR %.0f  |  ETA: %.0fs", mult, suppliers[s].deliveryFee, suppliers[s].deliveryTime),
                              (int)sBox.x + 12, (int)sBox.y + 38, 14, 268, sCol);
        }
        if (hSup && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            currentScreen = SCREEN_ORDER;
    }

    DrawLine(1598, 398, 1890, 398, gridLine);
    DrawTextTruncated("INSTANT CHAOS DIRECTOR DECK", 1600, 406, 15, 288, barRed);
    DrawTextTruncated(ev->autoChaosMode > 0 ? TextFormat("Next Auto-Chaos Roll in %.1fs", 6.5f - ev->randomEventTimer) : "Click any disruption to test AI live",
                      1600, 425, 13, 288, warnOrange);

    const char *chaosLabels[9] = {
        "1. Flash Sale Frenzy (3x SKUs)",
        "2. Viral Demand Surge (2.5x)",
        "3. Damage Cargo / Hike (+38%)",
        "4. Severe Weather Storm (-55%)",
        "5. Supplier Network Outage",
        "6. Energy Tariff Spike (2.1x)",
        "7. Staff Overtime Strike (1.85x)",
        "8. Warehouse Shrinkage (-24%)",
        "+ PR & Gov Subsidy Relief"};
    Color chaosColors[9] = {
        warnOrange, barRed, barRed, highlightPurple,
        barRed, warnOrange, warnOrange, barRed, textGreen};

    for (int b = 0; b < 9; b++)
    {
        Rectangle cBtn = {1598, 448 + b * 52, 292, 44};
        bool hc = CheckCollisionPointRec(mousePoint, cBtn);
        DrawDashboardCard(cBtn, hc ? chaosColors[b] : inputDark, chaosColors[b], 0.25f, hc);
        DrawCircle((int)cBtn.x + 16, (int)cBtn.y + 22, 5.0f, hc ? bgDark : chaosColors[b]);
        DrawTextTruncated(chaosLabels[b], (int)cBtn.x + 28, (int)cBtn.y + 13, 16, 254, hc ? bgDark : (b == 8 ? textGreen : textYellow));

        if (hc && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            TriggerChaosEventById(b, false, inventory, *itemCount, orders, *orderCount);
        }
    }

    // =========================================================================
    // 4. BOTTOM EDGE 1080P AI BRAIN & SHIPMENT BAR (x: 16..1904, y: 950..1066)
    // =========================================================================
    Rectangle bottomBar = {16, 950, 1888, 116};
    DrawDashboardCard(bottomBar, cardDark, gridLine, 0.14f, false);

    DrawTextTruncated("NDAL AUTONOMOUS AI PRICING & INVENTORY ENGINE", 32, 962, 15, 900, highlightCyan);
    DrawTextTruncated(ai->aiLastActionLog, 32, 986, 18, 900, textYellow);

    int evX = 32;
    if (ev->viralSurgeTimer > 0.0f)
    {
        DrawPillBadge((Rectangle){(float)evX, 1020, 145, 28}, TextFormat("VIRAL SURGE %.0fs", ev->viralSurgeTimer), barRed, bgDark);
        evX += 155;
    }
    if (ev->weatherDelayTimer > 0.0f)
    {
        DrawPillBadge((Rectangle){(float)evX, 1020, 145, 28}, TextFormat("STORM DELAY %.0fs", ev->weatherDelayTimer), highlightPurple, bgDark);
        evX += 155;
    }
    if (ev->energySpikeTimer > 0.0f)
    {
        DrawPillBadge((Rectangle){(float)evX, 1020, 145, 28}, TextFormat("POWER 2.1x %.0fs", ev->energySpikeTimer), warnOrange, bgDark);
        evX += 155;
    }
    if (ev->wageSurgeTimer > 0.0f)
    {
        DrawPillBadge((Rectangle){(float)evX, 1020, 145, 28}, TextFormat("WAGES 1.85x %.0fs", ev->wageSurgeTimer), barRed, bgDark);
        evX += 155;
    }
    if (ev->subsidyTimer > 0.0f)
    {
        DrawPillBadge((Rectangle){(float)evX, 1020, 145, 28}, TextFormat("SUBSIDY -20%% %.0fs", ev->subsidyTimer), textGreen, bgDark);
        evX += 155;
    }
    if (evX == 32)
    {
        DrawTextTruncated(TextFormat("Storewide AI Markup: %+.1f%%   |   8s Upkeep Breakdown: Staff EUR %.0f + Power EUR %.0f + Storage Tax EUR %.0f = EUR %.0f",
                                     ai->avgPriceMarkupPct, ai->lastStaffBill, ai->lastUtilityBill, ai->lastStorageBill, ai->lastTotalBill),
                          32, 1024, 15, 900, textDim);
    }

    DrawLine(952, 960, 952, 1056, gridLine);
    DrawTextTruncated("ACTIVE SHIPMENT TELEMETRY", 972, 962, 15, 450, highlightPurple);

    int drawnBotOrders = 0;
    for (int o = *orderCount - 1; o >= 0 && drawnBotOrders < 2; o--)
    {
        int by = 986 + drawnBotOrders * 36;
        float prog = orders[o].totalDeliveryTime > 0 ? (orders[o].elapsedTime / orders[o].totalDeliveryTime) : 1.0f;
        if (prog > 1.0f)
            prog = 1.0f;

        Color stCol = highlightCyan;
        const char *stTxt = "IN TRANSIT";
        if (orders[o].status == STATUS_DELIVERED)
        {
            stCol = textGreen;
            stTxt = "DELIVERED";
        }
        else if (orders[o].status == STATUS_OUT_FOR_DELIVERY)
        {
            stCol = warnOrange;
            stTxt = "ARRIVING";
        }
        if (orders[o].isDamaged)
        {
            stCol = barRed;
            stTxt = "DAMAGED";
        }

        DrawTextTruncated(TextFormat("Order #%d  %s (EUR %.0f)", orders[o].orderId, orders[o].supplierName, orders[o].totalCost),
                          972, by + 4, 16, 310, textYellow);

        Rectangle pTrack = {1295, (float)by + 7, 440, 14};
        DrawRectangleRounded(pTrack, 1.0f, 6, inputDark);
        if (prog > 0.02f)
        {
            DrawRectangleRounded((Rectangle){pTrack.x, pTrack.y, 440 * prog, 14}, 1.0f, 6, stCol);
        }
        DrawPillBadge((Rectangle){1750, (float)by + 1, 134, 26}, stTxt, stCol, bgDark);
        drawnBotOrders++;
    }
    if (drawnBotOrders == 0)
    {
        DrawTextTruncated("No active shipments in transit. NDAL AI is monitoring lean shelf depletion...", 972, 1004, 16, 880, textDim);
    }

    // =========================================================================
    // 5. CENTER 1080P VIEWPORT (x: 352..1568, y: 120..936, Size: 1216x816)
    // =========================================================================
    Rectangle centerArena = {352, 120, 1216, 816};

    DrawCentralDigitalTwinArena(centerArena, inventory, *itemCount, orders, *orderCount);

    if (currentScreen != SCREEN_MENU)
    {
        DrawRectangleRounded(centerArena, 0.02f, 12, (Color){11, 14, 22, 248});
        DrawRectangleLinesEx(centerArena, 2.0f, highlightCyan);

        Rectangle closeBtn = {centerArena.x + 968, centerArena.y + 14, 224, 40};
        bool hClose = CheckCollisionPointRec(mousePoint, closeBtn);
        DrawDashboardCard(closeBtn, hClose ? highlightCyan : inputDark, highlightCyan, 0.3f, hClose);
        DrawTextCenteredInBox(closeBtn, "< RETURN TO LIVE ARENA", 16, hClose ? bgDark : textYellow);
        if (hClose && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            currentScreen = SCREEN_MENU;
            showItemFormModal = false;
        }

        switch (currentScreen)
        {
        // -----------------------------------------------------------------
        // 1080P CATALOG TABLE & MULTI-LINE TELEMETRY GRAPH (SCREEN_VIEW)
        // -----------------------------------------------------------------
        case SCREEN_VIEW:
        {
            DrawTextTruncated(catalogShowGraph
                                  ? "NDAL PRODUCT TELEMETRY & ELASTICITY GRAPH"
                                  : TextFormat("NDAL PRODUCT CATALOG (%d / %d MAX SKUs)", activeStock, MAX_STORE_SKUS),
                              (int)centerArena.x + 24, (int)centerArena.y + 22, 22, 520, textYellow);

            Rectangle modeBtn = {centerArena.x + 560, centerArena.y + 14, 200, 40};
            bool hMode = CheckCollisionPointRec(mousePoint, modeBtn);
            DrawDashboardCard(modeBtn, catalogShowGraph ? highlightCyan : (hMode ? cardHover : inputDark), highlightCyan, 0.3f, hMode);
            DrawTextCenteredInBox(modeBtn, catalogShowGraph ? "<= TABLE CATALOG" : "ANALYTICS GRAPH", 16, catalogShowGraph ? bgDark : highlightCyan);
            if (hMode && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                catalogShowGraph = !catalogShowGraph;
            }

            Rectangle topActBtn = {centerArena.x + 772, centerArena.y + 14, 180, 40};
            bool hTopAct = CheckCollisionPointRec(mousePoint, topActBtn);
            if (!catalogShowGraph)
            {
                bool canAddMore = (activeStock < MAX_STORE_SKUS);
                Color addCol = canAddMore ? highlightPurple : textDim;
                DrawDashboardCard(topActBtn, (hTopAct && canAddMore) ? highlightPurple : inputDark, addCol, 0.3f, hTopAct && canAddMore);
                DrawTextCenteredInBox(topActBtn, canAddMore ? "+ ADD NEW SKU" : "MAX 12 SKUs FULL", 15, canAddMore ? textYellow : textDim);
                if (hTopAct && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                    if (canAddMore)
                    {
                        currentScreen = SCREEN_EVENTS;
                        showItemFormModal = true;
                        editTargetIndex = -1;
                        nameInput[0] = '\0';
                        qtyInput[0] = '\0';
                        priceInput[0] = '\0';
                        costInput[0] = '\0';
                        maxInput[0] = '\0';
                        minInput[0] = '\0';
                        nameCount = qtyCount = priceCount = costCount = maxCount = minCount = 0;
                    }
                    else
                    {
                        PushNotification("MAX CAPACITY: 12 Active SKUs reached! Delete or Edit an existing SKU first.", 2);
                    }
                }
            }
            else
            {
                DrawDashboardCard(topActBtn, hTopAct ? warnOrange : inputDark, warnOrange, 0.3f, hTopAct);
                DrawTextCenteredInBox(topActBtn, "TRIGGER FLASH SALE", 15, hTopAct ? bgDark : warnOrange);
                if (hTopAct && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                    inventory[selectedGraphSku].saleTimer = 20.0f;
                    RecordSkuTelemetrySample(selectedGraphSku, &inventory[selectedGraphSku], ai->simTime);
                    PushNotification(TextFormat("FLASH SALE triggered on %s!", inventory[selectedGraphSku].name), 2);
                }
            }

            if (catalogShowGraph)
            {
                DrawProductAnalyticsGraph(centerArena, mousePoint, inventory, *itemCount);
            }
            else
            {
                if (wheel != 0)
                    scrollIndex -= wheel;
                if (scrollIndex > activeStock - 8)
                    scrollIndex = activeStock - 8;
                if (scrollIndex < 0)
                    scrollIndex = 0;

                int yOff = (int)centerArena.y + 76;
                DrawTextTruncated("PRODUCT & WHOLESALE COST", (int)centerArena.x + 32, yOff, 15, 290, textDim);
                DrawTextTruncated("STOCK (+TRANSIT)", (int)centerArena.x + 340, yOff, 15, 190, textDim);
                DrawTextTruncated("AI RETAIL PRICE (MARKUP)", (int)centerArena.x + 545, yOff, 15, 230, textDim);
                DrawTextTruncated("TELEMETRY & CATALOG ACTIONS", (int)centerArena.x + 790, yOff, 15, 390, textDim);
                yOff += 26;
                DrawLine((int)centerArena.x + 24, yOff, (int)centerArena.x + 1192, yOff, gridLine);
                yOff += 12;

                int actIdx = 0, drawn = 0;
                for (int i = 0; i < MAX_ITEMS; i++)
                {
                    if (inventory[i].isActive)
                    {
                        if (actIdx >= scrollIndex && drawn < 8)
                        {
                            Rectangle row = {centerArena.x + 24, (float)yOff, 1168, 76};
                            DrawDashboardCard(row, (drawn % 2 == 0) ? cardDark : inputDark, gridLine, 0.15f, false);

                            float elast = GetItemPriceElasticity(i);
                            float mPct = (ai->aiPriceMult[i] - 1.0f) * 100.0f;

                            DrawTextTruncated(inventory[i].name, (int)row.x + 16, yOff + 12, 20, 280, textYellow);
                            DrawTextTruncated(TextFormat("Cost: EUR %.0f  |  Min:%d Max:%d  |  Vel: %.2fx",
                                                         inventory[i].orderCost, inventory[i].minStock, inventory[i].maxStock, elast),
                                              (int)row.x + 16, yOff + 42, 14, 290, textDim);

                            DrawTextTruncated(TextFormat("%d / %d (+%d)", inventory[i].quantity, inventory[i].maxStock, inventory[i].inTransit),
                                              (int)centerArena.x + 340, yOff + 14, 18, 180, inventory[i].quantity == 0 ? barRed : textYellow);
                            float fillR = inventory[i].maxStock > 0 ? (float)inventory[i].quantity / (float)inventory[i].maxStock : 0.0f;
                            if (fillR > 1.0f)
                                fillR = 1.0f;
                            Rectangle sTrack = {centerArena.x + 340, (float)yOff + 44, 165, 10};
                            DrawRectangleRounded(sTrack, 1.0f, 6, bgDark);
                            if (fillR > 0.02f)
                            {
                                Color bCol = (inventory[i].quantity <= inventory[i].minStock) ? barRed : highlightCyan;
                                DrawRectangleRounded((Rectangle){sTrack.x, sTrack.y, 165 * fillR, 10}, 1.0f, 6, bCol);
                            }

                            float dispP = (inventory[i].saleTimer > 0) ? (inventory[i].price * 0.88f) : inventory[i].price;
                            Color pCol = (mPct > 4.0f) ? warnOrange : ((mPct < -2.0f) ? textGreen : highlightCyan);
                            float uProf = dispP - GetEffectiveUnitCost(&inventory[i], ev->subsidyTimer);
                            DrawTextTruncated(TextFormat("EUR %.2f (%+.1f%%)", dispP, mPct),
                                              (int)centerArena.x + 545, yOff + 16, 19, 230, pCol);
                            DrawTextTruncated(TextFormat("Net Margin: %+.1f / u", uProf),
                                              (int)centerArena.x + 545, yOff + 42, 14, 230, uProf >= 0 ? textGreen : barRed);

                            Rectangle graphBtn = {centerArena.x + 786, (float)yOff + 18, 104, 40};
                            Rectangle saleBtn = {centerArena.x + 898, (float)yOff + 18, 114, 40};
                            Rectangle editBtn = {centerArena.x + 1020, (float)yOff + 18, 76, 40};
                            Rectangle delBtn = {centerArena.x + 1104, (float)yOff + 18, 72, 40};

                            bool hGrf = CheckCollisionPointRec(mousePoint, graphBtn);
                            bool hSale = CheckCollisionPointRec(mousePoint, saleBtn);
                            bool hEdit = CheckCollisionPointRec(mousePoint, editBtn);
                            bool hDel = CheckCollisionPointRec(mousePoint, delBtn);
                            bool onSale = (inventory[i].saleTimer > 0.0f);

                            DrawDashboardCard(graphBtn, hGrf ? highlightCyan : cardDark, highlightCyan, 0.3f, hGrf);
                            DrawTextCenteredInBox(graphBtn, "GRAPH", 14, hGrf ? bgDark : highlightCyan);
                            if (hGrf && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                            {
                                selectedGraphSku = i;
                                catalogShowGraph = true;
                            }

                            DrawDashboardCard(saleBtn, onSale ? warnOrange : (hSale ? warnOrange : cardDark), warnOrange, 0.3f, hSale);
                            DrawTextCenteredInBox(saleBtn, onSale ? TextFormat("SALE %.0fs", inventory[i].saleTimer) : "FLASH SALE", 14, (onSale || hSale) ? bgDark : warnOrange);
                            if (hSale && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                            {
                                inventory[i].saleTimer = 20.0f;
                                RecordSkuTelemetrySample(i, &inventory[i], ai->simTime);
                                PushNotification(TextFormat("%s placed on FLASH SALE!", inventory[i].name), 2);
                            }

                            DrawDashboardCard(editBtn, hEdit ? highlightPurple : cardDark, highlightPurple, 0.3f, hEdit);
                            DrawTextCenteredInBox(editBtn, "EDIT", 14, textYellow);
                            if (hEdit && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                            {
                                currentScreen = SCREEN_EVENTS;
                                showItemFormModal = true;
                                editTargetIndex = i;
                                strcpy(nameInput, inventory[i].name);
                                nameCount = (int)strlen(nameInput);
                                sprintf(qtyInput, "%d", inventory[i].quantity);
                                qtyCount = (int)strlen(qtyInput);
                                sprintf(priceInput, "%.2f", inventory[i].price);
                                priceCount = (int)strlen(priceInput);
                                sprintf(costInput, "%.2f", inventory[i].orderCost);
                                costCount = (int)strlen(costInput);
                                sprintf(maxInput, "%d", inventory[i].maxStock);
                                maxCount = (int)strlen(maxInput);
                                sprintf(minInput, "%d", inventory[i].minStock);
                                minCount = (int)strlen(minInput);
                            }

                            DrawDashboardCard(delBtn, hDel ? barRed : cardDark, barRed, 0.3f, hDel);
                            DrawTextCenteredInBox(delBtn, "DEL", 14, hDel ? bgDark : barRed);
                            if (hDel && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                            {
                                if (activeStock > 1)
                                {
                                    inventory[i].isActive = false;
                                    SaveInventory(inventory, *itemCount);
                                    PushNotification(TextFormat("Removed %s from active store aisles.", inventory[i].name), 0);
                                }
                                else
                                {
                                    PushNotification("Cannot delete the last remaining active SKU!", 2);
                                }
                            }

                            yOff += 86;
                            drawn++;
                        }
                        actIdx++;
                    }
                }
            }
            break;
        }

        // -----------------------------------------------------------------
        // 1080P SUPPLIERS & AI UTILITY MATRIX (SCREEN_ORDER)
        // -----------------------------------------------------------------
        case SCREEN_ORDER:
        {
            DrawTextTruncated("NDAL MULTI-SUPPLIER NETWORK & AI DISPATCH LOGIC", (int)centerArena.x + 24, (int)centerArena.y + 22, 24, 680, textYellow);

            for (int s = 0; s < MAX_SUPPLIERS; s++)
            {
                Rectangle sCard = {centerArena.x + 24, centerArena.y + 76 + s * 238, 1168, 220};
                Color bCol = suppliers[s].hasShortage ? barRed : (suppliers[s].priceSpikeTimer > 0 ? warnOrange : highlightCyan);
                DrawDashboardCard(sCard, cardDark, bCol, 0.08f, false);

                DrawTextTruncated(suppliers[s].name, (int)sCard.x + 24, (int)sCard.y + 16, 24, 480, textYellow);

                float subMult = (ev->subsidyTimer > 0.0f) ? 0.80f : 1.0f;
                float effCostMult = suppliers[s].costMultiplier * suppliers[s].priceSpikeMult * subMult;
                float effFee = suppliers[s].deliveryFee * suppliers[s].priceSpikeMult * subMult;
                float effTime = suppliers[s].deliveryTime * (ev->weatherDelayTimer > 0.0f ? 2.2f : 1.0f);

                DrawTextTruncated(TextFormat("Effective Cost Rate: %.0f%% of Wholesale   |   Flat Delivery Fee: EUR %.0f   |   Transit ETA: %.1fs",
                                             effCostMult * 100.0f, effFee, effTime),
                                  (int)sCard.x + 24, (int)sCard.y + 52, 17, 1100, highlightCyan);

                const char *roleDesc = (s == 0)
                                           ? "NDAL AI Role: Emergency Stockout Rescue & Low-Satisfaction Rapid Replenishment"
                                           : ((s == 1) ? "NDAL AI Role: Balanced Everyday Restocking & Moderate Lead-Time Buffer"
                                                       : "NDAL AI Role: High-Margin Bulk Restocking during Calm Market Conditions");
                DrawTextTruncated(roleDesc, (int)sCard.x + 24, (int)sCard.y + 82, 15, 1100, textDim);

                DrawLine((int)sCard.x + 24, (int)sCard.y + 112, (int)(sCard.x + sCard.width - 24), (int)sCard.y + 112, gridLine);
                int col = 0;
                for (int i = 0; i < MAX_ITEMS && col < 4; i++)
                {
                    if (inventory[i].isActive)
                    {
                        float unitP = inventory[i].orderCost * effCostMult;
                        DrawTextTruncated(TextFormat("%s: EUR %.1f/u", inventory[i].name, unitP),
                                          (int)sCard.x + 24 + col * 280, (int)sCard.y + 122, 15, 265, textYellow);
                        col++;
                    }
                }

                Rectangle shBtn = {sCard.x + 24, sCard.y + 158, 360, 44};
                bool hSh = CheckCollisionPointRec(mousePoint, shBtn);
                DrawDashboardCard(shBtn, suppliers[s].hasShortage ? barRed : (hSh ? cardHover : inputDark), barRed, 0.3f, hSh);
                DrawTextCenteredInBox(shBtn, suppliers[s].hasShortage ? TextFormat("SHORTAGE ACTIVE (%.0fs)", suppliers[s].shortageTimer) : "TOGGLE SUPPLY SHORTAGE (22s)", 15, textYellow);
                if (hSh && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                    suppliers[s].hasShortage = !suppliers[s].hasShortage;
                    suppliers[s].shortageTimer = suppliers[s].hasShortage ? 22.0f : 0.0f;
                }

                Rectangle hkBtn = {sCard.x + 408, sCard.y + 158, 360, 44};
                bool hHk = CheckCollisionPointRec(mousePoint, hkBtn);
                bool spiked = (suppliers[s].priceSpikeTimer > 0.0f);
                DrawDashboardCard(hkBtn, spiked ? warnOrange : (hHk ? cardHover : inputDark), warnOrange, 0.3f, hHk);
                DrawTextCenteredInBox(hkBtn, spiked ? TextFormat("PRICE HIKE +38%% (%.0fs)", suppliers[s].priceSpikeTimer) : "TRIGGER +38% PRICE HIKE (22s)", 15, spiked ? bgDark : textYellow);
                if (hHk && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                    suppliers[s].priceSpikeMult = 1.38f;
                    suppliers[s].priceSpikeTimer = 22.0f;
                }
            }
            break;
        }

        // -----------------------------------------------------------------
        // 1080P CHAOS DIRECTOR DECK & SKU EDITOR (SCREEN_EVENTS)
        // -----------------------------------------------------------------
        case SCREEN_EVENTS:
        {
            Rectangle tabChaos = {centerArena.x + 24, centerArena.y + 14, 280, 40};
            Rectangle tabSku = {centerArena.x + 318, centerArena.y + 14, 340, 40};
            bool hT1 = CheckCollisionPointRec(mousePoint, tabChaos);
            bool hT2 = CheckCollisionPointRec(mousePoint, tabSku);
            DrawDashboardCard(tabChaos, !showItemFormModal ? barRed : inputDark, barRed, 0.3f, hT1);
            DrawTextCenteredInBox(tabChaos, "8 CHAOS EVENTS DECK", 16, textYellow);
            DrawDashboardCard(tabSku, showItemFormModal ? highlightCyan : inputDark, highlightCyan, 0.3f, hT2);
            DrawTextCenteredInBox(tabSku, TextFormat("ADD / EDIT SKU (%d/%d MAX)", activeStock, MAX_STORE_SKUS), 16, showItemFormModal ? bgDark : textYellow);

            if (hT1 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                showItemFormModal = false;
            if (hT2 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                showItemFormModal = true;
                editTargetIndex = -1;
            }

            if (!showItemFormModal)
            {
                const char *evTitles[8] = {
                    "1. Flash Sale Frenzy (3x SKUs)", "2. Viral Demand Surge (2.5x)",
                    "3. Damaged Cargo Crisis (-40%)", "4. Severe Weather Storm (-55%)",
                    "5. Supplier Network Outage", "6. Energy Tariff Spike (2.1x)",
                    "7. Staff Overtime Strike (1.85x)", "8. Warehouse Shrinkage (-24%)"};
                const char *evSub[8] = {
                    "3 random SKUs on -12% sale, 2.2x demand velocity & +Satisfaction for 20s.",
                    "All active products surge to 2.5x customer buying speed for 18 seconds.",
                    "Damages an active shipment in transit, losing 40% of units & -6% Satisfaction.",
                    "Heavy rain & lightning slow all highway delivery trucks by 55% for 18 seconds.",
                    "Takes 1-2 suppliers offline for 22s, forcing expensive AI rerouting.",
                    "Facility utility and power bills surge 2.1x (EUR 242/8s) for 24 seconds.",
                    "Cashier & warehouse wages surge 1.85x (EUR 305/8s) and Satisfaction drops -7%.",
                    "Theft/spoilage removes 24% stock from 2 SKUs and drops Satisfaction -8%."};
                Color evCols[8] = {
                    warnOrange, barRed, barRed, highlightPurple,
                    barRed, warnOrange, warnOrange, barRed};

                for (int e = 0; e < 8; e++)
                {
                    int col = e % 2, row = e / 2;
                    Rectangle eCard = {centerArena.x + 24 + col * 592, centerArena.y + 74 + row * 180, 576, 164};
                    bool he = CheckCollisionPointRec(mousePoint, eCard);
                    DrawDashboardCard(eCard, he ? cardHover : cardDark, evCols[e], 0.1f, he);
                    DrawTextTruncated(evTitles[e], (int)eCard.x + 20, (int)eCard.y + 16, 20, 536, evCols[e]);
                    DrawTextTruncated(evSub[e], (int)eCard.x + 20, (int)eCard.y + 52, 15, 536, textYellow);

                    Rectangle tBtn = {eCard.x + 20, eCard.y + 102, 536, 44};
                    DrawDashboardCard(tBtn, he ? evCols[e] : inputDark, evCols[e], 0.3f, he);
                    DrawTextCenteredInBox(tBtn, "TRIGGER EVENT & WATCH LIVE IN ARENA", 16, he ? bgDark : textYellow);

                    if (he && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                    {
                        TriggerChaosEventById(e, false, inventory, *itemCount, orders, *orderCount);
                        currentScreen = SCREEN_MENU;
                    }
                }
            }
            else
            {
                DrawTextTruncated(editTargetIndex == -1
                                      ? TextFormat("Adding New SKU to Store Aisles (Slot %d of %d Max)", activeStock + 1, MAX_STORE_SKUS)
                                      : TextFormat("Editing Existing SKU #%d (%s)", inventory[editTargetIndex].id, inventory[editTargetIndex].name),
                                  (int)centerArena.x + 380, (int)centerArena.y + 72, 18, 650, highlightCyan);

                Rectangle nameBox = {centerArena.x + 380, centerArena.y + 110, 580, 54};
                Rectangle qtyBox = {centerArena.x + 380, centerArena.y + 190, 580, 54};
                Rectangle priceBox = {centerArena.x + 380, centerArena.y + 270, 580, 54};
                Rectangle costBox = {centerArena.x + 380, centerArena.y + 350, 580, 54};
                Rectangle maxBox = {centerArena.x + 380, centerArena.y + 430, 275, 54};
                Rectangle minBox = {centerArena.x + 685, centerArena.y + 430, 275, 54};
                Rectangle saveBtn = {centerArena.x + 380, centerArena.y + 525, 580, 64};

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

                DrawTextTruncated("Product Name", (int)centerArena.x + 110, (int)nameBox.y + 16, 20, 250, textDim);
                DrawDashboardCard(nameBox, inputDark, activeInput == 0 ? highlightCyan : gridLine, 0.2f, activeInput == 0);
                DrawTextTruncated(nameInput, (int)nameBox.x + 18, (int)nameBox.y + 16, 22, 540, textYellow);

                DrawTextTruncated("Initial Stock Qty", (int)centerArena.x + 110, (int)qtyBox.y + 16, 20, 250, textDim);
                DrawDashboardCard(qtyBox, inputDark, activeInput == 1 ? highlightCyan : gridLine, 0.2f, activeInput == 1);
                DrawTextTruncated(qtyInput, (int)qtyBox.x + 18, (int)qtyBox.y + 16, 22, 540, textYellow);

                DrawTextTruncated("Baseline Retail EUR", (int)centerArena.x + 110, (int)priceBox.y + 16, 20, 250, textDim);
                DrawDashboardCard(priceBox, inputDark, activeInput == 2 ? highlightCyan : gridLine, 0.2f, activeInput == 2);
                DrawTextTruncated(priceInput, (int)priceBox.x + 18, (int)priceBox.y + 16, 22, 540, textYellow);

                DrawTextTruncated("Wholesale Cost EUR", (int)centerArena.x + 110, (int)costBox.y + 16, 20, 250, textDim);
                DrawDashboardCard(costBox, inputDark, activeInput == 3 ? highlightCyan : gridLine, 0.2f, activeInput == 3);
                DrawTextTruncated(costInput, (int)costBox.x + 18, (int)costBox.y + 16, 22, 540, textYellow);

                DrawTextTruncated("Max / Min Limits", (int)centerArena.x + 110, (int)maxBox.y + 16, 20, 250, textDim);
                DrawDashboardCard(maxBox, inputDark, activeInput == 4 ? highlightCyan : gridLine, 0.2f, activeInput == 4);
                DrawTextTruncated(maxInput, (int)maxBox.x + 18, (int)maxBox.y + 16, 22, 240, textYellow);
                DrawDashboardCard(minBox, inputDark, activeInput == 5 ? highlightCyan : gridLine, 0.2f, activeInput == 5);
                DrawTextTruncated(minInput, (int)minBox.x + 18, (int)minBox.y + 16, 22, 240, textYellow);

                bool canSave = (editTargetIndex >= 0) || (activeStock < MAX_STORE_SKUS);
                bool hSave = CheckCollisionPointRec(mousePoint, saveBtn);
                DrawDashboardCard(saveBtn, !canSave ? inputDark : (hSave ? highlightCyan : highlightPurple), canSave ? highlightCyan : barRed, 0.25f, hSave && canSave);
                DrawTextCenteredInBox(saveBtn, canSave ? "SAVE PRODUCT SKU TO NDAL AISLES" : "MAX 12 ACTIVE SKUS REACHED", 20, (hSave && canSave) ? bgDark : textYellow);

                if (hSave && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && nameCount > 0)
                {
                    if (!canSave)
                    {
                        PushNotification("MAX CAPACITY: 12 Active SKUs reached! Delete a SKU in Catalog first.", 2);
                    }
                    else if (editTargetIndex == -1)
                    {
                        int idx = -1;
                        for (int k = 0; k < *itemCount; k++)
                        {
                            if (!inventory[k].isActive)
                            {
                                idx = k;
                                break;
                            }
                        }
                        if (idx == -1 && *itemCount < MAX_ITEMS)
                        {
                            idx = (*itemCount)++;
                        }
                        if (idx != -1)
                        {
                            inventory[idx].id = idx + 1;
                            strcpy(inventory[idx].name, nameInput);
                            inventory[idx].orderCost = (float)atof(costInput);
                            if (inventory[idx].orderCost <= 1.0f)
                                inventory[idx].orderCost = 65.0f;
                            inventory[idx].price = (float)atof(priceInput);
                            if (inventory[idx].price <= 1.0f)
                                inventory[idx].price = inventory[idx].orderCost * BASE_FAIR_MARGIN_MULT;
                            inventory[idx].maxStock = atoi(maxInput) > 0 ? atoi(maxInput) : 30;
                            if (inventory[idx].maxStock > 46)
                                inventory[idx].maxStock = 46;
                            inventory[idx].minStock = atoi(minInput) > 0 ? atoi(minInput) : 10;
                            inventory[idx].quantity = atoi(qtyInput) > 0 ? atoi(qtyInput) : (inventory[idx].minStock + 3);
                            if (inventory[idx].quantity > inventory[idx].maxStock)
                                inventory[idx].quantity = inventory[idx].maxStock;
                            inventory[idx].inTransit = 0;
                            inventory[idx].baseDemandRate = 0.28f;
                            inventory[idx].saleTimer = 0.0f;
                            inventory[idx].isActive = true;

                            ResetSkuFairPrice(idx, &inventory[idx], true);
                            ClearSkuTelemetry(idx);
                            RecordSkuTelemetrySample(idx, &inventory[idx], ai->simTime);
                            SpawnShopperAgent(idx, 1, inventory[idx].price, 0.0f, inventory[idx].quantity == 0);
                            PushNotification(TextFormat("Added '%s' to live store aisles (%d/%d SKUs)!", inventory[idx].name, activeStock + 1, MAX_STORE_SKUS), 1);
                        }
                    }
                    else if (editTargetIndex >= 0)
                    {
                        strcpy(inventory[editTargetIndex].name, nameInput);
                        inventory[editTargetIndex].orderCost = (float)atof(costInput);
                        if (inventory[editTargetIndex].orderCost <= 1.0f)
                            inventory[editTargetIndex].orderCost = 65.0f;
                        inventory[editTargetIndex].price = (float)atof(priceInput);
                        inventory[editTargetIndex].maxStock = atoi(maxInput) > 0 ? atoi(maxInput) : 30;
                        inventory[editTargetIndex].minStock = atoi(minInput) > 0 ? atoi(minInput) : 10;
                        inventory[editTargetIndex].quantity = atoi(qtyInput);
                        if (inventory[editTargetIndex].quantity > inventory[editTargetIndex].maxStock)
                        {
                            inventory[editTargetIndex].quantity = inventory[editTargetIndex].maxStock;
                        }
                        ResetSkuFairPrice(editTargetIndex, &inventory[editTargetIndex], false);
                        RecordSkuTelemetrySample(editTargetIndex, &inventory[editTargetIndex], ai->simTime);
                    }
                    SaveInventory(inventory, *itemCount);
                    showItemFormModal = false;
                    currentScreen = SCREEN_MENU;
                }
            }
            break;
        }

        // -----------------------------------------------------------------
        // 1080P AUTOMATED SHIPMENT LOG (SCREEN_ORDER_HISTORY)
        // -----------------------------------------------------------------
        case SCREEN_ORDER_HISTORY:
        {
            DrawTextTruncated("NDAL AUTOMATED SHIPMENT & LOGISTICS LOG", (int)centerArena.x + 24, (int)centerArena.y + 22, 24, 680, textYellow);

            if (wheel != 0)
                scrollHistory -= wheel;
            if (scrollHistory > *orderCount - 8)
                scrollHistory = *orderCount - 8;
            if (scrollHistory < 0)
                scrollHistory = 0;

            int yOff = (int)centerArena.y + 78;
            int drawn = 0;
            for (int i = *orderCount - 1; i >= 0; i--)
            {
                int listPos = (*orderCount - 1) - i;
                if (listPos >= scrollHistory && drawn < 8)
                {
                    Rectangle row = {centerArena.x + 24, (float)yOff, 1168, 78};
                    DrawDashboardCard(row, (drawn % 2 == 0) ? cardDark : inputDark, gridLine, 0.15f, false);

                    DrawTextTruncated(TextFormat("Order #%d  -  %s (%d SKUs)", orders[i].orderId, orders[i].supplierName, orders[i].itemCount),
                                      (int)row.x + 20, yOff + 14, 20, 460, textYellow);
                    DrawTextTruncated(TextFormat("Total Cost: EUR %.0f   |   Est. Net Margin: %+EUR %.0f", orders[i].totalCost, orders[i].estimatedProfit),
                                      (int)row.x + 20, yOff + 44, 15, 460, orders[i].estimatedProfit >= 0 ? textGreen : barRed);

                    float prog = orders[i].totalDeliveryTime > 0 ? (orders[i].elapsedTime / orders[i].totalDeliveryTime) : 1.0f;
                    if (prog > 1.0f)
                        prog = 1.0f;

                    const char *stText = "PROCESSING";
                    Color stCol = highlightCyan;
                    if (orders[i].status == STATUS_IN_TRANSIT)
                    {
                        stText = "IN TRANSIT";
                        stCol = highlightPurple;
                    }
                    else if (orders[i].status == STATUS_OUT_FOR_DELIVERY)
                    {
                        stText = "ARRIVING";
                        stCol = warnOrange;
                    }
                    else if (orders[i].status == STATUS_DELIVERED)
                    {
                        stText = "DELIVERED";
                        stCol = textGreen;
                    }
                    if (orders[i].isDamaged && orders[i].status != STATUS_DELIVERED)
                    {
                        stText = "DAMAGED";
                        stCol = barRed;
                    }

                    Rectangle pTrack = {centerArena.x + 510, (float)yOff + 24, 430, 18};
                    DrawRectangleRounded(pTrack, 1.0f, 8, bgDark);
                    if (prog > 0.02f)
                    {
                        DrawRectangleRounded((Rectangle){pTrack.x, pTrack.y, 430 * prog, 18}, 1.0f, 8, stCol);
                    }
                    DrawTextTruncated(TextFormat("%.0f%%  (%.1fs / %.0fs)", prog * 100.0f, orders[i].elapsedTime, orders[i].totalDeliveryTime),
                                      (int)pTrack.x + 140, yOff + 48, 14, 220, textDim);

                    DrawPillBadge((Rectangle){centerArena.x + 970, (float)yOff + 22, 195, 34}, stText, stCol, bgDark);

                    yOff += 88;
                    drawn++;
                }
            }
            break;
        }
        default:
            break;
        }
    }

    // =========================================================================
    // 6. FLOATING 1080P TOAST NOTIFICATIONS (Overlay at Bottom of Arena)
    // =========================================================================
    int notifY = 875;
    for (int n = 0; n < MAX_NOTIFICATIONS && n < 2; n++)
    {
        if (notifs[n].isActive)
        {
            Color nCol = highlightCyan;
            if (notifs[n].severity == 1)
                nCol = textGreen;
            else if (notifs[n].severity == 2)
                nCol = barRed;
            else if (notifs[n].severity == 3)
                nCol = highlightPurple;

            Rectangle toastRect = {760, (float)notifY, 790, 48};
            DrawDashboardCard(toastRect, (Color){14, 17, 26, 248}, nCol, 0.3f, true);
            DrawCircle((int)toastRect.x + 24, (int)toastRect.y + 24, 7.0f, nCol);
            DrawTextTruncated(notifs[n].message, (int)toastRect.x + 44, (int)toastRect.y + 15, 17, 730, textYellow);
            notifY -= 56;
        }
    }
}