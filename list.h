//
// Created by barry on 11/4/15.
//

#ifndef DISTANCE_VECTOR_ROUTING_PROTOCOL_IMPLEMENTATION_LIST_H
#define DISTANCE_VECTOR_ROUTING_PROTOCOL_IMPLEMENTATION_LIST_H

#include <stdint.h>

typedef struct listItem {
    void *item;
    struct listItem *next;
}listItem, list;

int addItem(list **listInstance, void *item);
int printList(list *listInstance, char * listType);
void *findRowByID(list *listInstance, int id);
int getSize(list *listInstance);
void *findRowByIPandPort(list *listInstance, char *ip, uint16_t port);
void *findNeighbourByID(list *listInstance, int id);
#endif //DISTANCE_VECTOR_ROUTING_PROTOCOL_IMPLEMENTATION_LIST_H

