/**
 * main.cc
 **/

#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>
#include <queue>
#include <signal.h>

#define min(X,Y)		(((X)>(Y))	?	(Y)		:		(X))

/**
 * Variables for logging
 **/
FILE *loggerRef = NULL;
unsigned char logFilePath[512];
unsigned char data[256];
unsigned char log_entry[512];
unsigned char msg_type[5];
struct NodeInfo n;
uint32_t data_len=0;
pthread_mutex_t logEntryLock ;


int joinTimeOutFlag = 0;
bool shutDownFlag = 0 ;
unsigned char initNeighboursFilePath[512];
map<string, struct CachePacket> MessageCache ;
pthread_mutex_t msgCacheLock ;
set< set<struct NodeInfo> > statusResponse ;
map<struct NodeInfo, int> neighbourNodeMap ;
pthread_mutex_t neighbourNodeMapLock ;
map<int, struct ConnectionInfo> ConnectionMap ;
pthread_mutex_t connectionMapLock ;
list<pthread_t > childThreadList;
MetaData *metadata;
list<struct JoinResponseInfo> joinResponses ;
bool joinNetworkPhaseFlag=false;
int acceptServerSocket = 0;
int nodeProcessId = -1;
int acceptProcessId = -1;
int keepAlivePid;

int statusTimerFlag = 0 ;
pthread_mutex_t statusMsgLock ;
pthread_cond_t statusMsgCV;
int checkTimerFlag = 0 ;
int softRestartEnable = 0 ;


unsigned char *GetUOID(char *obj_type, unsigned char *uoid_buf, long unsigned int uoid_buf_sz){

	//printf("[Main] \tGetUOID\n");
	static unsigned long seq_no=(unsigned long)1;
	unsigned char sha1_buf[SHA_DIGEST_LENGTH], str_buf[104];

	snprintf((char *)str_buf, sizeof(str_buf), "%s_%s_%1ld", metadata->node_instance_id, obj_type, (long)seq_no++);
	//	printf(".....REAL = %s\n" , str_buf);

	SHA1(str_buf, strlen((const char *)str_buf), sha1_buf);
	memset(uoid_buf, 0, uoid_buf_sz);
	memcpy(uoid_buf, sha1_buf, min(uoid_buf_sz,sizeof(sha1_buf)));

	//	printf(".....ENCODED = %s\n" , uoid_buf);
	return uoid_buf;
}


//determines the type of message.
void messageType(uint8_t m_type)
{
	//printf("[Log]\t\t..begin message type, m_type:%#x\n", m_type);
	memset(&msg_type, '\0', 4);
	const char *temp_m_type;

	switch(m_type)
	{

	case 0xfc :
		temp_m_type = "JNRQ";
		break;
	case 0xfb :
		temp_m_type ="JNRS";
		break;
	case 0xfa :
		temp_m_type ="HLLO";
		break;
	case 0xf8 :
		temp_m_type ="KPAV";
		break;
	case 0xf7 :
		temp_m_type ="NTFY";
		break;
	case 0xf6 :
		temp_m_type ="CKRQ";
		break;
	case 0xf5 :
		temp_m_type ="CKRS";
		break;
	case 0xac :
		temp_m_type ="STRQ";
		break;
	case 0xab :
		temp_m_type ="STRS";
		break;
	default : break;
	}

	//printf("[Log]\t\t...after switch, temp_m_type = %s\n", temp_m_type);

	for(unsigned int i=0;i<4;i++)
		msg_type[i]=temp_m_type[i];
	msg_type[4] = '\0';

	//printf("[Log]\t\t...end message type\n");
}

