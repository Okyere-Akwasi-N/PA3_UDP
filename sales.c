/*
Assignment  : PA 2 - MultiProcessing & IPC
Authors     : Kyle Mirra mirraka@dukes.jmu.edu      Akwasi Okyere akwasion@dukes.jmu.edu
Filename    : sales.c
*/

// sales.c
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h> 
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <time.h>   /* for RNG seeding */
#include <sys/time.h> /* for gettimeofday() */

#include "shmem.h"
#include "wrappers.h"
#include "message.h"

int     shmid, msgid ;
shData *shptr     ;
sem_t *mutex, *rendezvous_done, *rendezvous_print, *factory_log ;

pid_t supervisor_pid = -1 ;
pid_t factory_pids[MAXFACTORIES];
int numfactories_global = 0;

/* semaphore names (shared across processes) */
static const char *SEM_MUTEX_NAME           = "/mirraka_akwasion_mutex";
static const char *SEM_RENDEZVOUS_DONE_NAME = "/mirraka_akwasion_rendezvous_done";
static const char *SEM_RENDEZVOUS_PRINT_NAME= "/mirraka_akwasion_rendezvous_print";
static const char *SEM_FACTORY_LOG_NAME     = "/mirraka_akwasion_factory_log";

static void cleanup_exit() {
    /* try to unblock anyone waiting on rendezvous_done (safe even if no waiter) */
    if (rendezvous_done) Sem_post(rendezvous_done);

    /* close semaphores */
    if (mutex)            Sem_close(mutex);
    if (rendezvous_done)  Sem_close(rendezvous_done);
    if (rendezvous_print) Sem_close(rendezvous_print);
    if (factory_log)      Sem_close(factory_log);

    /* unlink named semaphores */
    Sem_unlink(SEM_MUTEX_NAME);
    Sem_unlink(SEM_RENDEZVOUS_DONE_NAME);
    Sem_unlink(SEM_RENDEZVOUS_PRINT_NAME);
    Sem_unlink(SEM_FACTORY_LOG_NAME);

    /* detach and remove shared memory */
    if (shptr) Shmdt(shptr);
    if (shmid != -1) shmctl(shmid, IPC_RMID, NULL);

    /* remove message queue */
    if (msgid != -1) msgctl(msgid, IPC_RMID, NULL);

    printf("\nAu revoir\n\n");
    exit(0);
}

static void goodbye(int sig) {
    fflush(stdout);
    switch (sig) {
        case SIGTERM:
            printf("nicely asked to TERMINATE by SIGTERM (%d).\n", sig);
            break;
        case SIGINT:
            printf("INTERRUPTED by SIGINT (%d)\n", sig);
            break;
        default:
            printf("unexpectedly SIGNALed by (%d)\n", sig);
    }

    /* kill children */
    if (supervisor_pid > 0) kill(supervisor_pid, SIGTERM);
    for (int i = 0; i < numfactories_global; ++i) {
        if (factory_pids[i] > 0) {
            kill(factory_pids[i], SIGTERM);
        }
    }

    cleanup_exit();
}

