#include "events.h"
#include "suppliers.h"
#include "ai.h"
#include "arena.h"
#include "raylib.h"
#include <string.h>
#include <math.h>

static ChaosEventState evState = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1, 0.0f};
static SimNotification notifications[MAX_NOTIFICATIONS];

void InitEvents(void)
{
    evState.viralSurgeTimer = 0.0f;
    evState.weatherDelayTimer = 0.0f;
    evState.subsidyTimer = 0.0f;
    evState.energySpikeTimer = 0.0f;
    evState.wageSurgeTimer = 0.0f;
    evState.shrinkageAlarmTimer = 0.0f;
    evState.lightningFlashAlpha = 0.0f;
    evState.autoChaosMode = 1;
    evState.randomEventTimer = 0.0f;

    for (int i = 0; i < MAX_NOTIFICATIONS; i++)
    {
        notifications[i].isActive = false;
    }
}

ChaosEventState *GetEventsState(void)
{
    return &evState;
}

SimNotification *GetNotifications(void)
{
    return notifications;
}

void PushNotification(const char *msg, int severity)
{
    for (int i = MAX_NOTIFICATIONS - 1; i > 0; i--)
    {
        notifications[i] = notifications[i - 1];
    }
    strncpy(notifications[0].message, msg, 119);
    notifications[0].message[119] = '\0';
    notifications[0].timer = 5.5f;
    notifications[0].severity = severity;
    notifications[0].isActive = true;
}

void UpdateNotificationTimers(float frameDt)
{
    for (int n = 0; n < MAX_NOTIFICATIONS; n++)
    {
        if (notifications[n].isActive)
        {
            notifications[n].timer -= frameDt;
            if (notifications[n].timer <= 0.0f)
                notifications[n].isActive = false;
        }
    }
}

void TriggerChaosEventById(int eventId, bool isAuto, InventoryItem inventory[], int itemCount, OrderRecord orders[], int orderCount)
{
    const char *prefix = isAuto ? "[AUTO-CHAOS] " : "[DIRECTOR] ";
    Color warnOrange = (Color){255, 195, 45, 255};
    Color barRed = (Color){255, 80, 100, 255};
    Color textGreen = (Color){55, 250, 155, 255};
    Supplier *suppliers = GetSuppliers();
    AiEconomyState *ai = GetAiEconomyState();

    switch (eventId)
    {
    case 0:
    {
        if (itemCount > 0)
        {
            int hits = 0;
            for (int k = 0; k < 3; k++)
            {
                int r = PickRandomActiveInventoryIndex(inventory);
                if (inventory[r].isActive)
                {
                    inventory[r].saleTimer = 20.0f;
                    hits++;
                }
            }
            if (hits > 0)
            {
                PushNotification(TextFormat("%sFlash Sale Frenzy! -12%% price, 2.2x demand (20s)!", prefix), 2);
                SpawnFloatingPopup(1060.0f, 350.0f, "FLASH SALE FRENZY 2.2x!", warnOrange);
            }
        }
        break;
    }
    case 1:
    {
        evState.viralSurgeTimer = 18.0f;
        for (int k = 0; k < 6 && itemCount > 0; k++)
        {
            int r = PickRandomActiveInventoryIndex(inventory);
            SpawnShopperAgent(r, 2, inventory[r].price * 1.5f, 0.0f, inventory[r].quantity == 0);
        }
        PushNotification(TextFormat("%sViral Demand Surge! Buying speed 2.5x (18s)!", prefix), 2);
        SpawnFloatingPopup(1060.0f, 380.0f, "VIRAL DEMAND SURGE 2.5x!", barRed);
        break;
    }
    case 2:
    {
        bool found = false;
        for (int o = 0; o < orderCount; o++)
        {
            if (orders[o].status != STATUS_DELIVERED && !orders[o].isDamaged)
            {
                orders[o].isDamaged = true;
                ai->customerSatisfaction -= 6.0f;
                if (ai->customerSatisfaction < 0.0f)
                    ai->customerSatisfaction = 0.0f;
                PushNotification(TextFormat("%sOrder #%d Damaged in Transit (-40%% cargo)!", prefix, orders[o].orderId), 2);
                SpawnFloatingPopup(820.0f, 215.0f, TextFormat("ORDER #%d DAMAGED (-40%%)!", orders[o].orderId), barRed);
                found = true;
                break;
            }
        }
        if (!found)
        {
            int s = GetRandomValue(0, MAX_SUPPLIERS - 1);
            suppliers[s].priceSpikeMult = 1.38f;
            suppliers[s].priceSpikeTimer = 22.0f;
            PushNotification(TextFormat("%s%s hiked prices +38%% (22s)!", prefix, suppliers[s].name), 2);
            SpawnFloatingPopup(820.0f, 215.0f, "SUPPLIER PRICE HIKE +38%!", warnOrange);
        }
        break;
    }
    case 3:
    {
        evState.weatherDelayTimer = 18.0f;
        evState.lightningFlashAlpha = 0.85f;
        PushNotification(TextFormat("%sSevere Weather! All deliveries slowed 55%% (18s)!", prefix), 2);
        break;
    }
    case 4:
    {
        int s1 = GetRandomValue(0, MAX_SUPPLIERS - 1);
        suppliers[s1].hasShortage = true;
        suppliers[s1].shortageTimer = 22.0f;
        if (!isAuto && GetRandomValue(0, 100) < 45)
        {
            int s2 = (s1 + 1) % MAX_SUPPLIERS;
            suppliers[s2].hasShortage = true;
            suppliers[s2].shortageTimer = 16.0f;
        }
        PushNotification(TextFormat("%sSupply Shortage! %s offline (22s)!", prefix, suppliers[s1].name), 2);
        SpawnFloatingPopup(540.0f, 260.0f, "SUPPLIER OFFLINE!", barRed);
        break;
    }
    case 5:
    {
        evState.energySpikeTimer = 24.0f;
        PushNotification(TextFormat("%sEnergy Tariff Spike! Utility bills 2.1x for 24s!", prefix), 2);
        SpawnFloatingPopup(1040.0f, 440.0f, "POWER TARIFF 2.1x!", warnOrange);
        break;
    }
    case 6:
    {
        evState.wageSurgeTimer = 24.0f;
        ai->customerSatisfaction -= 7.0f;
        if (ai->customerSatisfaction < 0.0f)
            ai->customerSatisfaction = 0.0f;
        PushNotification(TextFormat("%sStaff Overtime Dispute! Wages 1.85x & -7%% Satisfaction!", prefix), 2);
        SpawnFloatingPopup(1280.0f, 460.0f, "WAGE SURGE 1.85x (-7% SAT)", barRed);
        break;
    }
    case 7:
    {
        int totalLost = 0;
        for (int k = 0; k < 2 && itemCount > 0; k++)
        {
            int r = PickRandomActiveInventoryIndex(inventory);
            if (inventory[r].isActive && inventory[r].quantity > 2)
            {
                int lost = (int)ceilf(inventory[r].quantity * 0.24f);
                if (lost < 2)
                    lost = 2;
                if (lost > inventory[r].quantity)
                    lost = inventory[r].quantity;
                inventory[r].quantity -= lost;
                totalLost += lost;
            }
        }
        evState.shrinkageAlarmTimer = 5.0f;
        ai->customerSatisfaction -= 8.0f;
        if (ai->customerSatisfaction < 0.0f)
            ai->customerSatisfaction = 0.0f;
        SaveInventory(inventory, itemCount);
        PushNotification(TextFormat("%sWarehouse Shrinkage! -%d units lost & -8%% Satisfaction!", prefix, totalLost), 2);
        SpawnFloatingPopup(680.0f, 490.0f, TextFormat("SHRINKAGE -%d UNITS!", totalLost), barRed);
        break;
    }
    case 8:
    {
        evState.subsidyTimer = 20.0f;
        ai->customerSatisfaction += 12.0f;
        if (ai->customerSatisfaction > 100.0f)
            ai->customerSatisfaction = 100.0f;
        PushNotification(TextFormat("%sPR & Gov Relief: -20%% supplier costs & +12%% Satisfaction!", prefix), 1);
        SpawnFloatingPopup(980.0f, 360.0f, "PR BOOST +12% SAT & -20% COST!", textGreen);
        break;
    }
    }
}