//set buffer according to message type
void setData(uint8_t m_type, unsigned char *buffer)
{
	//printf("[Log]\t\t ... Set data");

	//unsigned char portNo[2];
	unsigned short int portNo;
	memset(&portNo, '\0', sizeof(portNo));
	unsigned char uoid[4];
	memset(&uoid, '\0', sizeof(uoid));
	uint32_t distance;
	memset(&distance, '\0', sizeof(distance));
	unsigned char hostName[256];
	memset(&hostName, '\0', sizeof(hostName));
	uint8_t errorCode  = 0;
	uint8_t statusType = 0x00;

	switch(m_type)
	{
	case 0xfc : 	//Join Request
		memcpy(&portNo, buffer+4, 2) ;
		for(unsigned int i=0;i < data_len-6;i++)
			hostName[i] = buffer[i+6];
		sprintf((char *)data, "%d %s", portNo, (char *)hostName);
		break;

	case 0xfb : 	//Join Response
		for(unsigned int i=0;i < 4;i++)
			uoid[i] = buffer[16+i];
		memcpy(&distance, &buffer[20], 4) ;
		memcpy(&portNo, &buffer[24], 2) ;
		for(unsigned int i=26;i < data_len;i++)
			hostName[i-26] = buffer[i];
		sprintf((char *)data, "%02x%02x%02x%02x %d %d %s", uoid[0], uoid[1], uoid[2], uoid[3], distance, portNo, hostName);
		break;

	case 0xfa : 	//Hello Message
		memcpy(&portNo, buffer, 2) ;
		for(unsigned int i=0;i < data_len-2;i++)
			hostName[i] = buffer[i+2];
		sprintf((char *)data, "%d %s", portNo, (char *)hostName);
		break;

	case 0xf8 : 	//m_type = "KPAV"
		break;
	case 0xf7 : 	//m_type = "NTFY"
		memcpy(&errorCode, buffer, 1);
		sprintf((char *)data, "%d", errorCode);
		break;
	case 0xf6 : 	//m_type = "CKRQ"

		break;
	case 0xf5 : 	//m_type = "CKRS"
		for(unsigned int i=0;i < 4;i++)
			uoid[i] = buffer[16+i];
		sprintf((char *)data, "%02x%02x%02x%02x", uoid[0], uoid[1], uoid[2], uoid[3]);
		break;
	case 0xac : 	//m_type = "STRQ"
		memcpy(&statusType, buffer, 1) ;
		if(statusType == 0x01)
			sprintf((char *)data, "%s", "neighbors");
		else
			sprintf((char *)data, "%s", "files");
		break;
	case 0xab : 	//m_type = "STRS"
		for(unsigned int i=0;i < 4;i++)
			uoid[i] = buffer[16+i];
		sprintf((char *)data, "%02x%02x%02x%02x", uoid[0], uoid[1], uoid[2], uoid[3]);
		break;
	}
}


//gets the neighbors hostname and port no for logging purposes
void getNeighbor(int sockfd)
{
	pthread_mutex_lock(&connectionMapLock);
		n = ConnectionMap[sockfd].neighbourNode;
	pthread_mutex_unlock(&connectionMapLock);
}


unsigned char *createLogRecord(unsigned char mode, int sockfd, unsigned char header[], unsigned char *buffer)
{

	//printf("[Log]\t\tCreate log record buffer:isNull?%s, header:isNull?%s\n"
		//					, (buffer==NULL)?"YES":"NO", (header==NULL)?"YES":"NO"  );

	uint8_t message_type=0;
	uint8_t ttl=0;
	unsigned char uoid[4];
	memset(&msg_type, '\0', 4);
	memset(&uoid, '\0', 4);
	unsigned char hostname[256];
	memset(&hostname, '\0', sizeof(hostname));
	memset(&data, '\0', sizeof(data));
	memset(&log_entry, '\0', sizeof(log_entry));
	struct timeval tv;
	memset(&tv, NULL, sizeof(tv));
	//struct node *n;
	memset(&n, 0, sizeof(n));

	gettimeofday(&tv, NULL);

	memcpy(&message_type, header, 1);
	//memcpy(uoid,       header+17, 4);
	//printf("[Log]\t\tIn LogEntry : ");
	//for(int i=1;i<=20;i++)
		//printf("%02x", header[i]);
	//printf("\n");

	for(int i=0;i<4;i++)
		uoid[i] = header[17+i];
	memcpy(&ttl,       header+21, 1);
	memcpy(&data_len,  header+23, 4);


	//determins the code of message_type
	//printf("[Log]\t\tGet message type, uoid:%s\n", uoid);
	messageType(message_type);

	//gets the hostname and port no of neighbor
	//printf("[Log]\t\tGet neighbour, uoid:%s , message_type:%ud\n", uoid, message_type);
	getNeighbor(sockfd);

	//set the data filed on logging
	//printf("[Log]\t\tSet data,  uoid:%s\n", uoid);
	setData(message_type, buffer);

	if(data == NULL)
		return NULL;

	if(mode == 'r')
	{
		// logging for read mode
		sprintf((char *)log_entry, "%c %10ld.%03d %s_%d %s %d %d %02x%02x%02x%02x %s\n", mode, tv.tv_sec, (int)(tv.tv_usec/1000), (char *)n.hostname, n.portNo, (char *)msg_type, (data_len + HEADER_SIZE),  ttl, uoid[0], uoid[1], uoid[2], uoid[3], (char *)data);
		fflush(loggerRef);
	}
	else if(mode == 's')
	{
		//log for messages sent
		sprintf((char *)log_entry, "%c %10ld.%03d %s_%d %s %d %d %02x%02x%02x%02x %s\n", mode, tv.tv_sec, (int)(tv.tv_usec/1000), (char *)n.hostname, n.portNo, (char *)msg_type, (data_len + HEADER_SIZE),  ttl, uoid[0], uoid[1], uoid[2], uoid[3], (char *)data);
		fflush(loggerRef);
	}
	else
	{
		// log for messages forwarded
		sprintf((char *)log_entry, "%c %10ld.%03d %s_%d %s %d %d %02x%02x%02x%02x %s\n", mode, tv.tv_sec, (int)(tv.tv_usec/1000), (char *)n.hostname, n.portNo, (char *)msg_type, (data_len + HEADER_SIZE),  ttl, uoid[0], uoid[1], uoid[2], uoid[3], (char *)data);
		fflush(loggerRef);
	}

	//printf("[Log]\t\tReturn log entry\n");
	return log_entry;
}

