#include <main.h>
#define HEADER_SIZE 27

using namespace std;


/**
 * Handling incoming connections
 **/

// Closes connection with a neighbour
void closeConnection(int reqSockfd){

	// Set the shurdown flag for this connection

	// Signal the write thread

	// Finally, close the socket
	shutdown(reqSockfd, SHUT_RDWR);
	close(reqSockfd) ;

	printf("This socket has been closed: %d\n", reqSockfd);


	// Todo: Perform check sequence when a neighbour is let go
}


//Pushes the message in the message queue at the very back of the queue
void pushMessageinQ(int reqSockfd, struct Message mes){

	pthread_mutex_lock(&ConnectionMap[reqSockfd].mQLock) ;

	(ConnectionMap[reqSockfd]).MessageQ.push_back(mes) ;

	pthread_mutex_unlock(&ConnectionMap[reqSockfd].mQLock) ;
}


void handleRequestByCase(int reqSockfd, uint8_t msgType, unsigned char *uo_id, uint8_t ttl, unsigned char *buffer, unsigned int dataLen) {

	// Hello message received
	if (msgType == 0xfa){

		printf(" => Hello message received\n") ;
		// Break the Tie 
		struct NodeInfo n ;
		n.portNo = 0 ;
		memcpy(&n.portNo, buffer, 2) ;
		//strcpy(n.hostname, const_cast<char *> ((char *)buffer+2)) ;
		for(unsigned int i=0;i < dataLen-2;i++)
			n.hostname[i] = buffer[i+2];
		n.hostname[dataLen-2] = '\0';

		pthread_mutex_lock(&connectionMapLock) ;
		ConnectionMap[reqSockfd].isReady++;
		ConnectionMap[reqSockfd].n = n;
		pthread_mutex_unlock(&connectionMapLock) ;

		pthread_mutex_lock(&neighbourNodeMapLock) ;
		//if (!neighbourNodeMap[n]){
		if (neighbourNodeMap.find(n)==neighbourNodeMap.end()){
			//printf("Adding %d in neighbor list\n", n.portNo) ;
			neighbourNodeMap[n] = reqSockfd ;
		}
		else{

			// kill one connection
			if (metadata->portNo < n.portNo){
				// dissconect this connection
				closeConnection(reqSockfd) ;
				neighbourNodeMap[n] = neighbourNodeMap[n] ;
				printf("Have to kill one connection\n") ;
			}
			else if(metadata->portNo > n.portNo){
				//				closeConnection(neighbourNodeMap[n]) ;
				neighbourNodeMap[n] = reqSockfd ;
				//printf("Have to break one connection with %d\n", n.portNo) ;
			}
			else{
				// Compare the hostname here
				unsigned char host[256] ;
				gethostname((char *)host, 256) ;
				host[255] = '\0' ;

				if (strcmp((char *)host, (char *)n.hostname) < 0){

					closeConnection(reqSockfd) ;
					neighbourNodeMap[n] = neighbourNodeMap[n] ;
					//printf("Have to break jj one connection\n") ;
				}
				else if(strcmp((char *)host, (char *)n.hostname) > 0){

					neighbourNodeMap[n] = reqSockfd;
					//printf("Have to break one connection with %d\n", n.portNo) ;					
				}
			}
		}
		pthread_mutex_unlock(&neighbourNodeMapLock) ;


	}
	// Join Request received
	else if(msgType == 0xfc){
		//printf("Join request received\n") ;

		// Check if the message has already been received or not
		pthread_mutex_lock(&msgCacheLock) ;
		if (MessageCache.find(string ((const char *)uo_id, SHA_DIGEST_LENGTH)   ) != MessageCache.end()){
			//printf("Message has already been received.\n") ;
			pthread_mutex_unlock(&msgCacheLock) ;
			return ;
		}
		pthread_mutex_unlock(&msgCacheLock) ;

		struct NodeInfo n ;
		n.portNo = 0 ;
		memcpy(&n.portNo, &buffer[4], 2) ;
		//strcpy(n.hostname, const_cast<char *> ((char *)buffer+6)) ;
		for(unsigned int i = 0;i<dataLen-6;i++)
			n.hostname[i] = buffer[i+6];
		n.hostname[dataLen-6] = '\0';

		pthread_mutex_lock(&connectionMapLock) ;
		ConnectionMap[reqSockfd].n = n;
		ConnectionMap[reqSockfd].joinFlag = 1;
		pthread_mutex_unlock(&connectionMapLock) ;

		//printf("JOIN REQUEST for %s:%d received\n", n.hostname, n.portNo) ;

		uint32_t location = 0 ;
		memcpy(&location, buffer, 4) ;

		struct CachePacket cachePacket;
		cachePacket.status = 1 ;
		cachePacket.sender = n ;
		cachePacket.status = 1 ;
		cachePacket.reqSockfd = reqSockfd ;
		cachePacket.msgLifeTime = metadata->msgLifeTime;
		pthread_mutex_lock(&msgCacheLock) ;
		MessageCache[string((const char *)uo_id, SHA_DIGEST_LENGTH) ] = cachePacket ;
		pthread_mutex_unlock(&msgCacheLock) ;

		// Respond the sender with the join response
		struct Message m ;
		m.msgType = 0xfb ;
		m.status = 0;
		m.ttl = 1 ;
		//strncpy((char *)m.uo_id, (const char *)uo_id, SHA_DIGEST_LENGTH) ;
		for(unsigned int i = 0;i<SHA_DIGEST_LENGTH;i++)
			m.uoid[i] = uo_id[i];

		//printf("Location: %d\n", location) ;
		m.location = metadata->location > location ?  metadata->location - location : location - metadata->location ;
		//printf("relative Location: %d\n", m.location) ;
		pushMessageinQ(reqSockfd, m ) ;

		--ttl ;
		// Push the request message in neighbors queue
		if (ttl >= 1 && metadata->ttl > 0){

			pthread_mutex_lock(&neighbourNodeMapLock) ;
			for (map<struct NodeInfo, int>::iterator it = neighbourNodeMap.begin()  ;  it != neighbourNodeMap.end()  ;  ++it)  {
				if( !((*it).second == reqSockfd)){
					//printf("Join req send to: %d\n", (*it).first.portNo) ;

					struct Message msg ;
					msg.msgType = 0xfc ;
					msg.ttl = (unsigned int)(ttl) < (unsigned int)metadata->ttl ? (ttl) : metadata->ttl  ;
					msg.location = location ;
					msg.buffer = (unsigned char *)malloc(dataLen) ;
					msg.dataLen = dataLen ;
					for (int i = 0 ; i < (int)dataLen ; i++)
						msg.buffer[i] = buffer[i] ;
					msg.status = 1 ;
					memset(msg.uoid, 0, SHA_DIGEST_LENGTH) ;
					//					memcpy((unsigned char *)m.uo_id, (const unsigned char *)uo_id, SHA_DIGEST_LENGTH) ;
					//					strncpy((char *)m.uo_id, (const char *)uo_id, SHA_DIGEST_LENGTH) ;
					for (int i = 0 ; i < 20 ; i++)
						msg.uoid[i] = uo_id[i] ;
					pushMessageinQ((*it).second, msg) ;
				}
			}
			pthread_mutex_unlock(&neighbourNodeMapLock);
		}

	} else if (msgType == 0xfb){
		printf(" => JOIN response received\n") ;
		unsigned char original_uo_id[SHA_DIGEST_LENGTH] ;
		//		memcpy((unsigned char *)original_uo_id, buffer, SHA_DIGEST_LENGTH) ;
		for (int i = 0 ; i < SHA_DIGEST_LENGTH ; i++){
			original_uo_id[i] = buffer[i] ;
			//printf("%02x-", original_uo_id[i]) ;
		}
		//printf("\n") ;

		struct NodeInfo n ;
		n.portNo = 0 ;
		memcpy(&n.portNo, &buffer[24], 2) ;
		//strcpy(n.hostname, const_cast<char *> ((char *)buffer+26)) ;
		for(unsigned int i = 0;i < dataLen-26;i++)
			n.hostname[i] = buffer[i+26];
		n.hostname[dataLen-26] = '\0';

		pthread_mutex_lock(&connectionMapLock) ;
		ConnectionMap[reqSockfd].n = n;
		pthread_mutex_unlock(&connectionMapLock) ;

		uint32_t location = 0 ;
		memcpy(&location, &buffer[20], 4) ;

		//Message not found in the cache
		if (MessageCache.find(string((const char *)original_uo_id, SHA_DIGEST_LENGTH)) == MessageCache.end()){
			//			printf("JOIN request was never forwarded from this node.\n") ;
			return ;
		}
		else{
			//message origiated from here
			if (MessageCache[string((const char *)original_uo_id, SHA_DIGEST_LENGTH)].status == 0){
				struct JoinResponseInfo jResp;
				jResp.neighbourPortNo = n.portNo;
				//strncpy(j.hostname, n.hostname, 256) ;
				for(int i=0;i<256;i++)
					jResp.neighbourHostname[i] = n.hostname[i];
				jResp.location = location ;
				joinResponses.insert(jResp)  ;
			}
			else if(MessageCache[ string((const char *)original_uo_id, SHA_DIGEST_LENGTH)   ].status == 1){ //message originated from somewhere else
				//				printf("Sending back the response to %d\n" ,MessageCache[ string((const char *)original_uo_id, SHA_DIGEST_LENGTH)].receivedFrom.portNo) ;
				// Message was forwarded from this node, see the receivedFrom member
				struct Message msg;
				pthread_mutex_lock(&msgCacheLock) ;
				int return_sock = MessageCache[ string((const char *)original_uo_id, SHA_DIGEST_LENGTH)].reqSockfd ;
				pthread_mutex_unlock(&msgCacheLock) ;

				msg.msgType = msgType ;
				msg.buffer = (unsigned char *)malloc((dataLen + 1)) ;
				msg.buffer[dataLen] = '\0' ;
				msg.dataLen = dataLen ;
				for (int i = 0 ; i < (int)dataLen; i++)
					msg.buffer[i] = buffer[i] ;
				for (int i = 0 ; i < 20; i++)
					msg.uoid[i] = uo_id[i] ;
				msg.ttl = 1 ;
				msg.status = 1 ;
				pushMessageinQ(return_sock, msg) ;
			}
		}
	}

/**
	if(msgType == 0xfa) {

		// Hello msg, break the tie here
		printf("... Hello message received\n") ;


	} else if(msgType == 0xfc){

		// Join request

	} else if(msgType == 0xfb){

		// Join response

	} else if(msgType == 0xf6){

		// Check request

	} else if(msgType == 0xf5){

		// Check response

	} else if(msgType == 0xf7){

		// Notify

	}
	**/
}

