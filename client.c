/**********************************
 * Include the common head file
 **********************************/
#include <string.h>
#include "pjudge.h"

struct sockaddr_in addr;
int sockfd;
socklen_t len;
char msg[MAXLINE+1], *ptr;
int main(int argc, char *argv[])
{
    sockfd=socket(AF_INET, SOCK_STREAM, 0);
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=inet_addr(hostip);
    addr.sin_port=htons(1991);
    len=sizeof(addr);
    connect(sockfd, (struct sockaddr *)&addr, len);
    int i;
    ptr=msg;
    for(i=0;i<argc;++i)
    {
        sprintf(ptr, "%s ", argv[i]);
        ptr+=strlen(argv[i])+1;
    }
    /** use charachter array instead of string, direct use string maybe cause bad address**/ 
    if(write(sockfd, msg, MAXLINE)<0)
        puts("write1 error!");
    sprintf(msg, "end");
    if(write(sockfd, msg, MAXLINE)<0)
        puts(strerror(errno));
    if(read(sockfd, &msg, MAXLINE)<0)
        puts("read error!");
    printf("stats: %s\n", msg);
    close(sockfd);
    exit(0);
}
