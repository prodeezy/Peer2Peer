#include <stdio.h>
#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

//#include "iniParser.h"
#include "main.h"

void *timerThread(void *dummy)
{
	while(!shutDownFlag)
	{

		sleep(1);	//The timer wakes up after one second,used as global timer.

		//printf("[Timer] \t..\n");

/**
		//Check if the node is in join phase and also it is a regular node
		if(joinNetworkPhaseFlag)
				printf("[Timer] \t...jointTimeout : %d, joinNetworkPhaseFlag :TRUE \n"
									, metadata->joinTimeOut );
		else
				printf("[Timer] \t...jointTimeout : %d, joinNetworkPhaseFlag :FALSE \n"
									, metadata->joinTimeOut );
**/
		if(!metadata->isBeacon && joinNetworkPhaseFlag)
		{
			//Check if join timeout has expired.
			/******
			if(metadata->joinTimeOut>0)
			{
				metadata->joinTimeOut=metadata->joinTimeOut-1;

				if(metadata->joinTimeOut==0)
				{
					//Timeout occured,hence kill the client/accept thread.
					printf("[Timer]\t Join Timeout, send SIGUSR1\n");
					kill(acceptProcessId,SIGUSR1);
					break;
				}
			}
			****/
			if(metadata->joinTimeOut > 0) {

				metadata->joinTimeOut--;

			} else {

				if(metadata->joinTimeOut > 0)
					metadata->joinTimeOut--;
				else
				{
					if(metadata->joinTimeOut==0)
					{
						printf("[Timer]\t Join Timeout, send SIGUSR1\n");
						kill(acceptProcessId, SIGUSR1);
						break;
					}
				}
			}
		}

		//Consider the auto shutdown time.
		if(!joinNetworkPhaseFlag)
		{
			if(metadata->autoShutDown>0)
			{
				metadata->autoShutDown--;
				if(metadata->autoShutDown==0)
				{
					printf("[Timer]\t Auto-Shutdown sequence! The node with port number %d will be killed now\n",metadata->portNo);
					printf("[Timer]\t send SIGTERM\n");

					initiateNotify(2);
					kill(nodeProcessId,SIGTERM);
				}
			}
		}

		//Status timer flag
		if(statusTimerFlag && !joinNetworkPhaseFlag)
		{
			if(metadata->statusResponseTimeout > 0)
			{
				metadata->statusResponseTimeout--;
				if(metadata->statusResponseTimeout==0)
				{

				//	printf("[Timer]\tWrite to status file\n");
				//	writeToStatusFile();

					printf("[Timer]\t Disable statusTimer.. (Halt) sending/receiveing status responses\n");
					pthread_mutex_lock(&statusMsgLock);
						statusTimerFlag = 0;
					pthread_cond_signal(&statusMsgCV);
					pthread_mutex_unlock(&statusMsgLock);
				}
			}
		}

		//Check message,what is checkresponsetimeout
		if(checkTimerFlag && !joinNetworkPhaseFlag)
		{
			metadata->checkResponseTimeout--;
			if(metadata->checkResponseTimeout==0)
			{
				checkTimerFlag=0;
				softRestartEnable=1;


				initiateNotify(3);

	/*****
				pthread_mutex_lock(&connectionMapLock);
				for(map<struct NodeInfo, int>::iterator i=neighbourNodeMap.begin();i!=neighbourNodeMap.end();i++) {
					printf("notify message will be called\n");
					notifyMessageSend((*i).second, 3);
				}
				pthread_mutex_unlock(&connectionMapLock);
	*****/

				sleep(1);
				kill(nodeProcessId,SIGTERM);
			}
		}

		//Delete messages whose lifetime is over from message cache
		if(!joinNetworkPhaseFlag)
		{
			pthread_mutex_lock(&msgCacheLock);
			map<string, struct CachePacket>::iterator temp;
			map<string, struct CachePacket>::iterator iterator;
			iterator=MessageCache.begin();
			while(iterator!=MessageCache.end())
			{
				(*iterator).second.msgLifetimeInCache--;
				if((*iterator).second.msgLifetimeInCache==0)
				{
					temp=iterator;
					MessageCache.erase(temp);
				}
				++iterator;
			}
			pthread_mutex_unlock(&msgCacheLock);
		}

		//KeepAliveTimeOut timer

		pthread_mutex_lock(&neighbourNodeMapLock);
		if(!neighbourNodeMap.empty() && !joinNetworkPhaseFlag)		//Check if the node has any connections
		{
			map<struct NodeInfo, int>::iterator i=neighbourNodeMap.begin();
			while(i!=neighbourNodeMap.end())
			{
				pthread_mutex_lock(&connectionMapLock);
				if(ConnectionMap[(*i).second].keepAliveTimeOut>0)
				{
					ConnectionMap[(*i).second].keepAliveTimeOut--;
					if(ConnectionMap[(*i).second].keepAliveTimeOut==0)
					{
						pthread_mutex_unlock(&connectionMapLock);
						closeConnection((*i).second);
						neighbourNodeMap.erase(i);
						pthread_mutex_lock(&connectionMapLock);
					}
				}
				i++;
				pthread_mutex_unlock(&connectionMapLock);
			}
		}
		pthread_mutex_unlock(&neighbourNodeMapLock);
	}
	printf("[Timer]\tExited!\n");
	pthread_exit(0);
	return 0;
}
