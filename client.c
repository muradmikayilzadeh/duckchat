/*
 * client.c
 * Author: Murad Mikayilzade
 *
 * Resources Used:
 * https://github.com/codeplea/Hands-On-Network-Programming-with-C
 * https://beej.us/guide/bgnet/html/multi/index.html
 * https://www.geeksforgeeks.org/socket-programming-cc/
 */

#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "raw.h"
#include "duckchat.h"


#define DEFAULT_CHANNEL "Common"
#define MAX_CHANNELS 10
#define UNUSED __attribute__((unused))

struct sockaddr_in server;
char username[USERNAME_MAX];
char active_channel[CHANNEL_MAX];
char subscribed[MAX_CHANNELS][CHANNEL_MAX];
int socket_fd;


void client_logout_request(void) {    
    struct request_logout logout_packet;
    logout_packet.req_type = REQ_LOGOUT;
    sendto(socket_fd, &logout_packet, sizeof(logout_packet), 0, (struct sockaddr *)&server, sizeof(server));
    exit(EXIT_SUCCESS);
}

void client_join_request(char *channel_name) {

    ++channel_name;
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (strcmp(subscribed[i], channel_name) == 0) {
            strncpy(active_channel, subscribed[i], (CHANNEL_MAX - 1));
            break;
        }
    }

    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (strcmp(subscribed[i], "") == 0) {
            strncpy(subscribed[i], channel_name, (CHANNEL_MAX - 1));
            strncpy(active_channel, channel_name, (CHANNEL_MAX - 1));
            break;
        }
    }

    struct request_join join_packet;
    join_packet.req_type = REQ_JOIN;
    strncpy(join_packet.req_channel, channel_name, (CHANNEL_MAX - 1));
    sendto(socket_fd, &join_packet, sizeof(join_packet), 0, (struct sockaddr *)&server, sizeof(server));
}

void client_leave_request(char *channel_name) {
    
    ++channel_name;
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (strcmp(subscribed[i], channel_name) == 0) {
            strcpy(subscribed[i], "");
            if (strcmp(active_channel, channel_name) == 0)
                strcpy(active_channel, "");
            break;
        }
    }

    struct request_leave leave_packet;
    leave_packet.req_type = REQ_LEAVE;
    strncpy(leave_packet.req_channel, channel_name, (CHANNEL_MAX - 1));
    sendto(socket_fd, &leave_packet, sizeof(leave_packet), 0, (struct sockaddr *)&server, sizeof(server));

    printf("You left channel: %s\n", channel_name);
}

void client_say_request(char *request) {
    
    if (strcmp(active_channel, "") == 0)
        return;

    struct request_say say_packet;
    say_packet.req_type = REQ_SAY;
    strncpy(say_packet.req_channel, active_channel, (CHANNEL_MAX - 1));
    strncpy(say_packet.req_text, request, (SAY_MAX - 1));
    sendto(socket_fd, &say_packet, sizeof(say_packet), 0, (struct sockaddr *)&server, sizeof(server));
}

void server_say_reply(const char *packet) {
    struct text_say *say_packet = (struct text_say *) packet;
    printf("[%s][%s]: %s\n", say_packet->txt_channel, say_packet->txt_username, say_packet->txt_text);
}

void client_list_request(void) {
    struct request_list list_packet;
    list_packet.req_type = REQ_LIST;
    sendto(socket_fd, &list_packet, sizeof(list_packet), 0, (struct sockaddr *)&server, sizeof(server));
}

void server_list_reply(const char *packet) {
    
    struct text_list *list_packet = (struct text_list *) packet;

    printf("Existing channels:\n");
    for (int i = 0; i < list_packet->txt_nchannels; i++){
        fprintf(stdout, "  %s\n", list_packet->txt_channels[i].ch_channel);
    }
}

void client_who_request(char *channel_name) {
    struct request_who who_packet;

    who_packet.req_type = REQ_WHO;
    strncpy(who_packet.req_channel, ++channel_name, (CHANNEL_MAX - 1));
    sendto(socket_fd, &who_packet, sizeof(who_packet), 0, (struct sockaddr *)&server, sizeof(server));
}

void server_who_reply(char *packet) {
    struct text_who *who_packet = (struct text_who *) packet;

    printf("Users on channel %s:\n", who_packet->txt_channel);
    for (int i = 0; i < who_packet->txt_nusernames; i++){
        printf("  %s\n", who_packet->txt_users[i].us_username);
    }
}

void client_switch_request(char *channel_name) {
    ++channel_name;
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (strcmp(subscribed[i], channel_name) == 0) {
            strncpy(active_channel, channel_name, (CHANNEL_MAX - 1));
            return;
        }
    }    
    fprintf(stdout, "Error: You are not subscribed to the channel: %s. Use /join command to subscribe/create to the channel\n", channel_name);
}


