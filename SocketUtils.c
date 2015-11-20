//
// Created by barry on 11/4/15.
//

#include "SocketUtils.h"
#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>



char *getHostnameFromIp(char *ipAddress) {
    if (ipAddress == NULL) {
        return NULL;
    }
    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    inet_pton(AF_INET, ipAddress, &sa.sin_addr);
    char hostName[HOST_NAME_SIZE];
    int result = getnameinfo((struct sockaddr *) &sa, sizeof(sa), hostName, sizeof(hostName), NULL, 0, 0);
    if (result != 0) {
        fprintf(stderr, "Error in getting hostname from IpAddress%s\n", strerror(errno));
        return NULL;
    }
    return strdup(hostName);
}

char *getIpfromHostname(char *hostName) {
    struct addrinfo *sockAddress = getAddressInfo(hostName, 0);
    if (sockAddress == NULL)
        return NULL;

    char IPAddress[INET_ADDRSTRLEN];
    struct sockaddr_in *address = (struct sockaddr_in *)sockAddress->ai_addr;
    const char *returned_ptr = inet_ntop(address->sin_family, &(address->sin_addr), IPAddress, sizeof(IPAddress));
    if(returned_ptr == NULL)
    {
        return NULL;
    }
    else
        return strdup(IPAddress);
}

struct addrinfo *getAddressInfo(char *hostName, int port) {

    char portString[5];
    sprintf(portString, "%d", port);
    struct addrinfo hints, *host_info_list;
    memset(&hints, 0, sizeof hints); //clear any hint
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_family = AF_INET;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_PASSIVE;
    int result;
    if (strcmp(hostName, "localhost") == 0) {
        result = getaddrinfo(NULL, portString, &hints, &host_info_list);
    }
    else {
        result = getaddrinfo(hostName, portString, &hints, &host_info_list);
    }
    if (result != 0 || host_info_list == NULL) {
        fprintf(stderr, "Error Getting AddressInfo: %s\n", strerror(errno));
        return NULL;
    }

    return host_info_list;
}