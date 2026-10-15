#ifndef EVENTS_H
#define EVENTS_H

#include <stdbool.h>
#include "inventory.h"

#define MAX_NOTIFICATIONS 6

typedef struct
{
    char message[120];
    float timer;
    int severity; // 0 = Info, 1 = Success, 2 = Chaos Alert, 3 = AI Dispatch
    bool isActive;
} SimNotification;

typedef struct
{
    float viralSurgeTimer;
    float weatherDelayTimer;
    float subsidyTimer;
    float energySpikeTimer;
    float wageSurgeTimer;
    float shrinkageAlarmTimer;
    float lightningFlashAlpha;
    int autoChaosMode; // 0 = OFF, 1 = LOW (14%), 2 = MED (25%), 3 = HIGH (40%)
    float randomEventTimer;
} ChaosEventState;

void InitEvents(void);
ChaosEventState *GetEventsState(void);
SimNotification *GetNotifications(void);
void PushNotification(const char *msg, int severity);
void UpdateNotificationTimers(float frameDt);
void UpdateEventsStep(float dt, InventoryItem inventory[], int itemCount, OrderRecord orders[], int orderCount);
void TriggerChaosEventById(int eventId, bool isAuto, InventoryItem inventory[], int itemCount, OrderRecord orders[], int orderCount);

#endif