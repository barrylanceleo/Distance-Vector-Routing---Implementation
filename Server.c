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
#include "user_interface.h"

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
    int num_nodes;
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
            num_nodes = atoi(topologyline);
            if(num_nodes ==0)
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

        //initialize the context variable
        nodeContext->num_nodes = num_nodes;

        //initialize the distance matrix
        nodeContext->distance_matrix = (uint16_t **)malloc(num_nodes * sizeof(uint16_t *));
        uint16_t i, j;
        for(i=0; i < num_nodes; i++)
        {
            nodeContext->distance_matrix[i] = (uint16_t *)malloc(num_nodes * sizeof(uint16_t));
        }

        //read the number of neighbours
        topologyline = readLine(&tf);
        if(topologyline!=NULL)
        {
            num_neighbors = atoi(topologyline);
            if(num_neighbors==0)
            {
                printf("Note: You have started this node with 0 neighbours.\n");
            }
            free(topologyline);
        }
        else {
            return -2; //error in topology file
        }

        //changes to make all nodes run on a single machine
//        //read the portnumber and id
//        topologyline = readLine(&tf);
//        int port = atoi(topologyline);
//        nodeContext->myPort = port;
//        topologyline = readLine(&tf);
//        int id = atoi(topologyline);
//        nodeContext->myId = id;

        //read the nodes' ips/ports and build add to the list
        for(i=0; i < num_nodes; i++)
        {
            int cost = INFINITY;
            int next_hop_id = UNDEFINED;
            topologyline = readLine(&tf);
            if(topologyline!=NULL)
            {
                int id;
                char ipAddress[IP_ADDR_LEN];
                int port;
                //parse the line
                int status = sscanf(topologyline, "%d%s%d", &id, ipAddress, &port);
                free(topologyline);
                if(status < 3 || status == EOF)
                {
                    fprintf(stderr,"Error in line format while reading routing_table_row ips and ports.\n");
                    return -2; //error in topology file
                }

                //find my own entry and handle it accordingly
                if(strcmp(nodeContext->myIPAddress, ipAddress) == 0)
                {
                    //changes to make all nodes run on a single machine
                    //initialize the context variables
                    nodeContext->myId = id;
                    nodeContext->myPort = port;
                }

                if(id == nodeContext->myId)
                {
                    cost = 0;
                    next_hop_id = id;
                    nodeContext->myDVIndex = i;
                }

                //create a routing_table_row
                routing_table_row *newRow = (routing_table_row *)malloc(sizeof(routing_table_row));
                newRow->id = id;
                newRow->ipAddress = strdup(ipAddress);
                newRow->port = port;
                newRow->cost = cost;
                newRow->next_hop_id = next_hop_id;
                newRow->DVIndex = i; //i will be the index for this row in the distance vector matrix

                //initialize the corresponding distance vector
                for(j=0; j < num_nodes; j++)
                {
                    if(j == newRow->DVIndex)
                    {
                        nodeContext->distance_matrix[newRow->DVIndex][j] = 0;
                    }
                    else{
                        nodeContext->distance_matrix[newRow->DVIndex][j] = INFINITY;
                    }
                }

                //add routing_table_row to the list
                addItem(&(nodeContext->routing_table), newRow);
            }
            else
            {
                fprintf(stderr,"Number of lines of server info less than the number of servers mentioned.\n");
                return -2; //error in topology file
            }

        }

        //read the cost of neighbours,update the cost in distance matrix and create a neighbourList
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

                //get the DVIndex
                int neighbourDVIndex;
                if((neighbourDVIndex= getDVIndex(nodeContext->routing_table, neighbourId)) == -1)
                {
                    fprintf(stderr,"Encountered unxpected server_id %u while reading costs.\n", neighbourId);
                    return -2; //error in topology file
                }

                //update the cost in the distance matrix
                nodeContext->distance_matrix[nodeContext->myDVIndex][neighbourDVIndex] = cost;

                routing_table_row *neighbour_row = (routing_table_row *) findRowByID(nodeContext->routing_table,
                                                                                     neighbourId);
                neighbour_row->cost = cost;
                neighbour_row->next_hop_id = neighbourId;

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
                newNeighbour->cost = cost;
                newNeighbour->timeoutFD = timeoutFD;

                //add neighbour to the list
                addItem(&nodeContext->neighbourList, newNeighbour);

                //add the timer to the master fd
                FD_SET(timeoutFD, &nodeContext->FDList);
                nodeContext->FDmax = timeoutFD>nodeContext->FDmax?timeoutFD:nodeContext->FDmax;

            }
            else
            {
                fprintf(stderr,"Number of lines of neighbour costs less than the number of neighbours mentioned.\n");
                return -2; //error in topology file
            }
        }

        //print the nodes and neighbours
