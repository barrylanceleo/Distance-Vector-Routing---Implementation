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
#define INFINITY 65535
typedef struct context
{
    int routing_update_interval;
    char *myHostName;
    char *myIPAddress;
    int myPort;
    int myId;
    int mySockFD;
    list *nodesList;
    list *neighbourList;
} context;


#endif //DISTANCE_VECTOR_ROUTING_PROTOCOL_IMPLEMENTATION_MAIN_H
