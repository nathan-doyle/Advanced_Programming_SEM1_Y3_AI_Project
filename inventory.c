#include "inventory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

static AuditRecord auditLogs[MAX_AUDIT_LOGS];
static int auditLogCount = 0;
static bool auditLoaded = false;

void GenerateSimulatedBarcode(int id, const char *name, char outBarcode[16])
{
    unsigned int hash = (unsigned int)(id * 131);
    if (name != NULL)
    {
        for (int i = 0; name[i] != '\0'; i++)
        {
            hash = hash * 31 + (unsigned char)toupper((unsigned char)name[i]);
        }
    }
    int digits[12];
    // GS1 Ireland Prefix: 539
    digits[0] = 5;
    digits[1] = 3;
    digits[2] = 9;
    digits[3] = (id / 10) % 10;
    digits[4] = id % 10;

    unsigned int h = hash;
    for (int i = 5; i < 12; i++)
    {
        digits[i] = (int)(h % 10);
        h /= 10;
    }
    // Standard EAN-13 check digit calculation
    int sum = 0;
    for (int i = 0; i < 12; i++)
    {
        sum += digits[i] * ((i % 2 == 0) ? 1 : 3);
    }
    int check = (10 - (sum % 10)) % 10;

    for (int i = 0; i < 12; i++)
    {
        outBarcode[i] = (char)('0' + digits[i]);
    }
    outBarcode[12] = (char)('0' + check);
    outBarcode[13] = '\0';
}

bool ItemMatchesSearch(const InventoryItem *item, const char *query)
{
    if (item == NULL || !item->isActive)
        return false;
    if (query == NULL || query[0] == '\0')
        return true;

    char qLower[64];
    int qLen = 0;
    for (int i = 0; query[i] != '\0' && qLen < 62; i++)
    {
        qLower[qLen++] = (char)tolower((unsigned char)query[i]);
    }
    qLower[qLen] = '\0';

    char nameLower[64];
    int nLen = 0;
    for (int i = 0; item->name[i] != '\0' && nLen < 62; i++)
    {
        nameLower[nLen++] = (char)tolower((unsigned char)item->name[i]);
    }
    nameLower[nLen] = '\0';

    if (strstr(nameLower, qLower) != NULL)
        return true;
    if (strstr(item->barcode, qLower) != NULL)
        return true;

    char idBuf[16];
    snprintf(idBuf, sizeof(idBuf), "#%d", item->id);
    if (strstr(idBuf, qLower) != NULL)
        return true;

    return false;
}

int GetTotalDiscrepancyCount(const InventoryItem inventory[], int *outTotalUnitVariance)
{
    int skuDiscrepancies = 0;
    int totalVar = 0;
    for (int i = 0; i < MAX_ITEMS; i++)
    {
        if (inventory[i].isActive)
        {
            int diff = inventory[i].quantity - inventory[i].expectedQty;
            if (diff != 0)
            {
                skuDiscrepancies++;
                totalVar += abs(diff);
            }
        }
    }
    if (outTotalUnitVariance != NULL)
        *outTotalUnitVariance = totalVar;
    return skuDiscrepancies;
}

void SaveInventory(InventoryItem inventory[], int count)
{
    FILE *file = fopen("inventory_db.csv", "w");
    if (file == NULL)
        return;
    for (int i = 0; i < count; i++)
    {
        if (inventory[i].barcode[0] == '\0')
        {
            GenerateSimulatedBarcode(inventory[i].id, inventory[i].name, inventory[i].barcode);
        }
        fprintf(file, "%d,%s,%s,%d,%d,%.2f,%.2f,%d,%d,%d,%.2f,%d\n",
                inventory[i].id,
                inventory[i].name,
                inventory[i].barcode,
                inventory[i].quantity,
                inventory[i].expectedQty,
                inventory[i].price,
                inventory[i].orderCost,
                inventory[i].maxStock,
                inventory[i].minStock,
                inventory[i].inTransit,
                inventory[i].baseDemandRate,
                inventory[i].isActive ? 1 : 0);
    }
    fclose(file);
}