//        printList(nodeContext->routing_table, "routing_table_row");
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
        //printf("Created Socket.\n");
    }

    int bindStatus;

    if ((bindStatus = bind(listernerSockfd, serverAddressInfo->ai_addr, serverAddressInfo->ai_addrlen)) == -1) {
        fprintf(stderr, "Error binding %d %s\n", bindStatus, strerror(errno));
        return -1;
    } else {
        //printf("Done binding socket to port %d.\n", port);
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
            routing_table_row *currentNode =(routing_table_row *) findRowByID(nodeContext->routing_table,
                                                                              currentNeighbour->id);
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
                    //printf("Sent data of %d bytes.\n", bytes_sent);
                }
            }while(bytes_sent != messageLength);

            currentItem = currentItem->next;
        } while (currentItem != NULL);
    }
    return 0;
}

int buildRoutingPacket(context *nodeConText, char *routing_message, int *update_size)
{
    //printf("Message size should be: %d bytes.\n", *update_size);

    uint16_t num_of_update_fields = htons((uint16_t)getSize(nodeConText->routing_table));
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
    listItem *currentItem = nodeConText->routing_table;
    if (currentItem == NULL) {
        fprintf(stderr, "No routing_table_row to send routing packet\n");
        return -1;
    }
    else {
        uint16_t zero = htons((uint16_t)0);
        uint16_t server_id;
        uint16_t cost;
        do {
            routing_table_row *currentNode = (routing_table_row *) currentItem->item;

            //parse the routing_table_row
            if((status= inet_pton(AF_INET, currentNode->ipAddress, ip)) !=1)
            {
                fprintf(stderr, "Error converting source ipAdress\n");
                return -1;
            }
            port = htons((uint16_t)currentNode->port);
            server_id = htons((uint16_t)currentNode->id);
            cost = htons((uint16_t)currentNode->cost);

            //copy routing_table_row details to the message
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
    //printf("Message size is: %d bytes.\n", *update_size);

    return 0;

}

int readPacket(int fd, char *message, int messageLength)
{

    struct sockaddr addr;
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

int updateDistanceMatrix(context *nodeContext, int sender_id, char *message, int messageLength)
{
    int parsed_size = 0;
    struct in_addr read_ip;
    uint16_t read_port, read_zero, read_id, read_cost;
    char readIP[INET_ADDRSTRLEN];
    const char *returned_ptr;
    //get the sender's DVindex
    int senderDVIndex;
    if((senderDVIndex= getDVIndex(nodeContext->routing_table, sender_id)) == -1)
    {
        return -1;
    }

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

        //get the corresponding DVindex
        int DVIndex;
        if((DVIndex= getDVIndex(nodeContext->routing_table, read_id)) == -1)
        {
            fprintf(stderr,"Found undefined server_id %u in message. Ignoring that cost.\n", read_id);
        }
        else
        {
            //update the matrix
            nodeContext->distance_matrix[senderDVIndex][DVIndex] = read_cost;
        }
        //printf("Node Info: %s %u %u %u %u\n", readIP, read_port, read_zero, read_id, read_cost);
    }

    //printDistanceMatrix(nodeContext);
    return 0;

}


int updateRoutingTable(context *nodeContext)
{

    if (nodeContext->neighbourList == NULL) {
        //printf("There are no neighbours so no update to the distance vector.\n");
        return -1;
    }
    //update routing table as per Bellman-Ford equation
    int change_flag = 0;
    uint16_t min_cost = INFINITY;
    uint16_t min_cost_node_id = UNDEFINED;
    uint16_t **distance_matrix = nodeContext->distance_matrix;
    uint16_t myId = nodeContext->myId;
    listItem *currentRTItem = nodeContext->routing_table;
    while (currentRTItem != NULL) //destination iteration
    {
        routing_table_row *destination_row = (routing_table_row *) currentRTItem->item;
        if(destination_row->id == myId) //destination is itself
        {
            currentRTItem = currentRTItem->next;
            continue;
        }

        int desinationDVIndex = getDVIndex(nodeContext->routing_table, destination_row->id);

        //iterate through the neighbours
        listItem *currentNeighbourItem = nodeContext->neighbourList;
        while (currentNeighbourItem != NULL) {
            neighbour *currentNeighbour = (neighbour *) currentNeighbourItem->item;

            int neighbourDVIndex = getDVIndex(nodeContext->routing_table, currentNeighbour->id);
            if(neighbourDVIndex == -1)
            {
                return -2;
            }

            int new_cost = (int) currentNeighbour->cost + distance_matrix[neighbourDVIndex][desinationDVIndex];
            if(new_cost < min_cost)
            {
                min_cost = new_cost;
                min_cost_node_id = currentNeighbour->id;
            }
            currentNeighbourItem = currentNeighbourItem->next;
        }

        if(distance_matrix[nodeContext->myDVIndex][desinationDVIndex] != min_cost)
        {
            change_flag = 1;
            distance_matrix[nodeContext->myDVIndex][desinationDVIndex] = min_cost;
        }
        min_cost = INFINITY;
        destination_row->cost = distance_matrix[nodeContext->myDVIndex][desinationDVIndex];
        destination_row->next_hop_id = min_cost_node_id;

        currentRTItem = currentRTItem->next;
    }

    return change_flag;
}

int sendRoutingUpdate(context *nodeContext)
{
    //check if there are neighbours
    if(nodeContext->neighbourList == NULL)
    {
        return -3;
    }

    //build the routing update
    //estimate the size of the update
    int num_nodes = getSize(nodeContext->routing_table);
    int update_size = 8 + 12*num_nodes;
    char update_message[update_size];
    int status =  buildRoutingPacket(nodeContext, update_message, &update_size);
    if(status != 0)
    {
        fprintf(stderr, "Unable to create routing message\n");
        return -1;
    }
    else
    {
        //send routing message to all neighbours
        if(broadcastToNeighbours(nodeContext, update_message, update_size) != 0)
        {
            return -2;
        }
        return 0;
    }
}

int updateLinkCost(context *nodeContext, uint16_t destination_id, uint16_t new_cost)
{

    //if destination is not a neighbour add it as a neighbour, else update its cost
    neighbour *neighbourNode;
    if((neighbourNode = findNeighbourByID(nodeContext->neighbourList, destination_id))==NULL)
    {

        if((findRowByID(nodeContext->routing_table, destination_id)) == NULL)
        {
            return -2;
        }
        printf("%d is not a neighbour of %d. So, adding it as a neighbour and updating the cost.\n"
                       "p.s. this will be a one-directional link from %d to %d, to make make it bi-directional "
                       "you need to add the same link on server %d\n",
               destination_id, nodeContext->myId, nodeContext->myId, destination_id, destination_id);

        //add neighbour
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
        newNeighbour->id = destination_id;
        newNeighbour->cost = new_cost;
        newNeighbour->timeoutFD = timeoutFD;

        //add neighbour to the list
        addItem(&(nodeContext->neighbourList), newNeighbour);

        //add the timer to the master fd
        FD_SET(timeoutFD, &nodeContext->FDList);
        nodeContext->FDmax = timeoutFD > nodeContext->FDmax? timeoutFD : nodeContext->FDmax;

    }
    else
    {
        neighbourNode->cost = new_cost;
    }

    //invoke an update on the Routing table as the neighbour cost has changed
    int status;
    if((status = updateRoutingTable(nodeContext)) == 1)
    {
        //there was a change in the distance vector, not sending update as per project req
        //sendRoutingUpdate(nodeContext);
    }
    else if (status ==  0)
    {
        //no change in the distance vector nothing to do
    }
    else
    {
        //nothing to update neighbour list is empty
    }
    return 0;
}

int disableLinkToNode(context *nodeContext, uint16_t node_id)
{
    //check if the node is a neighbour and remove it, if not throw an error
    int status;
    neighbour *nodeTodelete;
    if((nodeTodelete = findNeighbourByID(nodeContext->neighbourList, node_id)) == NULL)
    {
        return -2;
    }
    else
    {
        FD_CLR(nodeTodelete->timeoutFD, &nodeContext->FDList);
        if((status = removeNeighbourByID(&nodeContext->neighbourList, node_id)) != 0)
        {
            return -2;
        }
        else
        {
            //update the distance vector and routing table
            if((status = updateRoutingTable(nodeContext)) == 1)
            {
                //there was a change in the distance vector, not sending update as per project req
                //sendRoutingUpdate(nodeContext);
            }
            else if (status ==  0)
            {
                //no change in the distance vector nothing to do
            }
            else
            {
                //nothing to update neighbour list is empty
            }

        }
    }
    return 0;
}

int simulateNodeCrash(context *nodeContext)
{
    //simulate node crash by clearing the masterFDList and adding only stdin to it
    FD_ZERO(&nodeContext->FDList);
    FD_SET(STDIN, &nodeContext->FDList);
    nodeContext->FDmax = STDIN;

    return 0;
}


int runServer(char *topology_file_name, context *nodeContext)
{
    //initialize the context parameters
    nodeContext->myHostName = (char *) malloc(sizeof(char) * HOST_NAME_SIZE);
    gethostname(nodeContext->myHostName, HOST_NAME_SIZE);
    nodeContext->myIPAddress = getIpfromHostname(nodeContext->myHostName);
    printf("%s", nodeContext->myIPAddress);
    if(nodeContext->myIPAddress == NULL)
    {
        nodeContext->myIPAddress = "invalid";
        fprintf(stderr,"Unable to get Ip Address of server.\n");
        return -1; //probably internet failure
    }

    //read the topology file and initialize the nodeslist
    int status;
    if((status = readTopologyFile(topology_file_name, nodeContext)) != 0 )
    {
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
        else{
            printf("ERROR.\n");
            return -100; //catch all error
        }
    }


    //printDistanceMatrix(nodeContext);

    printf("My Info:\nID:%d Hostname:%s IP:%s Port:%d\n", nodeContext->myId, nodeContext->myHostName,
           nodeContext->myIPAddress, nodeContext->myPort);
    //printList(nodeContext->routing_table, "routing_table_row");
    //printList(nodeContext->neighbourList, "neighbour");
    nodeContext->mySockFD = startServer(nodeContext->myPort);
    if(nodeContext->mySockFD == -1)
    {
        //fprintf(stderr, "Error in creating socket.\n");
        return -3;
    }

    fd_set tempFDList; //temp file descriptor list to hold all sockets and stdin
    FD_ZERO(&tempFDList); // clear the temp set
    FD_SET(STDIN, &nodeContext->FDList); // add STDIN to master FD list
    nodeContext->FDmax = STDIN > nodeContext->FDmax?STDIN:nodeContext->FDmax;

    FD_SET(nodeContext->mySockFD, &nodeContext->FDList); //add the listener to master FD list and update fdmax
    nodeContext->FDmax = nodeContext->mySockFD > nodeContext->FDmax?nodeContext->mySockFD:nodeContext->FDmax;

    //create a routing_update_timerFD for routing updates
    int routing_update_timerFD = timerfd_create(CLOCK_REALTIME, 0);
    if (routing_update_timerFD == -1)
        fprintf(stderr, "Error creating timer: %s.\n", strerror(errno));

    struct itimerspec time_value;
    time_value.it_value.tv_sec = nodeContext->routing_update_interval; //initial time
    time_value.it_value.tv_nsec = 0;
    time_value.it_interval.tv_sec = nodeContext->routing_update_interval; //interval time
    time_value.it_interval.tv_nsec = 0;
    if (timerfd_settime(routing_update_timerFD, 0, &time_value, NULL) == -1)
        fprintf(stderr, "Error setting time in timer: %s.\n", strerror(errno));

    FD_SET(routing_update_timerFD, &nodeContext->FDList); //add the timer to master FD list and update fdmax
    nodeContext->FDmax = routing_update_timerFD > nodeContext->FDmax?routing_update_timerFD:nodeContext->FDmax;

    while (1) //keep waiting for input and data
    {
        printf("$");
        fflush(stdout); //print the terminal symbol
        tempFDList = nodeContext->FDList; //make a copy of masterFDList and use it as select() modifies the list

        //int select(int numfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
        if (select(nodeContext->FDmax + 1, &tempFDList, NULL, NULL, NULL) ==
            -1) //select waits till it gets data in an fd in the list
        {
            fprintf(stderr, "Error in select\n");
            return -1;
        }
        // an FD has data so iterate through the list to find the fd with data
        int fd;
        for (fd = 0; fd <= nodeContext->FDmax; fd++) {
            if (FD_ISSET(fd, &tempFDList)) //found a FD with data
            {
                if (fd == STDIN) //data from commandLine(STDIN)
                {
                    size_t commandLength = 50;
                    char *command = (char *) malloc(commandLength);
                    getline(&command, &commandLength, stdin); //get line the variable if space is not sufficient
//                    if (stringEquals(command, "\n")) //to handle the stray \n s
//                        continue;
                    if((status = handleCommand(nodeContext, command))!=0)
                    {
                        if(status == -1)
                        {
                            //printf("Empty command.\n");
                        }
                    }
                }
                else if (fd == nodeContext->mySockFD) //message from a host
                {

                    //maintain an update packet received counter to support "packets" function
                    nodeContext->received_packet_counter++;

                    //read the packet
                    int messageLength = 8 + getSize(nodeContext->routing_table)*12;
                    char message[messageLength];
                    struct sockaddr addr;
                    socklen_t fromlen = sizeof(addr);
                    int bytes_received = 0;
//                    do{
//                        bytes_received += recvfrom(nodeContext->mySockFD, (message +bytes_received),
//                                                   (messageLength - bytes_received), 0, &addr, &fromlen);
//                        if(bytes_received == -1)
//                        {
//                            fprintf(stderr, "Error reading: %s.\n", strerror(errno));
//                        }
//                        else
//                        {
//                            //printf("Received %d bytes.\n", bytes_received);
//                        }
//                    }while(bytes_received != messageLength);

                    bytes_received = recvfrom(nodeContext->mySockFD, (message +bytes_received),
                                                   (messageLength - bytes_received), 0, &addr, &fromlen);
                    if(bytes_received == -1)
                    {
                        fprintf(stderr, "Error reading: %s.\n", strerror(errno));
                    }
                    else
                    {
                        if(bytes_received < messageLength)
                        {
                            printf("Message received it shorter than the expected size. So, ignoring message.\n"
                                           "Received: %d, Expected: 8 + %d * 12 = %d\n", bytes_received,
                                   getSize(nodeContext->routing_table), messageLength);
                            continue;
                        }
                    }

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
                    const char *returned_ptr = inet_ntop(AF_INET, &read_ip, IPAddress, sizeof(IPAddress));
                    if(returned_ptr == NULL)
                    {
                    fprintf(stderr," unable to to get back the ipaddress");
                        continue;
                    }

                    //find the source's ID
                    routing_table_row *sender_row;

                    if((sender_row = findRowByIPandPort(nodeContext->routing_table,
                                         IPAddress, read_port)) == NULL)
                    {
                        printf("Received a Routing update packet from a %s/%u but it does not"
                                       " belong to this network. So, ignoring the packet.", IPAddress, read_port);
                        continue;
                    }

                    uint16_t sender_id = sender_row->id;

                    printf("RECEIVED A MESSAGE FROM SERVER %u.\n", sender_id);

                    //reset the routing update timer of the sender
                    neighbour *sender;
                    if((sender = findNeighbourByID(nodeContext->neighbourList, sender_id)) == NULL)
                    {
                        printf("SERVER %u is not a neighbour so ignoring message.\n", sender_id);
                        continue;
                    }
                    else
                    {
                        struct itimerspec curr_time_value;
                        if ((status = timerfd_gettime(sender->timeoutFD, &curr_time_value)) !=0)
                        {
                            fprintf(stderr, "Error getting routing update timer of neighbour: %s.\n", strerror(errno));
                            continue;
                        }
                        else
                        {
                            //reset the remaining time to the interval
                            curr_time_value.it_value.tv_sec = curr_time_value.it_interval.tv_sec;
                            curr_time_value.it_value.tv_nsec = curr_time_value.it_interval.tv_nsec;
                            if (timerfd_settime(sender->timeoutFD, 0, &curr_time_value, NULL) != 0)
                            {
                                fprintf(stderr, "Error reseting the neighbour's timer: %s.\n", strerror(errno));
                                continue;
                            }
                        }
                    }

                    //update the distance matrix
                    if(read_num_of_update_fields != getSize(nodeContext->routing_table))
                    {
                        printf("Received a Routing update packet with more fields than the number of nodes"
                                       " present in this node's topology file. So, ignoring the packet.");
                        continue;
                    }
                    else 
                    {
                        //update the distance matrix with the distance vector received
                        if((status = updateDistanceMatrix(nodeContext, sender_id, message + 8, messageLength - 8))!=0)
                        {
                            fprintf(stderr, "Error building distance vector.\n");
                        }

                        //printDistanceMatrix(nodeContext);

                        if((status = updateRoutingTable(nodeContext)) == 1)
                        {
                            //there was a change in the distance vector
                            //sendRoutingUpdate(nodeContext);

                        }
                        else if (status ==  0)
                        {
                            //no change in the distance vector
                        }
                        else
                        {
                            //nothing to update neighbour list is empty
                        }

                        //printDistanceMatrix(nodeContext);
                        //displayRoutingTable(nodeContext);
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
                        //printf("Timer expired: %lu.\n", timers_expired);
                    }

                    if((status = sendRoutingUpdate(nodeContext))!=0)
                    {
                        if(status == -3)
                        {
                            printf("Timeout: There are no neighbours to send routing update to.\n");
                        }
                        else
                        {
                            fprintf(stderr, "Error Sending routing update.\n");
                        }
                    }
                    else{
                        printf("Timeout: Sent routing update to all neighbours.\n");
                    }
                }
                else // handle other data
                {
                    //check if one of the neighbour timers has expired
                    neighbour *expiredNeighbour;
                    if((expiredNeighbour = findNeighbourByTimerFD(nodeContext->neighbourList, fd)) != NULL)
                    {
                        uint16_t expiredNodeId = expiredNeighbour->id;

                        //read the timer
                        uint64_t timers_expired;
                        int bytes_read = read(expiredNeighbour->timeoutFD, &timers_expired, sizeof(uint64_t));
                        if(bytes_read == -1)
                        {
                            fprintf(stderr, "Timer expired but error in reading: %s.\n", strerror(errno));
                        }
                        else{
                            //printf("Timer expired: %lu.\n", timers_expired);
                        }


                        if((status = disableLinkToNode(nodeContext, expiredNodeId)) == 0)
                        {
                            printf("Did not receive Routing update from Server %u for three routing intervals. "
                                           "So, removed it from the neighbour list.\n", expiredNodeId);
                        }
                        else if (status == -2){
                            printf("Did not receive Routing update from Server %u for three routing intervals. "
                                           "So, tried to remove it from the neighbour list, but failed.\n", expiredNodeId);
                            fprintf(stderr, "Server_id is not a neighbour so there is no direct link to disable.\n");
                        }
                        else
                        {
                            printf("Did not receive Routing update from Server %u for three routing intervals. "
                                           "So, tried to remove it from the neighbour list, but failed.\n", expiredNodeId);
                            fprintf(stderr, "Neighbour link removed but routing table not updated.This is bad.\n");
                        }
                    }
                    else
                    {

                        char buffer[100];
                        int bytes_read = read(expiredNeighbour->timeoutFD, buffer, sizeof(uint64_t));
                        printf("Received message: \"%s\" on fd: %d but not sure from whom.\n", buffer, fd);

                    }
                }
            }
        }
    }
    return 0;
}