/**
//Creates the log entry depending on mode.
unsigned char *createLogRecord(unsigned char mode, int sockfd, unsigned char header[], unsigned char *buffer)
{

	printf("[Log]\t\tCreate log record mode:%c , sock:%d , header:%s, buffer:%s", mode, sockfd, header, buffer);

	uint8_t m_type=0;
	uint8_t ttl=0;
	unsigned char uoid[4];
	memset(&m_type, '\0', 4);
	memset(&uoid, '\0', 4);
	unsigned char hostname[256];
	memset(&hostname, '\0', sizeof(hostname));
	memset(&data, '\0', sizeof(data));
	memset(&log_entry, '\0', sizeof(log_entry));
	struct timeval tv;
	memset(&tv, NULL, sizeof(tv));
	memset(&n, 0, sizeof(n));

	printf("[Log]\t\tCreate log 2\n");

	gettimeofday(&tv, NULL);

	memcpy(&m_type, header, 1);
	printf("[Log]\t\tEntry : ");
	for(int i=1;i<=20;i++)
		printf("%02x", header[i]);
	printf("\n");

	for(int i=0;i<4;i++)
		uoid[i] = header[17+i];
	memcpy(&ttl,       header+21, 1);
	memcpy(&data_len,  header+23, 4);


	printf("[Log]\t\tCreate log 3\n");

	//find out the hex value
	messageType(m_type);

	//gets the hostname and port no of neighbor
	pthread_mutex_lock(&connectionMapLock);
	n = ConnectionMap[sockfd].neighbourNode;
	pthread_mutex_unlock(&connectionMapLock);

	//set the data filed on logging
	setData(m_type, buffer);

	printf("[Log]\t\tCreate log 4\n");

	if(data == NULL)
		return NULL;

	if(mode == 'r')
	{
		printf("[Log]\t\tCreate log 5\n");
		// logging for read mode
		sprintf((char *)log_entry, "%c %10ld.%03d %s_%d %s %d %d %02x%02x%02x%02x %s\n", mode, tv.tv_sec, (int)(tv.tv_usec/1000), (char *)n.hostname, n.portNo, (char *)m_type, (data_len + HEADER_SIZE),  ttl, uoid[0], uoid[1], uoid[2], uoid[3], (char *)data);
		fflush(loggerRef);
	}
	else if(mode == 's')
	{
		if(ConnectionMap.find(sockfd) == ConnectionMap.end()) {

			printf("[Log]\t\t sock:%d not in ConnectionMap \n" , sockfd);
		} else {

			printf("[Log]\t\t sock:%d exists in ConnectionMap \n" , sockfd);
		}

		printf("[Log]\t\tCreate log 6 .. \n");
		printf("[Log]\t\t %c %10ld.%03d %s_%d.. \n", mode,    tv.tv_sec,        (int)(tv.tv_usec/1000),   (char *)n.hostname, n.portNo);
		printf("[Log]\t\t %s   .. \n", (char *)m_type);
		printf("[Log]\t\t    %d    .. \n", (data_len + HEADER_SIZE));
		printf("[Log]\t\t       %d  .. \n", ttl);
		printf("[Log]\t\t %02x%02x%02x%02x %s .. \n", uoid[0], uoid[1], uoid[2], uoid[3], (char *)data);

		//log for messages sent
		sprintf((char *)log_entry, "%c %10ld.%03d %s_%d %s %d %d %02x%02x%02x%02x %s\n"
				, mode,    tv.tv_sec,        (int)(tv.tv_usec/1000),   (char *)n.hostname , n.portNo
				, (char *)m_type, (data_len + HEADER_SIZE),  ttl
				, uoid[0]  , uoid[1]       , uoid[2]                 , uoid[3], (char *)data);
		fflush(loggerRef);
	}
	else
	{
		printf("[Log]\t\tCreate log 7\n");
		// log for messages forwarded
		sprintf((char *)log_entry, "%c %10ld.%03d %s_%d %s %d %d %02x%02x%02x%02x %s\n", mode, tv.tv_sec, (int)(tv.tv_usec/1000), (char *)n.hostname, n.portNo, (char *)m_type, (data_len + HEADER_SIZE),  ttl, uoid[0], uoid[1], uoid[2], uoid[3], (char *)data);
		fflush(loggerRef);
	}
	printf("[Log]\t\tCreate exit\n");
	return log_entry;
}
**/

