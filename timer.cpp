#include <signal.h>
#include "main.h"

void *general_timer(void *dummy)
{
	while(!globalShutdownToggle)
	{
		sleep(1);
		if(globalShutdownToggle)
		{
			//writeLog("//Global shutdown toggle true encountered in timer thread\n");
			printf("Global shutdown toggle true encountered in timer thread\n");
			break;
		}

		if(!joinNetworkPhaseFlag)
		{
			//Auto-shutdown check
			if(metadata->autoShutDown > 0)
			{
				metadata->autoShutDown--;
				//printf("Time left to be killed--%d\n",metadata->autoShutDown);
			}
			if(metadata->autoShutDown == 0)
			{
				//writeLog("//The auto-shutdown timer count is zero KABOOM....\nSIGTERM called");
				printf("//The auto-shutdown timer count is zero.\nSIGTERM called");
				kill(nodeProcessId,SIGTERM);
			}

			//screw up the message cache
			pthread_mutex_lock(&msgCacheLock);
			map<string, struct CachePacket>::iterator temp;
			for(map<string,struct CachePacket>::iterator it=MessageCache.begin();it!=MessageCache.end();it++)
			{
				if((*it).second.msgLifetimeInCache > 0)
				{
					(*it).second.msgLifetimeInCache--;
					if((*it).second.msgLifetimeInCache == 0)
					{
						MessageCache.erase(it);
					}
				}
			}
			pthread_mutex_unlock(&msgCacheLock);
			//printf("[Timer] ... statusFloodTimeout = %d\n" , metadata->statusFloodTimeout);
			fflush(stdout);
			if(statusTimerFlag!=0)
			{
				if(metadata->statusFloodTimeout > 0) {

					metadata->statusFloodTimeout--;
					metadata->statusFloodTimeout--;
					//printf("[Timer] set statusFloodTimeout = %d\n" , metadata->statusFloodTimeout);
					fflush(stdout);

				}

				if(metadata->statusFloodTimeout <= 0) 
				{

					printf("[Timer]\t status flood timed out\n");
					fflush(stdout);
					pthread_mutex_lock(&statusMsgLock);
						printf("[Timer]\t **** Signal the status' condition variable\n");
						fflush(stdout);
						pthread_cond_signal(&statusMsgCV);
						statusTimerFlag=0;
					pthread_mutex_unlock(&statusMsgLock);
				}
			}

			//keepalivetimer tracking
			pthread_mutex_lock(&currentNeighboursLock);
			if(!currentNeighbours.empty())
			{
				for(map< struct NodeInfo, int >::iterator i=currentNeighbours.begin();i!=currentNeighbours.end();i++)
				{
					pthread_mutex_lock(&connectionMapLock);
					if(ConnectionMap[(*i).second].keepAliveTimeOut > 0)
						ConnectionMap[(*i).second].keepAliveTimeOut--;
					if(ConnectionMap[(*i).second].keepAliveTimeOut==0)
					{
						pthread_mutex_unlock(&connectionMapLock);
						destroyConnectionAndCleanup((*i).second);
						currentNeighbours.erase(i);
						pthread_mutex_lock(&connectionMapLock);
					}
					pthread_mutex_unlock(&connectionMapLock);
				}
			}
			pthread_mutex_unlock(&currentNeighboursLock);
		}
		else
		{
			if(!metadata->iAmBeacon)
			{
				if(metadata->joinTimeOut > 0)
					metadata->joinTimeOut--;

				if(metadata->joinTimeOut == 0)
				{
					kill(acceptProcessId,SIGUSR1);
					printf("Join time out,sigusr1 called\n");
					//writeLog("//Join time out occured\n");
					break;
				}
			}
		}
		//printf("Going back up---2\n");
	}

	printf("[Timer]\t********** Leaving timer thread \n");

	pthread_exit(0);

	return 0;
}
