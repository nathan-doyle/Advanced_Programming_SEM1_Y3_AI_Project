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
static int eventsSubTab = 0; // 0 = Chaos Deck, 1 = Add/Edit SKU, 2 = Shopify Web Store Scraper
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
static int scrollAudit = 0;
static bool fontFilterInitialized = false;

// SCREEN_VIEW sub-modes: 0 = Table Catalog, 1 = Analytics Graph, 2 = Staff Discrepancy Audit
static int catalogViewMode = 0;
static int selectedGraphSku = 0;
static bool showLinePrice = true;
static bool showLineCost = true;
static bool showLineSales = true;
static bool showLineProfit = true;

// Search & Simulated Barcode Scanner State
static char searchQuery[48] = "\0";
static int searchLen = 0;
static bool searchBoxFocused = false;
static float barcodeScanLaserFlash = 0.0f;

// Staff Discrepancy Audit Tracker State
static int selectedStaffIdx = 0;
static int selectedReasonIdx = 0;
static const char *staffDirectory[4] = {
    "Nathan Doyle (Store Mgr)",
    "Alan Leeder (Inv Lead)",
    "Sarah O'Connor (Shift Lead)",
    "Liam Kelly (Warehouse Ops)"};
static const char *auditReasons[4] = {
    "Shrinkage / Theft",
    "Transit Cargo Damage",
    "POS Scan Miss",
    "Supplier Short-Ship"};

// Online Store / Shopify Scraper State
static char scraperUrlInput[128] = "www.keychron.com/products.json";
static int scraperUrlLen = 30;
static bool scraperBoxFocused = false;
static char scraperStatusMsg[160] = "Ready: Enter any Shopify domain (/products.json) or pick a preset below.";

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
// 1D PROCEDURAL EAN-13 BARCODE GRAPHIC RENDERER
// =============================================================================

static void DrawSimulatedBarcode(Rectangle box, const char *barcode, Color barColor)
{
    DrawRectangleRounded(box, 0.15f, 6, (Color){12, 15, 22, 255});
    DrawRectangleLinesEx(box, 1.0f, (Color){65, 78, 108, 200});

    const char *code = (barcode != NULL && barcode[0] != '\0') ? barcode : "5391042000018";
    int len = (int)strlen(code);

    int startX = (int)box.x + 6;
    int barTop = (int)box.y + 4;
    int barH = (int)box.height - 18;
    if (barH < 10)
        barH = 10;

    // Left guard bars
    DrawRectangle(startX, barTop, 2, barH + 3, barColor);
    DrawRectangle(startX + 4, barTop, 1, barH + 3, barColor);
    int curX = startX + 8;

    for (int i = 0; i < len && curX < (int)(box.x + box.width - 12); i++)
    {
        int d = (code[i] >= '0' && code[i] <= '9') ? (code[i] - '0') : (i % 10);
        int w1 = (d % 3) + 1;
        int gap1 = ((d + 1) % 2) + 1;
        int w2 = ((d * 2) % 2) + 1;

        DrawRectangle(curX, barTop, w1, barH, barColor);
        curX += w1 + gap1;
        if (curX + w2 < (int)(box.x + box.width - 10))
        {
            DrawRectangle(curX, barTop, w2, barH, barColor);
            curX += w2 + 2;
        }
        if (i == 5 && curX < (int)(box.x + box.width - 14))
        {
            DrawRectangle(curX, barTop, 1, barH + 3, barColor);
            curX += 3;
        }
    }

    // Right guard bar
    DrawRectangle((int)(box.x + box.width - 8), barTop, 2, barH + 3, barColor);

    if (barcodeScanLaserFlash > 0.0f)
    {
        int laserY = barTop + (barH / 2);
        DrawLineEx((Vector2){box.x + 2, (float)laserY}, (Vector2){box.x + box.width - 2, (float)laserY}, 2.5f, (Color){255, 65, 85, 240});
    }

    DrawTextCenteredInBox((Rectangle){box.x, box.y + box.height - 14, box.width, 13}, code, 11, (Color){215, 228, 248, 255});
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
    LoadAuditLogs();
    InitSuppliers();
    InitEvents();
    InitAiEconomy(inventory, itemCount);
    InitTelemetry(inventory);
    InitArena();

    PushNotification("NDAL HARDCORE ECONOMY: Barcodes, Discrepancy Audit & Store Scraper Online.", 0);
}

