//
// Created by barry on 11/4/15.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <sys/timerfd.h>
#include "Server.h"
#include "main.h"
#include "list.h"
#include "SocketUtils.h"

char *myHostName;
char *myIPAddress;
int myPort;
int myId;
int mySockFD;
list *nodesList = NULL;

char *readLine(FILE **fp)
{
    char line[LINE_BUFFER];
    if(fgets(line, LINE_BUFFER, *fp)!=NULL)
    {
        //remove the ending '/n'
        if(line[strlen(line)-1] == '\n')
        {
            line[strlen(line)-1] = 0;
        }
        else
        {
            fprintf(stderr,"Line not read completely in readLine().\n");
        }
        return strdup(line);
    }
    else
    {
        if(feof(*fp))
        {
            printf("File read completely.\n");
        }
        else if(ferror(*fp))
        {
            fprintf(stderr,"Error while reading file.\n");
        }
        return NULL;
    }
}

int readTopologyFile(char *topology_file_name, list **nodesList)
{
    int num_servers;
    int num_neighbors;
    FILE *tf = fopen(topology_file_name, "r");
    if(tf == NULL)
    {
        fprintf(stderr,"Unable to open topology file.\n");
        return -1; //unable to open topology file
    }
    else
    {
        char* topologyline;

        //read the number of servers
        topologyline = readLine(&tf);
        if(topologyline!=NULL)
        {
            num_servers = atoi(topologyline);
            if(num_servers==0)
            {
                fprintf(stderr,"Number of servers can't be zero.\n");
                return -2; //error in topology file
            }
            free(topologyline);
        }
        else
        {
            return -2; //error in topology file
        }

        //read the number of neighbours
        topologyline = readLine(&tf);
        if(topologyline!=NULL)
        {
            num_neighbors = atoi(topologyline);
            if(num_neighbors==0)
            {
                fprintf(stderr,"Number of neighbours shouldn't be zero.\n");
                return -2; //error in topology file
            }
            free(topologyline);
        }
        else {
            return -2; //error in topology file
        }

        //read the portnumber and id
        topologyline = readLine(&tf);
        int port = atoi(topologyline);
        myPort = port;
        topologyline = readLine(&tf);
        int id = atoi(topologyline);
        myId = id;

        //read the nodes' ips/ports and build add to the list
        int i;
        for(i=0; i < num_servers; i++)
        {
            topologyline = readLine(&tf);
            if(topologyline!=NULL)
            {
                int id;
                char ipAddress[IP_ADDR_LEN];
                int port;
                int cost = INFINITY;
                //parse the line
                int status = sscanf(topologyline, "%d%s%d", &id, ipAddress, &port);
                free(topologyline);
                if(status < 3 || status == EOF)
                {
                    fprintf(stderr,"Error in line format while reading node ips and ports.\n");
                    return -2; //error in topology file
                }

                //find my own entry and handle it accordingly
                if(strcmp(myIPAddress, ipAddress) == 0)
                {

                    //changes to make all nodes run on a single machine
//                    cost = 0;
//                    //initialize global variables
//                    myId = id;
//                    myPort = port;

                    if(id == myId && port == myPort)
                    {
                        cost = 0;
                    }
                                        
                }

                //create a node
                node *newNode = (node *)malloc(sizeof(node));
                newNode->id = id;
                newNode->ipAddress = strdup(ipAddress);
                newNode->port = port;
                newNode->cost = cost;

                //add node to the list
                addItem(nodesList, newNode);
                //printf("%d %s %d %d\n", newNode->id, newNode->ipAddress, newNode->port, newNode->cost);
            }
            else
            {
                fprintf(stderr,"Number of lines of server info less than the number of servers mentioned.\n");
                return -2; //error in topology file
            }

        }

        //read the cost of neighbours and update the list
        for(i=0; i < num_neighbors; i++)
        {
            topologyline = readLine(&tf);
            if(topologyline!=NULL)
            {

                int serverId;
                int neighbourId;
                int cost;
                //parse the line
                int status = sscanf(topologyline, "%d%d%d", &serverId, &neighbourId, &cost);
                free(topologyline);
                if(status < 3 || status == EOF)
                {
                    fprintf(stderr,"Error in line format while reading neighbour costs.\n");
                    return -2; //error in topology file
                }

                //update the cost in the list
                node *neighbourNode = (node *)findNodeByID(*nodesList, neighbourId);
                neighbourNode->cost = cost;
            }
            else
            {
                fprintf(stderr,"Number of lines of neighbour costs less than the number of neighbours mentioned.\n");
                return -2; //error in topology file
            }
        }

        //print the nodes
        //printList(nodesList);

        fclose(tf);

        return 0;
    }
}

