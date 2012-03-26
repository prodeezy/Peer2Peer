/*
 * keyboardReader.cpp
 *
 *  Created on: Mar 25, 2012
 *      Author: gkowshik
 */

#include "main.h"


/**
 * Notify Error Codes:
 * 			0 - unknown
 * 			1 - user shutdown
 * 			2 - unexpected kill signal
 * 			3 - self restart
 */
void initiateNotify(int errorCode)
{

	printf("[Notify]\t initiate Notify sequence, errorCode:%d\n", errorCode);

    //send notify msg to all neighbors to inform shutdown of itself
    pthread_mutex_lock(&neighbourNodeMapLock);
    for(map<struct NodeInfo,int>::iterator i = neighbourNodeMap.begin();i != neighbourNodeMap.end();++i){

        printf("[Keyboard]\t\tNotify (%s : %d)\n" , (*i).first.hostname, (*i).first.portNo);
        notifyMessageSend((*i).second, errorCode);
    }
    pthread_mutex_unlock(&neighbourNodeMapLock);

    sleep(3);
}

void *keyboardReader(void *dummy)
{
	bool checkFlag=0;
	char *inputRead;
	inputRead = (char*)malloc(1024*sizeof(char));
	memset(inputRead,'\0',1024);
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(0,&rfds);
	signal(SIGUSR2, signalHandler);
	sigset_t new_t ;
	sigemptyset(&new_t);
	sigaddset(&new_t, SIGINT);
	pthread_sigmask(SIG_UNBLOCK, &new_t, NULL);

	while(!shutDownFlag)
	{
		//Todo: remove sleep
		sleep(1);

		signal(SIGINT, signalHandler);
		//printf("[Keyboard] \t ... \n");

		if(shutDownFlag) {
			printf("[Keyboard] \tShutdown detected.. Break!\n");
			break;
		}

		fgets((char*)inputRead,511,stdin);

		if(shutDownFlag) {
			printf("[Keyboard] \tShutdown detected.. Break!\n");
			break;
		}


		if(!strcasecmp((char *)inputRead, "shutdown\n"))
		{
			shutDownFlag=1;
			initiateNotify(1);
			kill(nodeProcessId,SIGTERM);

			break;
		}
		else if(strstr((char*)inputRead,"status neighbors")!=NULL)
		{
			//fprintf(stdin, "%s ", inp);
			checkFlag = 0;

			unsigned char *value = (unsigned char *)strtok((char *)inputRead, " ");
			value = (unsigned char *)strtok(NULL, " ");

			unsigned char fileName[256];
			memset(&fileName, '\0', 256);

			//gets the ttl value here
			value = (unsigned char *)strtok(NULL, " ");
			if(value ==NULL)
				continue;


			//check if the entered value id digit or not
			for(int j=0;j<(int)strlen((char *)value);j++)
				if(isdigit(value[j]) == 0)
					continue;
			metadata->status_ttl = (uint8_t)atoi((char *)value);


			//name of the status file
			value = (unsigned char *)strtok(NULL, "\n");
			if(value ==NULL)
				continue;
			strncpy((char *)metadata->status_file, (char *)value, strlen((char *)value)) ;
			metadata->status_file[strlen((char *)metadata->status_file)] = '\0';


			metadata->statusResponseTimeout = metadata->msgLifeTime + 10 ;
			FILE *fp = fopen((char *)metadata->status_file, "w") ;
			if (fp == NULL){
				//fprintf(stderr,"File open failed\n") ;
				writeLogEntry((unsigned char *)"In Keyboard thread: Status File open failed\n");
				exit(0) ;
			}
			fputs("V -t * -v 1.0a5\n", fp) ;
			fclose(fp) ;


			getStatus() ;

			printf("[Timer]\t Enable statusTimer.. (Start) sending/receiveing status responses, statusResponseTimeout:%d\n", metadata->statusResponseTimeout);
			// waiting for status time out, so that command prompt can be returned
			pthread_mutex_lock(&statusMsgLock) ;
				statusTimerFlag = 1 ;
			pthread_cond_wait(&statusMsgCV, &statusMsgLock);
			pthread_mutex_unlock(&statusMsgLock) ;

			printf("[Keyboard]\tGot the go ahead to write to status file!!!! statusResponseTimeout : %d\n" , metadata->statusResponseTimeout);

			writeToStatusFile() ;

		}
	}

//	printf("[Keyboard]\tExited!\n");

	free(inputRead);
	pthread_exit(0);
	return 0;
}