static void StepAllSimulationModules(float dt, InventoryItem inventory[], int *itemCount, OrderRecord orders[], int *orderCount)
{
    float rawDt = GetFrameTime();
    UpdateNotificationTimers(rawDt);
    if (barcodeScanLaserFlash > 0.0f)
    {
        barcodeScanLaserFlash -= rawDt;
        if (barcodeScanLaserFlash < 0.0f)
            barcodeScanLaserFlash = 0.0f;
    }

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
    DrawTextCenteredInBox(skuPill, TextFormat("SKU #%d: %s [%s] | Stock: %d/%d", sku->id, sku->name, sku->barcode, sku->quantity, sku->maxStock), 16, textYellow);

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
// STAFF DISCREPANCY & STOCK AUDIT VIEW RENDERER
// =============================================================================

static void DrawStaffDiscrepancyAuditView(Rectangle centerArena, Vector2 mousePoint, int wheel, InventoryItem inventory[], int itemCount)
{
    Color bgDark = (Color){11, 13, 20, 255};
    Color cardDark = (Color){22, 26, 38, 255};
    Color cardHover = (Color){36, 42, 60, 255};
    Color inputDark = (Color){15, 18, 28, 255};
    Color textYellow = (Color){255, 255, 242, 255};
    Color textDim = (Color){205, 218, 238, 255};
    Color highlightCyan = (Color){35, 245, 255, 255};
    Color highlightPurple = (Color){205, 135, 255, 255};
    Color barRed = (Color){255, 80, 100, 255};
    Color textGreen = (Color){55, 250, 155, 255};
    Color warnOrange = (Color){255, 195, 45, 255};
    Color gridLine = (Color){52, 60, 84, 255};

    AiEconomyState *ai = GetAiEconomyState();

    // 1. Staff & Audit Reason Control Strip
    float stripY = centerArena.y + 68.0f;
    Rectangle staffBtn = {centerArena.x + 24, stripY, 390, 42};
    Rectangle reasonBtn = {centerArena.x + 428, stripY, 320, 42};
    Rectangle simShrinkBtn = {centerArena.x + 762, stripY, 205, 42};
    Rectangle recAllBtn = {centerArena.x + 980, stripY, 212, 42};

    bool hStaff = CheckCollisionPointRec(mousePoint, staffBtn);
    bool hReason = CheckCollisionPointRec(mousePoint, reasonBtn);
    bool hSimSh = CheckCollisionPointRec(mousePoint, simShrinkBtn);
    bool hRecAll = CheckCollisionPointRec(mousePoint, recAllBtn);

    DrawDashboardCard(staffBtn, hStaff ? cardHover : inputDark, highlightCyan, 0.28f, hStaff);
    DrawTextCenteredInBox(staffBtn, TextFormat("AUDITOR: %s", staffDirectory[selectedStaffIdx]), 15, highlightCyan);
    if (hStaff && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        selectedStaffIdx = (selectedStaffIdx + 1) % 4;
    }

    DrawDashboardCard(reasonBtn, hReason ? cardHover : inputDark, highlightPurple, 0.28f, hReason);
    DrawTextCenteredInBox(reasonBtn, TextFormat("REASON: %s", auditReasons[selectedReasonIdx]), 15, textYellow);
    if (hReason && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        selectedReasonIdx = (selectedReasonIdx + 1) % 4;
    }

    DrawDashboardCard(simShrinkBtn, hSimSh ? warnOrange : inputDark, warnOrange, 0.28f, hSimSh);
    DrawTextCenteredInBox(simShrinkBtn, "SIMULATE SHRINKAGE", 14, hSimSh ? bgDark : warnOrange);
    if (hSimSh && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        int r = PickRandomActiveInventoryIndex(inventory);
        if (inventory[r].isActive && inventory[r].quantity >= 3)
        {
            int lost = GetRandomValue(2, 5);
            if (lost > inventory[r].quantity)
                lost = inventory[r].quantity;
            inventory[r].quantity -= lost;
            SaveInventory(inventory, itemCount);
            PushNotification(TextFormat("UNRECORDED SHRINKAGE: -%d u missing from %s (Book: %d vs Shelf: %d)",
                                        lost, inventory[r].name, inventory[r].expectedQty, inventory[r].quantity),
                             2);
        }
    }

    int totalVarUnits = 0;
    int discSkus = GetTotalDiscrepancyCount(inventory, &totalVarUnits);

    DrawDashboardCard(recAllBtn, (hRecAll && discSkus > 0) ? textGreen : inputDark, discSkus > 0 ? textGreen : gridLine, 0.28f, hRecAll);
    DrawTextCenteredInBox(recAllBtn, discSkus > 0 ? TextFormat("RECONCILE ALL (%d)", discSkus) : "ALL STOCK RECONCILED", 14, (hRecAll && discSkus > 0) ? bgDark : (discSkus > 0 ? textGreen : textDim));
    if (hRecAll && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && discSkus > 0)
    {
        int logged = 0;
        for (int i = 0; i < MAX_ITEMS; i++)
        {
            if (inventory[i].isActive && inventory[i].quantity != inventory[i].expectedQty)
            {
                RecordStaffAudit(staffDirectory[selectedStaffIdx], &inventory[i], auditReasons[selectedReasonIdx], ai->simTime);
                logged++;
            }
        }
        SaveInventory(inventory, itemCount);
        PushNotification(TextFormat("%s reconciled %d SKU discrepancies into audit_log.csv.", staffDirectory[selectedStaffIdx], logged), 1);
    }

    // 2. Live Shelf vs. Book Discrepancy Inspection Table (Top Half)
    int yOff = (int)centerArena.y + 124;
    DrawTextTruncated("PRODUCT & EAN-13 BARCODE", (int)centerArena.x + 32, yOff, 14, 300, textDim);
    DrawTextTruncated("BOOK (EXPECTED)", (int)centerArena.x + 360, yOff, 14, 150, textDim);
    DrawTextTruncated("PHYSICAL COUNT", (int)centerArena.x + 520, yOff, 14, 160, textDim);
    DrawTextTruncated("VARIANCE & LOSS", (int)centerArena.x + 695, yOff, 14, 210, textDim);
    DrawTextTruncated("STAFF AUDIT ACTION", (int)centerArena.x + 935, yOff, 14, 240, textDim);
    yOff += 22;
    DrawLine((int)centerArena.x + 24, yOff, (int)centerArena.x + 1192, yOff, gridLine);
    yOff += 8;

    int drawnRows = 0;
    for (int i = 0; i < MAX_ITEMS && drawnRows < 5; i++)
    {
        if (!inventory[i].isActive)
            continue;
        if (!ItemMatchesSearch(&inventory[i], searchQuery))
            continue;

        int diff = inventory[i].quantity - inventory[i].expectedQty;
        Rectangle row = {centerArena.x + 24, (float)yOff, 1168, 56};
        Color borderCol = (diff < 0) ? barRed : ((diff > 0) ? warnOrange : gridLine);
        DrawDashboardCard(row, (drawnRows % 2 == 0) ? cardDark : inputDark, borderCol, 0.15f, false);

        DrawSimulatedBarcode((Rectangle){row.x + 10, row.y + 6, 112, 44}, inventory[i].barcode, highlightCyan);
        DrawTextTruncated(inventory[i].name, (int)row.x + 132, yOff + 10, 17, 210, textYellow);
        DrawTextTruncated(TextFormat("Unit Cost: EUR %.1f", inventory[i].orderCost), (int)row.x + 132, yOff + 32, 13, 210, textDim);

        DrawTextTruncated(TextFormat("%d units", inventory[i].expectedQty), (int)centerArena.x + 365, yOff + 18, 18, 130, textYellow);

        // Physical Count with manual +/- audit buttons
        Rectangle minusBtn = {centerArena.x + 520, (float)yOff + 12, 32, 32};
        Rectangle plusBtn = {centerArena.x + 628, (float)yOff + 12, 32, 32};
        bool hMin = CheckCollisionPointRec(mousePoint, minusBtn);
        bool hPls = CheckCollisionPointRec(mousePoint, plusBtn);

        DrawDashboardCard(minusBtn, hMin ? barRed : bgDark, gridLine, 0.3f, hMin);
        DrawTextCenteredInBox(minusBtn, "-", 18, textYellow);
        if (hMin && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && inventory[i].quantity > 0)
        {
            inventory[i].quantity--;
        }

        DrawTextCenteredInBox((Rectangle){centerArena.x + 554, (float)yOff + 12, 72, 32}, TextFormat("%d u", inventory[i].quantity), 18, diff != 0 ? warnOrange : textGreen);

        DrawDashboardCard(plusBtn, hPls ? textGreen : bgDark, gridLine, 0.3f, hPls);
        DrawTextCenteredInBox(plusBtn, "+", 18, textYellow);
        if (hPls && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && inventory[i].quantity < inventory[i].maxStock)
        {
            inventory[i].quantity++;
        }

        if (diff == 0)
        {
            DrawPillBadge((Rectangle){centerArena.x + 695, (float)yOff + 14, 185, 28}, "MATCH (0 VARIANCE)", textGreen, bgDark);
        }
        else
        {
            float eurImpact = (float)diff * inventory[i].orderCost;
            DrawPillBadge((Rectangle){centerArena.x + 695, (float)yOff + 14, 210, 28},
                          TextFormat("%+d u (%+.0f EUR)", diff, eurImpact),
                          diff < 0 ? barRed : warnOrange, bgDark);
        }

        Rectangle logBtn = {centerArena.x + 925, (float)yOff + 10, 252, 36};
        bool hLog = CheckCollisionPointRec(mousePoint, logBtn);
        Color btnCol = (diff != 0) ? highlightCyan : textDim;
        DrawDashboardCard(logBtn, (hLog && diff != 0) ? highlightCyan : bgDark, btnCol, 0.28f, hLog && diff != 0);
        DrawTextCenteredInBox(logBtn, diff != 0 ? "RECONCILE & LOG VARIANCE" : "VERIFIED OK", 14, (hLog && diff != 0) ? bgDark : btnCol);

        if (hLog && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && diff != 0)
        {
            RecordStaffAudit(staffDirectory[selectedStaffIdx], &inventory[i], auditReasons[selectedReasonIdx], ai->simTime);
            SaveInventory(inventory, itemCount);
            PushNotification(TextFormat("AUDIT LOGGED: %s reconciled %s (%+d u, %s)",
                                        staffDirectory[selectedStaffIdx], inventory[i].name, diff, auditReasons[selectedReasonIdx]),
                             1);
        }

        yOff += 62;
        drawnRows++;
    }

    // 3. Persistent Staff Discrepancy Audit Log History (Bottom Panel)
    int logCount = 0;
    AuditRecord *logs = GetAuditLogs(&logCount);

    Rectangle logCard = {centerArena.x + 24, centerArena.y + 476, 1168, 322};
    DrawDashboardCard(logCard, inputDark, highlightPurple, 0.05f, false);
    DrawTextTruncated(TextFormat("STAFF DISCREPANCY AUDIT LOG HISTORY (%d RECORDS SAVED IN audit_log.csv)", logCount),
                      (int)logCard.x + 20, (int)logCard.y + 12, 16, 820, highlightPurple);

    if (wheel != 0 && CheckCollisionPointRec(mousePoint, logCard))
    {
        scrollAudit -= wheel;
    }
    if (scrollAudit > logCount - 5)
        scrollAudit = logCount - 5;
    if (scrollAudit < 0)
        scrollAudit = 0;

    int ly = (int)logCard.y + 42;
    DrawTextTruncated("AUDIT ID & TIME", (int)logCard.x + 20, ly, 13, 140, textDim);
    DrawTextTruncated("STAFF AUDITOR", (int)logCard.x + 170, ly, 13, 210, textDim);
    DrawTextTruncated("PRODUCT & BARCODE", (int)logCard.x + 395, ly, 13, 270, textDim);
    DrawTextTruncated("BOOK -> COUNTED", (int)logCard.x + 680, ly, 13, 160, textDim);
    DrawTextTruncated("VARIANCE / EUR", (int)logCard.x + 850, ly, 13, 140, textDim);
    DrawTextTruncated("REASON LOGGED", (int)logCard.x + 1000, ly, 13, 155, textDim);
    ly += 20;
    DrawLine((int)logCard.x + 16, ly, (int)(logCard.x + logCard.width - 16), ly, gridLine);
    ly += 8;

    if (logCount == 0)
    {
        DrawTextCenteredInBox((Rectangle){logCard.x, (float)ly + 40, logCard.width, 40},
                              "No discrepancies reconciled yet. Trigger 'Warehouse Shrinkage' or 'Simulate Shrinkage' above to test Staff Auditing!",
                              16, textDim);
    }
    else
    {
        int drawnLogs = 0;
        for (int idx = logCount - 1 - scrollAudit; idx >= 0 && drawnLogs < 5; idx--)
        {
            AuditRecord *r = &logs[idx];
            DrawTextTruncated(TextFormat("#%d (T=%.0fs)", r->auditId, r->timestamp), (int)logCard.x + 20, ly + 6, 15, 140, highlightCyan);
            DrawTextTruncated(r->staffName, (int)logCard.x + 170, ly + 6, 15, 215, textYellow);
            DrawTextTruncated(TextFormat("%s [%s]", r->itemName, r->barcode), (int)logCard.x + 395, ly + 6, 15, 275, textYellow);
            DrawTextTruncated(TextFormat("%d u  ->  %d u", r->expectedQty, r->countedQty), (int)logCard.x + 680, ly + 6, 15, 155, textDim);
            DrawTextTruncated(TextFormat("%+d u (%+.0f EUR)", r->discrepancy, r->financialImpact),
                              (int)logCard.x + 850, ly + 6, 15, 140, r->discrepancy < 0 ? barRed : warnOrange);
            DrawPillBadge((Rectangle){logCard.x + 995, (float)ly + 2, 155, 24}, r->reason, highlightPurple, bgDark);

            ly += 46;
            drawnLogs++;
        }
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
    int discrepancyUnitTotal = 0;
    int discrepancySkuCount = GetTotalDiscrepancyCount(inventory, &discrepancyUnitTotal);

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
    DrawDashboardCard(kpi4, hk4 ? cardHover : inputDark, (discrepancySkuCount > 0 || lowStockCount > 0) ? barRed : highlightPurple, 0.22f, hk4);
    DrawTextTruncated(TextFormat("CATALOG (%d/%d) & DISCREPANCIES", activeStock, MAX_STORE_SKUS), (int)kpi4.x + 16, (int)kpi4.y + 8, 13, 285, textDim);
    DrawTextTruncated(discrepancySkuCount > 0
                          ? TextFormat("%d SKU Discrepancies (%d u)!", discrepancySkuCount, discrepancyUnitTotal)
                          : TextFormat("%d Low SKUs  |  +%d In Transit", lowStockCount, inTransitUnits),
                      (int)kpi4.x + 16, (int)kpi4.y + 26, 19, 285, (discrepancySkuCount > 0 || lowStockCount > 0) ? barRed : textYellow);
    DrawTextTruncated("Click to Audit Discrepancies / Search", (int)kpi4.x + 16, (int)kpi4.y + 52, 13, 285, discrepancySkuCount > 0 ? warnOrange : textGreen);
    if (hk4 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        currentScreen = SCREEN_VIEW;
        catalogViewMode = (discrepancySkuCount > 0) ? 2 : 0;
    }

    // =========================================================================
    // 2. LEFT EDGE COMMAND & ADAPTIVE 1/2-COLUMN SHELF MONITOR (x: 16..336, y: 120..936)
    // =========================================================================
    Rectangle leftEdge = {16, 120, 320, 816};
    DrawDashboardCard(leftEdge, cardDark, gridLine, 0.03f, false);

    DrawTextCrisp("NDAL WORKSPACES", 32, 134, 15, highlightCyan);
    const char *navLabels[5] = {
        "Live Digital Twin Arena",
        "Catalog, Search & Audit",
        "Supplier Network & AI",
        "Chaos & Store Scraper",
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
        DrawTextTruncated(navLabels[n], (int)navBtn.x + 20, (int)navBtn.y + 10, 16, 230, isCurrent ? textYellow : textDim);

        if (n == 1 && discrepancySkuCount > 0)
        {
            DrawPillBadge((Rectangle){navBtn.x + 236, navBtn.y + 8, 48, 22}, TextFormat("%d!", discrepancySkuCount), barRed, bgDark);
        }

        if (isHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            currentScreen = navScreens[n];
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
        bool hasDisc = (inventory[i].quantity != inventory[i].expectedQty);
        float mPct = (ai->aiPriceMult[i] - 1.0f) * 100.0f;

        Color sBorder = hasDisc ? barRed : (onSale ? warnOrange : (isLow ? barRed : gridLine));
        DrawDashboardCard(skuBox, hSku ? cardHover : inputDark, sBorder, 0.2f, hSku);

        Color pCol = (mPct > 4.0f) ? warnOrange : ((mPct < -2.0f) ? textGreen : highlightCyan);
        float dispP = onSale ? (inventory[i].price * 0.88f) : inventory[i].price;

        if (!compactLeftGrid)
        {
            DrawTextTruncated(inventory[i].name, (int)skuBox.x + 12, (int)skuBox.y + 8, 16, 150, onSale ? warnOrange : textYellow);
            DrawTextTruncated(TextFormat("EUR %.0f (%+.0f%%)", dispP, mPct), (int)skuBox.x + 168, (int)skuBox.y + 8, 14, 118, pCol);
            DrawTextTruncated(hasDisc
                                  ? TextFormat("Stock: %d (Book:%d) [%s]", inventory[i].quantity, inventory[i].expectedQty, inventory[i].barcode)
                                  : TextFormat("Stock: %d/%d [%s]", inventory[i].quantity, inventory[i].maxStock, inventory[i].barcode),
                              (int)skuBox.x + 12, (int)skuBox.y + 28, 13, 268, hasDisc ? barRed : (isLow ? barRed : textDim));
        }
        else
        {
            DrawTextTruncated(inventory[i].name, (int)skuBox.x + 8, (int)skuBox.y + 6, 14, 126, onSale ? warnOrange : textYellow);
            DrawTextTruncated(hasDisc
                                  ? TextFormat("%d vs %d! (%+.0f%%)", inventory[i].quantity, inventory[i].expectedQty, mPct)
                                  : TextFormat("%d/%d %+.0f%%", inventory[i].quantity, inventory[i].maxStock, mPct),
                              (int)skuBox.x + 8, (int)skuBox.y + 26, 13, 126, (hasDisc || isLow) ? barRed : pCol);
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
            catalogViewMode = 1;
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

        Rectangle closeBtn = {centerArena.x + 980, centerArena.y + 14, 212, 40};
        bool hClose = CheckCollisionPointRec(mousePoint, closeBtn);
        DrawDashboardCard(closeBtn, hClose ? highlightCyan : inputDark, highlightCyan, 0.3f, hClose);
        DrawTextCenteredInBox(closeBtn, "< RETURN TO ARENA", 15, hClose ? bgDark : textYellow);
        if (hClose && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            currentScreen = SCREEN_MENU;
            searchBoxFocused = false;
            scraperBoxFocused = false;
        }

        switch (currentScreen)
        {
        // -----------------------------------------------------------------
        // 1080P CATALOG, BARCODE SEARCH, GRAPH & STAFF AUDIT (SCREEN_VIEW)
        // -----------------------------------------------------------------
        case SCREEN_VIEW:
        {
            // Top Mode Tabs
            Rectangle tabCat = {centerArena.x + 24, centerArena.y + 14, 235, 40};
            Rectangle tabGrf = {centerArena.x + 269, centerArena.y + 14, 220, 40};
            Rectangle tabAud = {centerArena.x + 499, centerArena.y + 14, 275, 40};
            Rectangle tabScr = {centerArena.x + 784, centerArena.y + 14, 184, 40};

            bool hTC = CheckCollisionPointRec(mousePoint, tabCat);
            bool hTG = CheckCollisionPointRec(mousePoint, tabGrf);
            bool hTA = CheckCollisionPointRec(mousePoint, tabAud);
            bool hTS = CheckCollisionPointRec(mousePoint, tabScr);

            DrawDashboardCard(tabCat, catalogViewMode == 0 ? highlightCyan : (hTC ? cardHover : inputDark), highlightCyan, 0.3f, hTC);
            DrawTextCenteredInBox(tabCat, TextFormat("CATALOG & BARCODES (%d)", activeStock), 14, catalogViewMode == 0 ? bgDark : textYellow);

            DrawDashboardCard(tabGrf, catalogViewMode == 1 ? highlightPurple : (hTG ? cardHover : inputDark), highlightPurple, 0.3f, hTG);
            DrawTextCenteredInBox(tabGrf, "TELEMETRY GRAPH", 14, catalogViewMode == 1 ? bgDark : textYellow);

            Color audTabCol = (discrepancySkuCount > 0) ? barRed : textGreen;
            DrawDashboardCard(tabAud, catalogViewMode == 2 ? audTabCol : (hTA ? cardHover : inputDark), audTabCol, 0.3f, hTA);
            DrawTextCenteredInBox(tabAud, discrepancySkuCount > 0 ? TextFormat("STAFF AUDIT (%d DISCREPANCY!)", discrepancySkuCount) : "STAFF DISCREPANCY AUDIT",
                                  14, catalogViewMode == 2 ? bgDark : (discrepancySkuCount > 0 ? barRed : textGreen));

            DrawDashboardCard(tabScr, hTS ? warnOrange : inputDark, warnOrange, 0.3f, hTS);
            DrawTextCenteredInBox(tabScr, "STORE SCRAPER >", 14, hTS ? bgDark : warnOrange);

            if (hTC && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                catalogViewMode = 0;
            if (hTG && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                catalogViewMode = 1;
            if (hTA && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                catalogViewMode = 2;
            if (hTS && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                currentScreen = SCREEN_EVENTS;
                eventsSubTab = 2;
            }

            if (catalogViewMode == 1)
            {
                DrawProductAnalyticsGraph(centerArena, mousePoint, inventory, *itemCount);
            }
            else if (catalogViewMode == 2)
            {
                DrawStaffDiscrepancyAuditView(centerArena, mousePoint, wheel, inventory, *itemCount);
            }
            else
            {
                // --- SEARCH & SIMULATED BARCODE SCANNER BAR ---
                float sBarY = centerArena.y + 66.0f;
                Rectangle searchBox = {centerArena.x + 24, sBarY, 560, 40};
                Rectangle clearSearchBtn = {centerArena.x + 594, sBarY, 96, 40};
                Rectangle simScanBtn = {centerArena.x + 700, sBarY, 270, 40};
                Rectangle addNewBtn = {centerArena.x + 980, sBarY, 212, 40};

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                    searchBoxFocused = CheckCollisionPointRec(mousePoint, searchBox);
                }

                if (searchBoxFocused)
                {
                    int key = GetCharPressed();
                    while (key > 0)
                    {
                        if (key >= 32 && key <= 125 && searchLen < 44)
                        {
                            searchQuery[searchLen++] = (char)key;
                            searchQuery[searchLen] = '\0';
                            scrollIndex = 0;
                        }
                        key = GetCharPressed();
                    }
                    if (IsKeyPressed(KEY_BACKSPACE) && searchLen > 0)
                    {
                        searchQuery[--searchLen] = '\0';
                        scrollIndex = 0;
                    }
                }

                DrawDashboardCard(searchBox, inputDark, searchBoxFocused ? highlightCyan : gridLine, 0.25f, searchBoxFocused);
                DrawTextTruncated(searchLen > 0 ? TextFormat("SEARCH / BARCODE: %s_", searchQuery) : "CLICK TO SEARCH BY PRODUCT NAME OR EAN-13 BARCODE...",
                                  (int)searchBox.x + 16, (int)searchBox.y + 12, 15, 530, searchLen > 0 ? highlightCyan : textDim);

                bool hClr = CheckCollisionPointRec(mousePoint, clearSearchBtn);
                DrawDashboardCard(clearSearchBtn, hClr ? barRed : inputDark, gridLine, 0.25f, hClr);
                DrawTextCenteredInBox(clearSearchBtn, "CLEAR", 14, hClr ? bgDark : textYellow);
                if (hClr && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                    searchQuery[0] = '\0';
                    searchLen = 0;
                    scrollIndex = 0;
                }

                bool hScan = CheckCollisionPointRec(mousePoint, simScanBtn);
                DrawDashboardCard(simScanBtn, hScan ? highlightPurple : inputDark, highlightPurple, 0.25f, hScan);
                DrawTextCenteredInBox(simScanBtn, "SIMULATE BARCODE SCAN", 14, hScan ? bgDark : highlightPurple);
                if (hScan && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                    int targetIdx = -1;
                    // Prefer scanning an item with a discrepancy first, else random active item
                    for (int i = 0; i < MAX_ITEMS; i++)
                    {
                        if (inventory[i].isActive && inventory[i].quantity != inventory[i].expectedQty)
                        {
                            targetIdx = i;
                            break;
                        }
                    }
                    if (targetIdx == -1)
                        targetIdx = PickRandomActiveInventoryIndex(inventory);

                    if (inventory[targetIdx].isActive)
                    {
                        strncpy(searchQuery, inventory[targetIdx].barcode, 44);
                        searchLen = (int)strlen(searchQuery);
                        barcodeScanLaserFlash = 1.6f;
                        scrollIndex = 0;
                        PushNotification(TextFormat("BARCODE SCANNED: [%s] -> %s (Shelf: %d | Book: %d)",
                                                    inventory[targetIdx].barcode, inventory[targetIdx].name,
                                                    inventory[targetIdx].quantity, inventory[targetIdx].expectedQty),
                                         0);
                    }
                }

                bool canAddMore = (activeStock < MAX_STORE_SKUS);
                bool hAddN = CheckCollisionPointRec(mousePoint, addNewBtn);
                DrawDashboardCard(addNewBtn, (hAddN && canAddMore) ? textGreen : inputDark, canAddMore ? textGreen : gridLine, 0.25f, hAddN && canAddMore);
                DrawTextCenteredInBox(addNewBtn, canAddMore ? "+ ADD NEW SKU" : "MAX 12 SKUS FULL", 14, (hAddN && canAddMore) ? bgDark : (canAddMore ? textGreen : textDim));
                if (hAddN && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && canAddMore)
                {
                    currentScreen = SCREEN_EVENTS;
                    eventsSubTab = 1;
                    editTargetIndex = -1;
                    nameInput[0] = qtyInput[0] = priceInput[0] = costInput[0] = maxInput[0] = minInput[0] = '\0';
                    nameCount = qtyCount = priceCount = costCount = maxCount = minCount = 0;
                }

                // Count matching items for scroll clamping
                int matchingCount = 0;
                for (int i = 0; i < MAX_ITEMS; i++)
                {
                    if (ItemMatchesSearch(&inventory[i], searchQuery))
                        matchingCount++;
                }

                if (wheel != 0)
                    scrollIndex -= wheel;
                if (scrollIndex > matchingCount - 7)
                    scrollIndex = matchingCount - 7;
                if (scrollIndex < 0)
                    scrollIndex = 0;

                int yOff = (int)centerArena.y + 118;
                DrawTextTruncated("BARCODE & PRODUCT DETAILS", (int)centerArena.x + 32, yOff, 14, 340, textDim);
                DrawTextTruncated("PHYSICAL vs BOOK STOCK", (int)centerArena.x + 390, yOff, 14, 210, textDim);
                DrawTextTruncated("AI RETAIL PRICE (MARKUP)", (int)centerArena.x + 615, yOff, 14, 210, textDim);
                DrawTextTruncated("TELEMETRY, AUDIT & ACTIONS", (int)centerArena.x + 835, yOff, 14, 350, textDim);
                yOff += 22;
                DrawLine((int)centerArena.x + 24, yOff, (int)centerArena.x + 1192, yOff, gridLine);
                yOff += 8;

                int matchIdx = 0, drawn = 0;
                for (int i = 0; i < MAX_ITEMS; i++)
                {
                    if (ItemMatchesSearch(&inventory[i], searchQuery))
                    {
                        if (matchIdx >= scrollIndex && drawn < 7)
                        {
                            int diff = inventory[i].quantity - inventory[i].expectedQty;
                            Rectangle row = {centerArena.x + 24, (float)yOff, 1168, 82};
                            DrawDashboardCard(row, (drawn % 2 == 0) ? cardDark : inputDark, diff != 0 ? barRed : gridLine, 0.15f, false);

                            // 1D Barcode Graphic + Product Title
                            DrawSimulatedBarcode((Rectangle){row.x + 12, row.y + 10, 118, 62}, inventory[i].barcode, highlightCyan);

                            float elast = GetItemPriceElasticity(i);
                            float mPct = (ai->aiPriceMult[i] - 1.0f) * 100.0f;

                            DrawTextTruncated(inventory[i].name, (int)row.x + 142, yOff + 14, 19, 235, textYellow);
                            DrawTextTruncated(TextFormat("Cost: EUR %.0f | Min:%d Max:%d | Vel:%.2fx",
                                                         inventory[i].orderCost, inventory[i].minStock, inventory[i].maxStock, elast),
                                              (int)row.x + 142, yOff + 44, 13, 240, textDim);

                            // Stock + Discrepancy Status
                            DrawTextTruncated(TextFormat("Shelf: %d/%d (+%d) | Book: %d",
                                                         inventory[i].quantity, inventory[i].maxStock, inventory[i].inTransit, inventory[i].expectedQty),
                                              (int)centerArena.x + 390, yOff + 14, 15, 215, (inventory[i].quantity == 0 || diff != 0) ? barRed : textYellow);

                            if (diff != 0)
                            {
                                DrawPillBadge((Rectangle){centerArena.x + 390, (float)yOff + 42, 195, 24},
                                              TextFormat("DISCREPANCY: %+d UNITS!", diff), barRed, bgDark);
                            }
                            else
                            {
                                float fillR = inventory[i].maxStock > 0 ? (float)inventory[i].quantity / (float)inventory[i].maxStock : 0.0f;
                                if (fillR > 1.0f)
                                    fillR = 1.0f;
                                Rectangle sTrack = {centerArena.x + 390, (float)yOff + 48, 185, 10};
                                DrawRectangleRounded(sTrack, 1.0f, 6, bgDark);
                                if (fillR > 0.02f)
                                {
                                    Color bCol = (inventory[i].quantity <= inventory[i].minStock) ? barRed : highlightCyan;
                                    DrawRectangleRounded((Rectangle){sTrack.x, sTrack.y, 185 * fillR, 10}, 1.0f, 6, bCol);
                                }
                            }

                            // Price & Margin
                            float dispP = (inventory[i].saleTimer > 0) ? (inventory[i].price * 0.88f) : inventory[i].price;
                            Color pCol = (mPct > 4.0f) ? warnOrange : ((mPct < -2.0f) ? textGreen : highlightCyan);
                            float uProf = dispP - GetEffectiveUnitCost(&inventory[i], ev->subsidyTimer);
                            DrawTextTruncated(TextFormat("EUR %.2f (%+.1f%%)", dispP, mPct),
                                              (int)centerArena.x + 615, yOff + 16, 18, 205, pCol);
                            DrawTextTruncated(TextFormat("Net Margin: %+.1f / u", uProf),
                                              (int)centerArena.x + 615, yOff + 44, 14, 205, uProf >= 0 ? textGreen : barRed);

                            // Action Buttons
                            Rectangle graphBtn = {centerArena.x + 830, (float)yOff + 20, 82, 40};
                            Rectangle auditBtn = {centerArena.x + 918, (float)yOff + 20, 92, 40};
                            Rectangle saleBtn = {centerArena.x + 1016, (float)yOff + 20, 86, 40};
                            Rectangle editBtn = {centerArena.x + 1108, (float)yOff + 20, 72, 40};

                            bool hGrf = CheckCollisionPointRec(mousePoint, graphBtn);
                            bool hAud = CheckCollisionPointRec(mousePoint, auditBtn);
                            bool hSale = CheckCollisionPointRec(mousePoint, saleBtn);
                            bool hEdit = CheckCollisionPointRec(mousePoint, editBtn);
                            bool onSale = (inventory[i].saleTimer > 0.0f);

                            DrawDashboardCard(graphBtn, hGrf ? highlightCyan : cardDark, highlightCyan, 0.3f, hGrf);
                            DrawTextCenteredInBox(graphBtn, "GRAPH", 13, hGrf ? bgDark : highlightCyan);
                            if (hGrf && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                            {
                                selectedGraphSku = i;
                                catalogViewMode = 1;
                            }

                            Color aCol = (diff != 0) ? barRed : textGreen;
                            DrawDashboardCard(auditBtn, hAud ? aCol : cardDark, aCol, 0.3f, hAud);
                            DrawTextCenteredInBox(auditBtn, diff != 0 ? "FIX AUDIT" : "AUDIT OK", 13, hAud ? bgDark : aCol);
                            if (hAud && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                            {
                                if (diff != 0)
                                {
                                    RecordStaffAudit(staffDirectory[selectedStaffIdx], &inventory[i], auditReasons[selectedReasonIdx], ai->simTime);
                                    SaveInventory(inventory, *itemCount);
                                    PushNotification(TextFormat("STAFF AUDIT: %s reconciled %s (%+d u)",
                                                                staffDirectory[selectedStaffIdx], inventory[i].name, diff),
                                                     1);
                                }
                                else
                                {
                                    catalogViewMode = 2;
                                }
                            }

                            DrawDashboardCard(saleBtn, onSale ? warnOrange : (hSale ? warnOrange : cardDark), warnOrange, 0.3f, hSale);
                            DrawTextCenteredInBox(saleBtn, onSale ? TextFormat("%.0fs", inventory[i].saleTimer) : "SALE", 13, (onSale || hSale) ? bgDark : warnOrange);
                            if (hSale && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                            {
                                inventory[i].saleTimer = 20.0f;
                                RecordSkuTelemetrySample(i, &inventory[i], ai->simTime);
                                PushNotification(TextFormat("%s placed on FLASH SALE!", inventory[i].name), 2);
                            }

                            DrawDashboardCard(editBtn, hEdit ? highlightPurple : cardDark, highlightPurple, 0.3f, hEdit);
                            DrawTextCenteredInBox(editBtn, "EDIT", 13, textYellow);
                            if (hEdit && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                            {
                                currentScreen = SCREEN_EVENTS;
                                eventsSubTab = 1;
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

                            yOff += 90;
                            drawn++;
                        }
                        matchIdx++;
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
        // 1080P CHAOS DECK, SKU EDITOR & SHOPIFY STORE SCRAPER (SCREEN_EVENTS)
        // -----------------------------------------------------------------
        case SCREEN_EVENTS:
        {
            Rectangle tabChaos = {centerArena.x + 24, centerArena.y + 14, 260, 40};
            Rectangle tabSku = {centerArena.x + 296, centerArena.y + 14, 290, 40};
            Rectangle tabScraper = {centerArena.x + 598, centerArena.y + 14, 360, 40};

            bool hT1 = CheckCollisionPointRec(mousePoint, tabChaos);
            bool hT2 = CheckCollisionPointRec(mousePoint, tabSku);
            bool hT3 = CheckCollisionPointRec(mousePoint, tabScraper);

            DrawDashboardCard(tabChaos, eventsSubTab == 0 ? barRed : inputDark, barRed, 0.3f, hT1);
            DrawTextCenteredInBox(tabChaos, "8 CHAOS EVENTS DECK", 15, textYellow);

            DrawDashboardCard(tabSku, eventsSubTab == 1 ? highlightCyan : inputDark, highlightCyan, 0.3f, hT2);
            DrawTextCenteredInBox(tabSku, TextFormat("ADD / EDIT SKU (%d/%d)", activeStock, MAX_STORE_SKUS), 15, eventsSubTab == 1 ? bgDark : textYellow);

            DrawDashboardCard(tabScraper, eventsSubTab == 2 ? warnOrange : inputDark, warnOrange, 0.3f, hT3);
            DrawTextCenteredInBox(tabScraper, "SHOPIFY / WEB STORE SCRAPER", 15, eventsSubTab == 2 ? bgDark : warnOrange);

            if (hT1 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                eventsSubTab = 0;
            if (hT2 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                eventsSubTab = 1;
                editTargetIndex = -1;
            }
            if (hT3 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                eventsSubTab = 2;

            if (eventsSubTab == 0)
            {
                const char *evTitles[8] = {
                    "1. Flash Sale Frenzy (3x SKUs)", "2. Viral Demand Surge (2.5x)",
                    "3. Damaged Cargo Crisis (-40%)", "4. Severe Weather Storm (-55%)",
                    "5. Supplier Network Outage", "6. Energy Tariff Spike (2.1x)",
                    "7. Staff Overtime Strike (1.85x)", "8. Warehouse Shrinkage (-24%)"};
                const char *evSub[8] = {
                    "3 random SKUs on -12% sale, 2.2x demand velocity & +Satisfaction for 20s.",
                    "All active products surge to 2.5x customer buying speed for 18 seconds.",
                    "Damages an active shipment in transit, creating a 40% Staff Audit Discrepancy.",
                    "Heavy rain & lightning slow all highway delivery trucks by 55% for 18 seconds.",
                    "Takes 1-2 suppliers offline for 22s, forcing expensive AI rerouting.",
                    "Facility utility and power bills surge 2.1x (EUR 242/8s) for 24 seconds.",
                    "Cashier & warehouse wages surge 1.85x (EUR 305/8s) and Satisfaction drops -7%.",
                    "Unrecorded theft removes 24% physical stock, creating a Book vs Shelf Discrepancy."};
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
            else if (eventsSubTab == 1)
            {
                DrawTextTruncated(editTargetIndex == -1
                                      ? TextFormat("Adding New SKU to Store Aisles (Slot %d of %d Max)", activeStock + 1, MAX_STORE_SKUS)
                                      : TextFormat("Editing Existing SKU #%d (%s | Barcode: %s)", inventory[editTargetIndex].id, inventory[editTargetIndex].name, inventory[editTargetIndex].barcode),
                                  (int)centerArena.x + 380, (int)centerArena.y + 72, 18, 750, highlightCyan);

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
                        nameInput[--nameCount] = '\0';
                    else if (activeInput == 1 && qtyCount > 0)
                        qtyInput[--qtyCount] = '\0';
                    else if (activeInput == 2 && priceCount > 0)
                        priceInput[--priceCount] = '\0';
                    else if (activeInput == 3 && costCount > 0)
                        costInput[--costCount] = '\0';
                    else if (activeInput == 4 && maxCount > 0)
                        maxInput[--maxCount] = '\0';
                    else if (activeInput == 5 && minCount > 0)
                        minInput[--minCount] = '\0';
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
                DrawTextCenteredInBox(saveBtn, canSave ? "SAVE PRODUCT SKU & GENERATE BARCODE" : "MAX 12 ACTIVE SKUS REACHED", 20, (hSave && canSave) ? bgDark : textYellow);

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
                            GenerateSimulatedBarcode(inventory[idx].id, inventory[idx].name, inventory[idx].barcode);
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
                            inventory[idx].expectedQty = inventory[idx].quantity;
                            inventory[idx].inTransit = 0;
                            inventory[idx].baseDemandRate = 0.28f;
                            inventory[idx].saleTimer = 0.0f;
                            inventory[idx].isActive = true;

                            ResetSkuFairPrice(idx, &inventory[idx], true);
                            ClearSkuTelemetry(idx);
                            RecordSkuTelemetrySample(idx, &inventory[idx], ai->simTime);
                            SpawnShopperAgent(idx, 1, inventory[idx].price, 0.0f, inventory[idx].quantity == 0);
                            PushNotification(TextFormat("Added '%s' [%s] to live store aisles!", inventory[idx].name, inventory[idx].barcode), 1);
                        }
                    }
                    else if (editTargetIndex >= 0)
                    {
                        strcpy(inventory[editTargetIndex].name, nameInput);
                        GenerateSimulatedBarcode(inventory[editTargetIndex].id, inventory[editTargetIndex].name, inventory[editTargetIndex].barcode);
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
                        inventory[editTargetIndex].expectedQty = inventory[editTargetIndex].quantity;
                        ResetSkuFairPrice(editTargetIndex, &inventory[editTargetIndex], false);
                        RecordSkuTelemetrySample(editTargetIndex, &inventory[editTargetIndex], ai->simTime);
                    }
                    SaveInventory(inventory, *itemCount);
                    currentScreen = SCREEN_VIEW;
                    catalogViewMode = 0;
                }
            }
            else if (eventsSubTab == 2)
            {
                // --- SHOPIFY / ONLINE STORE JSON SCRAPER PANEL ---
                Rectangle heroCard = {centerArena.x + 24, centerArena.y + 74, 1168, 220};
                DrawDashboardCard(heroCard, cardDark, warnOrange, 0.08f, false);

                DrawTextTruncated("SHOPIFY /products.json & PUBLIC E-COMMERCE API INVENTORY SCRAPER",
                                  (int)heroCard.x + 24, (int)heroCard.y + 18, 22, 1100, warnOrange);
                DrawTextTruncated("Automatically scrapes live product titles & retail prices from any Shopify store (/products.json) or JSON endpoint,",
                                  (int)heroCard.x + 24, (int)heroCard.y + 52, 16, 1120, textYellow);
                DrawTextTruncated("calculates wholesale supplier costs, assigns EAN-13 barcodes, and populates inventory_db.csv.",
                                  (int)heroCard.x + 24, (int)heroCard.y + 76, 16, 1120, textDim);

                Rectangle urlBox = {heroCard.x + 24, heroCard.y + 114, 790, 50};
                Rectangle scrapeNowBtn = {heroCard.x + 830, heroCard.y + 114, 314, 50};

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                    scraperBoxFocused = CheckCollisionPointRec(mousePoint, urlBox);
                }
                if (scraperBoxFocused)
                {
                    int key = GetCharPressed();
                    while (key > 0)
                    {
                        if (key >= 32 && key <= 125 && scraperUrlLen < 120)
                        {
                            scraperUrlInput[scraperUrlLen++] = (char)key;
                            scraperUrlInput[scraperUrlLen] = '\0';
                        }
                        key = GetCharPressed();
                    }
                    if (IsKeyPressed(KEY_BACKSPACE) && scraperUrlLen > 0)
                    {
                        scraperUrlInput[--scraperUrlLen] = '\0';
                    }
                }

                DrawDashboardCard(urlBox, inputDark, scraperBoxFocused ? highlightCyan : gridLine, 0.25f, scraperBoxFocused);
                DrawTextTruncated(TextFormat("STORE URL: %s_", scraperUrlInput), (int)urlBox.x + 18, (int)urlBox.y + 16, 18, 755, highlightCyan);

                bool hScrNow = CheckCollisionPointRec(mousePoint, scrapeNowBtn);
                DrawDashboardCard(scrapeNowBtn, hScrNow ? textGreen : inputDark, textGreen, 0.28f, hScrNow);
                DrawTextCenteredInBox(scrapeNowBtn, "SCRAPE LIVE STORE JSON", 16, hScrNow ? bgDark : textGreen);

                if (hScrNow && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                {
                    ScrapeOnlineStoreInventory(inventory, itemCount, scraperUrlInput, 0, scraperStatusMsg, sizeof(scraperStatusMsg));
                    InitAiEconomy(inventory, itemCount);
                    InitTelemetry(inventory);
                    PushNotification(scraperStatusMsg, 1);
                }

                DrawTextTruncated(scraperStatusMsg, (int)heroCard.x + 24, (int)heroCard.y + 180, 15, 1120, highlightCyan);

                // 4 One-Click Store Presets
                const char *pTitles[4] = {
                    "1. Keychron Shopify Store (/products.json)",
                    "2. DummyJSON Global Tech Store API",
                    "3. Shopify Streetwear & Sneaker Boutique",
                    "4. Restore Default NDAL PC Hardware (6 SKUs)"};
                const char *pDescs[4] = {
                    "Scrapes mechanical keyboards, mice & switches from keychron.com/products.json",
                    "Pulls consumer electronics & gadgets from dummyjson.com/products?limit=12",
                    "Imports 8 curated apparel & sneaker SKUs with retail/wholesale margins & barcodes",
                    "Resets inventory_db.csv back to the original 6 NDAL PC hardware components"};
                Color pCols[4] = {highlightCyan, highlightPurple, warnOrange, textGreen};

                for (int p = 0; p < 4; p++)
                {
                    int col = p % 2, row = p / 2;
                    Rectangle pCard = {centerArena.x + 24 + col * 592, centerArena.y + 314 + row * 228, 576, 208};
                    bool hp = CheckCollisionPointRec(mousePoint, pCard);
                    DrawDashboardCard(pCard, hp ? cardHover : cardDark, pCols[p], 0.1f, hp);

                    DrawTextTruncated(pTitles[p], (int)pCard.x + 22, (int)pCard.y + 20, 20, 530, pCols[p]);
                    DrawTextTruncated(pDescs[p], (int)pCard.x + 22, (int)pCard.y + 58, 15, 530, textYellow);
                    DrawTextTruncated("Auto-generates EAN-13 Barcodes, Book Stock & AI Elasticity Baselines", (int)pCard.x + 22, (int)pCard.y + 88, 14, 530, textDim);

                    Rectangle loadBtn = {pCard.x + 22, pCard.y + 136, 532, 48};
                    DrawDashboardCard(loadBtn, hp ? pCols[p] : inputDark, pCols[p], 0.3f, hp);
                    DrawTextCenteredInBox(loadBtn, "IMPORT STORE CATALOG & LAUNCH SIMULATION", 16, hp ? bgDark : textYellow);

                    if (hp && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                    {
                        ScrapeOnlineStoreInventory(inventory, itemCount, NULL, p + 1, scraperStatusMsg, sizeof(scraperStatusMsg));
                        InitAiEconomy(inventory, itemCount);
                        InitTelemetry(inventory);
                        PushNotification(scraperStatusMsg, 1);
                        currentScreen = SCREEN_VIEW;
                        catalogViewMode = 0;
                    }
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