int startServer(int port) {

    struct addrinfo *serverAddressInfo = getAddressInfo("localhost", port); //as server needs to be started on localhost

    if (serverAddressInfo == NULL){
        fprintf(stderr, "Couldn't get addrinfo for the host port %d\n", port);
        return -1;
    }

    int listernerSockfd;

    if ((listernerSockfd = socket(serverAddressInfo->ai_family, serverAddressInfo->ai_socktype,
                                  serverAddressInfo->ai_protocol)) == -1) {
        fprintf(stderr, "Error Creating socket: %d %s\n", listernerSockfd, strerror(errno));
        return -1;
    } else {
        printf("Created Socket.\n");
    }

    int bindStatus;

    if ((bindStatus = bind(listernerSockfd, serverAddressInfo->ai_addr, serverAddressInfo->ai_addrlen)) == -1) {
        fprintf(stderr, "Error binding %d %s\n", bindStatus, strerror(errno));
        return -1;
    } else {
        printf("Done binding socket to port %d.\n", port);
    }

    freeaddrinfo(serverAddressInfo);
    return listernerSockfd;
}


int broadcastToAllNodes(list *NodesList, char *msg, int messageLength) {
    struct listItem *currentItem = NodesList;
    if (currentItem == NULL) {
        printf("There are no nodes in list to broadcast\n");
        return -1;
    }
    else {
        do {
            node *currentNode = (struct node *) currentItem->item;
            struct addrinfo *serverAddressInfo = getAddressInfo(currentNode->ipAddress, currentNode->port);
            int bytes_sent = sendto(mySockFD, msg, messageLength * sizeof(char), 0,
                                    serverAddressInfo->ai_addr, sizeof(struct sockaddr_storage));
            if(bytes_sent == -1)
            {
                fprintf(stderr, "Error sending data: %s.\n", strerror(errno));
                return -1;
            }
            else{
                printf("Send data of %d bytes.\n", bytes_sent);
            }
            currentItem = currentItem->next;
        } while (currentItem != NULL);
    }
    return 0;
}

