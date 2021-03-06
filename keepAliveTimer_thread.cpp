#include "main.h"

using namespace std;

void *keepAliveTimer_thread(void *dummy)
{
	long counter =0;
	while(!globalShutdownToggle)
	{
		sleep(1);
		counter++;
		
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
					m.msgType= KEEPALIVE;
					//send once every 3 seconds
					if(counter%3 == 0)
							safePushMessageinQ((*i).second, m ,"keep_alive_timer_thread");

				}
				pthread_mutex_unlock(&connectionMapLock);
			}
			
			pthread_mutex_unlock(&currentNeighboursLock);
		}
		else
			break;
	}
}
