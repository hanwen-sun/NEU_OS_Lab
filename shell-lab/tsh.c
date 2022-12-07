/* 
 * tsh - A tiny shell program with job control
 * 
 * <Put your name and login ID here>
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */
#define MAXJID    1<<16   /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
int nextjid = 1;            /* next job ID to allocate */
char sbuf[MAXLINE];         /* for composing sprintf messages */
int pipeid = 0;             /*the pipe split id*/

struct job_t {              /* The job struct */
    pid_t pid;              /* job PID */
    int jid;                /* job ID [1, 2, ...] */
    int state;              /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE];  /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */


/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv); 
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs); 
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid); 
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid); 
int pid2jid(pid_t pid); 
void listjobs(struct job_t *jobs);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/*
 * main - The shell's main routine 
 */
int main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */
    
    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);   // 将2(异常)重定向到标准输出

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        // printf("%c\n", c);
        switch (c) {
        case 'h':             /* print help message */
            usage();
	    break;
        case 'v':             /* emit additional diagnostic info */
            verbose = 1;
	    break;
        case 'p':             /* don't print a prompt */
            emit_prompt = 0;  /* handy for automatic testing */
	    break;
	default:
            usage();
	}
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler); 

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1) {

	/* Read command line */
	if (emit_prompt) {
	    printf("%s", prompt);
	    fflush(stdout);
	}
	if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
	    app_error("fgets error");
	if (feof(stdin)) { /* End of file (ctrl-d) */
	    fflush(stdout);
	    exit(0);
	}

	/* Evaluate the command line */
	eval(cmdline);
	fflush(stdout);
	fflush(stdout);
    } 

    exit(0); /* control never reaches here */
}

