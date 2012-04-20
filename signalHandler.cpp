/*
 * sigHandler.cpp
 *
 *  Created on: Mar 25, 2012
 *      Author: gkowshik
 */

#include "main.h"

void signal_handler(int sig)
{

	if(sig == SIGTERM)
	{
		printf("[SigHandle] Caught SIGTERM\n");
		globalShutdownToggle=1;

		pthread_mutex_lock(&currentNeighboursLock);

		for(map<struct NodeInfo, int>::iterator i=currentNeighbours.begin();i!=currentNeighbours.end();i++) {

			printf("Close Connection to a neighbors\n");
			destroyConnectionAndCleanup((*i).second);
		}
		pthread_mutex_unlock(&currentNeighboursLock);

		shutdown(acceptServerSocket,SHUT_RDWR);
		close(acceptServerSocket);
		//pthread_kill(keyboard_thread,SIGUSR2);

	}

	if(sig == SIGALRM)
	{
		printf("[SigHandle] Caught SIGALARM\n");
		pthread_exit(0);
	}

	if(sig==SIGINT)
	{
		printf("[SigHandle]\tCaught SIGINT, should write status file if required\n");

		pthread_mutex_lock(&statusMsgLock) ;

			if(statusTimerFlag)
				metadata->statusFloodTimeout = 0;
			
			printf("***Going to signal the status serialize which is on wait\n");
			pthread_cond_signal(&statusMsgCV);

		pthread_mutex_unlock(&statusMsgLock) ;

	}

	if(sig == SIGUSR1 && metadata->joinTimeOut == 0)
	{
		printf("[SigHandle] Caught SIGUSR1\n");
		//Join request timeout
		joinTimeOutFlag = 1;
		printf("Close Connection\n");
		destroyConnectionAndCleanup((*currentNeighbours.begin()).second);
	}

	if(sig == SIGUSR2)
	{
		printf("[SigHandle] Caught SIGUSR2\n");
	}

	printf("[SigHandle]\t********** Leaving sighandler thread \n");
}
