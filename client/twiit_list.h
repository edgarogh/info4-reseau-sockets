#ifndef TWIIIIITER_TWIIT_LIST_H
#define TWIIIIITER_TWIIT_LIST_H

#include "../server/database.h"

struct twiiiiit_list_node {
    received_message twiiiiit;
    struct twiiiiit_list_node *next;
    struct twiiiiit_list_node *prev;
};

typedef struct twiiiiit_list_s
{
    size_t length;
    struct twiiiiit_list_node *tail;
    struct twiiiiit_list_node *head;
} twiiiiit_list;

twiiiiit_list* twiit_list_new(void);
twiiiiit_list* twiiiiit_append(twiiiiit_list* list, received_message twiiiiit);
twiiiiit_list* twiiiiit_prepend(twiiiiit_list* list, received_message twiiiiit);
void twiiiiit_delete(twiiiiit_list **list);


#endif //TWIIIIITER_TWIIT_LIST_H
