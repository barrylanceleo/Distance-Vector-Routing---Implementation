//
// Created by barry on 11/4/15.
//

#ifndef DISTANCE_VECTOR_ROUTING_PROTOCOL_IMPLEMENTATION_SERVER_H
#define DISTANCE_VECTOR_ROUTING_PROTOCOL_IMPLEMENTATION_SERVER_H

#include <stdint.h>
#include "main.h"
#define INFINITY -1 //since we are using unsigned int, this will be the largest number possible
#define UNDEFINED -1
#define STDIN 0
typedef struct routing_table_row
{
    uint16_t id;
    char *ipAddress;
    uint16_t port;
    uint16_t cost;
    uint16_t next_hop_id;
} routing_table_row;

typedef struct neighbour
{
    uint16_t id;
    uint16_t cost;
    int timeoutFD; //this expires when a routing update is not received
                    // from the neighbour for 3 routing intervals
} neighbour;


int runServer(char *topology_file_name, context *nodeContext);
int sendRoutingUpdate(context *nodeContext);
int updateLinkCost(context *nodeContext, uint16_t destination_id, uint16_t new_cost);
int printDistanceMatrix(context *nodeContext);
#endif //DISTANCE_VECTOR_ROUTING_PROTOCOL_IMPLEMENTATION_SERVER_H
