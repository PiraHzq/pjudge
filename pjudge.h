/***********************************
 * The common head file
 ***********************************/
#include <sys/types.h>
#include <sys/user.h>
#include <sys/reg.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/ptrace.h>
#include <syslog.h>
#include <signal.h>
#include <fcntl.h>
#include <dirent.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "mysql.h"
/***union for sem ****/
union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};

/**********************************
 * some useful value
 **********************************/
//#define DEBUG   1
#define MAXLINE 1024
#define SEM_MYSQL 199138
#define SEM_JUDGE 381991
#define USER_JUDGER

#define JG_WT   0
#define JG_CP   1
#define JG_RI   2
#define JG_AC   3
#define JG_RE   4
#define JG_TL   5
#define JG_ML   6
#define JG_OL   7
#define JG_PE   8
#define JG_CE   9
#define JG_WA   10

#define LANG_C  0
#define LANG_CC 1
#define LANG_JAVA 2

const char *codename[]={"main.c", "main.cc", "Main.java"};

const char *hostname  =   "localhost";
const char *hostip    =   "127.0.0.1";
const char *serverlog =   "/home/pira/pjudge/log/servererr.log";
const char *judgelog  =   "/home/pira/pjudge/log/judgeerr.log";
const char *dblog     =   "/home/pira/pjudge/log/dberr.log";
const char *oj_home   =   "/home/pira/pjudge";
const char *judgepath =   "/home/pira/pjudge/judge";
const char *java    =   "/home/pira/jdk1.7/bin/java";
const char *javac   =   "/home/pira/jdk1.7/bin/javac";

/*********************************
 * some constant for mysql
 *********************************/

const char *dbhost  =   "localhost";
const char *username=   "rick";
const char *password=   "12345";
const char *dbname  =   "foo";

char msgbuf[MAXLINE];



/**********************************
 * All function
 **********************************/
void logerrmsg(const char *filename, const char *fmt, ...);
void daemonize(void);
int lockfile(int fd);
int already_running(void);



/********************************
 * This function log the error message
 ********************************/
void logerrmsg(const char *filename, const char *fmt, ...)
{
    char msg[MAXLINE+1];
    va_list ap;
    FILE *fd;

    va_start(ap, fmt);
    vsprintf(msg, fmt, ap);
    va_end(ap);

    if((fd=fopen(filename, "a+"))==NULL)
    {
        syslog(LOG_ERR, "fopen %s err: %s", filename, strerror(errno));
        exit(1);
    }
    time_t now=time(NULL);
    fprintf(fd, "%s\t[ %d ]: %s\n", ctime(&now), getpid(), msg);
    fclose(fd);
}

int runcmd(const char *fmt, ...)
{
    char cmd[MAXLINE+1];
    int ret=0;
    va_list ap;
    va_start(ap, fmt);
    vsprintf(cmd, fmt, ap);
    va_end(ap);
    ret=system(cmd);
    return ret;
}

/********************************
 * This function init the daemon
 ********************************/
void daemonize(void)
{
    int i;
    pid_t pid;
    struct rlimit rl;
    struct sigaction sa;

    /* Clear file creation mask. */
    umask(0);

    /* Get maximum number of file descriptors */
    if(getrlimit(RLIMIT_NOFILE, &rl)<0)
        logerrmsg(serverlog, "server can't get file limit"); 

    /* Become a session leader to lose controlling TTY */
    if((pid=fork())<0)
        logerrmsg(serverlog, "server can't fork on first time");
    else if(pid!=0)exit(0);
    setsid();

    /* Ensure future opens won't allocate controlling TTYs */ 
    sa.sa_handler=SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags=0;
    if(sigaction(SIGHUP, &sa, NULL)<0)
        logerrmsg(serverlog, "server can't ignore SIGHUP");
    if((pid=fork())<0)
        logerrmsg(serverlog, "server can't fork on second time");
    else if(pid!=0)exit(0);

    /* Change the current working dirctory to the root so
     * we won't prevent file systems from being unmounted.
     */
    if(chdir(oj_home)<0)
        logerrmsg(serverlog, "server can't change directory to root");

    /* Close all open file descriptors */
    if(rl.rlim_max==RLIM_INFINITY)
        rl.rlim_max=1024;
    for(i=0;i<rl.rlim_max;++i)close(i);
    
    /* Attach file descriptors 0, 1, and 2 to /dev/null */
    open("/dev/null", O_RDWR);
    dup(0);
    dup(0);
}