// Prints error message received from server
void server_error_reply(char *packet) {
    struct text_error *error_packet = (struct text_error *) packet;
    printf("Error: %s\n", error_packet->txt_error);
}

// Driver code
int main(int argc, char *argv[]) {

    if (argc != 4) {
        printf("Usage: ./client server_socket server_port username\n");
        exit(EXIT_FAILURE);
    }

    struct sockaddr from_addr;
    socklen_t len = sizeof(server);

    struct timeval tv;
    tv.tv_sec = 300;
    tv.tv_usec = 100000;

    raw_mode();

    if ((socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        printf("Failed to create a socket.\n");
        exit(EXIT_FAILURE);
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(argv[2]));
    memcpy((char *)&server.sin_addr, (char *)gethostbyname(argv[1])->h_addr_list[0], gethostbyname(argv[1])->h_length);

    strncpy(username, argv[3], (USERNAME_MAX - 1));
    strncpy(active_channel, DEFAULT_CHANNEL, (CHANNEL_MAX - 1));
    strncpy(subscribed[0], DEFAULT_CHANNEL, (CHANNEL_MAX - 1));

    for (int i = 1; i < MAX_CHANNELS; i++)
        strcpy(subscribed[i], "");


    struct request_login login_packet;
    login_packet.req_type = REQ_LOGIN;
    strncpy(login_packet.req_username, username, (USERNAME_MAX - 1));
    sendto(socket_fd, &login_packet, sizeof(login_packet), 0, (struct sockaddr *)&server, sizeof(server));

    struct request_join join_packet;
    join_packet.req_type = REQ_JOIN;
    strncpy(join_packet.req_channel, DEFAULT_CHANNEL, (CHANNEL_MAX - 1));
    sendto(socket_fd, &join_packet, sizeof(join_packet), 0, (struct sockaddr *)&server, sizeof(server));


    printf("> ");
    fflush(stdout);

    fd_set receiver;
    char ch;
    int i, j;
    char buffer[1024], in_buff[100024];
    struct text *packet_type;
   
    while (1) {

        FD_ZERO(&receiver);
        FD_SET(socket_fd, &receiver);
        FD_SET(STDIN_FILENO, &receiver);


        if (select((socket_fd + 1), &receiver, NULL, NULL, &tv) < 0 ) {
            printf("Select failed\n");
        }

        else{
        
            if (FD_ISSET(socket_fd, &receiver)) {

                memset(in_buff, 0, sizeof(in_buff));
                if (recvfrom(socket_fd, in_buff, sizeof(in_buff), 0, &from_addr, &len) < 0)
                    continue;
                packet_type = (struct text *) in_buff;

                putchar('\r');

                switch (packet_type->txt_type) {
                    case TXT_SAY:
                        server_say_reply(in_buff);
                        break;
                    case TXT_LIST:
                        server_list_reply(in_buff);
                        break;
                    case TXT_WHO:
                        server_who_reply(in_buff);
                        break;
                    case TXT_ERROR:
                        server_error_reply(in_buff);
                        break;
                    default:
                        break;
                }

                printf("> ");
                fflush(stdout);
                for (j = 0; j < i; j++)
                    putchar(buffer[j]);
                fflush(stdout);
            }

            if (FD_ISSET(STDIN_FILENO, &receiver)) {

                if ((ch = getchar()) != '\n') {
                    if (ch == 127) {
                        if (i == 0)
                            continue;
                        i--;
                        putchar('\r');
                    } else if (!isprint(ch)) {
                        continue;
                    } else if (i != (SAY_MAX - 1)) {
                        buffer[i++] = ch;
                        putchar(ch);
                    }
                    fflush(stdout);
                    continue;
                }

                buffer[i] = '\0';
                i = 0;
                putchar('\n');

                if (buffer[0] == '/') {
                    if(strncmp(buffer, "/exit", 5) == 0){
                        client_logout_request();
                    }else if (strncmp(buffer, "/join ", 6) == 0) {
                        client_join_request(strchr(buffer, ' '));
                    } else if (strncmp(buffer, "/leave ", 7) == 0) {
                        client_leave_request(strchr(buffer, ' '));
                    } else if (strncmp(buffer, "/list", 5) == 0) {
                        client_list_request();
                    } else if (strncmp(buffer, "/who ", 5) == 0) {
                        client_who_request(strchr(buffer, ' '));
                    } else if (strncmp(buffer, "/switch ", 8) == 0) {
                        client_switch_request(strchr(buffer, ' '));
                    }else {
                        fprintf(stdout, "Unknown command\n");
                    }
                } else {
                    if (strcmp(buffer, "") != 0)
                        client_say_request(buffer);
                }
                printf("> ");
                fflush(stdout);
            }
        }
    }

    return 0;
}