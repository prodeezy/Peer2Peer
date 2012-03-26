#include "main.h""
#define HEADER_SIZE 27

using namespace std;


/**
 * Handling incoming connections
 **/



// while closing a connection, this function is called to erase the value of neighbor from the list maintained
void safelyDeleteSocketEntryFromNeighboursMap(int delSocket)
{
	printf("[Reader-%d]\tSafely delete socket from neighbours map\n" , delSocket);

	pthread_mutex_lock(&neighbourNodeMapLock) ;

	for (map<struct NodeInfo, int>::iterator entry = neighbourNodeMap.begin(); entry != neighbourNodeMap.end(); ++entry){

		if((*entry).second == delSocket)
		{
			neighbourNodeMap.erase((*entry).first);
			break;
		}
	}

	pthread_mutex_unlock(&neighbourNodeMapLock) ;
}


// Closes connection with a neighbour
void closeConnection(int reqSockfd){

	printf("[Reader-%d]\t Close connection\n", reqSockfd);

	// Set the shurdown flag for this connection
	pthread_mutex_lock(&connectionMapLock) ;
	(ConnectionMap[reqSockfd]).shutDown = 1 ;
	pthread_mutex_unlock(&connectionMapLock) ;

	// Signal the write thread
	//pthread_mutex_lock(&connectionMap[reqSockfd].mesQLock) ;
	//pthread_cond_signal(&connectionMap[reqSockfd].mesQCv) ;
	//pthread_mutex_unlock(&connectionMap[reqSockfd].mesQLock) ;


	// Finally, close the socket
	// Set the shurdown flag for this connection
	shutdown(reqSockfd, SHUT_RDWR);
	close(reqSockfd) ;

	//printf("[Reader]\tThis socket has been closed: %d\n", reqSockfd);


	// Todo: Perform check sequence when a neighbour is let go
}


void deleteFromNeighboursAndCloseConnection(int reqSockfd)
{

	safelyDeleteSocketEntryFromNeighboursMap(reqSockfd);

	closeConnection(reqSockfd);

}


//Pushes the message in the message queue at the very back of the queue
void safePushMessageinQ(char* sender, int connSocket, struct Message mes){

	pthread_mutex_lock(&ConnectionMap[connSocket].mQLock) ;

	(ConnectionMap[connSocket]).MessageQ.push_back(mes) ;

	pthread_mutex_unlock(&ConnectionMap[connSocket].mQLock) ;

	printf("[%s]\tPushed message into Q for socket:%d , size:%d\n", sender, connSocket, ConnectionMap[connSocket].MessageQ.size());
}


