#include "common.h"

int recvall(int fd, void *buf, int len)
{
    memset(buf, 0, len);
    int count, n, tmpSize;
    
    for (count = 0; count < len; count += n) {
        
        tmpSize = ( len-count < BUF_SIZ ) ? len-count : BUF_SIZ;
        n = recv(fd, buf+count, tmpSize, 0);
        
        if (n == 0)
            break;
        else if (n == -1)
            DIE("recvall\n");
    }
    
    return count;
}

int sendall(int fd, const void *str)
{
    static char buf[BUF_SIZ];
    memset(buf, 0, BUF_SIZ);
    strncpy(buf, str, BUF_SIZ);
    
    if (strlen(str) > BUF_SIZ-1)
        fprintf(stderr, "sendall: Message is too long.\n");
    
    int count, n;

    for (count = 0; count < BUF_SIZ; count += n) {
        if ((n = send(fd, buf+count, BUF_SIZ-count, 0)) == -1)
            DIE("sendall fd:%d\n", fd);
    }
    
    return count;
}

bool split_cmd(char *msg, int *index, char *tokens[MAX_TOKEN_COUNT])
{
    memset(tokens, '\0', sizeof(char *)*MAX_TOKEN_COUNT);
    
    char *tok = strtok(msg, " ");
    
    for ( *index = 0; tok != NULL; ++*index) {
        if (*index > MAX_TOKEN_COUNT-1)
            return false;
        tokens[*index] = tok;
        tok = strtok(NULL, " ");
    }
    --*index;
    
    return true;
}