#ifndef __MY_USER_H__
#define __MY_USER_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h> // bool
#include <time.h>
#include <unistd.h> // close

typedef struct node {
    char *name;
    int fd;
    char *IP;
    unsigned short port;
    bool is_online;
    char *stMsg;
    struct node *next;
} Usernode;

Usernode *newStack();

int addUser(char *name, int fd, char *IP, unsigned short port, Usernode *top);
void broadcast(int fd, Usernode *top);
void unicast(int fd, char *to_name, char *msg, Usernode *top);
void unicast_Offline(int fd, Usernode *top);

void logoutUser(int fd, Usernode *top);
void delAllUser(Usernode *top);
void showUser(Usernode *top);

#endif