// while closing a connection, this function is called to erase the value of neighbor from the list maintained
void safelyDeleteSocketEntryFromNeighboursMap(int val)
{
	pthread_mutex_lock(&neighbourNodeMapLock) ;

		for (map<struct node, int>::iterator it = nodeConnectionMap.begin(); it != nodeConnectionMap.end(); ++it){
			if((*it).second == val)
			{
				neighbourNodeMap.erase((*it).first);
				break;
			}
		}

	pthread_mutex_unlock(&neighbourNodeMapLock) ;
}



/**
 *   Read the header, message body and handle request by case.
 **/
void *read_thread(void *args) {

	long reqSockfd = (long)args;

	unsigned char header[HEADER_SIZE];
	unsigned char *buffer ;

	uint32_t dataLen=0;
	unsigned char uo_id[20] ;
	uint8_t messageType=0;
	uint8_t ttl=0;

	while(!(connectionMap[nSocket].shutDown)  ){

		memset(header, 0, HEADER_SIZE) ;
		memset(uoid, 0, 20) ;

		//Check for the JoinTimeOutFlag
		pthread_mutex_lock(&connectionMapLock) ;
			//close connection if either of the condition occurs, jointime out, shutdown
			if(joinTimeOutFlag || ConnectionMap[nSocket].keepAliveTimeOut == -1 || shutDown)
			{

				safelyDeleteSocketEntryFromNeighboursMap(nSocket);

				closeConnection(nSocket);

				// unlock before breaking
				pthread_mutex_unlock(&connectionMapLock) ;
				break;
			}

		pthread_mutex_unlock(&connectionMapLock) ;

		int retCode=(int)read(reqSockfd, header, HEADER_SIZE);

		// if
		if (retCode != HEADER_SIZE){
			printf("Socket Read error...from header\n") ;
			pthread_mutex_lock(&neighbourNodeMapLock) ;

			closeConnection(reqSockfd);
			//pthread_exit(0);
			//break;
			//return 0;
		}

		// populate header
		memcpy(&messageType, header, 1);
		memcpy(uo_id , &header[1], SHA_DIGEST_LENGTH);
		memcpy(&ttl,       header+21, 1);
		memcpy(&dataLen,  header+23, 4);

		// populate buffer
		buffer = (unsigned char *)malloc(sizeof(unsigned char)*(dataLen+1)) ;
		retCode=(int)read(reqSockfd, buffer, dataLen);
		buffer[dataLen] = '\0' ;

		// see the message type and react accordingly
		handleRequestByCase(reqSockfd, messageType, uo_id, ttl, buffer, dataLen);

		free(buffer);
	}

	printf("Read thread exiting..\n") ;
	pthread_exit(0);
	return 0;

}