int main (int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <numfactories> <ordersize>\n", argv[0]);
        exit(1);
    }

    int numfactories = atoi(argv[1]);
    int ordersize = atoi(argv[2]);

    if (numfactories <= 0 || numfactories > MAXFACTORIES) {
        fprintf(stderr, "numfactories must be between 1 and %d\n", MAXFACTORIES);
        exit(1);
    }
    if (ordersize <= 0) {
        fprintf(stderr, "ordersize must be > 0\n");
        exit(1);
    }

    numfactories_global = numfactories;
    for (int i = 0; i < MAXFACTORIES; ++i) factory_pids[i] = -1;
    shmid = -1;
    msgid = -1;
    shptr = NULL;

    key_t shmkey, msgkey;
    int shmflg, semmode ;
    int msgflg;

    int semflg = O_CREAT | O_EXCL ; // ensure exclusive creation
    semmode = S_IRUSR | S_IWUSR ;

    /* create named POSIX semaphores */
    mutex            = Sem_open(SEM_MUTEX_NAME,           semflg, semmode, 1) ;
    rendezvous_done  = Sem_open(SEM_RENDEZVOUS_DONE_NAME, semflg, semmode, 0) ; /* Supervisor -> Sales */
    rendezvous_print = Sem_open(SEM_RENDEZVOUS_PRINT_NAME,semflg, semmode, 0) ; /* Sales -> Supervisor */
    factory_log      = Sem_open(SEM_FACTORY_LOG_NAME,     semflg, semmode, 1) ; /* protect factory.log */

    /* signal handling */
    sigactionWrapper(SIGTERM, goodbye);
    sigactionWrapper(SIGINT, goodbye);

    /* seed RNG for factory capacity/duration */
    /* call srandom() exactly once with time(NULL) */
    srandom((unsigned) time(NULL));


    /* create message queue (ensure path exists at runtime) */
    msgkey = ftok("message.h", 6);

    msgflg = IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IWOTH | S_IWGRP | S_IRGRP | S_IROTH;
    msgid = Msgget(msgkey, msgflg);

    /* create shared memory (ensure path exists at runtime) */
    shmkey = ftok("shmem.h", 5);

    shmflg = IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR ;

    shmid = Shmget(shmkey, SHMEM_SIZE, shmflg);

    shptr = (shData *) Shmat (shmid, NULL, 0) ;

    /* initialize shared data */
    shptr->order_size      = ordersize;
    shptr->made            = 0;
    shptr->remain          = ordersize;
    shptr->activeFactories = numfactories;

    /*
     * Fork Supervisor
     * Supervisor stdout -> supervisor.log
     */
    supervisor_pid = Fork();
    
    if (supervisor_pid == 0) {
        
        /* child: redirect stdout and exec supervisor */
        int fd = open("supervisor.log", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR );
        if (fd == -1) {
            perror("open supervisor.log failed");
            exit(1);
        }
        if (dup2(fd, STDOUT_FILENO) == -1) {
            perror("dup2 supervisor.log failed");
            exit(1);
        }
        close(fd);

        char nf[16], shmkeystr[32], msgkeystr[32],
             sem_done_name[64], sem_print_name[64], sem_log_name[64];
        snprintf(nf, sizeof(nf), "%d", numfactories);
        snprintf(shmkeystr, sizeof(shmkeystr), "%d", shmkey);
        snprintf(msgkeystr, sizeof(msgkeystr), "%d", msgkey);
        snprintf(sem_done_name,  sizeof(sem_done_name),  "%s", SEM_RENDEZVOUS_DONE_NAME);
        snprintf(sem_print_name, sizeof(sem_print_name), "%s", SEM_RENDEZVOUS_PRINT_NAME);
        snprintf(sem_log_name,   sizeof(sem_log_name),   "%s", SEM_FACTORY_LOG_NAME);

        /* exec supervisor (assumes binary named "supervisor" in cwd) */
        execlp("./supervisor", "Supervisor",
              nf, shmkeystr, msgkeystr,
              sem_done_name, sem_print_name, sem_log_name,
              (char *) NULL);
        /* if execl fails */
        perror("execl supervisor");
        exit(1);
    }

    /*
     * Fork Factory processes
     * All factories share factory.log (append), and must use factory_log semaphore
     */
    for (int i = 0; i < numfactories; ++i) {
        /* generate capacity and duration in parent */
        int capacity = 10 + (random() % 41);   /* 10..50 inclusive */
        int duration = 500 + (random() % 701); /* 500..1200 ms inclusive */

        char idstr[16], capstr[16], durstr[16], shmkeystr[32], msgkeystr[32], semlogname[64];
        snprintf(idstr, sizeof(idstr), "%02d", i+1);
        snprintf(capstr, sizeof(capstr), "%d", capacity);
        snprintf(durstr, sizeof(durstr), "%d", duration);
        snprintf(shmkeystr, sizeof(shmkeystr), "%d", shmkey);
        snprintf(msgkeystr, sizeof(msgkeystr), "%d", msgkey);
        snprintf(semlogname, sizeof(semlogname), "%s", SEM_FACTORY_LOG_NAME);

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork factory");
            /* attempt to kill already forked children & supervisor, then cleanup */
            goodbye(SIGTERM);
        } else if (pid == 0) {
            /* child: redirect stdout to shared factory.log (append) */
            int fd = open("factory.log", O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR );
            if (fd == -1) {
                perror("open factory.log failed");
                exit(1);
            }
            if (dup2(fd, STDOUT_FILENO) == -1) {
                perror("dup2 factory.log failed");
                exit(1);
            }
            close(fd);

            /* exec factory with capacity/duration set by parent */
            execlp("./factory", "Factory",
                  idstr, capstr, durstr,
                  shmkeystr, msgkeystr, semlogname,
                  (char *) NULL);
            perror("execl factory");
            exit(1);
        } else {
            factory_pids[i] = pid;
            printf("SALES: Factory # %3d was created, with Capacity=%4d and Duration=%4d\n",
                   i + 1, capacity, duration);
        }
    }

    /*
     * Main Sales sequence:
     * a) signals are already arranged
     * b) wait for supervisor to indicate completion (rendezvous_done semaphore)
     */
    Sem_wait(rendezvous_done); /* wait until supervisor posts to indicate factories done */

    printf("SALES: Supervisor says all Factories have completed their mission\n");

    /* c) simulate printer check */
    sleep(2);

    printf("SALES: Permission granted to print the final report\n");

    /* d) allow supervisor to print final report via rendezvous_print */
    Sem_post(rendezvous_print);

    /* e) wait for all children to terminate and reap them */
    printf("SALES: Cleaning up after the Supervisor Factory Processes\n");
    for (int i = 0; i < numfactories; ++i) {
        if (factory_pids[i] > 0) {
            waitpid(factory_pids[i], NULL, 0);
        }
    }
    
    if (supervisor_pid > 0) {
        waitpid(supervisor_pid, NULL, 0);
    }

    /* final cleanup: detach/remove shared memory, remove message queue and semaphores */
    cleanup_exit();
    return 0;
}