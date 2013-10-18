/**********************************
 * Include the common head file
 **********************************/
#include "pjudge.h"
#include "oksyscall.h"

#define STD_MB 1048576
#define STD_T_LIM 2
#define STD_F_LIM (STD_MB<<5)
#define STD_M_LIM (STD_MB<<7)

MYSQL conn;
MYSQL_RES *res;
MYSQL_ROW row;
int sem_mysql;
int sem_judge;
char sql[MAXLINE+1];

int oksyscall[512];

void init_syscalls_limits(int language)
{
    int i;
    memset(oksyscall, 0, sizeof(oksyscall));
    if(language==LANG_C||language==LANG_CC)
    {
        for(i=0;SYS_CC[i];i++)
            oksyscall[SYS_CV[i]]=SYS_CC[i];
    }
    else if(language==LANG_JAVA)
    {
        for(i=0;SYS_JC[i];i++)
            oksyscall[SYS_JV[i]]=SYS_JC[i];
    }
}

void get_submit_info(int RunID, int *ProID, int *language, char *authorID)
{
    sprintf(sql, "SELECT ProID, language, authorID FROM submit WHERE RunID=%d", RunID);
    Mysql_query(&conn, sql);
    res=Mysql_use_result(&conn);
    if(res)
    {
        if((row=mysql_fetch_row(res))!=NULL)
        {
            sscanf(row[0], "%d", ProID);
            sscanf(row[1], "%d", language);
            sscanf(row[2], "%s", authorID);
        }
        mysql_free_result(res);
    }
}

void get_problem_info(int ProID, int *timelimit, int *casetimelimit, int *memlimit, int *spj)
{
    sprintf(sql, "SELECT timelimit, casetimelimit, memlimit, spj FROM problem WHERE ProID=%d", ProID);
    Mysql_query(&conn, sql);
    res=Mysql_use_result(&conn);
    if(res)
    {
        if((row=mysql_fetch_row(res))!=NULL)
        {
            sscanf(row[0], "%d", timelimit);
            sscanf(row[1], "%d", casetimelimit);
            sscanf(row[2], "%d", memlimit);
            sscanf(row[3], "%d", spj);
        }
        mysql_free_result(res);
    }
}


void init_judge_environment(int RunID, int language, char *workdir)
{
    sprintf(workdir, "%s/run%d", oj_home, RunID);
    runcmd("rm %s/errmsg/%d.err", oj_home, RunID);
    runcmd("rm -r %s", workdir);
    runcmd("mkdir %s", workdir);   
    runcmd("cp %s/code/%d %s/%s", oj_home, RunID, workdir, codename[language]);
}

void update_submit(int RunID, int status, int usedtime, int usedmem, int codelen)
{
    sprintf(sql, "UPDATE submit SET status=%d, time=%d, memory=%d, codelen=%d WHERE RunID=%d",
            status, usedtime, usedmem, codelen, RunID);
    Mysql_query(&conn, sql);
}

int isinput(const char name[])
{
    int len=strlen(name);
    if(len<4||strcmp(name+len-3, ".in"))return 0;
    return len-3;
}

void preparefiles(char *dname, int len, char *workdir, char *datapath)
{
    char name[MAXLINE+1];
    strncpy(name, dname, len);
    name[len]='\0';
#ifdef DEBUG
    logerrmsg(judgelog, "name:#%s# %d copy %s/%s.in to %s/data.in", dname, len, datapath, name, workdir);
#endif
    runcmd("cp -a %s/%s.in %s/data.in", datapath, name, workdir);
    runcmd("cp -a %s/%s.out %s/data.out", datapath, name, workdir);
}

void execute_submit(char *workdir, int language, int *usedtime, int *timelimit, int *casetimelimit, int *memlimit)
{
    nice(19);
    struct rlimit lim;
    //time limit
    if(*casetimelimit)
        lim.rlim_cur=*casetimelimit/1000+1;
    else
        lim.rlim_cur=(*timelimit-*usedtime)/1000+1;
    //java need more time
    if(language==LANG_JAVA)lim.rlim_cur*=2;

    lim.rlim_max=lim.rlim_cur;
    setrlimit(RLIMIT_CPU, &lim);
    alarm(0);
    alarm((int)(lim.rlim_max*10));

    //file size limit
    lim.rlim_max=STD_F_LIM+STD_MB;
    lim.rlim_cur=STD_F_LIM;
    setrlimit(RLIMIT_FSIZE, &lim);

    //proc limit
    lim.rlim_cur=lim.rlim_max=(language==LANG_JAVA)?50:1;
    setrlimit(RLIMIT_NPROC, &lim);

    //stack size limit
    lim.rlim_cur=lim.rlim_max=STD_MB<<6;
    setrlimit(RLIMIT_STACK, &lim);

    //memory limit
    lim.rlim_cur=STD_MB/1024*(*memlimit)/2*3;
    lim.rlim_max=STD_MB/1024*(*memlimit)*2;
    if(language!=LANG_JAVA)setrlimit(RLIMIT_AS, &lim);

    chdir(workdir);
    //open files
    freopen("data.in", "r", stdin);
    freopen("user.out", "w", stdout);
    freopen("error.out", "a+", stderr);

    //begin trace
    ptrace(PTRACE_TRACEME, 0, NULL, NULL);
    
    if(language!=LANG_JAVA)chroot(workdir);

    //change uid eid sid, prevention of malicious programs
    //while(setuid(USER_JUDGER)!=0)sleep(1);
    //while(setgid(USER_JUDGER)!=0)sleep(1);
    
    switch(language)
    {
        case LANG_C:
            execl("./main", "./main", (char *)NULL);
            break;
        case LANG_CC:
            execl("./main", "./main", (char *)NULL);
            break;
        case LANG_JAVA:
            execl(java, java, "Main", (char *)NULL);
            break;
    }
    exit(0);
}

