/*
 * server.c
 * Author: Murad Mikayilzade
 *
 * Resources Used:
 * https://github.com/codeplea/Hands-On-Network-Programming-with-C
 * https://beej.us/guide/bgnet/html/multi/index.html
 * https://www.geeksforgeeks.org/socket-programming-cc/

 * Hashmap and LinkedList implementations are provided by Prof. Joe Sventek
 */

#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "hashmap.h"
#include "linkedlist.h"
#include "duckchat.h"


#define DEFAULT_CHANNEL "Common"
#define MAX_CHANNELS 10
#define UNUSED __attribute__((unused))

int socket_fd;
HashMap *users = NULL;
HashMap *channels = NULL;
struct sockaddr_in client, server;

typedef struct {
    struct sockaddr_in *addr;
    char *ip_addr;
    char *username;
    LinkedList *channels;
} User;

User *malloc_user(const char *ip, const char *name, struct sockaddr_in *addr) {

    User *new_user = (User *)malloc(sizeof(User));

    if (new_user != NULL) {
        new_user->addr = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
        new_user->ip_addr = (char *)malloc(strlen(ip) + 1);
        new_user->username = (char *)malloc(strlen(name) + 1);
        new_user->channels = ll_create();

        *new_user->addr = *addr;
        strcpy(new_user->ip_addr, ip);
        strcpy(new_user->username, name);
    }

    return new_user;    
}

void free_user(User *user) {
    free(user->addr);
    free(user->ip_addr);
    free(user->username);
    ll_destroy(user->channels, free);
    free(user);
}

void server_send_error(struct sockaddr_in *addr, char *msg) {
    struct text_error error_packet;
    error_packet.txt_type = TXT_ERROR;
    strncpy(error_packet.txt_error, msg, (SAY_MAX - 1));
    sendto(socket_fd, &error_packet, sizeof(error_packet), 0, (struct sockaddr *)addr, sizeof(*addr));
}

void server_login_request(char *packet, char *client_ip, struct sockaddr_in *addr) {

    struct request_login *login_packet = (struct request_login *) packet;
    char name[USERNAME_MAX];
    strcpy(name, login_packet->req_username);

    User *user = malloc_user(client_ip, name, addr);
    if(user == NULL || !hm_put(users, client_ip, user, NULL)){
        server_send_error(addr, "Failed to log into the server.");
        if (user != NULL)
            free_user(user);
        return;
    }

    printf("%s logged in to the chat\n", user->username);
    return;
    
}

void server_logout_request(char *client_ip) {

    User *user;
    if (!hm_remove(users, client_ip, (void **)&user))
        return;

    printf("%s logged out\n", user->username);

    User *tmp;
    LinkedList *u_list;
    char *ch;

    while (ll_removeFirst(user->channels, (void **)&ch)) {

        if (!hm_get(channels, ch, (void **)&u_list)) {
            free(ch);
            continue;
        }

        for (long i = 0L; i < ll_size(u_list); i++) {
            (void)ll_get(u_list, i, (void **)&tmp);
            if (strcmp(user->ip_addr, tmp->ip_addr) == 0) {
                (void)ll_remove(u_list, i, (void **)&tmp);
            }
        }

        if (ll_isEmpty(u_list) && strcmp(ch, DEFAULT_CHANNEL)) {
            (void)hm_remove(channels, ch, (void **)&u_list);
            ll_destroy(u_list, NULL);
            printf("Removed the empty channel %s\n", ch);
        }
        free(ch);
    }
    free_user(user);
}

