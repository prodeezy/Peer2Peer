/*
 * keepAlive.cpp
 *
 *  Created on: Mar 25, 2012
 *      Author: gkowshik
 */

#include "main.h"

void *keepAliveSenderThread(void *dummy)
{
	while(!shutDownFlag)
	{

		//printf("[KeepAlive] \t..\n");
		//Get the lock to send keep alive msgs to its nieghbors
		pthread_mutex_lock(&neighbourNodeMapLock);
		for(map<struct NodeInfo,int>::iterator i=neighbourNodeMap.begin();i!=neighbourNodeMap.end();i++)
		{


			pthread_mutex_lock(&connectionMapLock);
			if(ConnectionMap[(*i).second].keepAliveTimer > 0) {

				ConnectionMap[(*i).second].keepAliveTimer--;
				//printf("[KeepAlive]\t\t... keepAliveTimer(%d) : %d\n"
					//	, (*i).second, ConnectionMap[(*i).second].keepAliveTimer);
			} else
			{
				if(ConnectionMap[(*i).second].keepAliveTimeOut!=0)	//Check the requirement for ready...
				{
					struct Message m;
					m.msgType=0xF8;
					m.status=0;

					//printf("[KeepAlive]\tPush keepalive message in queue\n");
					safePushMessageinQ("KeepAlive",(*i).second,m);
				}
			}
			pthread_mutex_unlock(&connectionMapLock);
		}
		pthread_mutex_unlock(&neighbourNodeMapLock);

		sleep(1);
	}

	printf("[KeepAlive]\tExited!\n");
	pthread_exit(0);
	return 0;

}
