//
// Created by barry on 11/4/15.
//

#ifndef DISTANCE_VECTOR_ROUTING_PROTOCOL_IMPLEMENTATION_MAIN_H
#define DISTANCE_VECTOR_ROUTING_PROTOCOL_IMPLEMENTATION_MAIN_H
#include "list.h"
#define LINE_BUFFER 1000
//253 is the maximum length of domain name
#define HOST_NAME_SIZE 253
#define IP_ADDR_LEN 15
//2147483647 is the biggest 4-byte int
typedef struct context
{
    int routing_update_interval;
    int num_nodes;
    char *myHostName;
    char *myIPAddress;
    int myPort;
    int myId;
    int mySockFD;
    list *routing_table;
    list *neighbourList;
    uint16_t **distance_matrix;
} context;


#endif //DISTANCE_VECTOR_ROUTING_PROTOCOL_IMPLEMENTATION_MAIN_H
