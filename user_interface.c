//
// Created by barry on 11/11/15.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "user_interface.h"
#include "Server.h"

int displayRoutingTable(context *nodeContext)
{
    listItem *currentItem = nodeContext->routing_table;
    if (currentItem == NULL) {
        printf("Routing table is empty.\n");
        return -1;
    }
    else {
        do {
            //for each routing_table_row
            routing_table_row *currentRow = (routing_table_row *) currentItem->item;
            printf("%u %u %u\n", currentRow->id,
                   currentRow->next_hop_id, currentRow->cost);
            currentItem = currentItem->next;
        }while (currentItem != NULL);
        return 0;
    }
}




int handleCommand(context * nodeContext, char *command) {

    //remove the last \n in command if present
    if (command[strlen(command) - 1] == '\n')
        command[strlen(command) - 1] = '\0';

    //split the command into parts

    //find the commandLength
    char delimiter = ' ';
    int i = 0, commandLength = 0;
    while(command[i] != '\0')
    {
        if(command[i] != delimiter)
        {
            commandLength++;
            //traverse till you find a delimiter
            do{
                i++;
            }while(command[i] != delimiter && command[i] != '\0');
        }
        else{
            i++;
        }
    }

    printf("commandLength: %d\n", commandLength);

    //create the commandParts array and update it
    char *commandParts[commandLength];
    i = 0;
    char *commandPart = strtok(command, " ");
    commandParts[i] = commandPart;
    i++;
    while(commandPart != NULL )
    {
        commandPart = strtok(NULL, " ");
        commandParts[i] = commandPart;
        i++;
    }

    int status;
    //print the commands
    for(i = 0; i<commandLength; i++)
    {
        printf("-%s-\n", commandParts[i]);
    }

    if(commandLength == 1 && strcmp(commandParts[0], "display") == 0)
    {
        printf("Routing Table:\n");
        displayRoutingTable(nodeContext);
    }
    else if (commandLength == 1 && strcmp(commandParts[0], "step") == 0)
    {
        if((status = sendRoutingUpdate(nodeContext))!=0)
        {
            fprintf(stderr, "Error Sending routing update.\n");
        }
    }
    else if (commandLength == 4 && strcmp(commandParts[0], "update") == 0)
    {
        uint16_t new_cost;
        uint16_t source_id = atoi(commandParts[1]);
        uint16_t destination_id = atoi(commandParts[2]);
        if(strcmp(commandParts[3], "inf") == 0)
        {
            new_cost = INFINITY;
        }
        else
        {
            new_cost = atoi(commandParts[3]);
        }

        //make the first id as the source id
        if(destination_id == nodeContext->myId)
        {
            //swap
            uint16_t temp = destination_id;
            destination_id = source_id;
            source_id = temp;
        }
        else if(source_id != nodeContext->myId)
        {
            printf("You can modify only the cost of links connected to this node.\n");
            return -1;
        }

        int status;
        if((status = updateLinkCost(nodeContext, destination_id, new_cost))!=0)
        {
            fprintf(stderr, "Error updating Link Cost.\n");
            return -1;
        }

        printDistanceMatrix(nodeContext);

    }

    return 0;
}