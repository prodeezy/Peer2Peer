#include "main.h"

using namespace std;

void *keepAliveTimer_thread(void *dummy)
{
	while(!globalShutdownToggle)
	{
		sleep(1);
		
		if(!globalShutdownToggle)
		{
			pthread_mutex_lock(&currentNeighboursLock);
			
			for(map<struct NodeInfo, int>::iterator i=currentNeighbours.begin();i!=currentNeighbours.end();i++)
			{
				pthread_mutex_lock(&connectionMapLock);
				if(ConnectionMap[(*i).second].keepAliveTimer > 0)
					ConnectionMap[(*i).second].keepAliveTimer--;
				if(ConnectionMap[(*i).second].keepAliveTimer!=-1 && ConnectionMap[(*i).second].isReady==2)
				{
					struct Message m;
					m.status = 0;
					m.msgType=0xf8;
					safePushMessageinQ((*i).second,m);
				}
				pthread_mutex_unlock(&connectionMapLock);
			}
			
			pthread_mutex_unlock(&currentNeighboursLock);
		}
		else
			break;
	}
}