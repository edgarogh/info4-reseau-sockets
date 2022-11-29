#include <stdbool.h>

#include "codec.h"

typedef void* followee_iterator;

/**
 * Initialise la base de données
 *
 * Doit être appelé avant d'utiliser les fonctions ci-dessous
 */
void database_initialize();

/**
 * Enregistre l'état d'un utilisateur dans la base de données (et le créé si besoin)
 *
 * Cette fonction devrait être appelée à la connexion de chaque utilisateur, sans quoi il risque d'être absent dans la
 * base de données s'il s'agit de sa promise connexion, ce qui lui empêche de faire la plupart des actions.
 * Cette fonction DOIT être appelée à la déconnexion de chaque utilisateur, afin d'enregistrer la date de déconnexion et
 * de permettre de rattraper les twiiiiits manqués ultérieurement.
 */
void database_update_user(char* user, bool is_online);

enum subscribe_result database_follow(char* follower, char* followee);
enum subscribe_result database_unfollow(char* follower, char* followee);

/**
 * Renvoie la liste d'abonnements d'un utilisateur donné, sous forme d'itérateur. Ainsi, l'énumération se fait
 * progressivement, au besoin, sans allouer de mémoire.
 */
followee_iterator database_list_followee(char* follower);

/**
 * Avance dans l'itérateur d'énumération des abonnements
 *
 * Si l'itérateur arrive à la fin de l'énumération, la fonction renvoie `false` et les ressources associées au curseur
 * sont libérées. Il n'est alors plus autorisé de rappeler cette fonction sur le même itérateur.
 * Sinon, elle renvoie `true`, écrit le nom de l'abonnement dans `out` et avance son curseur interne.
 */
bool database_list_followee_next(followee_iterator cursor, user_name* out);
