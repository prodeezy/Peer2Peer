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
		printf("msg from keyboard:%s",input);
		if(globalShutdownToggle)
			break;
		
		if(strstr((char*)input,"status neighbors")!=NULL)
		{
			unsigned char *v[4];
			
			for(int x=0;x<4;x++)
			{
				v[x]=(unsigned char*)strtok((char*)input," ");
				if(v[x]==NULL)
				{
					printf("Incorrect commandline usage\n");
					//writeLog("Incorrect commandline usage in status neighbors\nRe enter\n");
					flag=0;
					break;
				}
			}
			if(flag==0)
				break;
			
			for(int j=0;j<(int)strlen((char *)v[2]);j++)
			{
				if(isdigit(v[2][j]) == 0)
					continue;
				else
				{
					printf("TTL not a number\n");
					//writeLog("Incorrect commandline usage in status neighbors\nTTL not a number\nRe enter\n");
					flag=0;
				}
			}
			if(flag==0)
				break;
				
			metadata->status_ttl=(uint8_t)atoi((char *)v[2]);
			
			strncpy((char *)metadata->statusOutFileName, (char *)v[3], strlen((char *)v[3])) ;
			metadata->statusOutFileName[strlen((char *)metadata->statusOutFileName)] = '\0';

			metadata->statusFloodTimeout = metadata->lifeTimeOfMsg + 10 ;
			FILE *fp = fopen((char *)metadata->statusOutFileName, "w") ;
			
			if (fp == NULL)
			{
				//writeLogEntry((unsigned char *)"In Keyborad thread: Status File open failed\n");
				exit(0) ;
			}
			
			fputs("V -t * -v 1.0a5\n", fp);
			fclose(fp);
			floodStatusRequestsIntoNetwork();

			// waiting for status time out, so that command prompt can be returned
			pthread_mutex_lock(&statusMsgLock);
			statusTimerFlag = 1;
			pthread_cond_wait(&statusMsgCV, &statusMsgLock);
			pthread_mutex_unlock(&statusMsgLock);
			serializeStatusResponsesToFile();	
		}
		
		else if(strcasecmp((char*)input,"shutdown"))
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