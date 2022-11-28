#include <stdbool.h>

typedef void* followee_iterator;

/**
 * Initialise la base de données
 *
 * Doit être appelé avant d'utiliser les fonctions ci-dessous
 */
void database_initialize();

/**
 * Renvoie la liste d'abonnements d'un utilisateur donné, sous forme d'itérateur. Ainsi, l'énumération se fait
 * progressivement, au besoin, sans allouer de mémoire.
 */
followee_iterator database_list_followee(char* follower);

/**
 * Avance dans l'itérateur d'énumération des abonnements
 *
 * Si l'itérateur arrive à la fin de l'énumération, la fonction renvoie `false` et les ressources associées au curseur
 * sont libérées.
 * Sinon, elle renvoie `true`, écrit le nom de l'abonnement dans `out` et avance son curseur interne.
 */
bool database_list_followee_next(followee_iterator cursor, user_name* out);
