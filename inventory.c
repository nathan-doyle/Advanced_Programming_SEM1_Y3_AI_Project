#include "inventory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void SaveInventory(InventoryItem inventory[], int count)
{
    FILE *file = fopen("inventory_db.csv", "w");
    if (file == NULL)
        return;
    for (int i = 0; i < count; i++)
    {
        fprintf(file, "%d,%s,%d,%.2f,%.2f,%d,%d,%d,%d\n",
                inventory[i].id, inventory[i].name,
                inventory[i].quantity, inventory[i].price,
                inventory[i].orderCost, inventory[i].maxStock,
                inventory[i].minStock, inventory[i].inTransit,
                inventory[i].isActive ? 1 : 0);
    }
    fclose(file);
}

int LoadInventory(InventoryItem inventory[])
{
    FILE *file = fopen("inventory_db.csv", "r");
    if (file == NULL)
        return 0;

    int count = 0;
    int activeFlag = 0;

    while (count < MAX_ITEMS && fscanf(file, "%d,%49[^,],%d,%f,%f,%d,%d,%d,%d\n",
                                       &inventory[count].id, inventory[count].name,
                                       &inventory[count].quantity, &inventory[count].price,
                                       &inventory[count].orderCost, &inventory[count].maxStock,
                                       &inventory[count].minStock, &inventory[count].inTransit,
                                       &activeFlag) == 9)
    {

        inventory[count].isActive = (activeFlag == 1);
        count++;
    }
    fclose(file);
    return count;
}

void SaveOrders(OrderRecord orders[], int count)
{
    FILE *file = fopen("orders_db.csv", "w");
    if (file == NULL)
        return;
    for (int i = 0; i < count; i++)
    {
        fprintf(file, "%d,%d,%.2f,%.2f,%d,%d",
                orders[i].orderId, orders[i].itemCount, orders[i].totalCost,
                orders[i].estimatedProfit, (int)orders[i].status, orders[i].isActive ? 1 : 0);
        for (int j = 0; j < orders[i].itemCount; j++)
        {
            fprintf(file, ",%d,%d", orders[i].items[j].itemId, orders[i].items[j].orderQuantity);
        }
        fprintf(file, "\n");
    }
    fclose(file);
}

int LoadOrders(OrderRecord orders[])
{
    FILE *file = fopen("orders_db.csv", "r");
    if (file == NULL)
        return 0;
    int count = 0;
    char line[2048];
    while (count < MAX_ORDERS && fgets(line, sizeof(line), file))
    {
        char *token = strtok(line, ",");
        if (!token)
            continue;
        orders[count].orderId = atoi(token);
        token = strtok(NULL, ",");
        if (!token)
            continue;
        orders[count].itemCount = atoi(token);
        token = strtok(NULL, ",");
        if (!token)
            continue;
        orders[count].totalCost = atof(token);
        token = strtok(NULL, ",");
        if (!token)
            continue;
        orders[count].estimatedProfit = atof(token);
        token = strtok(NULL, ",");
        if (!token)
            continue;
        orders[count].status = (OrderStatus)atoi(token);
        token = strtok(NULL, ",");
        if (!token)
            continue;
        orders[count].isActive = (atoi(token) == 1);

        for (int j = 0; j < orders[count].itemCount; j++)
        {
            token = strtok(NULL, ",");
            if (!token)
                break;
            orders[count].items[j].itemId = atoi(token);
            token = strtok(NULL, ",");
            if (!token)
                break;
            orders[count].items[j].orderQuantity = atoi(token);
        }
        count++;
    }
    fclose(file);
    return count;
}