void handleRequestByCase(int reqSockfd, uint8_t msgType, unsigned char *uo_id, uint8_t ttl, unsigned char *buffer, unsigned int dataLen) {

	// Hello message received
	if (msgType == 0xfa){

		//printf("[Reader]\t => Hello message received, now break the Tie\n") ;

		// Break the Tie 
		struct NodeInfo otherNode ;
		otherNode.portNo = 0 ;
		memcpy(&otherNode.portNo, buffer, 2) ;
		//strcpy(n.hostname, const_cast<char *> ((char *)buffer+2)) ;
		for(unsigned int i=0;i < dataLen-2;i++)
			otherNode.hostname[i] = buffer[i+2];
		otherNode.hostname[dataLen-2] = '\0';

		pthread_mutex_lock(&connectionMapLock) ;
		ConnectionMap[reqSockfd].isReady++;
		ConnectionMap[reqSockfd].neighbourNode = otherNode;
		pthread_mutex_unlock(&connectionMapLock) ;

		pthread_mutex_lock(&neighbourNodeMapLock) ;
		//if (!neighbourNodeMap[n]){

		printf("[Reader]\tBreak tie with (%s : %d)\n", otherNode.hostname , otherNode.portNo);
		if (neighbourNodeMap.find(otherNode)==neighbourNodeMap.end()){

			printf("[Reader]\tAdding (%s : %d) in neighbor list\n"
					, otherNode.hostname , otherNode.portNo) ;
			neighbourNodeMap[otherNode] = reqSockfd ;
		}
		else{

			printf("[Reader]\t Kill one (%s : %d) <-OR-> (%s : %d)\n",
					metadata->hostName, metadata->portNo,
					otherNode.hostname , otherNode.portNo);
			// kill one connection
			if (metadata->portNo < otherNode.portNo){
				// dissconect this connection
				closeConnection(reqSockfd) ;
				neighbourNodeMap[otherNode] = neighbourNodeMap[otherNode] ;
				printf("[Reader]\tBreak connection with (%s : %d)\n", otherNode.hostname , otherNode.portNo);
			}
			else if(metadata->portNo > otherNode.portNo){
				//				closeConnection(neighbourNodeMap[n]) ;
				neighbourNodeMap[otherNode] = reqSockfd ;
				printf("[Reader]\tKeep connection with (%s : %d)\n", otherNode.hostname , otherNode.portNo);
			}
			else{
				// Compare the hostname here
				unsigned char host[256] ;
				gethostname((char *)host, 256) ;
				host[255] = '\0' ;

				if (strcmp((char *)host, (char *)otherNode.hostname) < 0){

					closeConnection(reqSockfd) ;
					neighbourNodeMap[otherNode] = neighbourNodeMap[otherNode] ;
					printf("[Reader]\tBreak  connection with (%s : %d)\n", otherNode.hostname , otherNode.portNo);
				}
				else if(strcmp((char *)host, (char *)otherNode.hostname) > 0){

					neighbourNodeMap[otherNode] = reqSockfd;
					printf("[Reader]\tKeep connection with (%s : %d)\n", otherNode.hostname , otherNode.portNo);
				}
			}
		}
		//printf("[Reader]\t Done tie breaking (%s : %d) <-OR-> (%s : %d)\n",
			//	metadata->hostName, metadata->portNo,
				//otherNode.hostname , otherNode.portNo);
		pthread_mutex_unlock(&neighbourNodeMapLock) ;


	}
	// Join Request received
	else if(msgType == 0xfc){
		printf("[Reader]\tJoin request received\n") ;

		// Check if the message has already been received or not
		pthread_mutex_lock(&msgCacheLock) ;
		if (MessageCache.find(string ((const char *)uo_id, SHA_DIGEST_LENGTH)   ) != MessageCache.end()){
			printf("[Reader]\tMessage has already been received. UOID = %s\n" , (const char *)uo_id) ;
			pthread_mutex_unlock(&msgCacheLock) ;
			return ;
		}
		pthread_mutex_unlock(&msgCacheLock) ;

		struct NodeInfo senderNode ;
		senderNode.portNo = 0 ;
		memcpy(&senderNode.portNo, &buffer[4], 2) ;
		//strcpy(n.hostname, const_cast<char *> ((char *)buffer+6)) ;
		for(unsigned int i = 0;i<dataLen-6;i++)
			senderNode.hostname[i] = buffer[i+6];
		senderNode.hostname[dataLen-6] = '\0';

		pthread_mutex_lock(&connectionMapLock) ;
		ConnectionMap[reqSockfd].neighbourNode = senderNode;
		ConnectionMap[reqSockfd].joinFlag = 1;
		pthread_mutex_unlock(&connectionMapLock) ;

		printf("[Reader]\tJOIN REQUEST for %s:%d received\n", senderNode.hostname, senderNode.portNo) ;

		uint32_t location = 0 ;
		memcpy(&location, buffer, 4) ;

		struct CachePacket cachePacket;
		cachePacket.status = 1 ;
		cachePacket.sender = senderNode ;
		cachePacket.status = 1 ;
		cachePacket.reqSockfd = reqSockfd ;
		cachePacket.msgLifetimeInCache = metadata->msgLifeTime;
		pthread_mutex_lock(&msgCacheLock) ;
		MessageCache[string((const char *)uo_id, SHA_DIGEST_LENGTH) ] = cachePacket ;
		pthread_mutex_unlock(&msgCacheLock) ;

		// Respond the sender with the join response
		struct Message m ;
		m.msgType = 0xfb ;
		m.status = 0;
		m.ttl = 1 ;

		for(unsigned int i = 0;i<SHA_DIGEST_LENGTH;i++)
			m.uoid[i] = uo_id[i];

		printf("[Reader]\tLocation: %d\n", location) ;
		m.location = metadata->location > location ?  metadata->location - location : location - metadata->location ;
		printf("[Reader]\trelative Location: %d\n", m.location) ;
		safePushMessageinQ("[Reader]",reqSockfd, m ) ;

		--ttl ;
		// Push the request message in neighbors queue
		if (ttl >= 1 && metadata->ttl > 0){

			pthread_mutex_lock(&neighbourNodeMapLock) ;
			for (map<struct NodeInfo, int>::iterator it = neighbourNodeMap.begin()  ;  it != neighbourNodeMap.end()  ;  ++it)  {
				if( !((*it).second == reqSockfd)){
					printf("[Reader]\tJoin req send to: %d\n", (*it).first.portNo) ;

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

					for (int i = 0 ; i < 20 ; i++)
						msg.uoid[i] = uo_id[i] ;
					safePushMessageinQ("[Reader]",(*it).second, msg) ;
				}
			}
			pthread_mutex_unlock(&neighbourNodeMapLock);
		}

	}else if(msgType == 0xac){

		printf("[Reader-%d]\tStatus request received\n", reqSockfd) ;

		// Check if the message has already been received or not
		pthread_mutex_lock(&msgCacheLock) ;
		if (MessageCache.find(string ((const char *)uo_id, SHA_DIGEST_LENGTH)   ) != MessageCache.end()){

			printf("[Reader-%d]\tMessage has already been received.\n", reqSockfd) ;
			pthread_mutex_unlock(&msgCacheLock) ;
			return ;
		}

		pthread_mutex_unlock(&msgCacheLock) ;

		uint8_t status_type = 0 ;
		memcpy(&status_type, buffer, 1) ;

		struct CachePacket cachePacket;
		cachePacket.status = 1 ;
		cachePacket.reqSockfd = reqSockfd ;
		cachePacket.msgLifetimeInCache = metadata->msgLifeTime;
		pthread_mutex_lock(&msgCacheLock) ;
			MessageCache[string((const char *)uo_id, SHA_DIGEST_LENGTH) ] = cachePacket ;
		pthread_mutex_unlock(&msgCacheLock) ;

		// Respond the sender with the status response
		struct Message statusRespMsg ;
		statusRespMsg.msgType = 0xab ;
		statusRespMsg.status = 0;
		statusRespMsg.status_type = status_type ;
		statusRespMsg.ttl = 1 ;

		for(unsigned int i = 0;i<SHA_DIGEST_LENGTH;i++)
			statusRespMsg.uoid[i] = uo_id[i];

		safePushMessageinQ("Reader", reqSockfd, statusRespMsg ) ;

		--ttl ;
		// Push the request message in neighbors queue
		printf("[Reader-%d]\t Push request in queue ... But (ttl >= 1 && metadata->ttl > 0) = %s\n"
								, reqSockfd, (ttl >= 1 && metadata->ttl > 0)?"Yes":"No" );

		if (ttl >= 1 && metadata->ttl > 0){
			pthread_mutex_lock(&neighbourNodeMapLock) ;
			for (map<struct NodeInfo, int>::iterator it = neighbourNodeMap.begin(); it != neighbourNodeMap.end(); ++it)
			{
				if( !((*it).second == reqSockfd)){
					//					printf("Status req send to: %d\n", (*it).first.portNo) ;

					struct Message floodedStatusMsg ;
					floodedStatusMsg.msgType = 0xac ;
					floodedStatusMsg.ttl = (unsigned int)(ttl) < (unsigned int)metadata->ttl ? (ttl) : metadata->ttl  ;
					floodedStatusMsg.buffer = (unsigned char *)malloc(dataLen) ;
					floodedStatusMsg.dataLen = dataLen ;
					for (int i = 0 ; i < (int)dataLen ; i++)
						floodedStatusMsg.buffer[i] = buffer[i] ;
					floodedStatusMsg.status = 1 ;
					memset(floodedStatusMsg.uoid, 0, SHA_DIGEST_LENGTH) ;
					for (int i = 0 ; i < 20 ; i++)
						floodedStatusMsg.uoid[i] = uo_id[i] ;
					safePushMessageinQ("Reader", (*it).second, floodedStatusMsg) ;
				}
			}
			pthread_mutex_unlock(&neighbourNodeMapLock);
		}

	}
	else if (msgType == 0xab){

			printf("[Reader-%d]\tStatus response received\n", reqSockfd) ;

			unsigned char original_uo_id[SHA_DIGEST_LENGTH] ;
			//		memcpy((unsigned char *)original_uo_id, buffer, SHA_DIGEST_LENGTH) ;
			for (int i = 0 ; i < SHA_DIGEST_LENGTH ; i++)
				original_uo_id[i] = buffer[i] ;



			unsigned short len1 = 0 ;
			memcpy((unsigned short *)&len1, &buffer[20], 2) ;


			struct NodeInfo n ;
			n.portNo = 0 ;
			memcpy((unsigned short int *)&n.portNo, &buffer[22], 2) ;
			for(int hh = 0 ; hh < len1 - 2 ; ++hh)
				n.hostname[hh] = buffer[24 + hh] ;
			//		strncpy(n.hostname, const_cast<char *> ((char *)buffer+24) , len1 - 2   ) ;
			n.hostname[len1-2] = '\0' ;
			//printf("%s\n", n.hostname) ;


			if (MessageCache.find(string((const char *)original_uo_id, SHA_DIGEST_LENGTH)) == MessageCache.end()){

				printf("[Reader-%d]Status request was never forwarded from this node.\n", reqSockfd) ;
				return ;
			}
			else{
				//message origiated from here
				printf("[Reader-%d] Message originated from this node.\n", reqSockfd) ;
				if (MessageCache [string((const char *)original_uo_id, SHA_DIGEST_LENGTH)].status == 0){

/**					// case of satus files
					if (MessageCache [string((const char *)original_uo_id, SHA_DIGEST_LENGTH)].status_type == 0x02){
						pthread_mutex_lock(&statusMsgLock) ;
						if (statusTimerFlag){
							int i = len1 + 22 ;
							if (i == (int)dataLen)
								statusResponseTypeFiles[n]  ;
							while ( i < (int)dataLen){
								unsigned int templen = 0 ;
								memcpy((unsigned int *)&templen, &buffer[i], 4) ;
								if (templen == 0){
									templen = dataLen - i ;
								}
								i += 4 ;
								//						n1.portNo = 0 ;
								//						memcpy((unsigned int *)&n1.portNo, &buffer[i], 2) ;
								//						i += 2 ;
								//						for (int h = 0 ; h < (int)templen - 2 ; ++h)
								//							n1.hostname[h] = buffer[i+h] ;
								//						n1.hostname[templen-2] = '\0' ;
								//						i = i +templen-2;
								//					strncpy(n.hostname, const_cast<char *> ((char *)buffer+i) , templen - 2   ) ;
								//printf("%d  <-----> %d\n", n.portNo, n1.portNo) ;
								//			printf("%s\n", n1.hostname) ;
								string record((char *)&buffer[i], templen) ;
								i += templen ;
								statusResponseTypeFiles[n].push_front(record);

								//	++i ;
							}
						}
						pthread_mutex_unlock(&statusMsgLock) ;
					}
**/
					// Case of status neighbors
					if(MessageCache [string((const char *)original_uo_id, SHA_DIGEST_LENGTH)].status_type == 0x01){

						printf("[Reader-%d]\tOriginated from here.. Case of status neighbours, Read the Status Response? %s\n"
								, reqSockfd, statusTimerFlag?"Yes":"No");

						pthread_mutex_lock(&statusMsgLock) ;
						if (statusTimerFlag){

							int i = len1 + 22 ;
							while ( i < (int)dataLen){

								unsigned int templen = 0 ;
								memcpy((unsigned int *)&templen, &buffer[i], 4) ;
								if (templen == 0){
									templen = dataLen - i ;
								}
								struct NodeInfo n1 ;
								i += 4 ;
								n1.portNo = 0 ;
								memcpy((unsigned int *)&n1.portNo, &buffer[i], 2) ;
								i += 2 ;
								for (int h = 0 ; h < (int)templen - 2 ; ++h)
									n1.hostname[h] = buffer[i+h] ;
								n1.hostname[templen-2] = '\0' ;
								i = i +templen-2;
								//					strncpy(n.hostname, const_cast<char *> ((char *)buffer+i) , templen - 2   ) ;
								//printf("%d  <-----> %d\n", n.portNo, n1.portNo) ;
								//			printf("%s\n", n1.hostname) ;
								set <struct NodeInfo> tempset ;
								tempset.insert(n) ;
								tempset.insert(n1) ;

								printf("[Reader-%d]\t Insert status response into statusReponses\n", reqSockfd);
								statusResponse.insert(tempset) ;

								//	++i ;
							}
						}
						pthread_mutex_unlock(&statusMsgLock) ;
					}

				}
				else if(MessageCache [ string((const char *)original_uo_id, SHA_DIGEST_LENGTH)   ].status == 1){

					printf("[Reader-%d]\tMessage originated from somewhere else, needs to be forwarded\n", reqSockfd);

					// Message was forwarded from this node, see the receivedFrom member
					struct Message m;
					pthread_mutex_lock(&msgCacheLock) ;
					int return_sock = MessageCache [ string((const char *)original_uo_id, SHA_DIGEST_LENGTH)].reqSockfd ;
					pthread_mutex_unlock(&msgCacheLock) ;

					m.msgType = msgType ;
					m.buffer = (unsigned char *)malloc((dataLen + 1)) ;
					m.buffer[dataLen] = '\0' ;
					m.dataLen = dataLen;
					for (int i = 0 ; i < (int)dataLen; i++)
						m.buffer[i] = buffer[i] ;
					for (int i = 0 ; i < SHA_DIGEST_LENGTH ; ++i)
						m.uoid[i] = uo_id[i] ;
					m.ttl = 1 ;
					m.status = 1 ;
					safePushMessageinQ("Reader", return_sock, m) ;
				}
			}
		}

	else if (msgType == 0xfb){

		printf("[Reader]\t => JOIN response received\n") ;
		unsigned char original_uo_id[SHA_DIGEST_LENGTH] ;
		//		memcpy((unsigned char *)original_uo_id, buffer, SHA_DIGEST_LENGTH) ;
		for (int i = 0 ; i < SHA_DIGEST_LENGTH ; i++){
			original_uo_id[i] = buffer[i] ;
			//printf("%02x-", original_uo_id[i]) ;
		}
		//printf("\n") ;

		struct NodeInfo responseNode ;
		responseNode.portNo = 0 ;
		memcpy(&responseNode.portNo, &buffer[24], 2) ;
		//strcpy(n.hostname, const_cast<char *> ((char *)buffer+26)) ;
		for(unsigned int i = 0;i < dataLen-26;i++)
			responseNode.hostname[i] = buffer[i+26];
		responseNode.hostname[dataLen-26] = '\0';

		pthread_mutex_lock(&connectionMapLock) ;
		ConnectionMap[reqSockfd].neighbourNode = responseNode;
		pthread_mutex_unlock(&connectionMapLock) ;

		uint32_t location = 0 ;
		memcpy(&location, &buffer[20], 4) ;

		//Message not found in the cache
		if (MessageCache.find(string((const char *)original_uo_id, SHA_DIGEST_LENGTH)) == MessageCache.end()){

			printf("[Reader]\tJOIN request was never forwarded from this node.\n") ;
			return ;
		}
		else{

			printf("[Reader]\t Got response to request forwarded through this Node. cache msg status:%d\n",
					MessageCache[string((const char *)original_uo_id, SHA_DIGEST_LENGTH)].status) ;

			//message origiated from here
			if (MessageCache[string((const char *)original_uo_id, SHA_DIGEST_LENGTH)].status == 0){

				printf("[Reader-%d]\t +++++++++++++ JOIN request originated from node (%s : %d). Add to joinResponses list \n"
						, reqSockfd, responseNode.hostname, responseNode.portNo) ;


				struct JoinResponseInfo jResp;
				jResp.neighbourPortNo = responseNode.portNo;
				for(int i=0;i<256;i++)
					jResp.neighbourHostname[i] = responseNode.hostname[i];
				jResp.location = location ;


				//pair<set<struct JoinResponseInfo>::iterator,bool> ret = joinResponses.insert(jResp)  ;
				joinResponses.push_back(jResp)  ;

				printf("[Reader-%d]\t joinResponses size : %d\n", reqSockfd, joinResponses.size());
				//printf("[Reader-%d]\t joinResponses size : %d, Inserted? %s \n"
				//					, reqSockfd, joinResponses.size()
				//				, (ret.second) ? "Yes" : "False");

			}
			//message originated from somewhere else
			else if(MessageCache[ string((const char *)original_uo_id, SHA_DIGEST_LENGTH)   ].status == 1){


				printf("[Reader]\tSending back the response to %d\n"
						,MessageCache[ string((const char *)original_uo_id, SHA_DIGEST_LENGTH)].sender.portNo) ;
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
				safePushMessageinQ("[Reader]", return_sock, msg) ;
			}
		}
	} else if(msgType == 0xf8){

		//printf("[Reader-%d]\tKeep Alive Message Received from : %d\n", reqSockfd, reqSockfd);

	} // Notify message received
	else if(msgType == 0xf7) {

		printf("[Reader-%d]\tRecieved Notify message \n", reqSockfd);

		deleteFromNeighboursAndCloseConnection(reqSockfd);
	}

	//KeepAlive message recieved
	//Resst the keepAliveTimer for this connection
	pthread_mutex_lock(&connectionMapLock) ;
	if(ConnectionMap.find(reqSockfd)!=ConnectionMap.end())
		ConnectionMap[reqSockfd].keepAliveTimeOut = metadata->keepAliveTimeOut;
	pthread_mutex_unlock(&connectionMapLock) ;
	//	if(connectionMap[reqSockfd].keepAliveTimer!=0)

}


