#ifndef TWIIIIITER_TWIIT_LIST_H
#define TWIIIIITER_TWIIT_LIST_H

#include "../server/database.h"

struct twiiiiit_list_node {
    database_twiiiiit twiiiiit;
    struct twiiiiit_list_node *next;
    struct twiiiiit_list_node *prev;
};

typedef struct twiiiiit_list_s
{
    size_t length;
    struct twiiiiit_list_node *tail;
    struct twiiiiit_list_node *head;
} twiiiiit_list;

#endif //TWIIIIITER_TWIIT_LIST_H
