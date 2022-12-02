/**
 * Le système d'encodage-décodage des messages est conçu pour fonctionner sur des processeurs de boutisme et de taille
 * de mots différent·s. Il est également conçu pour tolérer certaines erreurs et ne pas planter en cas de message
 * invalide.
 */

#include <byteswap.h>
#include <endian.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "codec.h"

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define htonll(value) bswap_64(value)
#else
#define htonll(value) value
#endif

void encode_s2c(const message_s2c* msg, char* restrict frame) {
    uint32_t n_tag = htonl(msg->tag);
    memcpy(frame, &n_tag, sizeof n_tag);
    frame += sizeof(uint32_t);

    switch (msg->tag) {
        case MESSAGE_S2C_LOGIN_STATUS:
        case MESSAGE_S2C_SUBSCRIBE_RESULT:
        case MESSAGE_S2C_KICK:
            n_tag = htonl(msg->login_status);
            memcpy(frame, &n_tag, sizeof n_tag);
            return;
        case MESSAGE_S2C_RECEIVED_MESSAGE:;
            int64_t time = htonll(msg->received_message.date);
            memcpy(frame, &time, sizeof time);
            frame += sizeof time;
            memcpy(frame, &msg->received_message.author, MAX_USERNAME_LENGTH);
            frame += MAX_USERNAME_LENGTH;
            memcpy(frame, &msg->received_message.message, MESSAGE_MAX_LENGTH);
            return;
        case MESSAGE_S2C_SUBSCRIPTION_ENTRY:
            memcpy(frame, msg->subscription_entry, MAX_USERNAME_LENGTH);
            return;
    }
}

bool decode_s2c(const char* frame, message_s2c* restrict msg) {
    uint32_t tag = ntohl(*((uint32_t*) frame));
    if (tag > MESSAGE_S2C_KICK) {
        printf("[ERROR] Tag S2C invalide %d\n", tag);
        return false;
    }
    frame += sizeof tag;

    switch (msg->tag = tag) {
        case MESSAGE_S2C_LOGIN_STATUS:
        case MESSAGE_S2C_SUBSCRIBE_RESULT:
        case MESSAGE_S2C_KICK:;
            int enum_len = msg->tag == MESSAGE_S2C_KICK ? 2 : 3;
            tag = ntohl(*((uint32_t*) frame));
            if (tag >= enum_len) {
                printf("[ERROR] Tag S2C interne invalide %d (>= %d)\n", tag, enum_len);
                return false;
            }
            msg->login_status = tag;
            return true;
        case MESSAGE_S2C_RECEIVED_MESSAGE:
            memcpy(&msg->received_message.date, frame, sizeof(int64_t));
            msg->received_message.date = htonll(msg->received_message.date);
            frame += sizeof(int64_t);
            memset(&msg->received_message.author, 0, MAX_USERNAME_LENGTH + MESSAGE_MAX_LENGTH);
            strncpy(msg->received_message.author, frame, MAX_USERNAME_LENGTH);
            frame += MAX_USERNAME_LENGTH;
            strncpy(msg->received_message.message, frame, MESSAGE_MAX_LENGTH);
            return true;
        case MESSAGE_S2C_SUBSCRIPTION_ENTRY:
            memcpy(msg->subscription_entry, frame, MESSAGE_MAX_LENGTH);
            return true;
    }
}

void encode_c2s(const message_c2s* msg, char* restrict frame) {
    uint32_t n_tag = htonl(msg->tag);
    memcpy(frame, &n_tag, sizeof n_tag);
    frame += sizeof(uint32_t);

    switch (msg->tag) {
        case MESSAGE_C2S_JOIN_AS:
        case MESSAGE_C2S_SUBSCRIBE_TO:
        case MESSAGE_C2S_UNSUBSCRIBE_TO:
            memcpy(frame, msg->join_as, MAX_USERNAME_LENGTH);
            return;
        case MESSAGE_C2S_LIST_SUBSCRIPTIONS:
            return;
        case MESSAGE_C2S_PUBLISH:
            memcpy(frame, msg->publish, MESSAGE_MAX_LENGTH);
    }
}

bool decode_c2s(const char* frame, message_c2s* restrict msg) {
    uint32_t tag = ntohl(*((uint32_t*) frame));
    if (tag > MESSAGE_C2S_PUBLISH) {
        printf("[ERROR] Tag C2S invalide %d\n", tag);
        return false;
    }
    frame += sizeof tag;

    switch (msg->tag = tag) {
        case MESSAGE_C2S_JOIN_AS:
        case MESSAGE_C2S_SUBSCRIBE_TO:
        case MESSAGE_C2S_UNSUBSCRIBE_TO:
            // memset pas forcément nécessaire, mais plus prudent
            memset(msg->join_as, 0, MAX_USERNAME_LENGTH);
            strncpy(msg->join_as, frame, MAX_USERNAME_LENGTH);
            return true;
        case MESSAGE_C2S_LIST_SUBSCRIPTIONS:
            return true;
        case MESSAGE_C2S_PUBLISH:
            memset(msg->publish, 0, MESSAGE_MAX_LENGTH);
            strncpy(msg->publish, frame, MESSAGE_MAX_LENGTH);
            return true;
    }
}
