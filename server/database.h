#include <stdbool.h>
#include <time.h>

#include "codec.h"
#include "constants.h"

typedef struct {
    int64_t date;
    user_name author;
    char message[MESSAGE_MAX_LENGTH];
} database_twiiiiit;

typedef void* user_iterator;
typedef void* twiiiiit_iterator;

/**
 * Initialise la base de données
 *
 * Doit être appelé avant d'utiliser les fonctions ci-dessous
 */
void database_initialize(const char* database_file);

/**
 * Enregistre l'état d'un utilisateur dans la base de données (et le créé si besoin)
 *
 * Cette fonction devrait être appelée à la connexion de chaque utilisateur, sans quoi il risque d'être absent dans la
 * base de données s'il s'agit de sa promise connexion, ce qui lui empêche de faire la plupart des actions.
 * Cette fonction DOIT être appelée à la déconnexion de chaque utilisateur, afin d'enregistrer la date de déconnexion et
 * de permettre de rattraper les twiiiiits manqués ultérieurement.
 */
void database_update_user(const char* user, bool is_online);

enum subscribe_result database_follow(const char* follower, const char* followee);
enum subscribe_result database_unfollow(const char* follower, const char* followee);

/**
 * Renvoie la liste d'abonnements d'un utilisateur donné, sous forme d'itérateur. Ainsi, l'énumération se fait
 * progressivement, au besoin, sans allouer de mémoire.
 */
user_iterator database_list_followee(const char* follower);

/**
 * Renvoie la liste d'abonnements d'un utilisateur donné, sous forme d'itérateur.
 *
 * @see database_list_followee()
 */
user_iterator database_list_followers(const char* followee);

/**
 * Avance dans un itérateur d'énumération d'utilisateurs
 *
 * Si l'itérateur arrive à la fin de l'énumération, la fonction renvoie `false` et les ressources associées au curseur
 * sont libérées. Il n'est alors plus autorisé de rappeler cette fonction sur le même itérateur.
 * Sinon, elle renvoie `true`, écrit le nom de l'abonnement dans `out` et avance son curseur interne.
 */
bool database_users_next(user_iterator restrict cursor, char* restrict out);

/**
 * Publie un twiiiiit dont `author` est l'auteur·ice
 *
 * Passer à cette fonction un auteur dont le nom n'est pas associé à un utilisateur est considéré comme une erreur, d'où
 * la nécessité de bien appeler database_update_user() lors de la connexion de n'importe qui.
 */
time_t database_save_twiiiiit(const char* author, const char* message);

/**
 * Renvoie un itérateur sur les twiiiiits manqués par un utilisateur donné, du plus ancien au plus récent
 *
 * Cette fonction devrait être appelée à la reconnexion, avant database_update_user(), sans quoi la dernière date de
 * déconnexion serait écrasée trop tôt. Il n'est pas illégal d'appeler cette fonction avec un `follower` qui n'est pas
 * encore enregistré dans la BDD, et cela est même voué à arriver à chaque première connexion.
 *
 * @see database_list_missed_twiiiiits_next()
 */
twiiiiit_iterator database_list_missed_twiiiiits(const char* follower);

/**
 * Avance dans un itérateur de twiiiiits et renvoie chaque twiiiiit par le second argument
 *
 * Les précautions sont les mêmes que database_users_next()
 */
bool database_twiiiiits_next(twiiiiit_iterator restrict iterator, database_twiiiiit* restrict out);