/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)   
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
*/
void eval(char *cmdline) 
{
    char *argv[MAXARGS];   // argument list execve()
    char buf[MAXLINE];    // holds modified line;
    int state;    // should the job run in bg or fg
    pid_t pid1, pid2;   // process id;
    sigset_t mask_all, mask_one, prev_one;

    strcpy(buf, cmdline);
    pipeid = 0;
    state = parseline(buf, argv);
    if(argv[0] == NULL)
        return;
    int pipefd[2];
    if(pipe(pipefd) < 0) {
        printf("pipe create error!\n");
    }
    char *argv_right[MAXARGS];
    int pipe_right  = 0;
    //printf("%s\n", argv[0]);
    // printf("%d\n", pipeid);
    if(pipeid) {
        int i = pipeid;
        for(i = pipeid; argv[i] != NULL; i++) {
            argv_right[pipe_right++] = argv[i];
            argv[i] = NULL;
        }
    } 
    
    if(!builtin_cmd(argv)) {
        //pid = fork();
        sigfillset(&mask_all);
        sigemptyset(&mask_one);
        sigaddset(&mask_one, SIGCHLD);
        sigprocmask(SIG_BLOCK, &mask_one, &prev_one);    // block sigchild;
        if((pid1 = fork()) == 0) {   /* child runs user job */
            setpgid(0, 0);     // leave the child away from the parent's process group;
            sigprocmask(SIG_SETMASK, &prev_one, NULL);   // unblock
            /*check whether the output need to redirect*/
            if(state & (1 << 1)) {
                // 默认 ">" 为倒数第二个;
                int i = 0;
                for(i = 0; argv[i] != 0; i++) {
                    if(!strcmp(argv[i], ">")) {
                        int fd = open(argv[i + 1], O_RDWR|O_CREAT|O_TRUNC, 0664);
                        dup2(fd, 1);   // duo2(oldfd, newfd);
                        close(fd);
                        argv[i] = NULL;
                        argv[i + 1] = NULL;
                    }
                }
            }
            //printf("%s\n", argv[0]);
            if(pipeid) {
                dup2(pipefd[1], 1);   // 标准输出的内容写入管道;
                close(pipefd[0]);
            }

            // printf("exec?\n");
            if(execve(argv[0], argv, environ) < 0) {   // 其中argv[0]是可执行目标文件, argv是参数列表, environ是环境变量;
                printf("%s: Command not found/\n", argv[0]);
                exit(0);
            }
        }

        else{
        if(pipeid) {    // 使用了pipe;
            sigprocmask(SIG_BLOCK, &mask_all, NULL);   // 保证Job在Delete之前成功添加!!!
            int bg = state & 1;
            addjob(jobs, pid1, bg + 1, cmdline);
            sigprocmask(SIG_SETMASK, &prev_one, NULL);  // unblock sigchild;
            if(!bg) {
                waitfg(pid1);
            }
            
            else {
                printf("%d %s", pid1, cmdline);
            } 
            //int status;
            //waitpid(pid1, &status, 0);   // 这里一定要指定等pid1!!!;

            // printf("%d %d\n",getpid(), pid);
            // printf("go here!\n");
            sigprocmask(SIG_BLOCK, &mask_one, &prev_one);    // block sigchild;
            if((pid2 = fork()) == 0) {
                // printf("fork success!\n");
                close(pipefd[1]);
                dup2(pipefd[0], 0);   // 管道读入的内容作为标准输入;
                setpgid(0, 0);     // leave the child away from the parent's process group;
                sigprocmask(SIG_SETMASK, &prev_one, NULL);   // unblock
                if(state & (1 << 1)) {
                    int i = 0;
                    for(i = 0; argv_right[i] != 0; i++) {
                        if(!strcmp(argv[i], ">")) {
                            int fd = open(argv[i + 1], O_RDWR|O_CREAT|O_TRUNC, 0664);
                            dup2(fd, 1);   // duo2(oldfd, newfd);
                            close(fd);
                            argv[i] = NULL;
                            argv[i + 1] = NULL;
                        }
                    }
                }
                
                
                if(execve(argv_right[0], argv_right, environ) < 0) {   // 其中argv[0]是可执行目标文件, argv是参数列表, environ是环境变量;
                    printf("%s: Command not found/\n", argv_right[0]);
                    printf("errno: %d\n", errno);
                    exit(0);
                }
            } 

            else {
                close(pipefd[0]);
                close(pipefd[1]);
                sigprocmask(SIG_BLOCK, &mask_all, NULL);   // 保证Job在Delete之前成功添加!!!
                int bg = state & 1;
                addjob(jobs, pipeid ? pid1:pid2, bg + 1, cmdline);
                sigprocmask(SIG_SETMASK, &prev_one, NULL);  // unblock sigchild;
                if(!bg) {
                    waitfg(0);
                }        

                else {
                    printf("%d %s", pipeid?pid1:pid2, cmdline);
                }

                // printf("go here!\n");
                return;
            }
            } 
        }
        
        
        close(pipefd[0]);
        close(pipefd[1]);
        sigprocmask(SIG_BLOCK, &mask_all, NULL);   // 保证Job在Delete之前成功添加!!!
        int bg = state & 1;
        addjob(jobs, pipeid ? pid2:pid1, bg + 1, cmdline);
        sigprocmask(SIG_SETMASK, &prev_one, NULL);  // unblock sigchild;
        if(!bg) {
            waitfg(0);
        }

        else {
            printf("%d %s", pipeid?pid2:pid1, cmdline);
        }

        //printf("go here!\n");
    }
        
    return;
}

/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.  
 */
