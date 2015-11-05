//
// Created by barry on 11/4/15.
//

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "list.h"
#include "Server.h"

int addItem(list **listInstance, void *item) {
    listItem *currentItem = *listInstance;

    if (item == NULL) {
        fprintf(stderr, "node is NULL in addNode()\n");
        return -1;
    }

    //create a new list item
    listItem *newItem = (listItem *)malloc(sizeof(listItem));
    newItem->item = item;
    newItem->next = NULL;

    //insert list item at the head
    if (*listInstance == NULL) {
        *listInstance = newItem;
        (*listInstance)->next = NULL;
        //printf("adding head\n");
    }
    else {
        //insert list item later than head
        while (currentItem->next != NULL) {
            currentItem = currentItem->next;
        }
        currentItem->next = newItem;
        //currentItem->next->next = NULL;
        //printf("adding other nodes\n");
    }
    return 0;
}

int printList(list *listInstance) {
    listItem *currentItem = listInstance;
    if (currentItem == NULL) {
        fprintf(stdout, "List is empty\n");
        return 0;
    }
    else {
        do {
            //specific to node struct
            node *currentNode = (node *) currentItem->item;
            printf("%d %s %d %d\n", currentNode->id, currentNode->ipAddress,
                   currentNode->port, currentNode->cost);
            currentItem = currentItem->next;
        }while (currentItem != NULL);
    }
    return 0;
}

void *findNodeByID(list *listInstance, int id) {
    listItem *currentItem = listInstance;
    if (currentItem == NULL) {
        fprintf(stdout, "List is empty\n");
        return NULL;
    }
    else {
        do {
            node *currentNode = (node *) currentItem->item;
            if (currentNode->id == id)
                return currentNode;
            currentItem = currentItem->next;
        } while (currentItem != NULL);
        fprintf(stdout, "Id not found\n");
        return NULL;
    }
}
