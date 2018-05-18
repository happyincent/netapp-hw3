#include "common.h"
#include "user.h"

#include <stdio.h>
#include <stdlib.h> // exit
#include <unistd.h> // close
#include <string.h> // mmset
#include <stdbool.h> // bool
#include <signal.h> // signal

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h> // inet_ntop
#include <sys/select.h> // select

#define PORT "1234"
#define QLEN 10

/* [passivesock Arguments]
 * service: Service associated with the desired port
 * transport: Transport protocol to use ("tcp" or "udp")
 * qlen: Maximum server request queue length                */
static int passivesock(const char *service, const char *transport, int qlen);

static bool login(int fd, struct sockaddr_storage cli_addr);
static bool getMsg(int fd);
static void logout(int fd, fd_set *afds);
static void stop_server();

static Usernode *UserStack;

int main()
{
    UserStack = newStack();
    signal( SIGINT, stop_server );
    
    const char * service = PORT;
    struct sockaddr_storage cli_addr;
    socklen_t cli_len = sizeof(cli_addr);
    fd_set rfds; // read file descriptor set
    fd_set afds; // active file descriptor set
    int nfds; // the process's fd table size
    int msock; // master server socket
    int ssock; // new client socket
    
    msock = passivesock(service, "tcp", QLEN);
    
    nfds = msock+1; // getdtablesize() output 2048 in "bash on windows"
    FD_ZERO(&afds);
    FD_SET(msock, &afds);
    
    while(1) {
        printf("nfds: %d\n", nfds);
        memcpy(&rfds, &afds, sizeof(rfds));
        
        if ( select(nfds, &rfds, NULL, NULL, NULL) < 0)
            DIE("server: select\n");
        
        if ( FD_ISSET(msock, &rfds) ) {
            if (( ssock = accept(msock, (struct sockaddr *)&cli_addr, &cli_len) )== -1)
                DIE("server: accept\n");
            
            if (login(ssock, cli_addr)) {
                FD_SET(ssock, &afds);
                if (ssock >= nfds)
                    nfds = ssock+1;
                showUser(UserStack);
            }
        }
        
        for (int fd = 0; fd < nfds; ++fd) {
            if ( fd != msock && FD_ISSET(fd, &rfds) ) {
                if ( getMsg(fd) == false ) {
                    logout(fd, &afds);
                    showUser(UserStack);
                }
            }
        }
    }
    
    return 0;
}

static int passivesock(const char *service, const char *transport, int qlen)
{
    int sockfd;
    struct addrinfo hints;
    struct addrinfo *servinfo, *p; // getaddrinfo
    
    int status;
    int yes = 1; // setsockopt
    char ip_str[INET6_ADDRSTRLEN] = "";
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_socktype = (strcmp(transport, "udp") == 0) ? SOCK_DGRAM : SOCK_STREAM;

    if ((status = getaddrinfo(NULL, service, &hints, &servinfo)) != 0)
        DIE( "server: getaddrinfo error: %s\n", gai_strerror(status) );
    
    for (p = servinfo; p != NULL; p = p->ai_next) {
        
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket\n");
            continue;
        }
        
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
            DIE("server: setsockopt\n");
        
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind\n");
            continue;
        }
        
        void *serv_addr = (p->ai_family == AF_INET) ?
                          (void *)&((struct sockaddr_in *)p->ai_addr)->sin_addr :
                          (void *)&((struct sockaddr_in6 *)p->ai_addr)->sin6_addr ;
        
        if (inet_ntop(p->ai_family, serv_addr, ip_str, sizeof(ip_str)) == NULL)
            DIE("server: server's inet_ntop\n");
        
        unsigned short serv_port = (p->ai_family == AF_INET) ?
                                   ntohs( ((struct sockaddr_in *)p->ai_addr)->sin_port ) :
                                   ntohs( ((struct sockaddr_in6 *)p->ai_addr)->sin6_port );
        
        printf("Server is listening on [%s]:%hu\n", ip_str, serv_port);
        printf("============================\n");
        
        break;
    }
    
    if (p == NULL)
        DIE("server: fail to bind\n");    
    freeaddrinfo(servinfo);
    
    /* start listen socket */    
    if (listen(sockfd, qlen) == -1)
        DIE("server: listen\n");
    
    return sockfd;
}

static bool login(int fd, struct sockaddr_storage cli_addr)
{
    /* get user's IP, PORT */
    char usrIP[INET6_ADDRSTRLEN] = "";
    unsigned short usrPORT;    
    
    struct sockaddr *sa = (struct sockaddr *)&cli_addr;    
    usrPORT = (sa->sa_family == AF_INET) ?
               ntohs( (((struct sockaddr_in*)sa)->sin_port) ) :
               ntohs( (((struct sockaddr_in6*)sa)->sin6_port) ) ;
    void *ip_addr = (sa->sa_family == AF_INET) ?
                     (void *)&(((struct sockaddr_in*)sa)->sin_addr) :
                     (void *)&(((struct sockaddr_in6*)sa)->sin6_addr) ;
    if (inet_ntop(cli_addr.ss_family, ip_addr, usrIP, sizeof(usrIP)) == NULL)
        DIE("server: login.inet_ntop\n");
    
    /* get user's name */
    char name[BUF_SIZ] = "";
    if ( recvall(fd, name, BUF_SIZ) == 0)
        return false;
    
    int type = addUser(name, fd, usrIP, usrPORT, UserStack);
    if (send(fd, &type, sizeof(type), 0) == -1)
        DIE("server: send user type\n");
    
    switch (type) {
        case NEW_USER:
            broadcast(fd, UserStack);
            break;
        case OLD_USER:
            broadcast(fd, UserStack);
            unicast_Offline(fd, UserStack);
            break;
        case ERR_USER:
            close(fd);
            return false;
    }
    
    return true;
}

static bool getMsg(int fd)
{
    char msg[BUF_SIZ] = "";
    int index;
    char *tokens[MAX_TOKEN_COUNT];
    
    /* recv from user's chat msg */
    if (recvall(fd, msg, BUF_SIZ) == 0 || strcmp(msg, "bye") == 0)
        return false;
    
    /* split cmd */
    if (split_cmd(msg, &index, tokens) == false) {
        fprintf(stderr, "server: split_cmd.Message is too long.\n");
        return true;
    }
    
    /* send Msg */
    if ( index+1 < 3 || strcmp(tokens[0], "chat") != 0 ) {
        fprintf(stderr, "server: getMsg.User send wrong cmd.\n");
        return true;
    }
    
    /* unicast to each dst */
    for (int i=1; i<index; ++i) {
        unicast(fd, tokens[i], tokens[index], UserStack);
    }
    
    return true;
}

static void logout(int fd, fd_set *afds)
{
    logoutUser(fd, UserStack);
    broadcast(fd, UserStack);
    close(fd);
    FD_CLR(fd, afds);
}

static void stop_server()
{
    delAllUser(UserStack);
    printf("\n\nServer is terminated.\n\n");
    exit(EXIT_SUCCESS);
}