/*********************************
 * Lock the entire file fd
 *********************************/
int lockfile(int fd)
{
    struct flock fl;
    fl.l_type=F_WRLCK;
    fl.l_start=0;
    fl.l_whence=SEEK_SET;
    fl.l_len=0;
    return(fcntl(fd, F_SETLK, &fl));
}



/*********************************
 * This function is use for check if the started 
 * server daemon is the single process
 *********************************/

#define LOCKFILE "/var/run/pjudge.pid"
#define LOCKMODE (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)

int already_running(void)
{
    int fd;
    char buf[16];
    fd=open(LOCKFILE, O_RDWR|O_CREAT, LOCKMODE);
    if(fd<0)
    {
        sprintf(msgbuf, "server can't open %s", LOCKFILE);
        logerrmsg(serverlog, msgbuf);
        exit(1);
    }
    if(lockfile(fd)<0)
    {
        if(errno==EACCES||errno==EAGAIN)
        {
            close(fd);
            return(1);
        }
        logerrmsg(serverlog, "server lockfile error");
        exit(1);
    }
    ftruncate(fd, 0);
    sprintf(buf, "%ld", (long)getpid());
    write(fd, buf, strlen(buf)+1);
    return(0);
}

/*********************************
 * Wrap some mysql function
 *********************************/

MYSQL *Mysql_real_connect(MYSQL *connection, 
        const char *server_host, const char *sql_user_name, 
        const char *sql_password, const char *db_name,
        unsigned int port_number, const char *unix_socket_name,
        unsigned int flags)
{
    MYSQL *ret;
    ret=mysql_real_connect(connection, server_host, sql_user_name, sql_password, 
            db_name, port_number, unix_socket_name, flags);
    if(ret==NULL)logerrmsg(dblog, "ERRNO %d: %s", mysql_errno(connection), mysql_error(connection));
    return ret;
}

int Mysql_query(MYSQL *connection, const char *query)
{
    int ret;
    ret=mysql_query(connection, query);
    if(ret)logerrmsg(dblog, "ERRNO %d: %s", mysql_errno(connection),  mysql_error(connection));
    return ret;
}

MYSQL_RES *Mysql_use_result(MYSQL *connection)
{
    MYSQL_RES *ret;
    ret=mysql_use_result(connection);
    if(ret==NULL)logerrmsg(dblog, "ERRNO %d: %s", mysql_errno(connection), mysql_error(connection));
    return ret;
}


MYSQL_RES *Mysql_store_result(MYSQL *connection)
{
    MYSQL_RES *ret;
    ret=mysql_store_result(connection);
    if(ret==NULL)logerrmsg(dblog, "ERRNO %d: %s", mysql_errno(connection), mysql_error(connection));
    return ret;
}


/*********************************
 * function for semaphore
 *********************************/

void set_semvalue(int sem_id, int val)
{
    union semun sem_union;
    sem_union.val=val;
    if(semctl(sem_id, 0, SETVAL, sem_union)==-1)
        logerrmsg(serverlog, "set_semvalue error!!!");
}

void del_semvalue(int sem_id)
{
    union semun sem_union;
    if(semctl(sem_id, 0, IPC_RMID, sem_union)==-1)
        logerrmsg(serverlog, "del_semvalue error!!!");
}

int semaphore_p(int sem_id)
{
    struct sembuf sem_b;
    sem_b.sem_num=0;
    sem_b.sem_op=-1;
    sem_b.sem_flg=SEM_UNDO;
    if(semop(sem_id, &sem_b, 1)==-1)
    {
        logerrmsg(judgelog, "semaphore_p failed!!!");
        return 1;
    }
    return 0;
}

int semaphore_v(int sem_id)
{
    struct sembuf sem_b;
    sem_b.sem_num=0;
    sem_b.sem_op=1;
    sem_b.sem_flg=SEM_UNDO;
    if(semop(sem_id, &sem_b, 1)==-1)
    {
        logerrmsg(judgelog, "semaphore_v failed!!!");
        return 1;
    }
    return 0;
}

