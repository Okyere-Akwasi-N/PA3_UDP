/*
Assignment  : PA 2 - MultiProcessing & IPC
Authors     : Kyle Mirra mirraka@dukes.jmu.edu      Akwasi Okyere akwasion@dukes.jmu.edu
Filename    : factory.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <fcntl.h>

#include "shmem.h"
#include "message.h"
#include "wrappers.h"


int main(int argc, char *argv[])
{
    if (argc != 7) {
        fprintf(stderr, "Usage: %s <factoryID> <capacity> <duration_ms> <shmkey> <msgkey> <factory_log_sem_name>\n", argv[0]);
        exit(1);
    }

    int facID    = atoi(argv[1]);
    int capacity = atoi(argv[2]);
    int duration = atoi(argv[3]); /* milliseconds */

    key_t shmkey = (key_t) atoi(argv[4]);
    key_t msgkey = (key_t) atoi(argv[5]);
    
    const char *factory_log_sem_name = argv[6];

    /* get shared memory segment (already created by Sales) */
    int shmid = Shmget(shmkey, SHMEM_SIZE, 0);
    if (shmid == -1) {
        perror("Factory: Shmget failed");
        exit(1);
    }

    shData *sharedData = (shData *) Shmat(shmid, NULL, 0);
    if (sharedData == (shData *) -1) {
        perror("Factory: Shmat failed");
        exit(1);
    }

    /* get message queue (already created by Sales) */
    int msgid = Msgget(msgkey, 0);
    if (msgid == -1) {
        perror("Factory: Msgget failed");
        Shmdt(sharedData);
        exit(1);
    }

    int semflag = O_RDWR ;

    /* open factory_log semaphore (used to protect prints AND sharedData updates here) */
    sem_t *sem_factory_log = Sem_open2(factory_log_sem_name, semflag);
    if (sem_factory_log == NULL) {
        perror("Factory: Sem_open2(factory_log) failed");
        Shmdt(sharedData);
        exit(1);
    }

    /* Start message under protected section to avoid interleaving */
    Sem_wait(sem_factory_log);
    printf("Factory # %2d: STARTED. My Capacity = %3d, in %4d milliSeconds\n", facID, capacity, duration);
    fflush(stdout);
    Sem_post(sem_factory_log);

    int iterations = 0;
    int totalPartsMade = 0;

    while (sharedData->remain > 0) {
        int partsToMake;

        /* reserve work and print atomically */
        Sem_wait(sem_factory_log);
        if (sharedData->remain <= 0) {
            Sem_post(sem_factory_log);
            break;
        }
        partsToMake = (sharedData->remain < capacity) ? sharedData->remain : capacity;
        sharedData->remain -= partsToMake;

        printf("Factory # %2d: Going to make %3d parts in %4d milliSecs\n", 
               facID, partsToMake, duration);
        fflush(stdout);
        Sem_post(sem_factory_log);

        /* simulate manufacturing */
        Usleep(duration * 1000);

        /* update made count under protection */
        Sem_wait(sem_factory_log);
        sharedData->made += partsToMake;
        Sem_post(sem_factory_log);

        /* send PRODUCTION message to Supervisor */
        msgBuf msg;
        msg.mtype = 1;
        msg.purpose = PRODUCTION_MSG;
        msg.facID = facID;
        msg.capacity = capacity;
        msg.partsMade = partsToMake;
        msg.duration = duration;

        if (msgsnd(msgid, &msg, MSG_INFO_SIZE, 0) == -1) {
            Sem_wait(sem_factory_log);
            fprintf(stderr, "Factory %d: msgsnd(PRODUCTION) failed: %s\n", facID, strerror(errno));
            fflush(stderr);
            Sem_post(sem_factory_log);
        }

        totalPartsMade += partsToMake;
        iterations++;
    }

    /* send COMPLETION message */
    msgBuf cm;
    cm.mtype = 1;
    cm.purpose = COMPLETION_MSG;
    cm.facID = facID;
    cm.capacity = capacity;
    cm.partsMade = totalPartsMade;
    cm.duration = duration;

    if (msgsnd(msgid, &cm, MSG_INFO_SIZE, 0) == -1) {
        Sem_wait(sem_factory_log);
        fprintf(stderr, "Factory %d: msgsnd(COMPLETION) failed: %s\n", facID, strerror(errno));
        fflush(stderr);
        Sem_post(sem_factory_log);
    }

    /* final termination print */
    Sem_wait(sem_factory_log);
    printf(">>> Factory # %2d: Terminating after making total of %4d parts in %4d iterations\n",
           facID, totalPartsMade, iterations);
    fflush(stdout);
    Sem_post(sem_factory_log);

    /* detach and close */
    Sem_close(sem_factory_log);
    Shmdt(sharedData);

    return 0;
}