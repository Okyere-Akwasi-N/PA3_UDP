//---------------------------------------------------------------------
// Assignment : PA-03 UDP Single-Threaded Server
// Date       :
// Author     : WRITE YOUR  NAME(S)  HERE  ... or risk losing points
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

    char  *myName = "MUST WRITE YOUR NAMES HERE" ; 
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


    // Prepare the server's socket address structure


    // missing code goes here



    // Send the initial request to the Factory Server
    msgBuf  msg1;


    // missing code goes here


    printf("\nPROCUREMENT Sent this message to the FACTORY server: "  );
    printMsg( & msg1 );  puts("");


    /* Now, wait for oreder confirmation from the Factory server */
    msgBuf  msg2;
    printf ("\nPROCUREMENT is now waiting for order confirmation ...\n" );


    // missing code goes here



    printf("PROCUREMENT received this from the FACTORY server: "  );
    printMsg( & msg2 );  puts("\n");



    // missing code goes here


    // Monitor all Active Factory Lines & Collect Production Reports
    while ( activeFactories > 0 ) // wait for messages from sub-factories
    {


        // missing code goes here



       // Inspect the incoming message


       // missing code goes here


       
    } 

    // Print the summary report
    totalItems  = 0 ;
    printf("\n\n****** PROCUREMENT Summary Report ******\n");


    // missing code goes here


    printf("==============================\n") ;


    // missing code goes here


    printf( "\n>>> Supervisor Terminated\n");



    // missing code goes here


    return 0 ;
}