int get_proc_status(int pid, char *option)
{
    char buf[MAXLINE+1];
    FILE *fp;
    int len, ret=0;
    len=strlen(option);
    sprintf(buf, "/proc/%d/status", pid);
    fp=fopen(buf, "r");
    while(fp&&fgets(buf, MAXLINE, fp))
    {
        if(strncmp(buf, option, len)==0)
            sscanf(buf+len+1, "%d", &ret);
    }
    if(fp)fclose(fp);
    return ret;
}

int get_file_size(const char *fname)
{
    struct stat fstat;
    if(stat(fname, &fstat)==-1)return 0;
    return (int)fstat.st_size;
}

void print_runtime_error(char *msg)
{
    FILE *fp=fopen("error.out", "a+");
    if(fp)
    {
        fprintf(fp, "%s", msg);
        fclose(fp);
    }
}

void trace_submit(int pid, int *status, int *usedtime, int *usedmem, int timelimit, int casetimelimit, int memlimit, int language, int spj)
{
    int pstat, sig, ecode;
    struct user_regs_struct regs;
    struct rusage usage;
    int tmpmem, tmptime, sub=0;
    while(1)
    {
        wait4(pid, &pstat, 0, &usage);
        if(language==LANG_JAVA)tmpmem=(usage.ru_minflt*getpagesize())>>10;
        else tmpmem=get_proc_status(pid, "VmPeak:");
        if(tmpmem>*usedmem)*usedmem=tmpmem;
        if(get_file_size("error.out"))
        {
            *status=JG_RE;
            ptrace(PTRACE_KILL, pid, NULL, NULL);
            break;
        }
        if(!spj&&get_file_size("user.out")>get_file_size("data.out")*2+1024)
        {
            *status=JG_OL;
            ptrace(PTRACE_KILL, pid, NULL, NULL);
            break;
        }
        if(WIFEXITED(pstat))break;
        ecode=WEXITSTATUS(pstat);
        if(ecode==0x05||ecode==0);
        else
        {
            if(*status==JG_AC)
            {
                switch(ecode)
                {
                    case SIGCHLD:
                    case SIGALRM:
                        alarm(0);
                    case SIGKILL:
                    case SIGXCPU:
                        *status=JG_TL;
                        break;
                    case SIGXFSZ:
                        *status=JG_OL;
                        break;
                    default:
                        *status=JG_RE;
                }
                print_runtime_error(strsignal(ecode));
            }
            ptrace(PTRACE_KILL, pid, NULL, NULL);
            break;
        }
        if(WIFSIGNALED(pstat))
        {
            sig=WTERMSIG(pstat);
            if(*status==JG_AC)
            {
                switch(sig)
                {
                    case SIGCHLD:
                    case SIGALRM:
                        alarm(0);
                    case SIGKILL:
                    case SIGXCPU:
                        *status=JG_TL;
                        break;
                    case SIGXFSZ:
                        *status=JG_OL;
                        break;
                    default:
                        *status=JG_RE;
                }
                print_runtime_error(strsignal(sig));
            }
            break;
        }
        ptrace(PTRACE_GETREGS, pid, NULL, &regs);
        if(oksyscall[regs.REG_SYSCALL]==0)
        {
            *status=JG_RE;
            char msg[MAXLINE+1];
            sprintf(msg, "does not allow syscall id:%d name:%s", (int)regs.REG_SYSCALL, syscallname[regs.REG_SYSCALL]);
            print_runtime_error(msg);
            ptrace(PTRACE_KILL, pid, NULL, NULL);
            break;
        }
        else if(sub==1&&oksyscall[regs.REG_SYSCALL]>0)
            --oksyscall[regs.REG_SYSCALL];
        sub=1-sub;
        ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
    }

    //pid success terminal , then check the time and memory limit
    tmptime=(usage.ru_utime.tv_sec*1000+usage.ru_utime.tv_usec/1000)
            +(usage.ru_stime.tv_sec*1000+usage.ru_stime.tv_usec/1000);
    *usedtime+=tmptime;

    if(*status!=JG_AC)return;

    if((casetimelimit>0&&tmptime>casetimelimit)||*usedtime>timelimit)*status=JG_TL;
    else if(*usedmem>memlimit)*status=JG_ML;
}

