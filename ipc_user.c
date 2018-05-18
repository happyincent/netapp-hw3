#include "ipc_user.h"

int addUser(char *name, int fd, char *IP, unsigned short port, Usernode *top)
{
    Usernode tmp;
    memset(&tmp, 0, sizeof(Usernode));
    
    int i;
    for (i=0; i<MAX_USERS; ++i) {
        if (memcmp(&tmp, (&top[i]), sizeof(Usernode)) == 0)
            break;
        
        if (strcmp((&top[i])->name, name) == 0) {
            if (strcmp((&top[i])->IP, IP) != 0 || (&top[i])->is_online)
                return ERR_USER;
            else {
                (&top[i])->fd = fd;
                (&top[i])->port = port;
                (&top[i])->is_online = true;
                return OLD_USER;
            }
        }
    }
    if (i == MAX_USERS)
        return ERR_USER;
    
    Usernode *node = (Usernode *)calloc(1, sizeof(Usernode));
    strncpy(node->name, name, BUF_SIZ);
    node->fd = fd;
    strncpy(node->IP, IP, INET6_ADDRSTRLEN);
    node->port = port;
    node->is_online = true;
    
    memcpy((&top[i]), node, sizeof(Usernode));
    free(node);
    
    return NEW_USER;
}

void broadcast(int fd, Usernode *top)
{
    char *from_name, *from_IP;
    unsigned short from_port;
    bool from_is_online;
    
    Usernode tmp;
    memset(&tmp, 0, sizeof(Usernode));
    
    for (int i=0; i<MAX_USERS; ++i) {
        if (memcmp(&tmp, (&top[i]), sizeof(Usernode)) == 0)
            break;
        
        if ((&top[i])->fd == fd) {
            from_name = (char *)calloc(1, strlen((&top[i])->name)+1);
            strcpy(from_name, (&top[i])->name);
            from_IP = (char *)calloc(1, strlen((&top[i])->IP)+1);
            strcpy(from_IP, (&top[i])->IP);
            from_port = (&top[i])->port;
            from_is_online = (&top[i])->is_online;
            break;
        }
    }
    
    char buf[BUF_SIZ] = "";
    if (from_is_online)
        sprintf(buf, "<User %s is on-line, IP address: [%s]:%hu.>\n", from_name, from_IP, from_port);
    else
        sprintf(buf, "<User %s is off-line.>\n", from_name);
    
    for (int i=0; i<MAX_USERS; ++i) {
        if (memcmp(&tmp, (&top[i]), sizeof(Usernode)) == 0)
            break;
        
        if ( (&top[i])->fd != fd && (&top[i])->is_online )
            strncpy((&top[i])->nowMsg, buf, BUF_SIZ);// sendall((&top[i])->fd, buf);
    }
}

void unicast(int fd, char *to_name, char *msg, Usernode *top)
{
    Usernode tmp;
    memset(&tmp, 0, sizeof(Usernode));
    
    char *from_name;
    
    for (int i=0; i<MAX_USERS; ++i) {
        if (memcmp(&tmp, (&top[i]), sizeof(Usernode)) == 0)
            break;
        
        if ((&top[i])->fd == fd) {
            from_name = (char *)calloc(1, strlen((&top[i])->name)+1);
            strcpy(from_name, (&top[i])->name);
            break;
        }
    }
    
    char buf[BUF_SIZ] = "";
    
    for (int i=0; i<MAX_USERS; ++i) {
        if (memcmp(&tmp, (&top[i]), sizeof(Usernode)) == 0)
            break;
        
        if (strcmp((&top[i])->name, to_name) == 0) {
            if ((&top[i])->is_online) {
                sprintf(buf, "%s: \"%s\"\n", from_name, msg);
                // sendall((&top[i])->fd, buf);
                strncpy((&top[i])->nowMsg, buf, BUF_SIZ);
            }
            else {
                time_t rawtime;
                struct tm * timeinfo;
                char buffer[80] = "";

                time (&rawtime);
                timeinfo = localtime (&rawtime);

                strftime (buffer, 80, "%I:%M%p %Y/%m/%d", timeinfo);
                
                sprintf(buf, "<User %s has sent you a message \"%s\" at %s.>\n", from_name, msg, buffer);
                strncat((&top[i])->stMsg, buf, BUF_SIZ);
                
                memset(buf, 0, BUF_SIZ);
                sprintf(buf, "User %s is off-line. The message will be passed when he/she comes back.\n", to_name);
                sendall(fd, buf);
            }
            return;
        }
    }
    
    sprintf(buf, "<User %s does not exist.>\n", to_name);
    sendall(fd, buf);
}

void unicast_Offline(int fd, Usernode *top)
{
    Usernode tmp;
    memset(&tmp, 0, sizeof(Usernode));
    
    for (int i=0; i<MAX_USERS; ++i) {
        if (memcmp(&tmp, (&top[i]), sizeof(Usernode)) == 0)
            break;
        if ((&top[i])->fd == fd) {
            if (strcmp((&top[i])->stMsg, "") != 0) {
                sendall(fd, (&top[i])->stMsg);
                memset((&top[i])->stMsg, 0, BUF_SIZ);
            }
            break;
        }
    }
}

void *unicast_Online(void *info)
{
    Msgnode node = *(Msgnode *)info;
    int fd = (&node)->fd;
    Usernode *top = (&node)->top;
    int semid = (&node)->semid;
    struct sembuf grab = (&node)->grab;
    struct sembuf release = (&node)->release;
    
    Usernode tmp;
    memset(&tmp, 0, sizeof(Usernode));
    
    while(1) {
        if (semop(semid, &grab, 1) == -1)
            DIE("server.unicast_Online semop grab\n");
        
        for (int i=0; i<MAX_USERS; ++i) {
            if (memcmp(&tmp, (&top[i]), sizeof(Usernode)) == 0)
                break;
            if ((&top[i])->fd == fd) {
                if (strcmp((&top[i])->nowMsg, "") != 0) {
                    sendall(fd, (&top[i])->nowMsg);
                    memset((&top[i])->nowMsg, 0, BUF_SIZ);
                }
                break;
            }
        }
        
        if (semop(semid, &release, 1) == -1)
            DIE("server.unicast_Online semop release\n");
    }
    
    return NULL;
}

void logoutUser(int fd, Usernode *top)
{
    Usernode tmp;
    memset(&tmp, 0, sizeof(Usernode));
    
    for (int i=0; i<MAX_USERS; ++i) {
        if (memcmp(&tmp, (&top[i]), sizeof(Usernode)) == 0)
            break;
        if ((&top[i])->fd == fd) {
            (&top[i])->is_online = false;
            break;
        }
    }
}

void showUser(Usernode *top)
{
    Usernode tmp;
    memset(&tmp, 0, sizeof(Usernode));
    
    for (int i=0; i<MAX_USERS; ++i) {
        if (memcmp(&tmp, (&top[i]), sizeof(Usernode)) == 0)
            break;
        printf("User: %s, [%s]:%hu, online: %d, fd: %d\n", (&top[i])->name, (&top[i])->IP, (&top[i])->port, (&top[i])->is_online, (&top[i])->fd);
    }
    printf("---------------------------\n");
}
