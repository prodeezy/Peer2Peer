/**
 * beaconConnect.cpp
 **/
#include "main.h"

struct ConnectionInfo constructBeaconConnInfo(struct NodeInfo n)
{
	struct ConnectionInfo cInfo;
	int mres = pthread_mutex_init(&cInfo.mQLock, NULL) ;
	if(mres != 0){
		perror("Mutex initialization failed");
		writeLogEntry((unsigned char *)"//Mutex lock initilization failed\n");
	}
	/*
				int cres = pthread_cond_init(&cInfo.msQCv, NULL) ;

				if (cres != 0){
					perror("CV initialization failed") ;
					writeLogEntry((unsigned char *)"//CV initilization failed\n");
				}
	 */
	cInfo.shutDown = 0;
	cInfo.keepAliveTimer = metadata->keepAliveTimeOut / 2;
	cInfo.keepAliveTimeOut = metadata->keepAliveTimeOut;
	cInfo.isReady = 0;
	cInfo.neighbourNode = n;
	return cInfo;
}

void waitForBeaconThreadsToComplete(list<pthread_t> & beaconChildThreads)
{
	printf("[BeaconSetup]\tWaiting for beacon threads to finish..\n");
	/**
				pthread_mutex_lock(&connectionMapLock) ;
					connectionMap[resSock].myReadId = connWriteThread;
				pthread_mutex_unlock(&connectionMapLock) ;
	 **/
	//pthread Join here
	void *beaconThreadResult;
	for(list<pthread_t>::iterator it = beaconChildThreads.begin();it != beaconChildThreads.end();++it){

		printf("[BeaconSetup]\t Child thread list size: %d \n", (int)(childThreadList.size()));
		int res = pthread_join((*it), &beaconThreadResult);
		if(res != 0){
			writeLogEntry((unsigned char *)"//Thread Creation failed!!!!\n");
			//exit(EXIT_FAILURE);
		}
		it = beaconChildThreads.erase(it);
		--it;
	}

}