//this functions writes the passed string into the log entry
void writeLogEntry(unsigned char *tobewrittendata)
{
	//printf("[Log]\treached here %s\n",tobewrittendata);
	fprintf(loggerRef, "%s", tobewrittendata);
	fflush(loggerRef);
}


/**
 * Kickoff the server listener thread
 */
pthread_t spawnServerListenerThread()
{
	//pthread_sigmask(SIG_BLOCK, &new_t, NULL);

	printf("[Main]\t Start Node Server listener thread\n");

	pthread_t serverThread;
	int createThreadRet;
	createThreadRet = pthread_create(&serverThread, NULL, nodeServerListenerThread , (void *)NULL);

	if (createThreadRet != 0) {
		perror("Thread creation failed");
		writeLogEntry((unsigned char *)"//Accept Thread creation failed\n");
		exit(EXIT_FAILURE);
	}


	return serverThread;
}


void populateBeaconNodeInfo(struct NodeInfo & otherBeaconNode, list<struct NodeInfo*>::iterator beaconIterator)
{
	//memset(&otherBeaconNode, 0, sizeof(otherBeaconNode));
	// fill host and port
	otherBeaconNode.portNo = (*beaconIterator)->portNo;
	for(unsigned int i = 0;i < strlen((const char*)((*beaconIterator)->hostname));i++)
		otherBeaconNode.hostname[i] = (*beaconIterator)->hostname[i];

	otherBeaconNode.hostname[strlen((const char*)((*beaconIterator)->hostname))] = '\0';
}

pthread_t spawnBeaconHandlerThread(struct NodeInfo otherBeaconNode)
{
	// spawn a beacon handler thread and add to list
	pthread_t connectBeaconThread;
	int res = pthread_create(&connectBeaconThread, NULL, beaconConnectThread , (void *)&otherBeaconNode);
	if (res != 0) {
		perror("Thread creation failed");
		exit(EXIT_FAILURE);
	}
	return connectBeaconThread;
}

/**
 * Make a list of other beacons (not including me)
 */
list<struct NodeInfo*> *enlistOtherBeacons() {


	list<struct NodeInfo*> *otherBeaconNodes;
	otherBeaconNodes = new list<struct NodeInfo*>;
	struct NodeInfo *bNode;

	for(list<struct NodeInfo*>::iterator it = metadata->beaconList->begin();it != metadata->beaconList->end();it++) {

		if((*it)->portNo != metadata->portNo){

			bNode = (struct NodeInfo*)(malloc(sizeof (struct NodeInfo)));
			strncpy((char*)(bNode->hostname), const_cast<char*>((char*)((*it)->hostname)), 256);
			bNode->portNo = (*it)->portNo;
			otherBeaconNodes->push_front(bNode);
		}
	}

	return otherBeaconNodes;
}


void resetNodeTimeoutsAndFlags()
{
	shutDownFlag = 0;
	metadata->joinTimeOut = metadata->joinTimeOutFixed;
	metadata->autoShutDown = metadata->autoShutDownFixed;
	metadata->checkResponseTimeout = metadata->joinTimeOutFixed;
	checkTimerFlag = 0;
	acceptServerSocket = 0;
}

void waitForChildThreadsToFinish(char *context)
{
	// wait for child threads to finish
	printf("[%s]\tWaiting for all threads to finish , size:%d\n", context, childThreadList.size());
	void *thread_result;
	int joinRet;
	for (list<pthread_t >::iterator it = childThreadList.begin(); it != childThreadList.end(); ++it){
		joinRet = pthread_join((*it), &thread_result);
		if (joinRet != 0) {
			perror("Thread join failed");
			exit(EXIT_FAILURE);
			//continue;
		}
	}
}

