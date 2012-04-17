#include "main.h"

using namespace std;

void initiateNotify(int errorCode)
{

	//printf("[Notify]\t initiate Notify sequence, errorCode:%d\n", errorCode);

    //send notify msg to all neighbors to inform shutdown of itself
    pthread_mutex_lock(&currentNeighboursLock);
    for(map<struct NodeInfo,int>::iterator i = currentNeighbours.begin();i != currentNeighbours.end();++i){

        //printf("[Keyboard]\t\tNotify (%s : %d)\n" , (*i).first.hostname, (*i).first.portNo);
        sendNotificationOfShutdown((*i).second, errorCode);
    }
    pthread_mutex_unlock(&currentNeighboursLock);

    sleep(3);
}

void *keyboard_thread(void *dummy)
{
	char *input=(char*)malloc(1024*sizeof(char));
	while(!globalShutdownToggle)
	{
		sleep(1);
		int flag=1;
		signal(SIGINT,signal_handler);
		
		printf("\nservent:%d> ",metadata->portNo);
		
		fgets((char*)input,511,stdin);
		//printf("msg from keyboard:%s",input);
		if(globalShutdownToggle)
			break;
		
		if(strstr((char*)input,"status neighbors")!=NULL)
		{
			unsigned char *tokens[4];
			tokens[0]=(unsigned char*)strtok((char*)input," ");
			for(int x=1;x<4;x++)
			{
				tokens[x]=(unsigned char*)strtok(NULL," ");
				printf("[Status] \tTOKEN:'%s'\n",tokens[x]);
				if(tokens[x]==NULL)
				{
					printf("Incorrect commandline usage\n");
					//writeLog("Incorrect commandline usage in status neighbors\nRe enter\n");
					flag=0;
					break;
				}
			}
			if(flag==0)
				break;
			
			metadata->status_ttl=(uint8_t)atoi((char *)tokens[2]);
			
			int fNameLen = strlen((char *)tokens[3]);
			printf("status file length is:\t%d\n" , fNameLen);

			strncpy((char *)metadata->statusOutFileName, (char *)tokens[3], fNameLen - 1) ;
			metadata->statusOutFileName[fNameLen] = '\0';
			
			printf("[Keyboard]\t Status file:'%s'\n",metadata->statusOutFileName);
			metadata->statusFloodTimeout = metadata->lifeTimeOfMsg + 10 ;
			printf("[Keyboard]\t  keyboard OPEN status file \n");
			FILE *fp = fopen((char *)metadata->statusOutFileName, "w") ;
			
			if (fp == NULL)
			{
				//writeLogEntry((unsigned char *)"In Keyborad thread: Status File open failed\n");
				printf("[status file: Failure to open\n");
				exit(0) ;
			}
			
			fputs("V -t * -v 1.0a5\n", fp);
			fclose(fp);
			
			printf("[Status]\t  keyboard CLOSE status file \n");


			floodStatusRequestsIntoNetwork();

			// waiting for status time out, so that command prompt can be returned
			printf("[Status] waiting for status timeout...\n");
			pthread_mutex_lock(&statusMsgLock);
				statusTimerFlag = 1;
				printf("[Keyboard] reset statusTimerFlag=1 , statusFloodTimeout:%d", metadata->statusFloodTimeout);
				pthread_cond_wait(&statusMsgCV, &statusMsgLock);
			pthread_mutex_unlock(&statusMsgLock);

			serializeStatusResponsesToFile();	
		}
		
		else if(strstr((char*)input,"shutdown") != NULL)
		{
			printf("shutdown caught\n");
			globalShutdownToggle = 1;
			pthread_mutex_lock(&currentNeighboursLock);
			printf("Got the lock\n");
			for(map<struct NodeInfo, int>::iterator it = currentNeighbours.begin(); it != currentNeighbours.end(); ++it)
				sendNotificationOfShutdown((*it).second, 1);
				
			pthread_mutex_unlock(&currentNeighboursLock);
			sleep(1);
			printf("will call SIGTERM\n");
			kill(nodeProcessId,SIGTERM);
			break;
		}
		

	}
}
