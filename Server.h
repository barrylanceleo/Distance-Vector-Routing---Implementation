//
// Created by barry on 11/4/15.
//

#ifndef DISTANCE_VECTOR_ROUTING_PROTOCOL_IMPLEMENTATION_SERVER_H
#define DISTANCE_VECTOR_ROUTING_PROTOCOL_IMPLEMENTATION_SERVER_H

#include <stdint.h>
#include "main.h"

#define STDIN 0
typedef struct node
{
    uint16_t id;
    char *ipAddress;
    int port;
    uint16_t cost;
} node;


typedef struct neighbour
{
    int id;
    int timeoutFD; //this expires when a routing update is not received
                    // from the neighbour for 3 routing intervals
} neighbour;


int runServer(char *topology_file_name, context *nodeContext);

#endif //DISTANCE_VECTOR_ROUTING_PROTOCOL_IMPLEMENTATION_SERVER_H
