//
// Created by barry on 11/11/15.
//

#ifndef DISTANCE_VECTOR_ROUTING_PROTOCOL_IMPLEMENTATION_USER_INTERFACE_H
#define DISTANCE_VECTOR_ROUTING_PROTOCOL_IMPLEMENTATION_USER_INTERFACE_H

#include "main.h"

int handleCommand(context * nodeContext, char *command);
int displayRoutingTable(context *nodeContext);
int printDistanceMatrix(context *nodeContext);
#endif //DISTANCE_VECTOR_ROUTING_PROTOCOL_IMPLEMENTATION_USER_INTERFACE_H