void find_next_nonspace(char *c1, char *c2, FILE *f1, FILE *f2, int *ret)
{
    while(isspace(*c1)||isspace(*c2))
    {
        if(*c1!=*c2)
        {
            if(*c2==EOF)
            {
                while(isspace(*c1))*c1=fgetc(f1);
                continue;
            }
            else if(*c1==EOF)
            {
                while(isspace(*c2))*c2=fgetc(f2);
                continue;
            }
            else if(*c1=='\r'&&*c2=='\n')*c1=fgetc(f1);
            else if(*c1=='\n'&&*c2=='\r')*c2=fgetc(f2);
            else *ret=JG_PE;
        }
        if(isspace(*c1))*c1=fgetc(f1);
        if(isspace(*c2))*c2=fgetc(f2);
    }
}

int compare(char *workdir)
{
    chdir(workdir);
    FILE *f1, *f2;
    int ret=JG_AC;
    f1=fopen("data.out", "r");
    f2=fopen("user.out", "r");
    if(!f1||!f2)
    {
        ret=JG_RE;
        goto end;
    }
    while(1)
    {
        char c1=fgetc(f1);
        char c2=fgetc(f2);
        find_next_nonspace(&c1, &c2, f1, f2, &ret);
        while(1)
        {
            while((!isspace(c1)&&c1)||(!isspace(c2)&&c2))
            {
                if(c1==EOF&&c2==EOF)goto end;
                if(c1==EOF||c2==EOF)break;
                if(c1!=c2)
                {
                    ret=JG_WA;
                    goto end;
                }
                c1=fgetc(f1);
                c2=fgetc(f2);
            }
            find_next_nonspace(&c1, &c2, f1, f2, &ret);
            if(c1==EOF&&c2==EOF)goto end;
            if(c1==EOF||c2==EOF)
            {
                ret=JG_WA;
                goto end;
            }
            if((c1=='\n'||!c1)&&(c2=='\n'||!c2))break;
        }
    }
    end:
    if(f1)fclose(f1);
    if(f2)fclose(f2);
    return ret;
}

void compare_output(int *status, char * datapath, char *workdir, int spj)
{
    int result;
    chdir(workdir);
    if(spj)
    {
        result=runcmd("%s/spj ./data.in ./data.out ./user.out", datapath);
        if(result==0)*status=JG_AC;
        else *status=JG_WA;
    }
    else *status=compare(workdir);
}

int compile(int language)
{
    const char *CP_C[]={ "gcc", "main.c", "-o", "main", "-fno-asm", "-O2",
                         "-Wall", "-lm", "--static", NULL};
    const char *CP_X[]={ "g++", "main.cc", "-o", "main", "-fno-asm", "-O2",
                         "-Wall", "-lm", "--static", NULL};
    const char *CP_J[]={ javac, "Main.java", NULL};

    int pid;
    pid=fork();
    if(pid<0)
    {
        logerrmsg(judgelog, "fork failed when complie");
        exit(1);
    }
    if(pid==0)
    {
        struct rlimit lim;
        lim.rlim_max=90*STD_MB;
        lim.rlim_cur=90*STD_MB;
        setrlimit(RLIMIT_FSIZE, &lim);
        freopen("ce.txt", "w", stderr);
        switch(language)
        {
            case LANG_C:
                execvp(CP_C[0], (char * const *)CP_C);
                break;
            case LANG_CC:
                execvp(CP_X[0], (char * const *)CP_X);
                break;
            case LANG_JAVA:
                execvp(CP_J[0], (char * const *)CP_J);
                break;
            default:
                logerrmsg(judgelog, "does not support language %d!", language);
        }
        logerrmsg(judgelog, "Compile failed error: %s", strerror(errno));
        exit(1);
    }
    else
    {
        int pstat=0;
        waitpid(pid, &pstat, 0);
        return pstat;
    }
}

