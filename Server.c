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
        int i, j;
        for(i=0; i < num_nodes; i++)
        {
            nodeContext->distance_matrix[i] = (uint16_t *)malloc(num_nodes * sizeof(uint16_t));
        }
        for(i=0; i < num_nodes; i++)
        {
            for(j=0; j < num_nodes; j++)
            {
                if(i==j)
                {
                    nodeContext->distance_matrix[i][j] = 0;
                }
                else{
                    nodeContext->distance_matrix[i][j] = INFINITY;
                }
            }
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
//                    cost = 0;
//                    //initialize the context variables
//                    nodeContext->myId = id;
//                    nodeContext->myPort = port;

                }

                if(id == nodeContext->myId)
                {
                    cost = 0;
                    next_hop_id = id;
                }

                //create a routing_table_row
                routing_table_row *newRow = (routing_table_row *)malloc(sizeof(routing_table_row));
                newRow->id = id;
                newRow->ipAddress = strdup(ipAddress);
                newRow->port = port;
                newRow->cost = cost;
                newRow->next_hop_id = next_hop_id;

                //add routing_table_row to the list
                addItem(&(nodeContext->routing_table), newRow);
                //printf("%d %s %d %d\n", newRow->id, newRow->ipAddress, newRow->port, newRow->cost);
            }
            else
            {
                fprintf(stderr,"Number of lines of server info less than the number of servers mentioned.\n");
                return -2; //error in topology file
            }

        }

        //update my cost in the distance matrix
        nodeContext->distance_matrix[nodeContext->myId-1][nodeContext->myId-1] = 0;

        //read the cost of neighbours,update the cost in distance matric and create a neighbourList
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

                //update the cost in the distance matrix
                nodeContext->distance_matrix[nodeContext->myId-1][neighbourId-1] = cost;

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
                addItem(&(nodeContext->neighbourList), newNeighbour);
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

int printDistanceMatrix(context *nodeContext)
{
    printf("Distance Matrix:\n");
    int i,j;
    for(i=0; i < nodeContext->num_nodes; i++)
    {
        for(j=0; j < nodeContext->num_nodes; j++)
        {
            printf("%u\t", nodeContext->distance_matrix[i][j]);
        }
        printf("\n");
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

int updateDistanceMatrix(context *nodeContext, int sender_id, char *message, int messageLength)
{
    int parsed_size = 0;
    struct in_addr read_ip;
    uint16_t read_port, read_zero, read_id, read_cost;
    char readIP[INET_ADDRSTRLEN];
    char *returned_ptr;
    int element_num = 0;
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

        //update the matrix
        nodeContext->distance_matrix[sender_id -1][read_id-1] = read_cost;

        //printf("Node Info: %s %u %u %u %u\n", readIP, read_port, read_zero, read_id, read_cost);

        element_num++;
    }

    return 0;

}


int updateRoutingTable(context *nodeContext)
{

    if (nodeContext->neighbourList == NULL) {
        printf("There are no neighbours so no update to the distance vector.\n");
        return -1;
    }
    //update routing table as per Bellman-Ford equation
    int change_flag = 0;
    uint16_t min_cost = INFINITY;
    uint16_t min_cost_node_id = UNDEFINED;
    uint16_t **distance_matrix = nodeContext->distance_matrix;
    uint16_t myId = nodeContext->myId;
    uint16_t destination;
    for(destination = 0; destination < nodeContext->num_nodes; destination++)
    {
        if(destination == myId-1) //destination is itself
            continue;

        //the intermediate nodes need to be the neighbours
        struct listItem *currentItem = nodeContext->neighbourList;
        do {
            neighbour *currentNeighbour = (neighbour *) currentItem->item;
            int new_cost = currentNeighbour->cost + distance_matrix[currentNeighbour->id-1][destination];
            if(new_cost < min_cost)
            {
                min_cost = new_cost;
                min_cost_node_id = currentNeighbour->id;
            }
            currentItem = currentItem->next;
        } while (currentItem != NULL);

        if(distance_matrix[myId-1][destination] != min_cost)
        {
            change_flag = 1;
            distance_matrix[myId-1][destination] = min_cost;
        }
        min_cost = INFINITY;
        routing_table_row *destination_row = findRowByID(nodeContext->routing_table, destination+1);
        if(destination_row != NULL)
        {
            destination_row->cost = distance_matrix[myId-1][destination];
            destination_row->next_hop_id = min_cost_node_id;
        }
        else
        {
            fprintf(stderr, "Error in update the Routing table.\n");
            return -1;
        }
    }

    return change_flag;
}

int sendRoutingUpdate(context *nodeContext)
{
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
        printf("%d is not a neighbour of %d. So, adding it as a neighbour and updating the cost.\n",
               destination_id, nodeContext->myId);

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
    }
    else
    {
        neighbourNode->cost = new_cost;
    }

    //invoke an update on the Routing table as the neighbour cost has changed
    int status;
    if((status = updateRoutingTable(nodeContext))!=0)
    {
        fprintf(stderr, "Error updating routing table.\n");
        return -1;
    }
    return 0;
}

int disableLinkToNode(context *nodeContext, uint16_t node_id)
{
    //check if the node is a neighbour and remove it, if not throw an error
    int status;
    if((status = removeNeighbourByID(&(nodeContext->neighbourList), node_id)) != 0)
    {
        return -2;
    }
    else
    {
        //update the routing table
        if((status = updateRoutingTable(nodeContext)) == 1)
        {
            //there was a change in the distance vector, not sending update as per project req
            //sendRoutingUpdate(nodeContext);
        }
        else if (status ==  0)
        {
            //no change in the distance vector
        }
        else
        {
            return -1;
        }
    }
    return 0;
}

int simulateNoodeCrash(context *nodeContext)
{



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
    time_value.it_value.tv_sec = nodeContext->routing_update_interval; //initial time
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
                    if((status = handleCommand(nodeContext, command))!=0)
                    {
                        if(status == -1)
                        {
                            printf("Empty command.\n");
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
                            //printf("Received %d bytes.\n", bytes_received);
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
                        continue;
                    }

                    //find the source's ID
                    routing_table_row *sender_row;

                    if((sender_row = findRowByIPandPort(nodeContext->routing_table,
                                         IPAddress, read_port)) == NULL)
                    {
                        printf("Received a Routing update packet from a node not"
                                       " present in this node's topology file. So, ignoring the packet.");
                        continue;
                    }

                    int sender_id = sender_row->id;

                    printf("RECEIVED A MESSAGE FROM SERVER %u.\n", sender_id);

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
                            //no change in the distance vecto
                        }
                        else
                        {
                            fprintf(stderr, "Error updating routing table.\n");
                        }

                        //printDistanceMatrix(nodeContext);
                        displayRoutingTable(nodeContext);
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
                        fprintf(stderr, "Error Sending routing update.\n");
                    }
                    else{
                        printf("Timeout: Sent routing update to all neighbours.\n");
                    }
                }
                else // handle other data
                {
                    printf("Received data on fd: %d but not sure from whom.\n", fd);
                }
            }
        }
    }
    return 0;
}