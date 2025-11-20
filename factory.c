//---------------------------------------------------------------------
// Assignment : PA-03 UDP Single-Threaded Server
// Date       : 11/21/2025
// Author     : Kyle Mirra      Akwasi Okyere
// File Name  : factory.c
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
#include <pthread.h>

#include "wrappers.h"
#include "message.h"

#define MAXSTR     200
#define IPSTRLEN    50

typedef struct sockaddr SA ;

int minimum( int a , int b)
{
    return ( a <= b ? a : b ) ; 
}

void subFactory( int factoryID , int myCapacity , int myDuration ) ;

void factLog( char *str )
{
    printf( "%s" , str );
    fflush( stdout ) ;
}

/*-------------------------------------------------------*/

// Global Variable for Future Thread to Shared
int   remainsToMake , // Must be protected by a Mutex
      actuallyMade ;  // Actually manufactured items


int   numActiveFactories = 1 , orderSize ;

pthread_mutex_t remains_mutex = PTHREAD_MUTEX_INITIALIZER;

int   sd ;      // Server socket descriptor
struct sockaddr_in  
             srvrSkt,       /* the address of this server   */
             clntSkt;       /* remote client's socket       */

//------------------------------------------------------------
//  Handle Ctrl-C or KILL 
//------------------------------------------------------------
void goodbye(int sig) 
{
    // missing code goes here
    fflush(stdout);
           
    msgBuf byeMsg;
    byeMsg.purpose = htonl(PROTOCOL_ERR);
    switch( sig ) {
        case SIGTERM:
            printf("nicely asked to TERMINATE by SIGTERM ( %d ).\n" , sig ) ;
            break ;
        case SIGINT:
            printf( "\n### I (%d) have been nicely asked to TERMINATE. "
           "goodbye\n\n" , getpid() ); 
            break ;
    }

    sendto(sd, &byeMsg, sizeof(byeMsg), 0, (SA *) &clntSkt, sizeof(clntSkt));
    pthread_mutex_destroy(&remains_mutex);
    close( sd ) ;
    exit( 0 ) ;

}

/*-------------------------------------------------------*/
int main( int argc , char *argv[] )
{
    sigactionWrapper(SIGTERM, goodbye);
    sigactionWrapper(SIGINT, goodbye);

    char  *myName = "Kyle Mirra and Akwasi Okyere" ; 
    unsigned short port = 50015 ;      /* service port number  */
    int    N = 1 ;                     /* Num threads serving the client */
    socklen_t     addrLen;          /* from-address length          */

    printf("\nThis is the FACTORY server developed by %s\n\n" , myName ) ;
    char myUserName[30] ;
    getlogin_r ( myUserName , 30 ) ;
    time_t  now;
    time( &now ) ;
    fprintf( stdout , "Logged in as user '%s' on %s\n\n" , myUserName ,  ctime( &now)  ) ;
    fflush( stdout ) ;

	switch (argc) 
	{
      case 1:
        break ;     // use default port with a single factory thread
      
      case 2:
        N = atoi( argv[1] ); // get from command line
        port = 50015;            // use this port by default
        break;

      case 3:
        N    = atoi( argv[1] ) ; // get from command line
        port = atoi( argv[2] ) ; // use port from command line
        break;

      default:
        printf( "FACTORY Usage: %s [numThreads] [port]\n" , argv[0] );
        exit( 1 ) ;
    }


    // missing code goes here
    // Create the socket
    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sd < 0) {
        err_sys("Couldn't create a UDP socket");
    }

    // Prepare the server's socket address
    memset( (void *) &srvrSkt, 0, sizeof(srvrSkt));
    srvrSkt.sin_family = AF_INET;
    srvrSkt.sin_port = htons(port);
    srvrSkt.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the server to the socket
    int status = bind(sd , (SA *) &srvrSkt, sizeof(srvrSkt));
    if (status < 0) {
        err_sys("Couldn't bind the socket to the server");
    }

    // Print the socket status
    char    ipStr[ IPSTRLEN ] ;    /* dotted-dec IP addr. */
    inet_ntop( AF_INET, (void *) & srvrSkt.sin_addr.s_addr , ipStr , IPSTRLEN ) ;
    printf( "Bound socket %d to IP %s Port %d\n" , sd , ipStr , ntohs( srvrSkt.sin_port ) );
    

    int forever = 1;
    while ( forever )
    {
        addrLen = sizeof(clntSkt);
        printf( "\nFACTORY server waiting for Order Requests\n" ) ; 

        // missing code goes here
        // Wait to receive request message
        msgBuf rcvMsg;
        if (recvfrom(sd, (void *) &rcvMsg, sizeof(rcvMsg), 0, (SA *) &clntSkt, &addrLen) < 0) {
            err_sys("Error receiving the order request from the client");
        }
        printf("\n\nFACTORY server received: " ) ;
        printMsg( & rcvMsg );  puts("");

        char clientIP[IPSTRLEN];
        inet_ntop(AF_INET, (void *) &clntSkt.sin_addr.s_addr, clientIP, IPSTRLEN);
        printf("        From IP %s Port %d", clientIP, ntohs(clntSkt.sin_port));

        // Set order size and remainsToMake
        orderSize = ntohl(rcvMsg.orderSize);

        pthread_mutex_lock(&remains_mutex);
        remainsToMake += orderSize;
        pthread_mutex_unlock(&remains_mutex);

        // Create the confirmation message
        msgBuf cnfMsg;
        cnfMsg.numFac = htonl(1);
        cnfMsg.purpose = htonl(ORDR_CONFIRM);


        // Send the confirmation message
        if (sendto(sd, (void *)&cnfMsg, sizeof(cnfMsg), 0, (SA * ) &clntSkt, sizeof(clntSkt)) < 0) {
            err_sys("Error sending the order confirmation message");
        }
        printf("\n\nFACTORY sent this Order Confirmation to the client " );
        printMsg(  & cnfMsg );  puts("");
        
        subFactory( 1 , 50 , 350 ) ;  // Single factory, ID=1 , capacity=50, duration=350 ms
    }
    return 0 ;
}

