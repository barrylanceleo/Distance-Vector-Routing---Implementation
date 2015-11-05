//
// Created by barry on 11/4/15.
//

#include "SocketUtils.h"
#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>


char *getIPAddress(struct sockaddr *address) {
    if (address == NULL) {
        return NULL;
    }

    char *IPAddress = (char *) malloc(sizeof(char)*INET_ADDRSTRLEN);
    struct sockaddr_in *IPstruct = (struct sockaddr_in *) address;
    inet_ntop(IPstruct->sin_family, &(IPstruct->sin_addr), IPAddress, INET_ADDRSTRLEN);

    return IPAddress;
}

int getPort(struct sockaddr *address) {
    int port;
    struct sockaddr_in *ipAddress = (struct sockaddr_in *) address;
    port = ntohs(ipAddress->sin_port);
    return port;
}

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
        fprintf(stderr, "Error in getting hostname from IpAddress%s\n", gai_strerror(result));
        return NULL;
    }
    return strdup(hostName);
}

char *getIpfromHostname(char *hostName) {
    struct addrinfo *sockAddress = getAddressInfo(hostName, NULL);
    if (sockAddress == NULL)
        return NULL;
    char *ipAddress = getIPAddress(sockAddress->ai_addr);
    return ipAddress;
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
        fprintf(stderr, "Error Getting AddressInfo: %s\n", gai_strerror(result));
        printf("Unable to get the AddressInfo. Please check the hostname/ipAddress provided.\n");
        return NULL;
    }

    return host_info_list;
}