int LoadInventory(InventoryItem inventory[])
{
    if (!auditLoaded)
        LoadAuditLogs();

    FILE *file = fopen("inventory_db.csv", "r");
    if (file == NULL)
        return 0;

    int count = 0;
    char line[512];

    while (count < MAX_ITEMS && fgets(line, sizeof(line), file) != NULL)
    {
        int id = 0, qty = 0, expQty = 0, maxS = 30, minS = 10, inTrans = 0, actFlag = 0;
        float price = 0.0f, cost = 0.0f, demand = 0.30f;
        char name[50] = {0};
        char barcode[16] = {0};

        // Try new 12-column format (with barcode and expectedQty)
        if (sscanf(line, "%d,%49[^,],%15[^,],%d,%d,%f,%f,%d,%d,%d,%f,%d",
                   &id, name, barcode, &qty, &expQty, &price, &cost, &maxS, &minS, &inTrans, &demand, &actFlag) == 12)
        {
            inventory[count].id = id;
            strncpy(inventory[count].name, name, 49);
            strncpy(inventory[count].barcode, barcode, 15);
            inventory[count].quantity = qty;
            inventory[count].expectedQty = expQty;
            inventory[count].price = price;
            inventory[count].orderCost = cost;
            inventory[count].maxStock = maxS;
            inventory[count].minStock = minS;
            inventory[count].inTransit = 0;
            inventory[count].baseDemandRate = (demand <= 0.05f) ? 0.30f : demand;
            inventory[count].saleTimer = 0.0f;
            inventory[count].isActive = (actFlag == 1);
            count++;
        }
        // Fallback: legacy 10-column format
        else if (sscanf(line, "%d,%49[^,],%d,%f,%f,%d,%d,%d,%f,%d",
                        &id, name, &qty, &price, &cost, &maxS, &minS, &inTrans, &demand, &actFlag) == 10)
        {
            inventory[count].id = id;
            strncpy(inventory[count].name, name, 49);
            GenerateSimulatedBarcode(id, name, inventory[count].barcode);
            inventory[count].quantity = qty;
            inventory[count].expectedQty = qty;
            inventory[count].price = price;
            inventory[count].orderCost = cost;
            inventory[count].maxStock = maxS;
            inventory[count].minStock = minS;
            inventory[count].inTransit = 0;
            inventory[count].baseDemandRate = (demand <= 0.05f) ? 0.30f : demand;
            inventory[count].saleTimer = 0.0f;
            inventory[count].isActive = (actFlag == 1);
            count++;
        }
    }
    fclose(file);
    return count;
}

void SeedDefaultInventory(InventoryItem inventory[], int *count)
{
    *count = 6;
    const char *names[6] = {
        "RTX-4080 Core", "DDR5-64GB Kit", "NVMe 2TB Gen5",
        "850W Gold PSU", "AM5 Logic Board", "Liquid Cooler"};
    int qtys[6] = {20, 24, 26, 20, 18, 22};
    float prices[6] = {545.0f, 165.0f, 130.0f, 118.0f, 215.0f, 105.0f};
    float costs[6] = {380.0f, 110.0f, 88.0f, 78.0f, 145.0f, 72.0f};
    int maxS[6] = {32, 36, 40, 32, 28, 34};
    int minS[6] = {10, 12, 13, 10, 9, 11};
    float dem[6] = {0.25f, 0.32f, 0.36f, 0.28f, 0.24f, 0.30f};

    for (int i = 0; i < 6; i++)
    {
        inventory[i].id = i + 1;
        strncpy(inventory[i].name, names[i], 49);
        GenerateSimulatedBarcode(i + 1, names[i], inventory[i].barcode);
        inventory[i].quantity = qtys[i];
        inventory[i].expectedQty = qtys[i];
        inventory[i].price = prices[i];
        inventory[i].orderCost = costs[i];
        inventory[i].maxStock = maxS[i];
        inventory[i].minStock = minS[i];
        inventory[i].inTransit = 0;
        inventory[i].baseDemandRate = dem[i];
        inventory[i].saleTimer = 0.0f;
        inventory[i].isActive = true;
    }
    SaveInventory(inventory, *count);
}