void subFactory( int factoryID , int myCapacity , int myDuration )
{
    char    strBuff[ MAXSTR ] ;   // snprint buffer
    int     partsImade = 0 , myIterations = 0 ;
    msgBuf  msg;

    while (remainsToMake > 0)
    {
        
        // See if there are still any parts to manufacture

        pthread_mutex_lock(&remains_mutex);
        if ( remainsToMake <= 0 ) {
            pthread_mutex_unlock(&remains_mutex);
            break ;   // Not anymore, exit the loop
        
        }

        // missing code goes here
        // Calculate how many parts to make and sleep for the duration
        int partsToMake = minimum(remainsToMake, myCapacity);
        remainsToMake -= partsToMake;
        pthread_mutex_unlock(&remains_mutex);

        Usleep(myDuration * 1000);

        partsImade += partsToMake;
        myIterations++;

        printf("Factory #%3d: Going to make %5d parts in %4d mSec\n", factoryID, partsToMake, myDuration);

        // Send a Production Message to Supervisor
        msg.facID = htonl(factoryID);
        msg.capacity = htonl(myCapacity);
        msg.partsMade = htonl(partsToMake);
        msg.duration = htonl(myDuration);
        msg.purpose = htonl(PRODUCTION_MSG);

        if (sendto(sd, (void *) &msg, sizeof(msg), 0, (SA *) &clntSkt, sizeof(clntSkt)) < 0) {
            err_sys("Error sending production message");
        }
    }

    // Send a Completion Message to Supervisor
    msgBuf cmpMsg;
    cmpMsg.facID = htonl(1);
    cmpMsg.purpose = htonl(COMPLETION_MSG);

    if (sendto(sd, (void *) &cmpMsg, sizeof(cmpMsg), 0, (SA *) &clntSkt, sizeof(clntSkt)) < 0) {
        err_sys("Error sending completion message");
    }

    snprintf( strBuff , MAXSTR , ">>> Factory # %-3d: Terminating after making total of %-5d parts in %-4d iterations\n" 
          , factoryID, partsImade, myIterations);
    factLog( strBuff ) ;
    
}
// lab computers
// L24820 L24821
// L24814