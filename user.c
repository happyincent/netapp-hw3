#include "user.h"
#include "common.h"

Usernode *newStack()
{
    Usernode *top = (Usernode *)calloc(1, sizeof(Usernode));
    top->next = NULL;
    return top;
}

int addUser(char *name, int fd, char *IP, unsigned short port, Usernode *top)
{
    for (Usernode *tmp = top->next; tmp != NULL; tmp = tmp->next) {
        if (strcmp(tmp->name, name) == 0) {
            if (strcmp(tmp->IP, IP) != 0 || tmp->is_online)
                return ERR_USER;
            else {
                tmp->fd = fd;
                tmp->port = port;
                tmp->is_online = true;
                return OLD_USER;
            }
        }
    }
    
    Usernode *node = (Usernode *)calloc(1, sizeof(Usernode));
    node->name = (char *)calloc(1, strlen(name)+1);
    strcpy(node->name, name);
    node->fd = fd;
    node->IP = (char *)calloc(1, strlen(IP)+1);
    strcpy(node->IP, IP);
    node->port = port;
    node->is_online = true;
    node->stMsg = NULL;
    node->next = top->next;
    top->next = node;
    
    return NEW_USER;
}

void broadcast(int fd, Usernode *top)
{
    char *from_name, *from_IP;
    unsigned short from_port;
    bool from_is_online;
    
    for (Usernode *src = top->next; src != NULL; src = src->next) {
        if (src->fd == fd) {
            from_name = (char *)calloc(1, strlen(src->name)+1);
            strcpy(from_name, src->name);
            from_IP = (char *)calloc(1, strlen(src->IP)+1);
            strcpy(from_IP, src->IP);
            from_port = src->port;
            from_is_online = src->is_online;
            break;
        }
    }
    
    char buf[BUF_SIZ] = "";
    if (from_is_online)
        sprintf(buf, "<User %s is on-line, IP address: [%s]:%hu.>\n", from_name, from_IP, from_port);
    else
        sprintf(buf, "<User %s is off-line.>\n", from_name);
    
    for (Usernode *dst = top->next; dst != NULL; dst = dst->next) {
        if ( dst->fd != fd && dst->is_online )
            sendall(dst->fd, buf);
    }
}

void unicast(int fd, char *to_name, char *msg, Usernode *top)
{
    char *from_name;
    for (Usernode *src = top->next; src != NULL; src = src->next) {
        if (src->fd == fd) {
            from_name = (char *)calloc(1, strlen(src->name)+1);
            strcpy(from_name, src->name);
            break;
        }
    }
    
    char buf[BUF_SIZ] = "";
    for (Usernode *dst = top->next; dst != NULL; dst = dst->next) {
        if (strcmp(dst->name, to_name) == 0) {
            if (dst->is_online) {
                sprintf(buf, "%s: \"%s\"\n", from_name, msg);
                sendall(dst->fd, buf);
            }
            else {
                time_t rawtime;
                struct tm * timeinfo;
                char buffer[80] = "";

                time (&rawtime);
                timeinfo = localtime (&rawtime);

                strftime (buffer, 80, "%I:%M%p %Y/%m/%d", timeinfo);
                
                sprintf(buf, "<User %s has sent you a message \"%s\" at %s.>\n", from_name, msg, buffer);
                if (dst->stMsg == NULL)
                    dst->stMsg = (char *)calloc(strlen(buf)+1, sizeof(char));
                else
                    dst->stMsg = realloc(dst->stMsg, strlen(dst->stMsg)+strlen(buf)+1);
                strncat(dst->stMsg, buf, BUF_SIZ);
                
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
    for (Usernode *dst = top->next; dst != NULL; dst = dst->next) {
        if (dst->fd == fd) {
            if (dst->stMsg != NULL) {
                sendall(fd, dst->stMsg);
                free(dst->stMsg);
                dst->stMsg = NULL;
            }
            break;
        }
    }
}

void logoutUser(int fd, Usernode *top)
{
    for (Usernode *tmp = top->next; tmp != NULL; tmp = tmp->next) {
        if (tmp->fd == fd) {
            tmp->is_online = false;
            break;
        }
    }
}

void delAllUser(Usernode *top)
{
    while (top->next != NULL) {
        Usernode *tmp = top->next;
        top->next = top->next->next;
        if (tmp->name != NULL)
            free(tmp->name);
        if (tmp->IP != NULL)
            free(tmp->IP);
        if (tmp->stMsg != NULL)
            free(tmp->stMsg);
        free(tmp);
    }
    
    if (top != NULL)
        free(top);
}

void showUser(Usernode *top)
{
    for (Usernode *tmp = top->next; tmp != NULL; tmp = tmp->next) {
        printf("User: %s, [%s]:%hu, online: %d, fd: %d\n", tmp->name, tmp->IP, tmp->port, tmp->is_online, tmp->fd);
    }
    printf("---------------------------\n");
}
