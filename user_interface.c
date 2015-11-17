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
            printf("%10s %10s %10s\n", "Destination", "Next_Hop", "Cost");
        do {
            //for each routing_table_row
            routing_table_row *currentRow = (routing_table_row *) currentItem->item;
            if(currentRow->cost != INFINITY)
            {
                printf("%10u %10u %10u\n", currentRow->id,
                       currentRow->next_hop_id, currentRow->cost);
            }
            else{
//                printf("%10u %10s %10s\n", currentRow->id,
//                              "Undefined", "Infinity");
            }
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

    //printf("commandLength: %d\n", commandLength);

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
//    //print the commands
//    for(i = 0; i<commandLength; i++)
//    {
//        printf("-%s-\n", commandParts[i]);
//    }

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
            printf("update ERROR: You can modify only the cost of links which have an end at this node.\n");
            return -1;
        }
        if((status = updateLinkCost(nodeContext, destination_id, new_cost))!=0)
        {
            printf("update ERROR: Unable to modify the Link Cost.\n");
            return -1;
        }
        else
        {
            printf("update SUCCESS.\n");
        }
        //printDistanceMatrix(nodeContext);
    }
    else if (commandLength == 1 && strcmp(commandParts[0], "packets") == 0)
    {
        printf("packets SUCCESS %d.\n", nodeContext->received_packet_counter);
        nodeContext->received_packet_counter = 0;
    }
    else if (commandLength == 2 && strcmp(commandParts[0], "disable") == 0)
    {
        uint16_t server_id;
        if((server_id = atoi(commandParts[0])) <= 0)
        {
            printf("disable ERROR invalid server_id.\n");
        }
        else{

            if((status = disableLinkToNode(nodeContext, server_id)) == 0)
            {
                printf("disable SUCCESS.\n");
            }
            else if (status == -2){
                printf("disable ERROR given server_id is not a neighbour so there is no direct link to disable.\n");
            }
            else
            {
                printf("disable ERROR neighbour link removed but routing table not updated.This is bad.\n");
            }
        }
    }
    else if (commandLength == 1 && strcmp(commandParts[0], "crash") == 0)
    {
        simulateNodeCrash(nodeContext);
        printf("crash SUCCESS this node will not send/receive routing updates further.\n");
    }
    return 0;
}