void init() {

	printf("[Main]\t___________________ Init ___________________\n");
	if (metadata->isBeacon) {

		printf("******* Start Beacon Node\n");

		pthread_t beaconServerThread = spawnServerListenerThread();
		childThreadList.push_front(beaconServerThread);

		list<struct NodeInfo*> *otherBeaconNodes = enlistOtherBeacons();



		/**
		 * Create a connectBeacon thread for each beacon partner, which retries till there's a full mesh
		 **/
		for(list<struct NodeInfo *>::iterator beaconIterator = otherBeaconNodes->begin()  ;
				beaconIterator != otherBeaconNodes->end() && shutDownFlag!=1  ;
				++beaconIterator)  {

			struct NodeInfo otherBeaconNode;
			populateBeaconNodeInfo(otherBeaconNode, beaconIterator);


			pthread_t connectBeaconThread = spawnBeaconHandlerThread(otherBeaconNode);
			childThreadList.push_front(connectBeaconThread);


			// try again in a second
			sleep(1);
		}

	} else { 

		// Regular node
		printf("___________________ Start Regular Node\n");

		FILE *neighboursFile;
		while(!shutDownFlag) {

			//check if the init_neighbor_list exsits or not
			printf("[Main]\t Loop Begin : Check if neighbours file exists\n");
			neighboursFile = fopen((const char *)initNeighboursFilePath, "r");

			if(neighboursFile == NULL)
			{
				printf("[Main]\tInit neighbours doesn't exist, Perform Join Phase!\n");
				memset(&acceptProcessId, 0, sizeof(acceptProcessId));
				acceptProcessId=getpid();
				signal(SIGUSR1, signalHandler);

				// Perform join sequence
				joinNetworkPhaseFlag = true;

				printf("[Main]\tStart join phase\n");
				performJoinNetworkPhase();

				printf("Neighbour List does not exist... perform Join sequence\n");
				//                                writeLogEntry((unsigned char *)"//Initiating the Join process, since Init_Neighbor file not present\n");

				printf("Join successfull\n");
				printf("Terminating the Join process, Init Neighbors identified\n");
				//                                writeLogEntry((unsigned char *)"//Terminating the Join process, Init Neighbors identified\n");
				//                                metadata->joinTimeOut = metadata->joinTimeOut_permanent;

				joinNetworkPhaseFlag = false;

				metadata->joinTimeOut = metadata->joinTimeOutFixed;
				continue ;
			}

			metadata->joinTimeOut = -1;


			pthread_t serverThread = spawnServerListenerThread();
			childThreadList.push_front(serverThread);


			// Connect to neighbors in the list
			unsigned char nodeLineBuf[256];
			memset(&nodeLineBuf, '\0', 256);

			list<struct NodeInfo*> *neighbourNodesList;
			neighbourNodesList = new list<struct NodeInfo*>;
			struct NodeInfo *nodeInfo;




			/**
			 *  Read from the init_neighbours_file
			 **/
			while(   fgets((char *)nodeLineBuf, 255, neighboursFile) != NULL   )
			{
				//fgets(nodeName, 255, f);
				unsigned char *neighbourHostname = (unsigned char *)strtok((char *)nodeLineBuf, ":");
				unsigned char *neighbourPortno = (unsigned char *)strtok(NULL, ":");

				nodeInfo = (struct NodeInfo *)malloc(sizeof(struct NodeInfo)) ;
				strncpy((char *)nodeInfo->hostname, (char *)(neighbourHostname), strlen((char *)neighbourHostname)) ;

				nodeInfo->hostname[strlen((char *)neighbourHostname)]='\0';
				nodeInfo->portNo = atoi((char *)neighbourPortno);

				neighbourNodesList->push_back(nodeInfo) ;

			}



			int nodeConnected = 0;
			int connSocket;
			/**
			 *  Participation phase:
			 *   If a node is participating in the network, when it makes a connection
			 *    to another node, the first message it sends must be a hello message.
			 **/
			printf("[Main]\tParticipation phase\n");
			for(list<struct NodeInfo *>::iterator it = neighbourNodesList->begin();
					it != neighbourNodesList->end() && shutDownFlag!=1;
					it++)  {

				struct NodeInfo neighbourNode;
				neighbourNode.portNo = (*it)->portNo ;
				strncpy((char *)neighbourNode.hostname, (const char *)(*it)->hostname, 256) ;


				pthread_mutex_lock(&neighbourNodeMapLock) ;
				//Neighbour already in list
				if (neighbourNodeMap.find(neighbourNode)!=neighbourNodeMap.end()){

					printf("[Main]\t Neighbour (%s : %d) already in node connections list\n"
							, neighbourNode.hostname, neighbourNode.portNo);
					nodeConnected++;
					it = neighbourNodesList->erase(it) ;
					--it ;
					pthread_mutex_unlock(&neighbourNodeMapLock) ;

					// Do not connect and continue
					continue ;
				}


				pthread_mutex_unlock(&neighbourNodeMapLock) ;

				printf("[Main]\t    Connecting to %s:%d\n", (*it)->hostname, (*it)->portNo) ;
				if(shutDownFlag)
				{
					printf("[Main]\t Shutdown set \n") ;
					shutdown(connSocket, SHUT_RDWR);
					close(connSocket);
					break;
				}



				//trying to connect the host name and port no
				connSocket = connectTo(  "Main", (*it)->hostname,   (*it)->portNo) ;
				if(shutDownFlag)
				{
					shutdown(connSocket, SHUT_RDWR);
					close(connSocket);
					break;
				}
				if (connSocket == -1 ) {
					// Connection could not be established
					// now we have to reset the network, call JOIN
					printf("[Main]\t........... CONTINUE loop");
					continue;
				}



				// Initialize connection info
				struct ConnectionInfo connInfo ;
				struct NodeInfo n;
				n.portNo = (*it)->portNo ;
				strncpy((char *)n.hostname, (const char *)(*it)->hostname, 256) ;
				it = neighbourNodesList->erase(it) ;
				--it ;
				int mres = pthread_mutex_init(&connInfo.mQLock, NULL) ;
				if (mres != 0){
					perror("Mutex initialization failed");
					writeLogEntry((unsigned char *)"//Mutex Initializtaion failed\n");

				}


				//Shutdown init to zero
				connInfo.shutDown = 0 ;
				connInfo.keepAliveTimer = metadata->keepAliveTimeOut/2;
				connInfo.keepAliveTimeOut = metadata->keepAliveTimeOut;
				connInfo.isReady = 0;
				connInfo.neighbourNode = n;
				//signal(SIGUSR2, my_handler);


				// Add to dispatcher map and add this outgoing message to it's mQ
				pthread_mutex_lock(&connectionMapLock) ;
				ConnectionMap[connSocket] = connInfo ;
				pthread_mutex_unlock(&connectionMapLock) ;

				// Push a Hello type message in the writing queue
				printf("[Main]\tSend 'Hello' to neighbour node!\n");
				struct Message outgoingMsg ;
				outgoingMsg.msgType = 0xfa ;
				outgoingMsg.status = 0 ;
				//					m.fromConnect = 1 ;
				safePushMessageinQ("[Main]",connSocket, outgoingMsg) ;





				// Create a reader thread for the connection
				pthread_t connReaderThread ;
				int ret = pthread_create(&connReaderThread, NULL, connectionReaderThread , (void *)connSocket);
				if (ret != 0) {
					//perror("Thread creation failed");
					writeLogEntry((unsigned char *)"//Read Thread creation failed\n");
					exit(EXIT_FAILURE);
				}
				pthread_mutex_lock(&connectionMapLock) ;
				ConnectionMap[connSocket].myReadId = connReaderThread;
				childThreadList.push_front(connReaderThread);
				pthread_mutex_unlock(&connectionMapLock) ;




				// Create a write thread for connection
				pthread_t connWriterThread ;
				ret = pthread_create(&connWriterThread, NULL, connectionWriterThread , (void *)connSocket);
				if (ret != 0) {
					//perror("Thread creation failed");
					writeLogEntry((unsigned char *)"//Write Thread creation failed\n");
					exit(EXIT_FAILURE);
				}
				childThreadList.push_front(connWriterThread);
				pthread_mutex_lock(&connectionMapLock) ;
				ConnectionMap[connSocket].myWriteId = connWriterThread;
				pthread_mutex_unlock(&connectionMapLock) ;

				nodeConnected++;


				if(nodeConnected == (int)metadata->minNeighbor) {

					printf("[Main]\t Successfully connected to minimum neighbours, BREAK!\n");
					break;
				}


			}

			//succesfully connected to minNeighbors
			if(nodeConnected == (int)(metadata->minNeighbor) || shutDownFlag) {

				printf("[Main]\t Connected successfully to min neighbours || shutDown occurred ... Breaking out..\n");

				// set global shutdown so that the serverListenerThread dies
				fclose(neighboursFile);
				//shutDownFlag = 1;
				//shutdown(acceptServerSocket, SHUT_RDWR);
				//close(acceptServerSocket);
				//waitForChildThreadsToFinish("Main");

				sleep(1);

				break;
			} else {

				//Coulndn't connect to the Min Neighbors, need to soft restart
				fclose(neighboursFile);
				//remove((char *)tempInitFile);

				// set global shutdown so that the serverListenerThread dies
				shutDownFlag = 1;
				shutdown(acceptServerSocket, SHUT_RDWR);
				close(acceptServerSocket);


				//Send NOTIFY message beacouse of restart
				initiateNotify(3);
				/**
				pthread_mutex_lock(&neighbourNodeMapLock);
				for (map<struct NodeInfo, int>::iterator it = neighbourNodeMap.begin(); it != neighbourNodeMap.end(); ++it)
					notifyMessageSend((*it).second, 3);
				pthread_mutex_unlock(&neighbourNodeMapLock);
				 **/

				sleep(1);

				/**
				 * Close all connections from this node
				 **/
				printf("[Main]\tClose all connections\n");
				pthread_mutex_lock(&neighbourNodeMapLock) ;
				for (map<struct NodeInfo, int>::iterator it = neighbourNodeMap.begin(); it != neighbourNodeMap.end(); ++it) {
					closeConnection((*it).second);
				}
				neighbourNodeMap.clear();
				pthread_mutex_unlock(&neighbourNodeMapLock) ;


				waitForChildThreadsToFinish("Main");


				childThreadList.clear();

				printf("[Main]\tClear join responses\n");
				joinResponses.clear();


				printf("[Main]\tReset timeouts and flags\n");
				resetNodeTimeoutsAndFlags();


				printf("[Main]\t   CONTINUE loop\n");

				continue;

			}

			printf("[Main]\t___________________ End of node loop");
		}

		//fclose(neighboursFile);
	}


}