int runServer(char *topology_file_name, int routing_update_interval)
{
    //initialize the globale parameters
    myHostName = (char *) malloc(sizeof(char) * HOST_NAME_SIZE);
    gethostname(myHostName, HOST_NAME_SIZE);
    myIPAddress = getIpfromHostname(myHostName);
    if(myIPAddress == NULL)
    {
        myIPAddress = 'invalid';
        fprintf(stderr,"Unable to get Ip Address of server.\n");
        return -1; //probably internet failure
    }

    //read the topology file and initialize the nodeslist
    int status = readTopologyFile(topology_file_name, &nodesList);
    if(status == -1)
    {
        printf("Topology File not found.\n");
        return -2;//error in topology file
    }
    else if(status == -2)
    {
        printf("Error in topology file format.\n");
        return -2;//error in topology file
    }

    printf("%d %s %s %d\n", myId, myHostName, myIPAddress, myPort);
    printList(nodesList);

    mySockFD = startServer(myPort);
    if(mySockFD == -1)
    {
        //fprintf("Error in creating socket.\n");
        return -3;
    }

    fd_set masterFDList, tempFDList; //Master file descriptor list to add all sockets and stdin
    int fdmax; //to hold the max file descriptor value
    FD_ZERO(&masterFDList); // clear the master and temp sets
    FD_ZERO(&tempFDList);
    FD_SET(STDIN, &masterFDList); // add STDIN to master FD list
    fdmax = STDIN;

    FD_SET(mySockFD, &masterFDList); //add the listener to master FD list and update fdmax
    if (mySockFD > fdmax)
        fdmax = mySockFD;

    //create a timerFD for routing updates
    int timerFD = timerfd_create(CLOCK_REALTIME, 0);
    if (timerFD == -1)
        fprintf(stderr, "Error creating timer: %s.\n", strerror(errno));

    struct itimerspec time_value;
    time_value.it_value.tv_sec = 5; //initial time
    time_value.it_value.tv_nsec = 0;
    time_value.it_interval.tv_sec = routing_update_interval; //interval time
    time_value.it_interval.tv_nsec = 0;
    if (timerfd_settime(timerFD, 0, &time_value, NULL) == -1)
        fprintf(stderr, "Error setting time in timer: %s.\n", strerror(errno));

    FD_SET(timerFD, &masterFDList); //add the timer to master FD list and update fdmax
    if (timerFD > fdmax)
        fdmax = timerFD;


    while (1) //keep waiting for input and data
    {
        printf("$");
        fflush(stdout); //print the terminal symbol
        tempFDList = masterFDList; //make a copy of masterFDList and use it as select() modifies the list

        //int select(int numfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
        if (select(fdmax + 1, &tempFDList, NULL, NULL, NULL) ==
            -1) //select waits till it gets data in an fd in the list
        {
            fprintf(stderr, "Error in select\n");
            return -1;
        }
        // an FD has data so iterate through the list to find the fd with data
        int fd;
        for (fd = 0; fd <= fdmax; fd++) {
            if (FD_ISSET(fd, &tempFDList)) //found a FD with data
            {
                if (fd == STDIN) //data from commandLine(STDIN)
                {
                    size_t commandLength = 50;
                    char *command = (char *) malloc(commandLength);
                    getline(&command, &commandLength, stdin); //get line the variable if space is not sufficient
//                    if (stringEquals(command, "\n")) //to handle the stray \n s
//                        continue;
//                    //printf("--Got data: %s--\n",command);
//                    handleCommands(command, "SERVER");
                }
                else if (fd == mySockFD) //message from a host
                {

                    char buffer[10];
                    struct sockaddr_storage addr;
                    int fromlen = sizeof(addr);
                    int bytes_received = recvfrom(mySockFD, buffer, 10, 0, &addr, &fromlen);
                    buffer[bytes_received] = '\0';
                    if(bytes_received == -1)
                    {
                        fprintf(stderr, "Error reading: %s.\n", strerror(errno));
                    }
                    else if(bytes_received == 0)
                    {
                        printf(" Recevied a 0 byte.\n");
                    }
                    else
                    {
                        printf(" Recevied: %s.\n", buffer);
                    }

                }
                else if (fd == timerFD) //message from a host
                {

                    //read the timer
                    uint64_t timers_expired;
                    int bytes_read = read(timerFD, &timers_expired, sizeof(uint64_t));
                    if(bytes_read == -1)
                    {
                        fprintf(stderr, "Timer expired but error in reading: %s.\n", strerror(errno));
                    }
                    else{
                        printf("Timer expired: %lu.\n", timers_expired);
                    }

                    //send routing message to all hosts
                    if((status = broadcastToAllNodes(nodesList, "Hello", 5))== -1)
                    {
                        return -4;
                    }
                }
                else // handle other data
                {
                    printf("Received data from fd: %d but not sure who it is.\n", fd);

                }
            }
        }
    }

    return 0;
}