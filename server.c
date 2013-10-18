/**********************************
 * Include the common head file
 **********************************/
#include <semaphore.h>
#include "pjudge.h"

#define JUDGENUM 5
struct sockaddr_in server_addr;
struct sockaddr_in client_addr;
int server_sockfd, sockfd;
socklen_t server_len, client_len;
pthread_t solve_thread;
pid_t judge_pid[JUDGENUM];
MYSQL conn;
int sem_mysql;//semaphore for operate mysql
int sem_judge;//semaphore for judge manage
sem_t sem_thread;//semaphore for limit total number of threads

static void maintainjudge(int signo)
{
    int id;
    for(id=0;id<JUDGENUM;++id)
        if(judge_pid[id]<1||judge_pid[id]==waitpid(judge_pid[id], NULL, WNOHANG))
        {
            /** use vfork() instead of fork(), to make sure child process run first,
             * otherwise there will have some thing wrong, and I don't know what's wrong.
             * sometimes the the child haven't call execl() and logerrmsg() after fork() 
             * success return 0, and there became two same process!!! **/
            if((judge_pid[id]=vfork())<0)
                logerrmsg(serverlog, "vfork error before make a judge!");
            else if(judge_pid[id]==0)
            {
                //start judge exec
                execl(judgepath, "judge", (char *)0);
                logerrmsg(serverlog, "judge error exit");
                //when execl failed, use _exit exit, make parent running
                _exit(1);
            }
        }
}

int getword(char *ptr)
{
    while(*ptr==' '||*ptr=='\n')++ptr;
    if(*ptr=='\0')return 1;
    return 0;
}

int getwaitsubmit()
{
    MYSQL_RES *res;
    MYSQL_ROW row;
    int ret=0;
    if(!Mysql_query(&conn, "SELECT count(*) FROM submit WHERE status=0"))
    {
        res=Mysql_use_result(&conn);
        if(res)
        {
            if((row=mysql_fetch_row(res)))
                sscanf(row[0], "%d", &ret);
            mysql_free_result(res);
        }
    }
    return ret;
}

void updatedb(char *argv)
{
    char *ptr;
    char cname[MAXLINE+1], opt[MAXLINE+1], str[MAXLINE+1];
    int val, type=0, start=-1, end=-1;
    /*********************************
     * t - 1 problem 2 submit 3 contest
     * s - start id
     * e - end id
     *********************************/
    sscanf(argv, "%s", cname);
    ptr=argv+strlen(cname)+1;
    while(1)
    {
        if(getword(ptr))break;
        sscanf(ptr, "%s", opt);
        ptr+=strlen(opt)+1;
        if(getword(ptr))return;
        sscanf(ptr, "%s", str);
        sscanf(str, "%d", &val);
        ptr+=strlen(str)+1;
        if(opt[1]=='t')type=val;
        if(opt[1]=='s')start=val;
        if(opt[1]=='e')end=val;
    }
#ifdef DEBUG
    char msg[1234];
    sprintf(msg, "type: %d start: %d end: %d", type, start, end);
    logerrmsg(serverlog, msg);
#endif
    if(!type||start<0||end<0)return;

    char sql[MAXLINE+1];
    char class[22];

    if(type==1)sprintf(class, "ProID");
    else if(type==2)sprintf(class, "RunID");
    else sprintf(class, "contest");

    sprintf(sql, "UPDATE submit SET status=0 WHERE %s>=%d AND %s<=%d", class, start, class, end);
#ifdef DEBUG
    logerrmsg(serverlog, sql);
#endif
    Mysql_query(&conn, sql);
}

void *work(void *arg)
{
    //Besure release the resource when thread stoped
    pthread_detach(pthread_self());

    int sockfd=(int)arg;
    char msg[MAXLINE+1];
    //read and process the contrl message
    while(read(sockfd, msg, MAXLINE)>0)
    {
        if(strcmp(msg, "end")==0)break;
        if(strcmp(msg, "judge")==0)continue;
        //update the mysql data
        updatedb(msg);
#ifdef DEBUG
        logerrmsg(serverlog, msg);
#endif
    }

    /** set semaphore to wake judge**/
    set_semvalue(sem_mysql, getwaitsubmit());

    /** use charachter array instead of string, direct use string maybe cause bad address**/ 
    sprintf(msg, "Accept");
    if(write(sockfd, msg, MAXLINE)<=0)
        logerrmsg(serverlog, strerror(errno));

    close(sockfd);
    /** release the thread**/
    sem_post(&sem_thread);
    pthread_exit(NULL);
}

void serverexit(void)
{
    logerrmsg(serverlog, "there maybe something wrong!!!");
    del_semvalue(sem_mysql);
    del_semvalue(sem_judge);
    sem_destroy(&sem_thread);
}

int main(int argc, char *argv[])
{
    /** Besure the process became daemon process **/
    daemonize();
    if(already_running())
    {
        logerrmsg(serverlog, "The server already running");
        exit(1);
    }

    int i;
    for(i=0;i<JUDGENUM;++i)
        judge_pid[i]=-1;
    
    atexit(serverexit);
    if(signal(SIGCHLD, maintainjudge)==SIG_ERR)
    {
        logerrmsg(serverlog, "Can't catch SIGCHLD!!!");
        exit(1);
    }
    //build judges
    kill(getpid(), SIGCHLD);

    server_sockfd=socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family=AF_INET;
    server_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    server_addr.sin_port=htons(1991);
    server_len=sizeof(server_addr);
    bind(server_sockfd, (struct sockaddr *)&server_addr, server_len);
    listen(server_sockfd, 5);

    mysql_init(&conn);
    Mysql_real_connect(&conn, dbhost, username, password, dbname, 0, NULL, 0);
    /*********************************
     * Semaphore between process init
     *********************************/

    sem_mysql=semget((key_t)SEM_MYSQL, 1, 0666|IPC_CREAT);
    sem_judge=semget((key_t)SEM_JUDGE, 1, 0666|IPC_CREAT);
    if(sem_mysql<0||sem_judge<0)
    {
        logerrmsg(serverlog, "Create semaphore failed!");
        exit(1);
    }
    set_semvalue(sem_mysql, getwaitsubmit());
    set_semvalue(sem_judge, 1);
    
    /*********************************
     * Semaphore with thread
     *********************************/
    if(sem_init(&sem_thread, 0, JUDGENUM))
    {
        logerrmsg(serverlog, "Semaphore init error/ with thread!!");
        exit(1);
    }

    while(1)
    {
        sem_wait(&sem_thread);
        client_len=sizeof(client_addr);
        sockfd=accept(server_sockfd, (struct sockaddr *)&client_addr, &client_len);
        pthread_create(&solve_thread, NULL, work, (void *)sockfd);
    }
    mysql_close(&conn);
    exit(0);
}