//checks if the node is a Regular node or a Beacon node
int isBeaconNode(struct NodeInfo me)
{
	// check if I am among the beacon nodes
	for(list<struct NodeInfo *>::iterator it = metadata->beaconList->begin();
			it != metadata->beaconList->end();
			it++)   {

		NodeInfo* beaconNode = *it;
		printf("[Main]\t \t=> Me=[%s : %d] , Beacon[%s : %d]\n" , me.hostname, me.portNo, beaconNode->hostname, beaconNode->portNo);

		//if((strcasecmp((char *)n.hostname, (char *)(*it)->hostName)==0) && (n.portNo == (*it)->portNo))
		if(  (strcasecmp((char *)me.hostname, (char *)beaconNode->hostname) == 0)
				&&     (me.portNo == beaconNode->portNo)  ) {

			return true;
		}
	}
	return false;
}

void cleanup(){

	int res ;
	void *thread_result ;

	printf("[Main]\t Cleanup\n");

	//KeepAlive Timer Thread, Sends KeepAlive Messages
	pthread_t keepAliveThread ;
	printf("[Main]\tKickoff keepalive thread..\n");
	res = pthread_create(&keepAliveThread, NULL, keepAliveSenderThread , (void *)NULL);
	if (res != 0) {
		//perror("Thread creation failed");
		writeLogEntry((unsigned char *)"//In Main: Thread creation failed\n");
		exit(EXIT_FAILURE);
	}
	childThreadList.push_front(keepAliveThread);



	pthread_t k_thread ;
	printf("[Main]\tKickoff keyboard thread..\n");
	res = pthread_create(&k_thread, NULL, keyboardReader , (void *)NULL);
	if (res != 0) {

		perror("Thread creation failed");
		writeLogEntry((unsigned char *)"//In Main: Thread creation failed\n");
		exit(EXIT_FAILURE);
	}
	childThreadList.push_front(k_thread);




	// Call the timer thread
	// Thread creation and join code taken from WROX Publications book
	printf("[Main]\tKickoff timer thread..\n");
	pthread_t timer_thread ;
	res = pthread_create(&timer_thread, NULL, timerThread, (void *)NULL);
	if (res != 0) {
		//perror("Thread creation failed");
		writeLogEntry((unsigned char *)"//In Main: Thread creation failed\n");
		exit(EXIT_FAILURE);
	}
	childThreadList.push_front(timer_thread);




	printf("[Main]\tWait for all child threads to finish, size:%d\n", childThreadList.size());
	int threadsLeft = childThreadList.size();
	for (list<pthread_t >::iterator it = childThreadList.begin(); it != childThreadList.end(); ++it){
		//printf("Value is : %d and SIze: %d\n", (int)(*it), (int)childThreadList.size());
		res = pthread_join((*it), &thread_result);
		threadsLeft--;
		if (res != 0) {
			//perror("Thread join failed");
			//			writeLogEntry((unsigned char *)"//In Main: Thread joining failed\n");
			exit(EXIT_FAILURE);
			//continue;
		}

		printf("[Main]\t\t Child thread finished, now size:%d, threads left : %d\n", childThreadList.size(), threadsLeft);
	}



	//clearing out the data structres used, if incase needed a restart

	pthread_mutex_lock(&neighbourNodeMapLock) ;
	neighbourNodeMap.clear();
	pthread_mutex_unlock(&neighbourNodeMapLock) ;

	pthread_mutex_lock(&connectionMapLock) ;
	ConnectionMap.clear();
	pthread_mutex_unlock(&connectionMapLock) ;

	childThreadList.clear();
	joinResponses.clear();
}