int GetActiveSkuCount(const InventoryItem inventory[])
{
    int c = 0;
    for (int i = 0; i < MAX_ITEMS; i++)
    {
        if (inventory[i].isActive)
            c++;
    }
    return c;
}

int PickRandomActiveInventoryIndex(const InventoryItem inventory[])
{
    int activeIdx[MAX_ITEMS];
    int count = 0;
    for (int i = 0; i < MAX_ITEMS; i++)
    {
        if (inventory[i].isActive)
            activeIdx[count++] = i;
    }
    if (count == 0)
        return 0;
    return activeIdx[GetRandomValue(0, count - 1)];
}

int GetActiveSlotForInventoryIndex(const InventoryItem inventory[], int invIdx)
{
    int slot = 0;
    for (int i = 0; i < MAX_ITEMS; i++)
    {
        if (inventory[i].isActive)
        {
            if (i == invIdx)
                return slot;
            slot++;
        }
    }
    return 0;
}

Rectangle GetShelfBayRect(int activeSlot, int totalActiveSkus)
{
    if (activeSlot < 0)
        activeSlot = 0;
    bool twoAisleMode = (totalActiveSkus > 6);
    if (!twoAisleMode)
    {
        int row = activeSlot % 6;
        return (Rectangle){764.0f, 320.0f + row * 98.0f, 156.0f, 86.0f};
    }
    else
    {
        int col = (activeSlot / 6) % 2;
        int row = activeSlot % 6;
        float x = (col == 0) ? 743.0f : 925.0f;
        return (Rectangle){x, 320.0f + row * 98.0f, 130.0f, 86.0f};
    }
}

// =============================================================================
// STAFF DISCREPANCY AUDIT LOG PERSISTENCE
// =============================================================================

AuditRecord *GetAuditLogs(int *outCount)
{
    if (!auditLoaded)
        LoadAuditLogs();
    if (outCount != NULL)
        *outCount = auditLogCount;
    return auditLogs;
}

void SaveAuditLogs(void)
{
    FILE *f = fopen("audit_log.csv", "w");
    if (f == NULL)
        return;
    for (int i = 0; i < auditLogCount; i++)
    {
        fprintf(f, "%d,%s,%s,%s,%d,%d,%d,%.2f,%s,%.1f\n",
                auditLogs[i].auditId,
                auditLogs[i].staffName,
                auditLogs[i].itemName,
                auditLogs[i].barcode,
                auditLogs[i].expectedQty,
                auditLogs[i].countedQty,
                auditLogs[i].discrepancy,
                auditLogs[i].financialImpact,
                auditLogs[i].reason,
                auditLogs[i].timestamp);
    }
    fclose(f);
}

void LoadAuditLogs(void)
{
    auditLoaded = true;
    auditLogCount = 0;
    FILE *f = fopen("audit_log.csv", "r");
    if (f == NULL)
        return;

    char line[512];
    while (auditLogCount < MAX_AUDIT_LOGS && fgets(line, sizeof(line), f) != NULL)
    {
        AuditRecord *r = &auditLogs[auditLogCount];
        if (sscanf(line, "%d,%31[^,],%49[^,],%15[^,],%d,%d,%d,%f,%31[^,],%f",
                   &r->auditId, r->staffName, r->itemName, r->barcode,
                   &r->expectedQty, &r->countedQty, &r->discrepancy,
                   &r->financialImpact, r->reason, &r->timestamp) == 10)
        {
            auditLogCount++;
        }
    }
    fclose(f);
}