void server_join_request(char *packet, char *client_ip) {
    
    User *user, *tmp;
    LinkedList *u_list = NULL;
    int len;
    long i;
    char *channel_name = NULL;
    struct request_join *join_packet = (struct request_join *) packet;

    if (!hm_get(users, client_ip, (void **)&user))
        return;

    if(strlen(join_packet->req_channel) > (CHANNEL_MAX - 1))
        len = CHANNEL_MAX - 1;
    else 
        strlen(join_packet->req_channel);

    channel_name = (char *)malloc(len + 1);
    memcpy(channel_name, join_packet->req_channel, len);
    channel_name[len] = '\0';

    ll_add(user->channels, channel_name);

    if (!hm_get(channels, channel_name, (void **)&u_list)) {
        u_list = ll_create();
        ll_add(u_list, user);
        hm_put(channels, channel_name, u_list, NULL);
        printf("%s created the channel %s\n", user->username, channel_name);
    } else {
        for (i = 0L; i < ll_size(u_list); i++) {
            (void)ll_get(u_list, i, (void **)&tmp);
            if (strcmp(user->ip_addr, tmp->ip_addr) == 0) {
                printf("%s joined the channel %s\n", user->username, channel_name);
                return;
            }
        }
        ll_add(u_list, user);
        printf("%s joined the channel %s\n", user->username, channel_name);
        return;
    }
}

void server_leave_request(char *packet, char *client_ip) {

    User *user, *tmp;
    LinkedList *u_list;
    long i;
    char *ch;
    char channel[CHANNEL_MAX];
    struct request_leave *leave_packet = (struct request_leave *) packet;

    if (!hm_get(users, client_ip, (void **)&user))
        return;

    memset(channel, 0, sizeof(channel));
    strncpy(channel, leave_packet->req_channel, (CHANNEL_MAX - 1));

    if (!hm_get(channels, channel, (void **)&u_list)) {
        printf("Channel named %s does not exist\n", leave_packet->req_channel);
        server_send_error(user->addr, "Channel you are trying to delete do not exist.\n");
        return;
    }

    // unsubsribing user from this channel
    for (i = 0L; i < ll_size(user->channels); i++) {
        (void)ll_get(user->channels, i, (void **)&ch);
        if (strcmp(channel, ch) == 0) {
            ll_remove(user->channels, i, (void **)&ch);
            free(ch);
            printf("%s left the channel %s\n", user->username, channel);
            break;
        }
    }

    for (i = 0L; i < ll_size(u_list); i++) {
        (void)ll_get(u_list, i, (void **)&tmp);
        if (strcmp(user->ip_addr, tmp->ip_addr) == 0) {
            (void)ll_remove(u_list, i, (void **)&tmp);
            break;
        }
    }

    if (ll_isEmpty(u_list) && strcmp(channel, DEFAULT_CHANNEL)) {
        (void)hm_remove(channels, channel, (void **)&u_list);
        ll_destroy(u_list, NULL);
        printf("Removed the empty channel %s\n", channel);
    }

    return;
}

void server_say_request(char *packet, char *client_ip) {
    
    User *user;
    long len;
    if (!hm_get(users, client_ip, (void **)&user))
        return;

    struct request_say *say_packet = (struct request_say *) packet;
    struct text_say msg_packet;
    
    LinkedList *ch_users;
    if (!hm_get(channels, say_packet->req_channel, (void **)&ch_users))
        return;

    User **listeners;
    listeners = (User **)ll_toArray(ch_users, &len);

    msg_packet.txt_type = TXT_SAY;
    strncpy(msg_packet.txt_channel, say_packet->req_channel, (CHANNEL_MAX - 1));
    strncpy(msg_packet.txt_username, user->username, (USERNAME_MAX - 1));
    strncpy(msg_packet.txt_text, say_packet->req_text, (SAY_MAX - 1));

    for (long i = 0L; i < len; i++)
        sendto(socket_fd, &msg_packet, sizeof(msg_packet), 0, (struct sockaddr *)listeners[i]->addr, sizeof(*listeners[i]->addr));

    printf("[%s][%s]: \"%s\"\n", msg_packet.txt_channel, user->username, msg_packet.txt_text);

    free(listeners);
}

void server_list_request(char *client_ip) {

    User *user;
    if (!hm_get(users, client_ip, (void **)&user))
        return;

    size_t nbytes;
    long len = 0L;
    char **channel_list = NULL;
    struct text_list *list_packet = NULL;


    channel_list = hm_keyArray(channels, &len);

    nbytes = sizeof(struct text_list) + (sizeof(struct channel_info) * len);
    list_packet = malloc(nbytes);
    list_packet->txt_type = TXT_LIST;
    list_packet->txt_nchannels = (int)len;

    for (long i = 0L; i < len; i++)
        strncpy(list_packet->txt_channels[i].ch_channel, channel_list[i], (CHANNEL_MAX - 1));

    sendto(socket_fd, list_packet, nbytes, 0, (struct sockaddr *)user->addr, sizeof(*user->addr));
    printf("%s listed available channels on server\n", user->username);

    free(channel_list);
    free(list_packet);
    return;
}