void initializeLocks()
{
	int ret = pthread_mutex_init(&connectionMapLock, NULL) ;
	if(ret != 0){
		perror("Mutex initialization failed");
		writeLogEntry((unsigned char *)"//Mutex initialization failed\n");
	}
	ret = pthread_mutex_init(&msgCacheLock, NULL) ;
	if(ret != 0){
		perror("Mutex initialization failed");
		writeLogEntry((unsigned char *)"//Mutex initialization failed\n");
	}

	ret = pthread_mutex_init(&logEntryLock, NULL) ;
	if (ret != 0){
		//perror("Mutex initialization failed") ;
		writeLogEntry((unsigned char *)"//Mutex initialization failed\n");
	}

	ret = pthread_mutex_init(&statusMsgLock, NULL) ;
	if (ret != 0){
		//perror("Mutex initialization failed") ;
		writeLogEntry((unsigned char *)"//Mutex initialization failed\n");
	}

	ret = pthread_cond_init(&statusMsgCV, NULL) ;
	if (ret != 0){
		//perror("CV initialization failed") ;
		writeLogEntry((unsigned char *)"//CV initialization failed\n");
	}

}


int usage()
{
	printf("Incorrect command line argument--\n\nUsage is: \"sv_node [-reset] <.ini file>\"\n");
	return false;
}