//This fnction pushes the NOTIFY message at the very begining of the message queue, so as to send it at priority
void notifyMessageSend(int resSock, uint8_t errorCode)
{
	printf("[Notify] Send ------->  %d\n", resSock);
	struct Message notifyMsg ;
	memset(&notifyMsg, 0, sizeof(notifyMsg));

	notifyMsg.msgType = 0xf7 ;
	notifyMsg.status = 0 ;
	notifyMsg.errorCode = errorCode ;

	//pushing the message and signaling the write thread
	safePushMessageinQ("[Notify]\t Push Notify in Q\n", resSock, notifyMsg);

}



/**
 *   Read the header, message body and handle request by case.
 **/
void *connectionReaderThread(void *args) {

	long reqSockfd = (long)args;

	if(ConnectionMap.find(reqSockfd) == ConnectionMap.end()) {

		printf("[Reader-%d] Socket not found in ConnectionMap, something went WRONG!\n");
		pthread_exit(0);
		return 0;
	}

	printf("[Reader-%d]\t__________ Started reader for (%s : %d)\n"
			, reqSockfd, ConnectionMap[reqSockfd].neighbourNode.hostname
			, ConnectionMap[reqSockfd].neighbourNode.portNo);

	unsigned char header[HEADER_SIZE];
	unsigned char *buffer ;

	uint32_t dataLen=0;
	unsigned char uo_id[20] ;
	uint8_t messageType=0;
	uint8_t ttl=0;

	NodeInfo partnerNode;
	while(!(ConnectionMap[reqSockfd].shutDown)  ){

		//printf("[Reader-%d]\t ...\n", reqSockfd);

		partnerNode = ConnectionMap[reqSockfd].neighbourNode;

		memset(header, 0, HEADER_SIZE) ;
		memset(uo_id, 0, 20) ;

		//Check for the JoinTimeOutFlag
		// --->
		pthread_mutex_lock(&connectionMapLock) ;
		//close connection if either of the condition occurs, jointime out, shutdown
		if( ConnectionMap[reqSockfd].keepAliveTimeOut == -1 || shutDownFlag ) //|| joinTimeOutFlag
		{
			// <---
			pthread_mutex_unlock(&connectionMapLock) ;

			deleteFromNeighboursAndCloseConnection(reqSockfd);

			printf("[Reader-%d]\t breaking out of Reader loop\n", reqSockfd);
			break;
		}
		// <---
		pthread_mutex_unlock(&connectionMapLock) ;

		int retCode=(int)read(reqSockfd, header, HEADER_SIZE);

		// --->
		pthread_mutex_lock(&connectionMapLock) ;
		//close connection if either of the condition occurs, jointime out, shutdown
		if( ConnectionMap[reqSockfd].keepAliveTimeOut == -1 || shutDownFlag ) //|| joinTimeOutFlag
		{
			// <---
			pthread_mutex_unlock(&connectionMapLock) ;

			deleteFromNeighboursAndCloseConnection(reqSockfd);

			printf("[Reader-%d]\t breaking out of Reader loop\n", reqSockfd);
			break;
		}
		// <---
		pthread_mutex_unlock(&connectionMapLock) ;


		if (retCode != HEADER_SIZE){

			printf("[Reader-%d]\tSocket Read error...from header (%s : %d)\n"
					, reqSockfd
					, partnerNode.hostname
					, partnerNode.portNo) ;

			deleteFromNeighboursAndCloseConnection(reqSockfd);
			//pthread_exit(0);
			printf("[Reader-%d] Breaking out of reader loop\n" , reqSockfd);
			break;
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

		if (retCode != (int)dataLen){

			printf("[Reader-%d]Socket Read error...from data\n", reqSockfd) ;
			deleteFromNeighboursAndCloseConnection(reqSockfd);
			break;
		}

		buffer[dataLen] = '\0' ;

		// see the message type and react accordingly
		handleRequestByCase(reqSockfd, messageType, uo_id, ttl, buffer, dataLen);

		//logging the infomation, at the recievers end
		if(!(joinNetworkPhaseFlag && messageType == 0xfa))
		{
			pthread_mutex_lock(&logEntryLock) ;
			// Writing data to log file, but first creating log entry
			//printf("[Reader]\t\tcreating LOG ENTRY from sock:%d , header:%s , buf:%s\n"
			//		, reqSockfd, header, buffer);
			unsigned char *logEntry = createLogRecord('r', reqSockfd, header, buffer);
			if(logEntry!=NULL) {
				//printf("[Reader]\t\twriting LOG ENTRY\n");
				writeLogEntry(logEntry);
			}
			pthread_mutex_unlock(&logEntryLock) ;
		}


		free(buffer);

		//printf("[Reader-%d]\t....... End of Reader loop\n", reqSockfd);
	}

	printf("[Reader-%d]\t Exited\n", reqSockfd) ;
	pthread_exit(0);
	return 0;

}

/**
 * Node server thread that listens for connections
 */
void *nodeServerListenerThread(void *) {


	/**
	 *  Listen, bind and other server listener setup code
	 **/

	printf("[Server]\tListener thread started....\n");

	struct addrinfo hints, *servinfo, *p;
	//int nSocket=0 ; // , portGiven = 0 , portNum = 0;
	unsigned char port_buf[10] ;
	memset(port_buf, '\0', 10) ;

	int ret = 0;
	//maintains the child threads ID, used for joining
	list<pthread_t > serverListenerChildThreads;

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
		if ((acceptServerSocket = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}
		int yes = 1 ;
		if (setsockopt(acceptServerSocket, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		printf("[Server]\tBIND to address\n");

		if (bind(acceptServerSocket, p->ai_addr, p->ai_addrlen) == -1) {
			perror("server: bind");
			close(acceptServerSocket);
			continue;
		}
		break;
	}


	// return on failure to bind
	if (p == NULL) {
		fprintf(stderr, "server: failed to bind socket\n");
		close(acceptServerSocket);
		writeLogEntry((unsigned char *)"//server: failed to bind socket\n");
		exit(1) ;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(acceptServerSocket, 5) == -1) {
		fprintf(stderr, "server: Error in listen\n");
		close(acceptServerSocket);
		writeLogEntry((unsigned char *)"//Error in listen!!!\n");
		exit(1);
	}


	// Accept handler code begins here
	for(;;) {

		//printf("[Server]\t Server waiting for requests..\n");

		int cli_len = 0, acceptSocketfd = 0;
		struct sockaddr_in cli_addr ;


		if(shutDownFlag)
		{
			printf("[Server]\tShutdown detected, break!\n");
			//shutdown(acceptServerSocket, SHUT_RDWR);
			break;
			//close(acceptServerSocket);
		}

		acceptSocketfd = accept(acceptServerSocket,
				(struct sockaddr *) &cli_addr,
				(socklen_t *)&cli_len);

		// Server received a connection from someone
		if(acceptSocketfd > 0) {



			printf("[Server]\tI got a connection from (%s , %d)\n", inet_ntoa(cli_addr.sin_addr),(cli_addr.sin_port));



			// Create connection info object
			struct ConnectionInfo connInfo ;
			int mres = pthread_mutex_init(&connInfo.mQLock, NULL) ;
			if (mres != 0){
				perror("[Server]\tMutex initialization failed");
				writeLogEntry((unsigned char *)"//Mutex initialization failed\n");
			}
			/*int cres = pthread_cond_init(&cn.mQCv, NULL) ;
			if (cres != 0){
				perror("CV initialization failed") ;
				writeLogEntry((unsigned char *)"//CV initialization failed\n");
			}*/
			connInfo.shutDown = 0 ;
			connInfo.keepAliveTimer = metadata->keepAliveTimeOut/2;
			connInfo.keepAliveTimeOut = metadata->keepAliveTimeOut;
			connInfo.isReady = 0;
			printf("[Server]\tAdd to ConnectionMap[%d] = (%s : %d)\n", acceptSocketfd, connInfo.neighbourNode.hostname);
			pthread_mutex_lock(&connectionMapLock) ;
			ConnectionMap[acceptSocketfd] = connInfo ;
			pthread_mutex_unlock(&connectionMapLock) ;




			// Spawn Reader thread
			int res ;
			pthread_t readRequestThread ;
			res = pthread_create(&readRequestThread, NULL, connectionReaderThread , (void *)acceptSocketfd);
			if (res != 0) {
				perror("Thread creation failed");
				writeLogEntry((unsigned char *)"//In Accept: Read Thread creation failed\n");
				exit(EXIT_FAILURE);
			}
			ConnectionMap[acceptSocketfd].myReadId = readRequestThread;
			serverListenerChildThreads.push_front(readRequestThread);





			// send Hello ??
			struct Message m;
			m.msgType = 0xfa ;
			m.status = 0;
			//m.fromConnect = 0;
			printf("[Server]\tSending HELLO message\n");
			safePushMessageinQ("[Server]",acceptSocketfd, m) ;




			// Spawn Writer thread
			pthread_t writerThread ;
			res = pthread_create(&writerThread, NULL, connectionWriterThread , (void *)acceptSocketfd);
			if (res != 0) {
				printf("[Server]\tIn Accept: Write Thread creation failed\n");
				writeLogEntry((unsigned char *)"//In Accept: Write Thread creation failed\n");
				exit(EXIT_FAILURE);
			}

			//		close(newreqSockfd) ;
			ConnectionMap[acceptSocketfd].myWriteId = writerThread;
			serverListenerChildThreads.push_front(writerThread);

			//printf("[Server]\t Wait a sec\n");
			sleep(1);
		}
		sleep(1);
	}

	printf("[Server]\tWait for server child threads to finish, size : %d\n", serverListenerChildThreads.size());
	void *thread_result ;
	//Accept thread waits for child thread to exit, joins here
	for (list<pthread_t >::iterator it = serverListenerChildThreads.begin(); it != serverListenerChildThreads.end(); ++it){

		//printf("Value is : %d\n", (pthread_t)(*it).second.myReadId);
		int res = pthread_join((*it), &thread_result);
		if (res != 0) {
			printf("[Server]\tIn Accept: Thread Join failed\n");
			writeLogEntry((unsigned char *)"//In Accept: Thread Join failed\n");
			exit(EXIT_FAILURE);
		}
	}
	printf("[Server]\t Server terminated normally\n") ;
	serverListenerChildThreads.clear();

	printf("[Server]\tExited!\n");
	pthread_exit(0);
	return 0;
}

void fillInfoToConnectionMap(   int newreqSockfd,
		int shutDown ,
		unsigned int keepAliveTimer ,
		int keepAliveTimeOut ,
		int isReady ,
		bool joinFlag,
		NodeInfo n) {

	printf("[Server]\t Fill info to connection map :%d\n", newreqSockfd);
	// Populate ConnectionMap for this reqSockfd
	struct ConnectionInfo cInfo ;

	int mret = pthread_mutex_init(&cInfo.mQLock, NULL) ;
	if (mret != 0){
		perror("Mutex initialization failed");
		writeLogEntry((unsigned char *)"//Mutex initialization failed\n");
	}

	/**	int cret = pthread_cond_init(&cInfo.mQCv, NULL) ;
	if (cret != 0)	{
		perror("CV initialization failed") ;
		writeLogEntry((unsigned char *)"//CV initialization failed\n");
	}
	 **/

	cInfo.shutDown = shutDown ;
	cInfo.keepAliveTimer = keepAliveTimer;
	cInfo.keepAliveTimeOut = keepAliveTimeOut;
	cInfo.isReady = isReady;
	cInfo.neighbourNode = n;

	pthread_mutex_lock(&connectionMapLock) ;
	ConnectionMap[newreqSockfd] = cInfo ;
	pthread_mutex_unlock(&connectionMapLock) ;

}



