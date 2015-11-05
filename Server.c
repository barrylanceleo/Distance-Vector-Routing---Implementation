//
// Created by barry on 11/4/15.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "Server.h"
#include "main.h"
#include "list.h"
#include "SocketUtils.h"

char *myHostName;
char *myIPAddress;
int myPort;
int myId;
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
                    cost = 0;
                    //initialize global variables
                    myId = id;
                    myPort = port;
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

        return 1;
    }
}

int runServer(char *topology_file_name, int routing_update_interval)
{
    //initialize the globale parameters
    myHostName = (char *) malloc(sizeof(char) * HOST_NAME_SIZE);
    gethostname(myHostName, HOST_NAME_SIZE);
    myIPAddress = getIpfromHostname(myHostName);

    //read the topology file and initialize the nodeslist
    int status = readTopologyFile(topology_file_name, &nodesList);
    printf("%d %s %s %d\n", myId, myHostName, myIPAddress, myPort);
    printList(nodesList);
    if(status == -1)
    {
        printf("Topology File not found.\n");
        return -1;//error in topology file
    }
    else if(status == -2)
    {
        printf("Error in topology file format.\n");
        return -1;//error in topology file
    }
    return 0;
}