int parseline(const char *cmdline, char **argv) 
{
    static char array[MAXLINE]; /* holds local copy of command line */
    char *buf = array;          /* ptr that traverses command line */
    char *delim;                /* points to first space delimiter */  
    int argc;                   /* number of args */
    int bg;                     /* background job? */
    int redirect = 0;               /*whether need to redirect output?*/
    int Pipe = 0;              /*whether use pipe*/
    pipeid = 0;

    strcpy(buf, cmdline);
    buf[strlen(buf)-1] = ' ';  /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
	    buf++;

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'') {
	buf++;
	delim = strchr(buf, '\'');
    }
    else {
	delim = strchr(buf, ' ');
    }

    while (delim) {
	argv[argc++] = buf;
	*delim = '\0';
    //printf("%s\n", argv[argc - 1]);
    if(!strcmp(buf, ">")) {
        // printf("buf: %s\n", buf);
        redirect = 1;
    }

    if(!strcmp(buf, "|")) {
        Pipe = 1;
        pipeid = argc;
        argv[argc - 1] = NULL;
    }
        
	buf = delim + 1;
	while (*buf && (*buf == ' ')) /* ignore spaces */
	       buf++;

	if (*buf == '\'') {
	    buf++;
	    delim = strchr(buf, '\'');
	}
	else {
	    delim = strchr(buf, ' ');
	}
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* ignore blank line */
	return 1;

    /* should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0) {
	argv[--argc] = NULL;
    }

    //int i = 0;
    //for(i = 0; i < argc; i++)
        //printf("%s\n",argv[i]);
    return (bg | redirect << 1 | Pipe << 2);
}

/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 */
int builtin_cmd(char **argv) 
{
    if(!strcmp(argv[0], "quit"))
        exit(0);
    if(!strcmp(argv[0], "jobs")) {
        // printf("process jobs!\n");
        listjobs(jobs);
        return 1;
    }
    if(!strcmp(argv[0], "bg") || !strcmp(argv[0], "fg")) {
        do_bgfg(argv);
        return 1;
    }
    if(!strcmp(argv[0], "&"))
        return 1;
    
    return 0;     /* not a builtin command */
}

/* 
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv) 
{
    int jid = 0, pid = 0;
    struct  job_t *job;
    if(argv[1] == NULL) {
        printf("%s command requires PID or %%jobid argument\n", argv[0]);
        return;
    }

    char *c = argv[1];
    if(!isdigit(*c) && *c != '%') {
        //printf("%c\n", *c);
        printf("%s: argument must be a PID or %%jobid\n", argv[0]);
        return;
    }
    c++;

    while(c && *c != '\0') {
        //printf("%c\n", *c);
        if(!isdigit(*c++)) {
            printf("%s: argument must be a PID or %%jobid\n", argv[0]);
            return;
        }
    }
        
    
    if(sscanf(argv[1], "%%%d", &jid) > 0) {
        job = getjobjid(jobs, jid);
        if (job == NULL || !(job->state == 3 || job->state == 2))
        {
            printf("%%%d: No such job\n", jid);
            return ;
        }
        pid = job->pid;
    }
    if(sscanf(argv[1], "%d", &pid) > 0) {
        job = getjobpid(jobs, pid);
        if(job == NULL || !(job->state == 3 || job->state == 2)){
            printf("(%d): No such process\n", pid);
            return ;
        }
        jid = job->jid;
    }

    if(!strcmp(argv[0], "bg")) {
        job->state = 2;
        kill(-pid, SIGCONT);
        printf("[%d] (%d) %s",job->jid, job->pid, job->cmdline);
    }
    if(!strcmp(argv[0], "fg")) {
        job->state = 1;
        kill(-pid, SIGCONT);
        waitfg(pid);   // 这里的pid没用;
        // printf("go here!\n");
    }

    return;
}

/* 
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
    // 这个时候的信号除了sigchild都是block;
    sigset_t mask_one;
    sigemptyset(&mask_one);
    //sigaddset(&mask_one, SIGCHLD);
    
    while(fgpid(jobs)) {             // suspend将原来的被mask信号临时替换为 传入的mask_one
        sigsuspend(&mask_one);   // 即都不mask, 并pause()(前两步是原子的), 等待下一次接受到信号被唤醒, 再次替换为原来的mask;
    }                            // 然后再进入循环体;

    //printf("waitfg!\n");
    return;
}

/*****************
 * Signal handlers
 *****************/

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
void sigchld_handler(int sig) 
{
    //printf("call chld handler!\n");

    int status;
    sigset_t mask, prev;
    sigfillset(&mask);
    int olderrno = errno;
    pid_t pid;
    while((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {    // 有子进程被终止或停止，都返回pid;(SIGCONT返回-1)
        //printf("chld pid: %d\n", pid);
        sigprocmask(SIG_BLOCK, &mask, &prev);   // 这里需要mask, 防止出现正常消亡再加control c或control z;
        if(WIFEXITED(status)) {    // 子进程正常exit()退出;
            // int pid = fgpid(jobs);
            // printf("handle pid: %d\n", pid);
            if(pid != 0) 
                deletejob(jobs, pid);
        }

        if(WIFSTOPPED(status)) {    // 特别注意, 这里也可能是别的进程发来的信号, 所以这里也要处理;
            //printf("chld catch stopped! %d\n", pid);
            struct job_t *job = getjobpid(jobs, pid);
            
            if(job->state != 3) {
                printf("Job [%d] (%d) stopped by SIGTSTP.\n", job->jid, pid);
                job->state = 3;
            }
            
            return;
        }
        if(WIFSIGNALED(status)) {   // 非正常终止的信号;
            //printf("unknown signal: %d\n", pid);    
            struct job_t *job = getjobpid(jobs, pid);   

            if(job != NULL) {
                printf("Job [%d] (%d) stopped by SIGINT.\n", job->jid, pid);
                deletejob(jobs, pid);
            }    
            
            return;
        }

        sigprocmask(SIG_SETMASK, &prev, NULL);
    }
    errno = olderrno;

    return;
}

/* 
 * sigint_handler - The kernel sends a SI0GINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig) 
{
    // printf("%d caught sigint!\n", getpid());
    sigset_t mask, prev;
    sigfillset(&mask);
    sigprocmask(SIG_BLOCK, &mask, &prev); 

    int fid = fgpid(jobs);
    int jid = pid2jid(fid);
    if(fid == 0) {
        printf("no job to be kill!\n");
        return;
    }

    printf("Job [%d] (%d) terminated by SIGINT.\n",jid, fid);
    // printf("fid: %d\n", fid);
    deletejob(jobs, fid);
    sigprocmask(SIG_SETMASK, &prev, NULL); 

    kill(-fid, SIGINT);
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig) 
{
    //printf("call sigtstp_handler!\n");
    sigset_t mask, prev;
    sigfillset(&mask);
    sigprocmask(SIG_BLOCK, &mask, &prev); 
    int fid = fgpid(jobs);
    int jid = pid2jid(fid);
    if(fid == 0) {
        printf("no job to be stopped!\n");
        return;
    }
    
    // printf("fid: %d\n", fid);
    printf("Job [%d] (%d) stopped by SIGTSTP.\n",jid, fid);
    (*getjobpid(jobs, fid)).state = ST;   // 这里不能错!!!
    sigprocmask(SIG_SETMASK, &prev, NULL);
    kill(-fid, SIGTSTP);

    return;
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs) 
{
    int i, max=0;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid > max)
	    max = jobs[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline) 
{
    int i;
    
    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == 0) {
	    jobs[i].pid = pid;
	    jobs[i].state = state;
	    jobs[i].jid = nextjid++;
	    if (nextjid > MAXJOBS)
		nextjid = 1;
	    strcpy(jobs[i].cmdline, cmdline);
  	    if(verbose){
	        printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
            }
            return 1;
	}
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == pid) {
	    clearjob(&jobs[i]);
	    nextjid = maxjid(jobs)+1;
	    return 1;
	}
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].state == FG)
	    return jobs[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
    int i;

    if (pid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid)
	    return &jobs[i];
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid) 
{
    int i;

    if (jid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid == jid)
	    return &jobs[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid) {
            return jobs[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs) 
{
    int i;
    
    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid != 0) {
	    printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
	    switch (jobs[i].state) {
		case BG: 
		    printf("Running ");
		    break;
		case FG: 
		    printf("Foreground ");
		    break;
		case ST: 
		    printf("Stopped ");
		    break;
	    default:
		    printf("listjobs: Internal error: job[%d].state=%d ", 
			   i, jobs[i].state);
	    }
	    printf("%s", jobs[i].cmdline);
	}
    }
}
/******************************
 * end job list helper routines
 ******************************/


/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void) 
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler) 
{
    struct sigaction action, old_action;

    action.sa_handler = handler;  
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
	unix_error("Signal error");
    return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig) 
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}



