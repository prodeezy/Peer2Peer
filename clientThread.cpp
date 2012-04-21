#include "main.h"

int clientConnect(UCHAR *hostname,short int server_pt)
{

    //printf("--->In clientConnect:hostname==%s and port %d\n",hostname,server_pt);
	char server_port[10];
	memset(server_port,'\0',10);
	sprintf((char *)server_port,"%d",server_pt);
	//printf("Port number in char is %s\n",server_port);
	struct addrinfo hints, *server_info;    
    int nSocket=0;
    int status;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int r_no;

    if ((r_no = getaddrinfo((char*)hostname, server_port, &hints, &server_info)) != 0) 
	{
            printf("\nError in getaddrinfo() function\n");
			printf("%s %s\n",hostname,server_port);
            exit(0);
    }

    printf("[Client] FETCH a new socket\n");
    if ((nSocket = socket(server_info->ai_family, server_info->ai_socktype,server_info->ai_protocol)) == -1)
	{
            perror("*****Client: socket");
            exit(0);
    }
    
    status = connect(nSocket, server_info->ai_addr, server_info->ai_addrlen);        
    if (status < 0)
		return -1;
    else
	{	
		doLog((UCHAR*)"//Connected to a node\n");
        return nSocket;
	}
}

void* clientThread(void *info)
{
	struct NodeInfo node;
	memset (&node,0,sizeof(node));
	node =*((struct NodeInfo*)info);
	int r1=0;
	int r2=0;
	list<pthread_t>childThreadList;
	
	memcpy((unsigned char*)node.hostname, ((unsigned char*)(((struct NodeInfo*)info)->hostname)),sizeof(node.hostname));	
	
	node.portNo=(((NodeInfo*)info)->portNo);
	printf("In client thread, host:%s port:%d\n",node.hostname,node.portNo);
	//printf("Inside client thread\n");
	while(!globalShutdownToggle)
	{

		pthread_mutex_lock(&currentNeighboursLock);

		if(currentNeighbours.find(node)!=currentNeighbours.end())
		{
		//printf("----->Lock left in client Thread\n");
			pthread_mutex_unlock(&currentNeighboursLock);
			//sleep(1);

		} else {

			//make connection to this beacon
			pthread_mutex_unlock(&currentNeighboursLock);
			//printf("[Client] Call clientConnect\n");
			r1 = clientConnect(node.hostname,node.portNo);
			if(r1==-1)
			{
				doLog((unsigned char*)"//In clientThread: Connection to a port failed\n");
			}
			else
			{
				printf("[Client] .... Connection made ....\n");
				struct ConnectionInfo ci;
				int minit = pthread_mutex_init(&ci.mQLock, NULL) ;
				if (minit != 0){
					//perror("Mutex initialization failed");
					doLog((unsigned char*)"//In server thread :Mutex initialization failed\n");
				}
				int cinit = pthread_cond_init(&ci.mQCv, NULL) ;
				if (cinit != 0){
					//perror("CV initialization failed") ;
					doLog((unsigned char*)"//In server thread :CV initialization failed\n");
				}
				
				ci.shutDown = 0 ;
				ci.keepAliveTimer = metadata->keepAliveTimeOut/2;
				ci.keepAliveTimeOut = metadata->keepAliveTimeOut;
				ci.isReady = 0;													//Check if this parameter is required
				ci.neighbourNode=node;
				
				pthread_mutex_lock(&connectionMapLock) ;
				ConnectionMap[r1] = ci ;
				pthread_mutex_unlock(&connectionMapLock) ;
				
				pthread_t rThread, wThread;
				
				if((pthread_create(&rThread, NULL, connectionReaderThread , (void *)r1))!=0)
				{
					//printf("//The connectionReaderThread couldnt be created in the server thread.\nHence the node will shutdown");
					doLog((unsigned char*)"//The connectionReaderThread couldnt be created in the server thread.\nHence the node will shutdown");
					exit(1);
				}
				
				pthread_mutex_lock(&connectionMapLock);
				ConnectionMap[r1].myReadId = rThread;
				pthread_mutex_unlock(&connectionMapLock);
				childThreadList.push_front(rThread);
				
				struct Message msg;
				msg.msgType = 0xfa ;
				msg.status = 0;
				msg.fromConnect = 1;
				safePushMessageinQ(r1, msg,"client_thread") ;

				if(pthread_create(&wThread, NULL, connectionWriterThread , (void *)r1)!=0)
				{
					//printf("In clientThread: connectionWriterThread creation failed\n");
					doLog((unsigned char*)"//The connectionWriterThread couldnt be created in the server thread.\nHence the node will shutdown");
					exit(1);
				}

				ConnectionMap[r1].myWriteId = wThread;
				childThreadList.push_front(wThread);
				
				//Join all threads
				void *thread;
				for(list<pthread_t>::iterator i=childThreadList.begin(); i!=childThreadList.end();i++)
				{
					if((pthread_join((*i),&thread))!=0)
					{
						//printf("In clientThread: join failed\n");
						doLog((unsigned char*)"//In server_thread: Couldnt join the threads");
						exit(1);
					}
				}
				childThreadList.clear();
			}
		}

		if(globalShutdownToggle) {
			break;
		}
		printf("[Client] sleep for %d secs\n", metadata->beaconRetryTime);
		sleep(metadata->beaconRetryTime) ;
	}

	printf("[Client]\t********** Leaving Client thread \n");
}
