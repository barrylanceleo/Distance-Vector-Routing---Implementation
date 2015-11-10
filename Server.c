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
#include <arpa/inet.h>
#include "Server.h"
#include "SocketUtils.h"

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

int readTopologyFile(char *topology_file_name, context *nodeContext)
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
        nodeContext->myPort = port;
        topologyline = readLine(&tf);
        int id = atoi(topologyline);
        nodeContext->myId = id;

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
                if(strcmp(nodeContext->myIPAddress, ipAddress) == 0)
                {

                    //changes to make all nodes run on a single machine
//                    cost = 0;
//                    //initialize the context variables
//                    nodeContext.myId = id;
//                    nodeContext.myPort = port;

                    if(id == nodeContext->myId && port == nodeContext->myPort)
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
                addItem(&(nodeContext->nodesList), newNode);
                //printf("%d %s %d %d\n", newNode->id, newNode->ipAddress, newNode->port, newNode->cost);
            }
            else
            {
                fprintf(stderr,"Number of lines of server info less than the number of servers mentioned.\n");
                return -2; //error in topology file
            }

        }

        //read the cost of neighbours,update the cost in nodesList and create a neighbourList
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

                //update the cost in the NodesList
                node *neighbourNode = (node *)findNodeByID(nodeContext->nodesList, neighbourId);
                neighbourNode->cost = cost;

                //create a timeoutFD for the neighbour routing update timeout
                int timeoutFD = timerfd_create(CLOCK_REALTIME, 0);
                if (timeoutFD == -1)
                    fprintf(stderr, "Error creating timer: %s.\n", strerror(errno));

                struct itimerspec time_value;
                time_value.it_value.tv_sec = nodeContext->routing_update_interval*3; //initial time
                time_value.it_value.tv_nsec = 0;
                time_value.it_interval.tv_sec = nodeContext->routing_update_interval*3; //interval time
                time_value.it_interval.tv_nsec = 0;
                if (timerfd_settime(timeoutFD, 0, &time_value, NULL) == -1)
                    fprintf(stderr, "Error setting time in timer: %s.\n", strerror(errno));

                //create a neighbour
                neighbour *newNeighbour = (neighbour *)malloc(sizeof(neighbour));
                newNeighbour->id = neighbourId;
                newNeighbour->timeoutFD = timeoutFD;

                //add node to the list
                addItem(&(nodeContext->neighbourList), newNeighbour);
            }
            else
            {
                fprintf(stderr,"Number of lines of neighbour costs less than the number of neighbours mentioned.\n");
                return -2; //error in topology file
            }
        }

        //print the nodes and neighbours
//        printList(nodeContext->nodesList, "node");
//        printList(nodeContext->neighbourList, "neighbour");

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


int broadcastToNeighbours(context *nodeContext, char *msg, int messageLength) {
    struct listItem *currentItem = nodeContext->neighbourList;
    if (currentItem == NULL) {
        printf("There are no neighbours in list to broadcast\n");
        return -1;
    }
    else {
        char *ipAddress;
        char *port;
        do {
            neighbour *currentNeighbour = (neighbour *) currentItem->item;
            node *currentNode =(node *)findNodeByID(nodeContext->nodesList, currentNeighbour->id);
            struct addrinfo *serverAddressInfo = getAddressInfo(currentNode->ipAddress, currentNode->port);
            int bytes_sent = sendto(nodeContext->mySockFD, msg, messageLength * sizeof(char), 0,
                                    serverAddressInfo->ai_addr, sizeof(struct sockaddr_storage));
            if(bytes_sent == -1)
            {
                fprintf(stderr, "Error sending data: %s.\n", strerror(errno));
                return -1;
            }
            else{
                printf("Sent data of %d bytes.\n", bytes_sent);
            }
            currentItem = currentItem->next;
        } while (currentItem != NULL);
    }
    return 0;
}

int handleCommands(char *command) {
    //remove the last \n in command if present
    if (command[strlen(command) - 1] == '\n')
        command[strlen(command) - 1] = '\0';

    //split the command into parts

    //find the commandLength
    char delimiter = ' ';
    int i = 0, commandLength = 0;
    while(command[i] != '\0')
    {
        if(command[i] != delimiter)
        {
            commandLength++;
            //traverse till you find a delimiter
            do{
                i++;
            }while(command[i] != delimiter && command[i] != '\0');
        }
        else{
            i++;
        }
    }

    //check if the command is empty
    if(commandLength == 0)
    {
        return -1;
    }
    printf("commandLength: %d\n", commandLength);

    //create the commandParts array and update it
    char *commandParts[commandLength];
    i = 0;
    char *commandPart = strtok(command, " ");
    commandParts[i] = commandPart;
    i++;
    while(commandPart != NULL )
    {
        commandPart = strtok(NULL, " ");
        commandParts[i] = commandPart;
        i++;
    }

    //print the commands
    for(i = 0; i<commandLength; i++)
    {
        printf("-%s-\n", commandParts[i]);
    }



    return 0;
}

