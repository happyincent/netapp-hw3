#ifndef __MY_IPC_USER_H__
#define __MY_IPC_USER_H__

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h> // bool
#include <time.h>
#include <unistd.h> // close
#include <arpa/inet.h> // INET6_ADDRSTRLEN
#include <sys/sem.h> // semaphore
#include <sys/ipc.h> // semaphore

#define MAX_USERS 10

typedef struct node {
    char name[BUF_SIZ];
    int fd;
    char IP[INET6_ADDRSTRLEN];
    unsigned short port;
    bool is_online;
    char stMsg[BUF_SIZ];
    char nowMsg[BUF_SIZ];
} Usernode;

typedef struct msgnode {
    int fd;
    Usernode *top;
    int semid;
    struct sembuf grab;
    struct sembuf release;
} Msgnode;

int addUser(char *name, int fd, char *IP, unsigned short port, Usernode *top);
void broadcast(int fd, Usernode *top);
void unicast(int fd, char *to_name, char *msg, Usernode *top);
void unicast_Offline(int fd, Usernode *top);
void *unicast_Online(void *info);

void logoutUser(int fd, Usernode *top);
void showUser(Usernode *top);

#endif