void server_who_request(const char *packet, char *client_ip) {

    User *user, **user_list = NULL;
    if (!hm_get(users, client_ip, (void **)&user))
        return;

    LinkedList *users;
    size_t nbytes;
    long len = 0L;
    struct text_who *send_packet = NULL;
    struct request_who *who_packet = (struct request_who *) packet;

    if (!hm_get(channels, who_packet->req_channel, (void **)&users)) {
        printf("Channel named %s does not exist\n", send_packet->txt_channel);
        server_send_error(user->addr, "Channel does not exist.\n");
        return;
    }

    user_list = (User **)ll_toArray(users, &len);

    nbytes = sizeof(struct text_who) + (sizeof(struct user_info) * len);
    send_packet = malloc(nbytes);
    send_packet->txt_type = TXT_WHO;
    send_packet->txt_nusernames = (int)len;
    strncpy(send_packet->txt_channel, who_packet->req_channel, (CHANNEL_MAX - 1));

    for (long i = 0L; i < len; i++)
        strncpy(send_packet->txt_users[i].us_username, user_list[i]->username, (USERNAME_MAX - 1));

    sendto(socket_fd, send_packet, nbytes, 0, (struct sockaddr *)user->addr, sizeof(*user->addr));
    printf("%s listed all users on channel %s", user->username, who_packet->req_channel);

    free(user_list);
    free(send_packet);
    return;
}


// Server Driver Code
int main(int argc, char *argv[]) {

    if (argc != 3) {
        printf("Usage: ./server domain_name port_number\n");
        exit(EXIT_FAILURE);
    }

    LinkedList *default_ll;

    struct timeval tv;
    tv.tv_sec = 300;
    tv.tv_usec = 100000;

    socklen_t addr_len = sizeof(client);
    fd_set receiver;
    char buffer[1024], client_ip[64];
    struct text *packet_type;

    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(argv[2]));
    memcpy((char *)&server.sin_addr, (char *)gethostbyname(argv[1])->h_addr_list[0], gethostbyname(argv[1])->h_length);

    if ((socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        printf("Failed to create a socket.\n");
        exit(EXIT_FAILURE);
    }
    if (bind(socket_fd, (struct sockaddr *)&server, sizeof(server)) < 0){
        printf("Failed bind.\n");
        exit(EXIT_FAILURE);
    }

    users = hm_create(100L, 0.0f);
    channels = hm_create(100L, 0.0f);
    default_ll = ll_create();

    if(users == NULL || channels == NULL || default_ll == NULL || !hm_put(channels, DEFAULT_CHANNEL, default_ll, NULL)){
        printf("Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }


    while (1) {

        FD_ZERO(&receiver);
        FD_SET(socket_fd, &receiver);
        select((socket_fd + 1), &receiver, NULL, NULL, &tv);
    
        memset(buffer, 0, sizeof(buffer));
        if (recvfrom(socket_fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&client, &addr_len) < 0)
            continue;
        sprintf(client_ip, "%s:%d", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
        packet_type = (struct text *) buffer;
        switch (packet_type->txt_type) {
            case REQ_LOGIN:
                server_login_request(buffer, client_ip, &client);
                break;
            case REQ_LOGOUT:
                server_logout_request(client_ip);
                break;
            case REQ_JOIN:
                server_join_request(buffer, client_ip);
                break;
            case REQ_LEAVE:
                server_leave_request(buffer, client_ip);
                break;
            case REQ_SAY:
                server_say_request(buffer, client_ip);
                break;
            case REQ_LIST:
                server_list_request(client_ip);
                break;
            case REQ_WHO:
                server_who_request(buffer, client_ip);
                break;
            default:
                break;
        }
    }

    return 0;
}