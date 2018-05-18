#include "common.h"
#include "ipc_user.h"

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
#include <sys/shm.h> // shared memory
#include <sys/sem.h> // semaphore
#include <sys/ipc.h> // semaphore

#include <pthread.h>

#define PORT "1234"
#define QLEN 10

/* [passivesock Arguments]
 * service: Service associated with the desired port
 * transport: Transport protocol to use ("tcp" or "udp")
 * qlen: Maximum server request queue length                */
static int passivesock(const char *service, const char *transport, int qlen);

static bool login(int fd, struct sockaddr_storage cli_addr);
static bool getMsg(int fd);
static void logout(int fd);
static void stop_server();

static int shmid, semid;
static struct sembuf grab    = {0, -1, SEM_UNDO};
static struct sembuf release = {0,  1, SEM_UNDO};

static Usernode *shm;

int main()
{
    signal( SIGINT, stop_server );
    
    const char * service = PORT;
    struct sockaddr_storage cli_addr;
    socklen_t cli_len = sizeof(cli_addr);
    int msock; // master server socket
    int ssock; // new client socket
    
	if ((shmid = shmget(IPC_PRIVATE, MAX_USERS * sizeof(Usernode), IPC_CREAT | 0666)) == -1)
		DIE("server: shmget\n");
    
	if ((shm = (Usernode *)shmat(shmid, NULL, 0)) == (void *)-1)
		DIE("server: shmat\n");
    memset(shm, 0, MAX_USERS*sizeof(Usernode));
    
    if ((semid = semget(IPC_PRIVATE, 1, IPC_CREAT | IPC_EXCL | 0666)) == -1)
		DIE("server: semget\n");
    if (semctl(semid, 0, SETVAL, 1) == -1)
		DIE("main semctl\n");
    
    msock = passivesock(service, "tcp", QLEN);
    
    while (1) {
        
        if (( ssock = accept(msock, (struct sockaddr *)&cli_addr, &cli_len) )== -1)
            DIE("server: accept\n");
        
        if (semop(semid, &grab, 1) == -1)
            DIE("server.login semop grab\n");
        
        bool if_login = login(ssock, cli_addr);
        
        if (semop(semid, &release, 1) == -1)
            DIE("server.login semop release\n");
        
        if (!if_login) {
            close(ssock);
            continue;
        }
        
        switch (fork()) {
            case -1:
                DIE("server: fork\n");
            case 0:
                close(msock);
                
                pthread_t tid;
                Msgnode node = {ssock, shm, semid, grab, release};
                
                if ( pthread_create(&tid, NULL, &unicast_Online, &node) != 0 )
                    DIE("server: pthread_create\n");
                
                if ( getMsg(ssock) == false )
                    logout(ssock);

                exit(0);
            default:
                break;
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
    
    int type = addUser(name, fd, usrIP, usrPORT, shm);
    if (send(fd, &type, sizeof(type), 0) == -1)
        DIE("server: send user type\n");
    
    switch (type) {
        case NEW_USER:
            broadcast(fd, shm);
            break;
        case OLD_USER:
            broadcast(fd, shm);
            unicast_Offline(fd, shm);
            break;
        case ERR_USER:
            close(fd);
            return false;
    }
    
    showUser(shm);
    return true;
}

static bool getMsg(int fd)
{
    char msg[BUF_SIZ] = "";
    int index;
    char *tokens[MAX_TOKEN_COUNT];
    
    /* recv from user's chat msg */
    while (recvall(fd, msg, BUF_SIZ) > 0) {
        if (strcmp(msg, "bye") == 0)
            return false;
    
        /* split cmd */
        if (split_cmd(msg, &index, tokens) == false) {
            fprintf(stderr, "server: split_cmd.Message is too long.\n");
            continue;
        }
            
        /* send Msg */
        if ( index+1 < 3 || strcmp(tokens[0], "chat") != 0 ) {
            fprintf(stderr, "server: getMsg.User send wrong cmd.\n");
            continue;
        }
        
        if (semop(semid, &grab, 1) == -1)
            DIE("server.getMsg semop grab\n");
        
        /* unicast to each dst */
        for (int i=1; i<index; ++i) {
            unicast(fd, tokens[i], tokens[index], shm);
        }
        
        if (semop(semid, &release, 1) == -1)
            DIE("server.getMsg semop release\n");
    }
    
    return false;
}

static void logout(int fd)
{
    if (semop(semid, &grab, 1) == -1)
		DIE("server.logout semop grab\n");
    
    logoutUser(fd, shm);
    broadcast(fd, shm);
    close(fd);
    showUser(shm);
    
    if (semop(semid, &release, 1) == -1)
		DIE("server.logout semop release\n");
}

static void stop_server()
{
    if (shmctl(shmid, IPC_RMID, (struct shmid_ds *)NULL) == -1)
		DIE("stop_server shmctl\n");
	if (semctl(semid, 0, IPC_RMID) == -1)
		DIE("stop_server semctl\n");
    
    printf("\n\nServer is terminated.\n\n");
    exit(EXIT_SUCCESS);
}