int runServer(char *topology_file_name, context *nodeContext)
{
    //initialize the context parameters
    nodeContext->myHostName = (char *) malloc(sizeof(char) * HOST_NAME_SIZE);
    gethostname(nodeContext->myHostName, HOST_NAME_SIZE);
    nodeContext->myIPAddress = getIpfromHostname(nodeContext->myHostName);
    if(nodeContext->myIPAddress == NULL)
    {
        nodeContext->myIPAddress = "invalid";
        fprintf(stderr,"Unable to get Ip Address of server.\n");
        return -1; //probably internet failure
    }

    //read the topology file and initialize the nodeslist
    int status = readTopologyFile(topology_file_name, nodeContext);
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

    printf("%d %s %s %d\n", nodeContext->myId, nodeContext->myHostName, nodeContext->myIPAddress, nodeContext->myPort);
    printList(nodeContext->nodesList, "node");
    printList(nodeContext->neighbourList, "neighbour");
    nodeContext->mySockFD = startServer(nodeContext->myPort);
    if(nodeContext->mySockFD == -1)
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

    FD_SET(nodeContext->mySockFD, &masterFDList); //add the listener to master FD list and update fdmax
    if (nodeContext->mySockFD > fdmax)
        fdmax = nodeContext->mySockFD;

    //create a routing_update_timerFD for routing updates
    int routing_update_timerFD = timerfd_create(CLOCK_REALTIME, 0);
    if (routing_update_timerFD == -1)
        fprintf(stderr, "Error creating timer: %s.\n", strerror(errno));

    struct itimerspec time_value;
    time_value.it_value.tv_sec = 1; //initial time
    time_value.it_value.tv_nsec = 0;
    time_value.it_interval.tv_sec = nodeContext->routing_update_interval; //interval time
    time_value.it_interval.tv_nsec = 0;
    if (timerfd_settime(routing_update_timerFD, 0, &time_value, NULL) == -1)
        fprintf(stderr, "Error setting time in timer: %s.\n", strerror(errno));

    FD_SET(routing_update_timerFD, &masterFDList); //add the timer to master FD list and update fdmax
    if (routing_update_timerFD > fdmax)
        fdmax = routing_update_timerFD;


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
                    if((status = handleCommands(command))!=0)
                    {
                        if(status == -1)
                        {
                            printf("Empty command.\n");
                        }
                    }
                }
                else if (fd == nodeContext->mySockFD) //message from a host
                {

                    char buffer[10];
                    struct sockaddr_storage addr;
                    socklen_t fromlen = sizeof(addr);
                    int bytes_received = recvfrom(nodeContext->mySockFD, buffer, 10, 0, &addr, &fromlen);
                    buffer[bytes_received] = '\0';
                    if(bytes_received == -1)
                    {
                        fprintf(stderr, "Error reading: %s.\n", strerror(errno));
                    }

                    //get the portnum and ipaddress from receiver
                    char fromIP[INET_ADDRSTRLEN];
                    struct sockaddr_in *fromAddr = ((struct sockaddr_in *)&addr);
                    char *returned_ptr = inet_ntop(fromAddr->sin_family, &(fromAddr->sin_addr),
                                                   fromIP, sizeof(fromIP));
                    int fromPort = ntohs(fromAddr->sin_port);
                    if(returned_ptr == NULL || fromPort == -1)
                    {
                        fprintf(stderr, "Error getting IP address and port from receiver: "
                                "%s.\n", strerror(errno));
                    }


                    if(bytes_received == 0)
                    {
                        printf(" Recevied a 0 byte from %s %d.\n", fromIP, fromPort);
                    }
                    else
                    {
                        printf(" Recevied: %s from %s %d.\n", buffer, fromIP, fromPort);
                    }

                }
                else if (fd == routing_update_timerFD) //message from a host
                {

                    //read the timer
                    uint64_t timers_expired;
                    int bytes_read = read(routing_update_timerFD, &timers_expired, sizeof(uint64_t));
                    if(bytes_read == -1)
                    {
                        fprintf(stderr, "Timer expired but error in reading: %s.\n", strerror(errno));
                    }
                    else{
                        printf("Timer expired: %lu.\n", timers_expired);
                    }

                    //send routing message to all hosts
                    if(broadcastToNeighbours(nodeContext, "Hello", 5) == -1)
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