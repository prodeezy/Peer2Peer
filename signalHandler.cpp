/*
 * sigHandler.cpp
 *
 *  Created on: Mar 25, 2012
 *      Author: gkowshik
 */

#include "main.h"

void signalHandler(int sig)
{

	if(sig == SIGTERM)
	{
		printf("[SigHandle] Caught SIGTERM\n");
		shutDownFlag=1;

		pthread_mutex_lock(&neighbourNodeMapLock);

		for(map<struct NodeInfo, int>::iterator i=neighbourNodeMap.begin();i!=neighbourNodeMap.end();i++) {

			printf("Close Connection to a neighbors\n");
			closeConnection((*i).second);
		}
		pthread_mutex_unlock(&neighbourNodeMapLock);

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
		printf("[SigHandle]\tCaught SIGINT, should write status file\n");

		pthread_mutex_lock(&statusMsgLock) ;

			if(statusTimerFlag)
				metadata->statusResponseTimeout = 0;

			pthread_cond_signal(&statusMsgCV);

		pthread_mutex_unlock(&statusMsgLock) ;

	}

	if(sig == SIGUSR1 && metadata->joinTimeOut == 0)
	{
		printf("[SigHandle] Caught SIGUSR1\n");
		//Join request timeout
		joinTimeOutFlag = 1;
		printf("Close Connection\n");
		closeConnection((*neighbourNodeMap.begin()).second);
	}

	if(sig == SIGUSR2)
	{
		printf("[SigHandle] Caught SIGUSR2\n");
	}

}
