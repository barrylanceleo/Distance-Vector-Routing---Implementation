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
            if(currentRow->cost != INFINITY && currentRow->cost != 0)
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

int printDistanceMatrix(context *nodeContext)
{
    printf("Distance Matrix:\n");
    int i,j;
    for(i=0; i < nodeContext->num_nodes; i++)
    {
        for(j=0; j < nodeContext->num_nodes; j++)
        {
            if(nodeContext->distance_matrix[i][j] == INFINITY)
            {
                printf("%s\t\t", "inf");
            }
            else
            {
                printf("%u\t\t", nodeContext->distance_matrix[i][j]);
            }
        }
        printf("\n");
    }
    return 0;
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

    if(commandLength == 1 && strcasecmp(commandParts[0], "help") == 0)
    {
        printf("List of commands:\n"
                       "1.help\n2.display\n3.step\n4.update\n5.packets\n6.disable\n6.crash\n");
    }
    else if(commandLength == 1 && strcasecmp(commandParts[0], "display") == 0)
    {
        printf("Routing Table:\n");
        displayRoutingTable(nodeContext);
    }
    else if (commandLength == 1 && strcasecmp(commandParts[0], "step") == 0)
    {
        if((status = sendRoutingUpdate(nodeContext))!=0)
        {
            if(status == -3)
            {
                printf("step SUCCESS There are no neighbours to send routing update to.\n");
            }
            else
            {
                fprintf(stderr, "step ERROR unable to send routing update.\n");
            }
        }
        else
        {
            printf("step SUCCESS.\n");
        }
    }
    else if (commandLength == 4 && strcasecmp(commandParts[0], "update") == 0)
    {
        uint16_t new_cost;
        uint16_t source_id = atoi(commandParts[1]);
        uint16_t destination_id = atoi(commandParts[2]);
        if(strcasecmp(commandParts[3], "inf") == 0)
        {
            new_cost = INFINITY;
        }
        else
        {
            new_cost = atoi(commandParts[3]);
        }

        if(new_cost <= 0 || new_cost > INFINITY)
        {
            printf("update ERROR: The link cost needs to between 1 and %d.\n"
                           "This is limited by the 2 byte cost variable size.\n", INFINITY-1);
            return -1;
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
            if(status == -2)
            {
                printf("update ERROR: Given destination server_id doesn't belong to the network.\n");
            }
            else
            {
                printf("update ERROR\n");
            }
            return -1;
        }
        else
        {
            printf("update SUCCESS. Updated %u -- %u with cost %u.\n", source_id, destination_id, new_cost);
        }
    }
    else if (commandLength == 1 && strcasecmp(commandParts[0], "packets") == 0)
    {
        printf("packets SUCCESS %d.\n", nodeContext->received_packet_counter);
        nodeContext->received_packet_counter = 0;
    }
    else if (commandLength == 2 && strcasecmp(commandParts[0], "disable") == 0)
    {
        uint16_t server_id;
        if((server_id = atoi(commandParts[1])) <= 0)
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
                //do  nothing
            }
        }
    }
    else if (commandLength == 1 && strcasecmp(commandParts[0], "crash") == 0)
    {
        simulateNodeCrash(nodeContext);
        printf("crash SUCCESS this node will not send/receive routing updates further.\n");
    }
    else
    {
        if(commandParts[0][0] = 0)
        {
            //do nothing;
        }

        printf("Unsupported Command. Enter \"help\" to the list of supported commands.\n");
    }
    return 0;
}