void RecordStaffAudit(const char *staffName, InventoryItem *item, const char *reason, float simTime)
{
    if (item == NULL)
        return;
    if (!auditLoaded)
        LoadAuditLogs();

    if (auditLogCount >= MAX_AUDIT_LOGS)
    {
        for (int i = 0; i < MAX_AUDIT_LOGS - 1; i++)
        {
            auditLogs[i] = auditLogs[i + 1];
        }
        auditLogCount = MAX_AUDIT_LOGS - 1;
    }

    AuditRecord *rec = &auditLogs[auditLogCount];
    rec->auditId = (auditLogCount > 0) ? (auditLogs[auditLogCount - 1].auditId + 1) : 1001;
    strncpy(rec->staffName, staffName ? staffName : "Staff", 31);
    rec->staffName[31] = '\0';
    strncpy(rec->itemName, item->name, 49);
    rec->itemName[49] = '\0';
    strncpy(rec->barcode, item->barcode, 15);
    rec->barcode[15] = '\0';
    rec->expectedQty = item->expectedQty;
    rec->countedQty = item->quantity;
    rec->discrepancy = item->quantity - item->expectedQty;
    rec->financialImpact = (float)rec->discrepancy * item->orderCost;
    strncpy(rec->reason, reason ? reason : "Stock Audit", 31);
    rec->reason[31] = '\0';
    rec->timestamp = simTime;

    auditLogCount++;
    // Reconcile book stock to match physical counted stock
    item->expectedQty = item->quantity;
    SaveAuditLogs();
}

// =============================================================================
// SHOPIFY / ONLINE STORE JSON INVENTORY SCRAPER
// =============================================================================

static void CleanScrapedProductTitle(const char *raw, char *out, int maxLen)
{
    int w = 0;
    for (int i = 0; raw[i] != '\0' && w < maxLen - 1; i++)
    {
        unsigned char c = (unsigned char)raw[i];
        if (c == '\\' && raw[i + 1] == 'u')
        {
            i += 5; // skip unicode escape \uXXXX
            continue;
        }
        if (c == ',' || c == '"' || c == '\\')
            c = ' ';
        if (c >= 32 && c <= 125)
        {
            out[w++] = (char)c;
        }
    }
    out[w] = '\0';
}