void *beaconConnectThread(void *args) {


	pthread_t myID = pthread_self();
	//myConnectThread[myID] = 0;

	struct NodeInfo partnerBeacon;
	memset(&partnerBeacon, 0, sizeof(partnerBeacon));
	partnerBeacon = *((struct NodeInfo *)args);

	printf("[BeaconSetup]\t ++++++++ Start Beacon connect thread for (%s : %d) \n",
								partnerBeacon.hostname, partnerBeacon.portNo);

	//stores the read & write child threads of the connection
	list<pthread_t > beaconChildThreads;

	int beaconSocket = 0;
	int res = 0;

	while(!shutDownFlag){

		//printf("[BeaconSetup]\tBeacon trying to connect to partner beacon (%s : %d)\n" , partnerBeacon.hostname, partnerBeacon.portNo);
		//signal(SIGUSR2, my_handler);

		pthread_mutex_lock(&neighbourNodeMapLock) ;

		/**
		 * Check if beacon already in connected neighbour's map
		 */
		if (neighbourNodeMap.find(partnerBeacon)!=neighbourNodeMap.end()){

			printf("[BeaconSetup]\tBeacon already connected to (%s : %d)\n", partnerBeacon.hostname, partnerBeacon.portNo);
			pthread_mutex_unlock(&neighbourNodeMapLock) ;

		}
		else {
			/**
			 * If not already in map:
			 *  - create connection with partner beacon,
			 *  - create Hello message and add to messageQ
			 *  - create read/write threads, add threads to beaconChild threads
			 *  - wait for all threads to finish
			 */

			pthread_mutex_unlock(&neighbourNodeMapLock) ;
			printf("[BeaconSetup]\tConnecting to (%s : %d)\n", partnerBeacon.hostname, partnerBeacon.portNo) ;

			//sleep(1);
			beaconSocket = connectTo("BeaconSetup", (unsigned char *)partnerBeacon.hostname, partnerBeacon.portNo) ;
			if (beaconSocket == -1 ){
				// Connection could not be established
				printf("[BeaconSetup]\tConnection could not be established with (%s : %d)\n", partnerBeacon.hostname, partnerBeacon.portNo);
			}
			else{


				printf("[BeaconSetup]\tConstruct beacon conn info for (%s : %d)\n", partnerBeacon.hostname, partnerBeacon.portNo);
				struct ConnectionInfo cInfo = constructBeaconConnInfo(partnerBeacon);


				//signal(SIGUSR2, my_handler);
				// Put conn info into dispatcher map
				pthread_mutex_lock(&connectionMapLock) ;
					ConnectionMap[beaconSocket] = cInfo ;
				pthread_mutex_unlock(&connectionMapLock) ;

				// Create a Hello type message
				struct Message helloMsg ;
				helloMsg.msgType = 0xfa ;
				helloMsg.status = 0 ;
				//m.fromConnect = 1 ;
				printf("[BeaconSetup]\tPush hello into Q for (%s : %d)\n", partnerBeacon.hostname, partnerBeacon.portNo);
				safePushMessageinQ("[BeaconSetup]",beaconSocket, helloMsg) ;




				// Create a read thread for this connection
				pthread_t connReadThread ;
				printf("[BeaconSetup]\tCreate reader for conn (%s : %d)\n", partnerBeacon.hostname, partnerBeacon.portNo);
				int readThreadRes = pthread_create(&connReadThread, NULL, connectionReaderThread , (void *)beaconSocket);
				if (readThreadRes != 0) {
					perror("Thread creation failed");
					writeLogEntry((unsigned char *)"//Read Thread Creation failed!!!!\n");
					exit(EXIT_FAILURE);
				}
				beaconChildThreads.push_front(connReadThread);



				/**
				pthread_mutex_lock(&connectionMapLock) ;
					ConnectionMap[resSock].myReadId = connReadThread;
				pthread_mutex_unlock(&connectionMapLock) ;
				 **/



				// Create a write thread
				pthread_t connWriteThread ;
				printf("[BeaconSetup]\tCreate writer for conn (%s : %d)\n", partnerBeacon.hostname, partnerBeacon.portNo);
				int writeThreadRes = pthread_create(&connWriteThread, NULL, connectionWriterThread , (void *)beaconSocket);
				if (res != 0) {
					perror("Thread creation failed");
					writeLogEntry((unsigned char *)"//Write Thread Creation failed!!!!\n");
					exit(EXIT_FAILURE);
				}
				beaconChildThreads.push_front(connWriteThread);



				/**
				 * Wait for all threads to join
				 **/
				printf("[BeaconSetup]\tWaiting for beacon threads to finish for (%s : %d)\n", partnerBeacon.hostname, partnerBeacon.portNo);
				waitForBeaconThreadsToComplete(beaconChildThreads);



				if(shutDownFlag) {

						printf("[BeaconSetup]\t Exiting1...\n");
						break;
				}
			}
		}//end of else


		if(shutDownFlag) {
			printf("[BeaconSetup]\t Exiting2...(%s : %d)\n", partnerBeacon.hostname, partnerBeacon.portNo);
			break;
		}

		printf("[BeaconSetup]\tWait for %d secs before connecting again to (%s : %d)\n", metadata->beaconRetryTime, partnerBeacon.hostname, partnerBeacon.portNo);
		// Wait for 'retry' time before making the connections again
		//myConnectThread[myID] = 1;
		sleep(metadata->beaconRetryTime) ;
		//myConnectThread[myID] = 0;
		//printf("[BeaconSetup]\tDone sleeping!!!\n");

	}

	printf("[BeaconSetup]\t Shutting down.....(%s : %d)\n", partnerBeacon.hostname, partnerBeacon.portNo);
	beaconChildThreads.clear();
	pthread_exit(0);

	printf("[BeaconSetup]\tExited!\n");
}

