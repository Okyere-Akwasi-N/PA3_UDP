/*
Assignment  : PA 2 - MultiProcessing & IPC
Authors     : Kyle Mirra mirraka@dukes.jmu.edu      Akwasi Okyere akwasion@dukes.jmu.edu
Filename    : supervisor.c
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
    if (argc < 6) {
        fprintf(stderr, "Usage: %s <numfactories> <shmkey> <msgkey> <rendezvous_done_sem> <rendezvous_print_sem> [factory_log_sem]\n", argv[0]);
        exit(1);
    }

    int numfactories = atoi(argv[1]);
    key_t shmkey = (key_t) atoi(argv[2]);
    key_t msgkey = (key_t) atoi(argv[3]);
    const char *sem_done_name  = argv[4];
    const char *sem_print_name = argv[5];

    /* attach shared memory */
    int shmid = Shmget(shmkey, SHMEM_SIZE, 0);

    shData *sharedData = (shData *) Shmat(shmid, NULL, 0);

    /* connect to message queue */
    int msgid = Msgget(msgkey, 0);


    int semflg = O_RDWR ;
    /* open rendezvous semaphores created by Sales */
    sem_t *sem_rendezvous = Sem_open2(sem_done_name, semflg);

    sem_t *printReportSem = Sem_open2(sem_print_name, semflg);

    /* local aggregates (index 1..numfactories) */
    int *factoryParts = calloc(numfactories + 1, sizeof(int));
    int *factoryIters = calloc(numfactories + 1, sizeof(int));
    if (!factoryParts || !factoryIters) {
        fprintf(stderr, "Supervisor: calloc failed\n");
        Sem_close(sem_rendezvous);
        Sem_close(printReportSem);
        Shmdt(sharedData);
        exit(1);
    }

    printf("SUPERVISOR: Started\n");
    fflush(stdout);

    int active = numfactories;
    msgBuf m;

    while (active > 0) {
        if (msgrcv(msgid, &m, MSG_INFO_SIZE, 1, 0) == -1) {
            perror("Supervisor: msgrcv failed");
            exit(1);
        }

        int idx = m.facID;
        if (m.purpose == PRODUCTION_MSG) {
            printf("SUPERVISOR: Factory # %d produced %3d parts in %4d milliSecs\n",
                   m.facID, m.partsMade, m.duration);
            fflush(stdout);

            if (idx >= 1 && idx <= numfactories) {
                factoryParts[idx] += m.partsMade;
                factoryIters[idx] ++;
            }
        } else if (m.purpose == COMPLETION_MSG) {
            printf("SUPERVISOR: Factory # %d        COMPLETED its task\n", m.facID);
            fflush(stdout);
            active--;
        } else {
            /* ignore unsupported messages */
        }
    }

    printf("SUPERVISOR: Manufacturing is complete. Awaiting permission to print final report\n");
    fflush(stdout);

    /* notify Sales that manufacturing is done */
    Sem_post(sem_rendezvous);

    /* wait for Sales permission to print */
    Sem_wait(printReportSem);

    /* print final report */
    printf("\n****** SUPERVISOR: Final Report ******\n");
    int grandTotal = 0;
    for (int i = 1; i <= numfactories; ++i) {
        printf("Factory # %d made a total of %4d parts in %5d iterations\n",
               i, factoryParts[i], factoryIters[i]);
        grandTotal += factoryParts[i];
    }
    printf("===============================\n");
    printf("Grand total parts made = %d    vs    order size of %d\n\n",
           grandTotal, sharedData->order_size);
    printf(">>> Supervisor Terminated\n");
    fflush(stdout);

    /* cleanup */
    free(factoryParts);
    free(factoryIters);
    Sem_close(sem_rendezvous);
    Sem_close(printReportSem);
    Shmdt(sharedData);

    return 0;
}