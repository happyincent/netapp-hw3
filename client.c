#include "common.h"

#include <stdio.h>
#include <stdlib.h> // exit
#include <unistd.h> // close
#include <string.h> // mmset
#include <pthread.h> // pthread

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h> // inet_ntop

/* [connectsock Arguments]
 * host: Name of host to which connection is desired
 * service: Service associated with the desired port
 * transport: Transport protocol to use ("tcp" or "udp")    */
static int connectsock(const char *host, const char *service, const char *transport);

static void *getMsg(void *fd);

int main()
{
    bool if_login = false;
    pthread_t tid;
    
    int sockfd;
    char cmd[BUF_SIZ] = "", cmd2[BUF_SIZ] = "";
    int index;
    char *tokens[MAX_TOKEN_COUNT];
    
    while ( fputs("\n[B023040025 @ hw3]$ ", stderr) ) {
        
        if (fgets(cmd, BUF_SIZ, stdin) == NULL)
            DIE("client: fgets cmd\n");
        cmd[strlen(cmd)-1] = '\0'; // remove newline
        strcpy(cmd2, cmd);
        
        /* split cmd */
        if (split_cmd(cmd, &index, tokens) == false) {
            fputs("client.split_cmd.Message is too long.\n", stderr);
            continue;
        }
        
        /* connect [IP] [PORT] [username] */
        if (index == 3 && strcmp(tokens[0], "connect") == 0) {
            if (if_login) {
                fputs("Already login.\n", stderr);
                continue;
            }
            
            if ((sockfd = connectsock(tokens[1], tokens[2], "tcp")) == -1)
                continue;
            
            sendall(sockfd, tokens[3]); // send username
            
            int type;
            recvall(sockfd, &type, sizeof(type));
            switch (type) {
                case NEW_USER: fputs("Login successfully. (NEW USER)\n", stderr); break;
                case OLD_USER: fputs("Login successfully. (OLD USER)\n", stderr); break;
                case ERR_USER:
                    fputs("Login unsuccessfully.\n( Maybe this user is on-line now or\n  You're IP is not same as last time or\n  Sever is full loaded. )\n", stderr);
                    close(sockfd);
                    continue;
            }
            
            /* create thread to listen from server */
            if ( pthread_create(&tid, NULL, &getMsg, &sockfd) != 0 )
                DIE("client: pthread_create\n");
            
            if_login = true;
        }
        
        /* chat [username] [message] */
        else if (index>=2 && strcmp(tokens[0], "chat") == 0) {
            if (!if_login) {
                fputs("Please login before chat!!\n", stderr);
                continue;
            }
            sendall(sockfd, cmd2); // send cmd
        }
        
        /* bye */
        else if (index==0 && strcmp(tokens[0], "bye") == 0) {
            if (if_login) {
                fputs("Goodbye.\n\n", stderr);
                sendall(sockfd, "bye"); // send bye
                close(sockfd);
                pthread_join(tid, NULL);
                break;
            }
            else
                fputs("Please login before logout!!\n", stderr);
        }
        
        /* wrong cmd */
        else {
            fputs("usage 1: connect [IP] [PORT] [username]\n", stderr);
            fputs("usage 2: chat [username] [message]\n", stderr);
            fputs("usage 3: chat {[username1] [username2] ...} [message]\n", stderr);
            fputs("usage 4: bye\n", stderr);
        }
        
        memset(cmd, 0, BUF_SIZ);
    }
    
    return 0;
}

static int connectsock(const char *host, const char *service, const char *transport)
{
    int sockfd;
    struct addrinfo hints;
    struct addrinfo *servinfo, *p; // getaddrinfo
    int status;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = (strcmp(transport, "udp") == 0) ? SOCK_DGRAM : SOCK_STREAM;

    if ((status = getaddrinfo(host, service, &hints, &servinfo)) != 0)
        DIE( "client: getaddrinfo error: %s\n", gai_strerror(status) );
    
    for (p = servinfo; p != NULL; p = p->ai_next) {
        
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            fprintf(stderr, "client: socket\n");
            continue;
        }
        
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            fprintf(stderr, "client: connect\n");
            continue;
        }
        
        break;
    }
    
    if (p == NULL) {
        fprintf(stderr, "client: fail to connect");
        return -1;
    }
    freeaddrinfo(servinfo);
    
    return sockfd;
}

static void *getMsg(void *info)
{
    int fd = *(int *)info;
    char buf[BUF_SIZ] = "";
    
    while ( recvall(fd, buf, BUF_SIZ) > 0 ) {
        fprintf(stderr, "\n\n%s\n[B023040025 @ hw3]$ ", buf);
    }
    
    return NULL;
}