#include "constants.h"

typedef struct {
    long date;
    user_name author;
    char message[MESSAGE_MAX_LENGTH];
} received_message;

/**
 * Un message serveur vers client
 */
typedef struct {
    enum {
        MESSAGE_S2C_LOGIN_STATUS, // réponse à MESSAGE_C2S_JOIN_AS
        MESSAGE_S2C_RECEIVED_MESSAGE,
        MESSAGE_S2C_SUBSCRIBE_RESULT, // réponse à MESSAGE_C2S_SUBSCRIBE_TO et MESSAGE_C2S_UNSUBSCRIBE_TO
        MESSAGE_S2C_SUBSCRIPTION_ENTRY, // réponses multiples à MESSAGE_C2S_LIST_SUBSCRIPTIONS
        MESSAGE_S2C_KICK,
    } tag;
    union {
        enum {
            LOGIN_STATUS_OK,
            LOGIN_STATUS_ALREADY_USED,
            LOGIN_STATUS_ILLEGAL_NAME,
        } login_status;
        received_message received_message;
        enum subscribe_result {
            SUBSCRIBE_RESULT_OK,
            SUBSCRIBE_RESULT_NOT_FOUND,
            SUBSCRIBE_RESULT_UNCHANGED, // Si on suit déjà (resp. déjà désabonné)
        } subscribe_result;
        user_name subscription_entry; // Contient que des 0 à la fin de l'énumération
        enum {
            KICK_REASON_CLOSING,
            KICK_REASON_PROTOCOL_ERROR,
        } kick;
    };
} message_s2c;

/**
 * Un message client vers serveur
 */
typedef struct {
    enum {
        MESSAGE_C2S_JOIN_AS,
        MESSAGE_C2S_SUBSCRIBE_TO,
        MESSAGE_C2S_UNSUBSCRIBE_TO,
        MESSAGE_C2S_LIST_SUBSCRIPTIONS,
        MESSAGE_C2S_PUBLISH,
    } tag;
    union {
        user_name join_as;
        user_name subscribe_to;
        user_name unsubscribe_to;
        char publish[MESSAGE_MAX_LENGTH];
    };
} message_c2s;
