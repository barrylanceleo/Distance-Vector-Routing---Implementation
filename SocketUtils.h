//
// Created by barry on 11/4/15.
//

#ifndef DISTANCE_VECTOR_ROUTING_PROTOCOL_IMPLEMENTATION_SOCKETUTILS_H
#define DISTANCE_VECTOR_ROUTING_PROTOCOL_IMPLEMENTATION_SOCKETUTILS_H
char *getIpfromHostname(char *hostName);
struct addrinfo *getAddressInfo(char *hostName, int port);

#endif //DISTANCE_VECTOR_ROUTING_PROTOCOL_IMPLEMENTATION_SOCKETUTILS_H