/**
 * Command line variables
 **/
bool resetFlag = 0;
unsigned char *iniFileFromCommandline = NULL;

int parseCommandLine(int argc, unsigned char *argv[])
{
	int iniFlag=0;

	//not enough number of arguments
	if(argc<2 || argc > 3)
		return usage();

	for(int i=1;i<argc;i++)
	{
		argv++;
		if(*argv[0]=='-')
		{
			if(strcmp((char *)(*argv),"-reset")!=0)
				return usage();
			else
				resetFlag = 1;
		}
		else
		{
			if(strstr((char *)*argv, ".ini")==NULL)
				return usage();
			else
			{
				int len = strlen((char *)*argv);
				iniFileFromCommandline = (unsigned char *)malloc(sizeof(unsigned char)*(len+1));
				strncpy((char *)iniFileFromCommandline, (char *)*argv, len);
				iniFileFromCommandline[len]='\0';
				iniFlag = 1;
			}
		}
	}
	if(iniFlag!=1)
		return usage();
	else
		return true;
}



void constructLogAndInitneighbourFileNames()
{

	//Log file and init_neighbors_file
	memset(&logFilePath, '\0', 512);
	memset(&initNeighboursFilePath, '\0', 512);

	strncpy((char *)logFilePath, (char *)metadata->homeDir, strlen((char*)metadata->homeDir));
	strncpy((char*)(initNeighboursFilePath), (char*)(metadata->homeDir), strlen((char*)(metadata->homeDir)));

	logFilePath[strlen((char*)metadata->homeDir)] = '\0';
	initNeighboursFilePath[strlen((char*)(metadata->homeDir))] = '\0';

	strcat((char *)logFilePath, "/");
	strcat((char *)logFilePath, (char *)metadata->logFileName);
	strcat((char*)(initNeighboursFilePath), "/init_neighbor_list");

	printf("[Main]\tInit neighbours file path :%s\n", initNeighboursFilePath);
	printf("[Main]\tLog file path :%s\n", logFilePath);

}

int main(int argc, char *argv[]) {

	if(!parseCommandLine(argc, (unsigned char**)(argv))){
		exit(0);
	}
	fillDefaultMetaData();
	parseINI((char*)(iniFileFromCommandline));
	printMetaData();
	free(iniFileFromCommandline);
	metadata->joinTimeOutFixed = metadata->joinTimeOut;
	metadata->autoShutDownFixed = metadata->autoShutDown;
	metadata->cacheSize *= 1024;
	nodeProcessId = getpid();
	signal(SIGTERM, signalHandler);


	// Log file and Init_neighbours_file name
	constructLogAndInitneighbourFileNames();
	loggerRef = fopen((char *)logFilePath, "a");

	//writeLogEntry((unsigned char *)"//............TESTING...........\n");

	if(strcmp((char *)metadata->homeDir, "\0")==0 || metadata->portNo == 0 || metadata->location == 0)
	{
		//Program should exit
		writeLogEntry((unsigned char *)"//Mandatory elements of Nodes were not populated\n");
		exit(0);
	}

	// Assign a node ID and node instance id to this node
	{
		unsigned char host1[256];
		memset(host1, '\0', 256);
		gethostname((char*)(host1), 256);
		host1[255] = '\0';
		sprintf((char*)(metadata->node_id), "%s_%d", host1, metadata->portNo);
		printf("My node ID: %s\n", metadata->node_id);
		setNodeInstanceId();
	}
	initializeLocks();
	struct NodeInfo me;
	//strcpy((char *)n.hostname, (char *)metadata->hostName);
	for(int i = 0;i < (int)(strlen((char*)(metadata->hostName)));i++)
		me.hostname[i] = metadata->hostName[i];

	me.hostname[strlen((char*)(metadata->hostName))] = '\0';
	me.portNo = metadata->portNo;

	metadata->isBeacon = isBeaconNode(me);
	if(metadata->isBeacon) {

		printf("I am a BEACON node\n");
	} else {

		printf("I am a REGULAR node\n");
	}


	// Call the init function
	while(!shutDownFlag || softRestartEnable) {

		softRestartEnable = 0 ;
		signal(SIGTERM, signalHandler);

		printf("[Main]\tStarting Init .. \n");
		//Initialiing the threads, establishing connections
		init() ;

		//Cleaning up, shutting down
		cleanup() ;

		if(softRestartEnable) {
			resetNodeTimeoutsAndFlags();
		}

		/**
		memset(&tv, 0, sizeof(tv));
		gettimeofday(&tv, NULL);
		unsigned char stopLogging[256];
		memset(stopLogging, '\0', 256);
		sprintf((char *)stopLogging, "// %10ld.%03ld : Logging Stopped\n", tv.tv_sec, (tv.tv_usec/1000));
		writeLogEntry(stopLogging);
		 **/
	}
}



