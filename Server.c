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
        do {
            neighbour *currentNeighbour = (neighbour *) currentItem->item;
            node *currentNode =(node *)findNodeByID(nodeContext->nodesList, currentNeighbour->id);
            struct addrinfo *serverAddressInfo = getAddressInfo(currentNode->ipAddress, currentNode->port);

            //keep resending untill all data is sent
            int bytes_sent = 0;
            do{
                bytes_sent += sendto(nodeContext->mySockFD, (msg + bytes_sent) , (messageLength - bytes_sent) * sizeof(char), 0,
                         serverAddressInfo->ai_addr, sizeof(struct sockaddr_storage));
                if(bytes_sent == -1)
                {
                    fprintf(stderr, "Error sending data: %s.\n", strerror(errno));
                    return -1;
                }
                else{
                    printf("Sent data of %d bytes.\n", bytes_sent);
                }
            }while(bytes_sent != messageLength);

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

int buildRoutingPacket(context *nodeConText, char *routing_message, int *update_size)
{
    printf("Message size should be: %d bytes.\n", *update_size);

    uint16_t num_of_update_fields = htons((uint16_t)getSize(nodeConText->nodesList));
    uint16_t port = htons((uint16_t)nodeConText->myPort);
    struct in_addr *ip =(struct in_addr *)malloc(sizeof(struct in_addr));
    int status;
    if((status= inet_pton(AF_INET, nodeConText->myIPAddress, ip)) !=1)
    {
        fprintf(stderr, "Error converting source ipAdress\n");
        return -1;
    }

    //add these 3 to the message
    int message_size = 0;
    memcpy(routing_message, &num_of_update_fields, 2);
    message_size = message_size + 2;
    memcpy(routing_message+message_size, &port, 2);
    message_size = message_size + 2;
    memcpy(routing_message+message_size, ip, 4);
    message_size = message_size + 4;

    //add the neighbour info to the the message
    listItem *currentItem = nodeConText->nodesList;
    if (currentItem == NULL) {
        fprintf(stderr, "No node to send routing packet\n");
        return -1;
    }
    else {
        uint16_t zero = htons((uint16_t)0);
        uint16_t server_id;
        uint16_t cost;
        do {
            node *currentNode = (node *) currentItem->item;

            //parse the node
            if((status= inet_pton(AF_INET, currentNode->ipAddress, ip)) !=1)
            {
                fprintf(stderr, "Error converting source ipAdress\n");
                return -1;
            }
            port = htons((uint16_t)currentNode->port);
            server_id = htons((uint16_t)currentNode->id);
            cost = htons((uint16_t)currentNode->cost);

            //copy node details to the message
            memcpy(routing_message+message_size, ip, 4);
            message_size = message_size + 4;
            memcpy(routing_message+message_size, &port, 2);
            message_size = message_size + 2;
            memcpy(routing_message+message_size, &zero, 2);
            message_size = message_size + 2;
            memcpy(routing_message+message_size, &server_id, 2);
            message_size = message_size + 2;
            memcpy(routing_message+message_size, &cost, 2);
            message_size = message_size + 2;

            currentItem = currentItem->next;
        } while (currentItem != NULL);
    }

    *update_size = message_size;
    printf("Message size is: %d bytes.\n", *update_size);

    return 0;

}

int readPacket(int fd, char *message, int messageLength)
{

    struct sockaddr_storage addr;
    socklen_t fromlen = sizeof(addr);
    int bytes_received = 0;
    do{
        bytes_received += recvfrom(fd, (message + bytes_received),
                                   (messageLength - bytes_received), 0, &addr, &fromlen);
        if(bytes_received == -1)
        {
            fprintf(stderr, "Error reading: %s.\n", strerror(errno));
            return -1;
        }
        else
        {
            printf("Received %d bytes.\n", bytes_received);
        }
    }while(bytes_received != messageLength);
    return 0;
}

int updateCosts(char *message, int messageLength)
{
    //just print it now
    int parsed_size = 0;
    struct in_addr read_ip;
    uint16_t read_port, read_zero, read_id, read_cost;
    char readIP[INET_ADDRSTRLEN];
    char *returned_ptr;
    while(parsed_size != messageLength)
    {
        memcpy(&read_ip, message+parsed_size, 4);
        parsed_size += 4;
        memcpy(&read_port, message+parsed_size, 2);
        parsed_size += 2;
        memcpy(&read_zero, message+parsed_size, 2);
        parsed_size += 2;
        memcpy(&read_id, message+parsed_size, 2);
        parsed_size += 2;
        memcpy(&read_cost, message+parsed_size, 2);
        parsed_size += 2;
        returned_ptr = inet_ntop(AF_INET, &read_ip, readIP, sizeof(readIP));
        if(returned_ptr == NULL)
        {
            fprintf(stderr," unable to to get back the ipaddress");
        }
        read_port = ntohs(read_port);
        read_zero = ntohs(read_zero);
        read_id = ntohs(read_id);
        read_cost = ntohs(read_cost);
        printf("Node Info: %s %u %u %u %u\n", readIP, read_port, read_zero, read_id, read_cost);
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
                    //read a packet
                    int messageLength = 8 + getSize(nodeContext->nodesList)*12;
                    char message[messageLength];
                    struct sockaddr_storage addr;
                    socklen_t fromlen = sizeof(addr);
                    int bytes_received = 0;
                    do{
                        bytes_received += recvfrom(nodeContext->mySockFD, (message +bytes_received),
                                                   (messageLength - bytes_received), 0, &addr, &fromlen);
                        if(bytes_received == -1)
                        {
                            fprintf(stderr, "Error reading: %s.\n", strerror(errno));
                        }
                        else
                        {
                            printf("Received %d bytes.\n", bytes_received);
                        }
                    }while(bytes_received != messageLength);

                    //read the num_fields_sent, source ip, port

                    int parsed_size = 0;
                    uint16_t read_num_of_update_fields, read_port;
                    struct in_addr read_ip;
                    memcpy(&read_num_of_update_fields, message +parsed_size, 2);
                    parsed_size += 2;
                    memcpy(&read_port, message +parsed_size, 2);
                    parsed_size += 2;
                    memcpy(&read_ip, message +parsed_size, 4);
                    parsed_size += 4;
                    read_num_of_update_fields = ntohs(read_num_of_update_fields);
                    read_port = ntohs(read_port);
                    char IPAddress[INET_ADDRSTRLEN];
                    char *returned_ptr = inet_ntop(AF_INET, &read_ip, IPAddress, sizeof(IPAddress));
                    if(returned_ptr == NULL)
                    {
                    fprintf(stderr," unable to to get back the ipaddress");
                    }

                    printf("Received Routing update from : %s %u with %u entries.\n", IPAddress, read_port, read_num_of_update_fields);

                    //update Routing table
                    if((status = updateCosts(message+8, messageLength-8))!=0)
                    {
                        fprintf(stderr, "Error Updating cost.\n");
                    }

                }
                else if (fd == routing_update_timerFD) //time to send a routing update
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

                    //build the routing update
                    //estimate the size of the update
                    int num_nodes = getSize(nodeContext->nodesList);
                    int update_size = 8 + 12*num_nodes;
                    char update_message[update_size];
                    int status =  buildRoutingPacket(nodeContext, update_message, &update_size);
                    if(status != 0)
                    {
                        fprintf(stderr, "Unable to create routing message\n");
                    }
                    else
                    {
                        //send routing message to all hosts
                        if(broadcastToNeighbours(nodeContext, update_message, update_size) == -1)
                        {
                            return -4;
                        }
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