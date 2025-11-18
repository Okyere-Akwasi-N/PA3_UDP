//---------------------------------------------------------------------
// Assignment : PA-03 UDP Single-Threaded Server
// Date       : 11/21/2025
// Author     : Kyle Mirra      Akwasi Okyere
// File Name  : procurement.c
//---------------------------------------------------------------------

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "wrappers.h"
#include "message.h"

#define MAXFACTORIES    20

typedef struct sockaddr SA ;

/*-------------------------------------------------------*/
int main( int argc , char *argv[] )
{
    int     numFactories ,      // Total Number of Factory Threads
            activeFactories ,   // How many are still alive and manufacturing parts
            iters[ MAXFACTORIES+1 ] = {0} ,  // num Iterations completed by each Factory
            partsMade[ MAXFACTORIES+1 ] = {0} , totalItems = 0;

    unsigned int addrLen;

    char  *myName = "Kyle Mirra and Akwasi Okyere" ; 
    printf("\nPROCUREMENT: Started. Developed by %s\n\n" , myName );    

    char myUserName[30] ;
    getlogin_r ( myUserName , 30 ) ;
    time_t  now;
    time( &now ) ;
    fprintf( stdout , "Logged in as user '%s' on %s\n\n" , myUserName ,  ctime( &now)  ) ;
    fflush( stdout ) ;
    
    if ( argc < 4 )
    {
        printf("PROCUREMENT Usage: %s  <order_size> <FactoryServerIP>  <port>\n" , argv[0] );
        exit( -1 ) ;  
    }

    unsigned        orderSize  = atoi( argv[1] ) ;
    char	       *serverIP   = argv[2] ;
    unsigned short  port       = (unsigned short) atoi( argv[3] ) ;
 

    /* Set up local and remote sockets */
    // missing code goes here
    int sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sd < 0) {
        err_sys("Error creating socket");
    }

    // Prepare the server's socket address structure
    // missing code goes here
    struct sockaddr_in srvrSkt;
    memset((void *) &srvrSkt, 0, sizeof(srvrSkt));
    srvrSkt.sin_family = AF_INET;
    if (inet_pton(AF_INET, serverIP, (void *) &srvrSkt.sin_addr.s_addr) != 1) {
        err_sys("Invalid IP Address");
    }

    // Send the initial request to the Factory Server
    msgBuf  msg1;
    // missing code goes here
    msg1.orderSize = orderSize;
    msg1.purpose = REQUEST_MSG;

    if (sendto(sd, (void *) &msg1, sizeof(msg1), 0, (SA *) &srvrSkt, sizeof(srvrSkt)) < 0) {
        err_sys("Error sending request message");
    }

    printf("\nPROCUREMENT Sent this message to the FACTORY server: "  );
    printMsg( & msg1 );  puts("");


    /* Now, wait for order confirmation from the Factory server */
    msgBuf  msg2;
    printf ("\nPROCUREMENT is now waiting for order confirmation ...\n" );

    addrLen = sizeof(srvrSkt);
    // missing code goes here
    if (recvfrom(sd, (void *) &msg2, sizeof(msg2), 0, (SA *) &srvrSkt, &addrLen) < 0) {
        err_sys("Error receiving order confirmation message");
    }


    printf("PROCUREMENT received this from the FACTORY server: "  );
    printMsg( & msg2 );  puts("\n");

    // missing code goes here
    numFactories = msg2.numFac;
    activeFactories = numFactories;

    // Monitor all Active Factory Lines & Collect Production Reports
    while ( activeFactories > 0 ) // wait for messages from sub-factories
    {
        // missing code goes here
        msgBuf updtMsg;
        if (recvfrom(sd, (void *) &updtMsg, sizeof(updtMsg), 0, (SA *) &srvrSkt, &addrLen) < 0) {
            err_sys("Error receiving update message");
        }

       // Inspect the incoming message
        if (updtMsg.purpose == PRODUCTION_MSG) {
            iters[updtMsg.facID]++;
            partsMade[updtMsg.facID] += updtMsg.partsMade;
            totalItems += updtMsg.partsMade;
            printf("PROCUREMENT: Factory #%3d produced %5d parts in %5d milliSecs\n", updtMsg.facID, updtMsg.partsMade, updtMsg.duration);
        } else {
            activeFactories--;
        }
    } 

    // Print the summary report
    // missing code goes here
    totalItems  = 0 ;
    printf("\n\n****** PROCUREMENT Summary Report ******\n");
    for (int i = 1; i <= numFactories; i++) {
        printf("Factory #%3d made a total of %5d parts in %3d iterations\n", i, partsMade[i], iters[i]);
    }

    printf("==============================\n") ;


    // missing code goes here
    printf("Grandd total parts made = %5d vs order size of %5d\n", totalItems, orderSize);

    printf( "\n>>> PROCUREMENT Terminated\n");

    return 0 ;
}
