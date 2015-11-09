//
// Created by barry on 11/4/15.
//

#ifndef DISTANCE_VECTOR_ROUTING_PROTOCOL_IMPLEMENTATION_SERVER_H
#define DISTANCE_VECTOR_ROUTING_PROTOCOL_IMPLEMENTATION_SERVER_H
#define STDIN 0
typedef struct node
{
    int id;
    char *ipAddress;
    int port;
    int cost;
} node;

int runServer(char *topology_file_name, int routing_update_interval);

#endif //DISTANCE_VECTOR_ROUTING_PROTOCOL_IMPLEMENTATION_SERVER_H
