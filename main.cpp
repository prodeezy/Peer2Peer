/**
 * main.cc
 **/

#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>
#include <queue>
#include <signal.h>


struct MetaData *metadata;

void init() {
	
	if (isBeacon) {
	
	} else { 

		while(!shutdown) {

                        //check if the init_neighbor_list exsits or not			
			File *neighboursFile = fopen("init_neighbor_list", "r");
                        if(neighboursFile == NULL)
                        {
				// Perform join sequence

                                printf("Neighbour List does not exist... perform Join sequence\n");
//                                writeLogEntry((unsigned char *)"//Initiating the Join process, since Init_Neighbor file not present\n");

                                //Adding Signal Handler for USR1 signal
//                                accept_pid=getpid();
//                                pthread_sigmask(SIG_UNBLOCK, &new_t, NULL);
//                                signal(SIGUSR1, my_handler);

                                joinNetwork();

                                printf("Join successfull\n");
                                printf("Terminating the Join process, Init Neighbors identified\n");
//                                writeLogEntry((unsigned char *)"//Terminating the Join process, Init Neighbors identified\n");
//                                metadata->joinTimeOut = metadata->joinTimeOut_permanent;
                                continue ;
                        }
			
		}			
	}
}

int main(int argc, char *argv[]) {

	int ret = pthread_mutex_init(&connectionMapLock, NULL) ;
        if (ret != 0){
                perror("Mutex initialization failed") ;
                //writeLogEntry((unsigned char *)"//Mutex initialization failed\n");
        }

	ret = pthread_mutex_init(&MsgCacheLock, NULL) ;
        if (ret != 0){
                perror("Mutex initialization failed") ;
                //writeLogEntry((unsigned char *)"//Mutex initialization failed\n");
        }	

        // Call the init function
//        while(!shutDown || softRestartFlag){
		

//	}
}

