#include <stdio.h>
#include <malloc.h>
#include "twiit_list.h"

twiiiiit_list* twiit_list_new(void)
{
    twiiiiit_list *new = malloc(sizeof *new);
    if (new != NULL)
    {
        new->length = 0;
        new->head = NULL;
        new->tail = NULL;
    }
    return new;
}

twiiiiit_list* twiiiiit_append(twiiiiit_list* list, database_twiiiiit twiiiiit)
{
    if (list != NULL) /* On vérifie si notre liste a été allouée */
    {
        struct twiiiiit_list_node *new = malloc(sizeof *new); /* Création d'un nouveau node */
        if (new != NULL) /* On vérifie si le malloc n'a pas échoué */
        {
            new->twiiiiit = twiiiiit; /* On 'enregistre' notre donnée */
            new->next = NULL; /* On fait pointer p_next vers NULL */
            if (list->tail == NULL) /* Cas où notre liste est vide (pointeur vers fin de liste à  NULL) */
            {
                new->prev = NULL; /* On fait pointer p_prev vers NULL */
                list->head = new; /* On fait pointer la tête de liste vers le nouvel élément */
                list->tail = new; /* On fait pointer la fin de liste vers le nouvel élément */
            }
            else /* Cas où des éléments sont déjà présents dans notre liste */
            {
                list->tail->next = new; /* On relie le dernier élément de la liste vers notre nouvel élément (début du chaînage) */
                new->prev = list->tail; /* On fait pointer p_prev vers le dernier élément de la liste */
                list->tail = new; /* On fait pointer la fin de liste vers notre nouvel élément (fin du chaînage: 3 étapes) */
            }
            list->length++; /* Incrémentation de la taille de la liste */
        }
    }
    return list; /* on retourne notre nouvelle liste */
}

twiiiiit_list* twiiiiit_prepend(twiiiiit_list* list, database_twiiiiit twiiiiit)
{
    if (list != NULL)
    {
        struct twiiiiit_list_node *new = malloc(sizeof *new);
        if (new != NULL)
        {
            new->twiiiiit = twiiiiit;
            new->prev = NULL;
            if (list->tail == NULL)
            {
                new->next = NULL;
                list->head = new;
                list->tail = new;
            }
            else
            {
                list->head->prev = new;
                new->next = list->head;
                list->head = new;
            }
            list->length++;
        }
    }
    return list;
}

void twiiiiit_delete(twiiiiit_list **list)
{
    if (*list != NULL)
    {
        struct twiiiiit_list_node *temp = (*list)->head;
        while (temp != NULL)
        {
            struct node *del = temp;
            temp = temp->next;
            free(del);
        }
        free(*list), *list = NULL;
    }
}
