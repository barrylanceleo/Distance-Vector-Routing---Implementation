#include <stdio.h>
#include <stdlib.h>
#include "Server.h"
#include "main.h"

void printUsage()
{
    printf("Usage: <filename> -t <topology-file-name> -i <routing-update-interval>");
}

int main(int argc, char **args)
{
    //create a context variable and initialize it
    context nodeContext;
    nodeContext.routing_update_interval = 0;
    nodeContext.myHostName = NULL;
    nodeContext.myIPAddress = NULL;
    nodeContext.myPort = 0;
    nodeContext.myId = 0;
    nodeContext.myDVIndex = 0;
    nodeContext.mySockFD = 0;
    nodeContext.routing_table = NULL;
    nodeContext.neighbourList = NULL;
    nodeContext.distance_matrix = NULL;
    nodeContext.received_packet_counter = 0;
    FD_ZERO(&nodeContext.FDList);
    nodeContext.FDmax = 0;

    if(argc == 5)
    {
        char *topology_file_name = args[2];
        nodeContext.routing_update_interval = atoi(args[4]);
        //atoi(nptr) is same as strtol(nptr, NULL, 10);
        if(nodeContext.routing_update_interval == 0)
        {
            printf("Invalid routing update interval provided.\n");
            printUsage();
            printf("\n");
        }

        int status ;
        if(status = (runServer(topology_file_name, &nodeContext) !=0))
        {
            if(status == -1)
            {
                printf("Probably Internet Failure. Terminating...\n");
                return -1;
            }
            else if(status == -2)
            {
                printf("Error in topology file. Terminating...\n");
                return -1;
            }
            if(status == -3)
            {
                printf("Error creating socket. Terminating...\n");
                return -1;
            }
            else
            {
                printf("ERROR. Terminating...\n");
                return -1;
            }
        }
    }
    else if(argc == 0)
    {
        printf("Topology file and routing update interval not specified during start up.\n");
        printUsage();
        printf("\n");
    }
    else{
        printf("Invalid command line arguments.\n");
        printUsage();
        printf("\n");
    }

    return 0;
}