int ScrapeOnlineStoreInventory(InventoryItem inventory[], int *itemCount, const char *customUrl, int presetIdx, char *outStatusMsg, int statusMaxLen)
{
    char targetUrl[256] = {0};

    if (presetIdx == 1)
    {
        strcpy(targetUrl, "https://www.keychron.com/products.json?limit=12");
    }
    else if (presetIdx == 2)
    {
        strcpy(targetUrl, "https://dummyjson.com/products?limit=12");
    }
    else if (presetIdx == 0 && customUrl != NULL && customUrl[0] != '\0')
    {
        if (strncmp(customUrl, "http://", 7) == 0 || strncmp(customUrl, "https://", 8) == 0)
        {
            strncpy(targetUrl, customUrl, 250);
        }
        else
        {
            snprintf(targetUrl, sizeof(targetUrl), "https://%s", customUrl);
        }
        // Automatically append /products.json?limit=12 if it's a bare Shopify domain
        if (strstr(targetUrl, ".json") == NULL && strstr(targetUrl, "dummyjson.com") == NULL && strstr(targetUrl, "fakestoreapi.com") == NULL)
        {
            int len = (int)strlen(targetUrl);
            if (len > 0 && targetUrl[len - 1] == '/')
                targetUrl[len - 1] = '\0';
            strncat(targetUrl, "/products.json?limit=12", sizeof(targetUrl) - strlen(targetUrl) - 1);
        }
    }

    int scrapedCount = 0;
    char scrapedNames[MAX_STORE_SKUS][50];
    float scrapedPrices[MAX_STORE_SKUS];

    // 1. Attempt live HTTP scrape via Windows curl.exe if a live URL is selected
    if (targetUrl[0] != '\0')
    {
        remove("shopify_scrape.json");
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "curl -sL -A \"Mozilla/5.0\" --max-time 5 \"%s\" -o shopify_scrape.json", targetUrl);
        int rc = system(cmd);

        if (rc == 0)
        {
            FILE *fp = fopen("shopify_scrape.json", "rb");
            if (fp != NULL)
            {
                fseek(fp, 0, SEEK_END);
                long fsize = ftell(fp);
                fseek(fp, 0, SEEK_SET);

                if (fsize > 32 && fsize < 2 * 1024 * 1024)
                {
                    char *json = (char *)malloc((size_t)fsize + 1);
                    if (json != NULL)
                    {
                        size_t readBytes = fread(json, 1, (size_t)fsize, fp);
                        json[readBytes] = '\0';

                        const char *cursor = json;
                        while (scrapedCount < MAX_STORE_SKUS && (cursor = strstr(cursor, "\"title\"")) != NULL)
                        {
                            cursor += 7;
                            const char *colon = strchr(cursor, ':');
                            if (!colon)
                                break;
                            const char *q1 = strchr(colon, '"');
                            if (!q1)
                                break;
                            q1++;
                            char rawTitle[80] = {0};
                            int ti = 0;
                            while (q1[ti] != '\0' && q1[ti] != '"' && ti < 75)
                            {
                                if (q1[ti] == '\\' && q1[ti + 1] != '\0')
                                    ti++;
                                rawTitle[ti] = q1[ti];
                                ti++;
                            }
                            rawTitle[ti] = '\0';
                            cursor = q1 + ti;

                            // Skip generic variant titles in Shopify JSON ("Default Title")
                            if (strcmp(rawTitle, "Default Title") == 0 || strlen(rawTitle) < 3)
                                continue;

                            // Find the nearest "price" field for this product
                            float foundPrice = 85.0f + (float)(scrapedCount * 18);
                            const char *pPos = strstr(cursor, "\"price\"");
                            if (pPos != NULL && (pPos - cursor) < 4000)
                            {
                                const char *pColon = strchr(pPos + 7, ':');
                                if (pColon != NULL)
                                {
                                    pColon++;
                                    while (*pColon == ' ' || *pColon == '"')
                                        pColon++;
                                    float parsedP = (float)atof(pColon);
                                    if (parsedP >= 5.0f && parsedP <= 3000.0f)
                                        foundPrice = parsedP;
                                    else if (parsedP > 0.5f && parsedP < 5.0f)
                                        foundPrice = parsedP * 25.0f;
                                }
                            }

                            CleanScrapedProductTitle(rawTitle, scrapedNames[scrapedCount], 28);
                            if (strlen(scrapedNames[scrapedCount]) >= 3)
                            {
                                scrapedPrices[scrapedCount] = foundPrice;
                                scrapedCount++;
                            }
                        }
                        free(json);
                    }
                }
                fclose(fp);
            }
        }
    }

    // 2. If live scrape succeeded, apply scraped catalog
    if (scrapedCount >= 3)
    {
        for (int i = 0; i < MAX_ITEMS; i++)
            inventory[i].isActive = false;

        for (int i = 0; i < scrapedCount; i++)
        {
            inventory[i].id = i + 1;
            strncpy(inventory[i].name, scrapedNames[i], 49);
            inventory[i].name[49] = '\0';
            GenerateSimulatedBarcode(i + 1, inventory[i].name, inventory[i].barcode);
            inventory[i].price = scrapedPrices[i];
            inventory[i].orderCost = roundf((scrapedPrices[i] / BASE_FAIR_MARGIN_MULT) * 100.0f) / 100.0f;
            if (inventory[i].orderCost < 5.0f)
                inventory[i].orderCost = 15.0f;
            inventory[i].maxStock = 28 + (i % 4) * 4;
            inventory[i].minStock = (int)(inventory[i].maxStock * 0.33f);
            inventory[i].quantity = inventory[i].minStock + 6;
            inventory[i].expectedQty = inventory[i].quantity;
            inventory[i].inTransit = 0;
            inventory[i].baseDemandRate = 0.24f + (float)(i % 4) * 0.04f;
            inventory[i].saleTimer = 0.0f;
            inventory[i].isActive = true;
        }
        *itemCount = scrapedCount;
        SaveInventory(inventory, *itemCount);
        if (outStatusMsg)
        {
            snprintf(outStatusMsg, (size_t)statusMaxLen, "LIVE SCRAPE SUCCESS: Imported %d SKUs + EAN-13 Barcodes from %s", scrapedCount, targetUrl);
        }
        return scrapedCount;
    }

    // 3. Offline / Instant Store Presets Fallback (Guaranteed Demo Reliability)
    const char *preset1Names[10] = {
        "Keychron Q1 Pro Wireless", "Keychron K2 HE Rapid", "Lemokey P1 Pro Custom",
        "Keychron M3 Mini Mouse", "PBT Double-Shot Keycaps", "Gateron Jupiter Switch",
        "Coiled Aviator Cable", "Walnut Keyboard Rest", "Acoustic Foam Upgrade", "Keychron Desk Mat XL"};
    float preset1Prices[10] = {215.0f, 145.0f, 139.0f, 69.0f, 45.0f, 38.0f, 29.0f, 34.0f, 24.0f, 32.0f};

    const char *preset2Names[10] = {
        "Apple AirPods Max Silver", "MacBook Pro M3 14-Inch", "Samsung Galaxy S24 Ultra",
        "Sony WH-1000XM5 Audio", "iPad Air M2 256GB WiFi", "Logitech MX Master 3S",
        "Kindle Paperwhite 16GB", "Beats Studio Pro ANC", "GoPro HERO12 Black", "DJI Mini 4 Pro Drone"};
    float preset2Prices[10] = {549.0f, 1599.0f, 1199.0f, 349.0f, 649.0f, 109.0f, 159.0f, 299.0f, 399.0f, 759.0f};

    const char *preset3Names[8] = {
        "Aero Knit Runner V2", "Oversized Heavy Hoodie", "Cargo Tech Parachute",
        "Merino Wool Beanie", "Retro Court Low 85", "Waterproof Shell Parka",
        "Crossbody Utility Bag", "Selvedge Denim Jacket"};
    float preset3Prices[8] = {165.0f, 110.0f, 128.0f, 48.0f, 145.0f, 240.0f, 78.0f, 195.0f};

    const char **chosenNames = preset1Names;
    float *chosenPrices = preset1Prices;
    int chosenCount = 10;
    const char *presetLabel = "Shopify Tech & Mechanical Keyboards Store";

    if (presetIdx == 2)
    {
        chosenNames = preset2Names;
        chosenPrices = preset2Prices;
        chosenCount = 10;
        presetLabel = "DummyJSON Global Electronics Store";
    }
    else if (presetIdx == 3)
    {
        chosenNames = preset3Names;
        chosenPrices = preset3Prices;
        chosenCount = 8;
        presetLabel = "Shopify Streetwear & Apparel Store";
    }
    else if (presetIdx == 4)
    {
        for (int i = 0; i < MAX_ITEMS; i++)
            inventory[i].isActive = false;
        SeedDefaultInventory(inventory, itemCount);
        if (outStatusMsg)
        {
            snprintf(outStatusMsg, (size_t)statusMaxLen, "RESTORED DEFAULT: Loaded 6 NDAL PC Hardware SKUs with EAN-13 Barcodes.");
        }
        return *itemCount;
    }

    for (int i = 0; i < MAX_ITEMS; i++)
        inventory[i].isActive = false;

    for (int i = 0; i < chosenCount; i++)
    {
        inventory[i].id = i + 1;
        strncpy(inventory[i].name, chosenNames[i], 49);
        GenerateSimulatedBarcode(i + 1, inventory[i].name, inventory[i].barcode);
        inventory[i].price = chosenPrices[i];
        inventory[i].orderCost = roundf((chosenPrices[i] / BASE_FAIR_MARGIN_MULT) * 100.0f) / 100.0f;
        inventory[i].maxStock = 28 + (i % 4) * 4;
        inventory[i].minStock = (int)(inventory[i].maxStock * 0.33f);
        inventory[i].quantity = inventory[i].minStock + 6;
        inventory[i].expectedQty = inventory[i].quantity;
        inventory[i].inTransit = 0;
        inventory[i].baseDemandRate = 0.24f + (float)(i % 4) * 0.04f;
        inventory[i].saleTimer = 0.0f;
        inventory[i].isActive = true;
    }
    *itemCount = chosenCount;
    SaveInventory(inventory, *itemCount);

    if (outStatusMsg)
    {
        snprintf(outStatusMsg, (size_t)statusMaxLen, "IMPORTED STORE CATALOG: %d SKUs + Barcodes loaded from %s", chosenCount, presetLabel);
    }
    return chosenCount;
}