void judge(int RunID)
{
#ifdef DEBUG
    logerrmsg(judgelog, "Judge %d compiling: RunID: %d", getpid(), RunID);
#endif
    char workdir[MAXLINE+1];
    char authorID[MAXLINE+1];
    int ProID, language, spj, timelimit, casetimelimit, memlimit;
    int status, usedtime, usedmem, codelen, acnum;
    
    get_submit_info(RunID, &ProID, &language, authorID);
    get_problem_info(ProID, &timelimit, &casetimelimit, &memlimit, &spj);

    init_judge_environment(RunID, language, workdir);
    chdir(workdir);

    codelen=get_file_size(codename[language]);

    int compilestat;
    compilestat=compile(language);
    if(compilestat)
    {
        update_submit(RunID, JG_CE, 0, 0, codelen);
        //and update to user, problem contest
        runcmd("cp -a %s/ce.txt %s/errmsg/%d.err", workdir, oj_home, RunID);
        runcmd("rm -r %s", workdir);
        return;
    }
    //compile ok, then run the program
    update_submit(RunID, JG_RI, 0, 0, codelen);
#ifdef DEBUG
    logerrmsg(judgelog, "Judge %d running and judging with %d: RunID: %d", getpid(), status, RunID);
#endif
    char datapath[MAXLINE+1];
    sprintf(datapath, "%s/data/%d", oj_home, ProID);

    DIR *dp;
    struct dirent *dirnode;
    if((dp=opendir(datapath))==NULL)
    {
        logerrmsg(judgelog, "opendir error with runid %d", RunID);
        exit(1);
    }
    for(status=JG_AC, acnum=usedtime=usedmem=0;status==JG_AC&&(dirnode=readdir(dp))!=NULL;)
    {
        int namelen=isinput(dirnode->d_name);
        if(namelen==0)continue;
        init_syscalls_limits(language);
        preparefiles(dirnode->d_name, namelen, workdir, datapath);

        int pid=vfork();
        if(pid<0)
        {
            logerrmsg(judgelog, "fork failed when execute submit!!");
            exit(1);
        }
        if(pid==0)execute_submit(workdir, language, &usedtime, &timelimit, &casetimelimit, &memlimit);
#ifdef DEBUG
        logerrmsg(judgelog, "RunID %d : before trace with status %d", RunID, status);
#endif
        trace_submit(pid, &status, &usedtime, &usedmem, timelimit, casetimelimit, memlimit, language, spj);
#ifdef DEBUG
        logerrmsg(judgelog, "RunID %d : end trace with status %d", RunID, status);
#endif
        if(status!=JG_AC)break;
#ifdef DEBUG
        logerrmsg(judgelog, "RunID %d : before compare with status %d", RunID, status);
#endif
        compare_output(&status, datapath, workdir, spj);
#ifdef DEBUG
        logerrmsg(judgelog, "RunID %d : end compare with status %d", RunID, status);
#endif
        if(status==JG_AC)++acnum;
    }

    update_submit(RunID, status, usedtime, usedmem, codelen);
    //update_user
    //update_problem
    //...
    //
    if(status==JG_RE)runcmd("cp -a %s/error.out %s/errmsg/%d.err", workdir, oj_home, RunID);
#ifdef DEBUG
    logerrmsg(judgelog, "Judge %d complete with %d : RunID: %d", getpid(), status,  RunID);
#else
    runcmd("rm -r %s", workdir);
#endif
}

int main(int argc, char *argv[])
{
    /**  **/
    int RunID, flag;
#ifdef DEBUG
    logerrmsg(judgelog, "Judge started with pid %d!", getpid());
#endif
    sem_mysql=semget((key_t)SEM_MYSQL, 1, 0666|IPC_CREAT);
    sem_judge=semget((key_t)SEM_JUDGE, 1, 0666|IPC_CREAT);
    if(sem_mysql<0||sem_judge<0)
    {
        logerrmsg(judgelog, "get semaphore failed!");
        exit(1);
    }

    mysql_init(&conn);
    if(!Mysql_real_connect(&conn, dbhost, username, password, dbname, 0, NULL, 0))exit(1);

    while(already_running()&&getppid()!=1)
    {
        if(semaphore_p(sem_mysql))exit(1);
        if(semaphore_p(sem_judge))exit(1);
        flag=1;
        sprintf(sql, "SELECT RunID FROM submit WHERE status=0 LIMIT 1");
        if(!Mysql_query(&conn, sql))
        {
            res=Mysql_use_result(&conn);
            if(res)
            {
                if((row=mysql_fetch_row(res)))
                {
                    sscanf(row[0], "%d", &RunID);
                    mysql_free_result(res);
                    update_submit(RunID, JG_CP, 0, 0, 0);
                    if(semaphore_v(sem_judge))exit(1);
                    flag=0;
                    judge(RunID);
                }
                else mysql_free_result(res);
            }
        }
        if(flag&&semaphore_v(sem_judge))exit(1);
    }
#ifdef DEBUG
    logerrmsg(judgelog, "Judge end with pid %d!", getpid());
#endif
    mysql_close(&conn);
    exit(0);
}
