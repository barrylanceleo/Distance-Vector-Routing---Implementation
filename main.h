//
// Created by barry on 11/4/15.
//

#ifndef DISTANCE_VECTOR_ROUTING_PROTOCOL_IMPLEMENTATION_MAIN_H
#define DISTANCE_VECTOR_ROUTING_PROTOCOL_IMPLEMENTATION_MAIN_H
#include <sys/select.h>
#include "list.h"
#define LINE_BUFFER 1000
//253 is the maximum length of domain name
#define HOST_NAME_SIZE 253
#define IP_ADDR_LEN 15
typedef struct context
{
    int routing_update_interval;
    int num_nodes;
    char *myHostName;
    char *myIPAddress;
    uint16_t myPort;
    uint16_t myId;
    uint16_t myDVIndex;
    int mySockFD;
    list *routing_table;
    list *neighbourList;
    uint16_t **distance_matrix;
    int received_packet_counter; //to support the "packets" function
    fd_set FDList; //master file descriptor list to hold all sockets and stdin
    int FDmax; //to hold the max file descriptor value
} context;


#endif //DISTANCE_VECTOR_ROUTING_PROTOCOL_IMPLEMENTATION_MAIN_H