void UpdateEventsStep(float dt, InventoryItem inventory[], int itemCount, OrderRecord orders[], int orderCount)
{
    if (evState.viralSurgeTimer > 0.0f)
    {
        evState.viralSurgeTimer -= dt;
        if (evState.viralSurgeTimer < 0.0f)
            evState.viralSurgeTimer = 0.0f;
    }
    if (evState.weatherDelayTimer > 0.0f)
    {
        evState.weatherDelayTimer -= dt;
        if (GetRandomValue(0, 100) < 3)
            evState.lightningFlashAlpha = 0.65f;
        if (evState.weatherDelayTimer < 0.0f)
            evState.weatherDelayTimer = 0.0f;
    }
    if (evState.lightningFlashAlpha > 0.0f)
    {
        evState.lightningFlashAlpha -= dt * 2.8f;
        if (evState.lightningFlashAlpha < 0.0f)
            evState.lightningFlashAlpha = 0.0f;
    }
    if (evState.subsidyTimer > 0.0f)
    {
        evState.subsidyTimer -= dt;
        if (evState.subsidyTimer < 0.0f)
            evState.subsidyTimer = 0.0f;
    }
    if (evState.energySpikeTimer > 0.0f)
    {
        evState.energySpikeTimer -= dt;
        if (evState.energySpikeTimer < 0.0f)
            evState.energySpikeTimer = 0.0f;
    }
    if (evState.wageSurgeTimer > 0.0f)
    {
        evState.wageSurgeTimer -= dt;
        if (evState.wageSurgeTimer < 0.0f)
            evState.wageSurgeTimer = 0.0f;
    }
    if (evState.shrinkageAlarmTimer > 0.0f)
    {
        evState.shrinkageAlarmTimer -= dt;
        if (evState.shrinkageAlarmTimer < 0.0f)
            evState.shrinkageAlarmTimer = 0.0f;
    }

    if (evState.autoChaosMode > 0)
    {
        evState.randomEventTimer += dt;
        if (evState.randomEventTimer >= 6.5f)
        {
            evState.randomEventTimer = 0.0f;
            int chanceThreshold = (evState.autoChaosMode == 1) ? 14 : ((evState.autoChaosMode == 2) ? 25 : 40);
            int roll = GetRandomValue(1, 100);
            if (roll <= chanceThreshold)
            {
                int evPick = GetRandomValue(0, 8);
                if (evPick == 8 && GetRandomValue(0, 100) < 65)
                    evPick = GetRandomValue(0, 7);
                TriggerChaosEventById(evPick, true, inventory, itemCount, orders, orderCount);
            }
        }
    }
}