#ifndef __MY_COMMON_H__
#define __MY_COMMON_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h> // bool
#include <sys/types.h>
#include <sys/socket.h>

#include <errno.h>
#define DIE(...)  fprintf(stderr, __VA_ARGS__), \
                  fprintf(stderr, "%s\n", strerror(errno)), \
                  exit(errno) \

#define BUF_SIZ 1024
#define MAX_TOKEN_COUNT 10

#define NEW_USER 0
#define OLD_USER 1
#define ERR_USER 2

int recvall(int fd, void *buf, int len);
int sendall(int fd, const void *str);
bool split_cmd(char *msg, int *index, char *tokens[MAX_TOKEN_COUNT]);

#endif