void *accept_connections(void *) {

	/**
	 *  Listen, bind and other server listener setup code
	 **/
	struct addrinfo hints, *servinfo, *p;
	//int nSocket=0 ; // , portGiven = 0 , portNum = 0;
	unsigned char port_buf[10] ;
	memset(port_buf, '\0', 10) ;

	int ret = 0;
	//maintains the child threads ID, used for joining
	list<pthread_t > acceptChildThreads;
	memset(&hints, 0, sizeof hints) ;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;        // using TCP sock streams
	hints.ai_flags = AI_PASSIVE; // use my IP
	sprintf((char *)port_buf, "%d", metadata->portNo) ;

	// Code until connection establishment has been taken from the Beej's guide
	if ((ret = getaddrinfo(NULL, (char *)port_buf, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
		exit(1) ;
	}

	// loop through all the results and make a socket
	for(p = servinfo; p != NULL; p = p->ai_next) {

		if ((nSocket_accept = socket(p->ai_family , p->ai_socktype , p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}
		int yes = 1 ;
		if (bind(nSocket_accept, p->ai_addr , p->ai_addrlen) == -1) {
			perror("server: bind");
			close(nSocket_accept);
			continue;
		}
		if (setsockopt(nSocket_accept , SOL_SOCKET , SO_REUSEADDR , &yes , sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}
		break;
	}

	// return on failure to bind
	if (p == NULL) {
		fprintf(stderr, "server: failed to bind socket\n");
		//writeLogEntry((unsigned char *)"//server: failed to bind socket\n");
		exit(1) ;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(nSocket_accept, 5) == -1) {
		fprintf(stderr, "server: Error in listen\n");
		//writeLogEntry((unsigned char *)"//Error in listen!!!\n");
		exit(1);
	}


	// Accept handler code begins here
	for(;;) {

		printf("[Server] Waiting for requests..\n");

		int cli_len = 0, acceptSocketfd = 0;
		struct sockaddr_in cli_addr ;

		acceptSocketfd = accept(nSocket_accept,
								(struct sockaddr *) &cli_addr,
								(socklen_t *)&cli_len);

		if(acceptSocketfd > 0) {

			printf("\n I got a connection from (%s , %d)", inet_ntoa(cli_addr.sin_addr),(cli_addr.sin_port));
			// Spawn thread to handle connection and read request		
			int res ;
			pthread_t readRequestThread ;
			res = pthread_create(&readRequestThread, NULL, read_thread , (void *)acceptSocketfd);
			if (res != 0) {
				perror("Thread creation failed");
				//                                writeLogEntry((unsigned char *)"//In Accept: Read Thread creation failed\n");
				exit(EXIT_FAILURE);
			}

			sleep(1);
			// send Hello ??
		}
	}

}

void fillInfoToConnectionMap(   int newreqSockfd,
								int shutDown ,
								unsigned int keepAliveTimer ,
								int keepAliveTimeOut ,
								int isReady ,
								bool joinFlag,
								NodeInfo n) {

	// Populate ConnectionMap for this reqSockfd
	struct ConnectionInfo cInfo ;

	int mret = pthread_mutex_init(&cInfo.mQLock, NULL) ;
	if (mret != 0){
		perror("Mutex initialization failed");
		//writeLogEntry((unsigned char *)"//Mutex initialization failed\n");
	}

	int cret = pthread_cond_init(&cInfo.mQCv, NULL) ;
	if (cret != 0)	{
		perror("CV initialization failed") ;
		//writeLogEntry((unsigned char *)"//CV initialization failed\n");
	}

	cInfo.shutDown = shutDown ;
	cInfo.keepAliveTimer = keepAliveTimer;
	cInfo.keepAliveTimeOut = keepAliveTimeOut;
	cInfo.isReady = isReady;
	cInfo.n = n;

	pthread_mutex_lock(&connectionMapLock) ;
		ConnectionMap[newreqSockfd] = cInfo ;
	pthread_mutex_unlock(&connectionMapLock) ;

}



