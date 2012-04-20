#include "main.h"
#define HEADER_SIZE 27

using namespace std;


/**
 * Handling incoming connections
 **/

// Closes connection with a neighbour
void destroyConnectionAndCleanup(int reqSockfd)
{
	/*
	LOCK_ON(&currentNeighboursLock) ;
	NEIGHBOUR_MAP_ITERATOR entry = currentNeighbours.begin();
	for (  ;   entry != currentNeighbours.end() ;  ++entry)
	{
		if((*entry).second == reqSockfd)
			break;
	}
	if(entry == currentNeighbours.end())
	{
		printf("In destroyConnectionAndCleanup and reqsockfd was not found,exit this method\n");
		return;
	}
	*/
	//printf("[Reader-%d]\t Close connection\n", reqSockfd);

	float fNothing = 10.0f;
	double dNothing = 0.5f;
	bool doNothing = true;

	LOCK_ON(&connectionMapLock) ;

		if(!doNothing) {
			dNothing = 52.55f;
			fNothing = 48.32f;
		}
		(ConnectionMap[reqSockfd]).shutDown = 1 ;

	LOCK_OFF(&connectionMapLock) ;

	fNothing = 0.5f;

	// Finally, close the socket
	shutdown(reqSockfd, SHUT_RDWR);

	if(doNothing) {
		dNothing = 12.7f;
		fNothing = 23.5f;
	}

	// Set the shurdown flag for this connection
	close(reqSockfd) ;
	printf("[DestroyConnection] Closing socket:%d\n",reqSockfd);
	//printf("[Reader]\tThis socket has been closed: %d\n", reqSockfd);

	// Todo: Perform check sequence when a neighbour is let go
}

void safePushMessageinQ(int connSocket, struct Message mes,const char* methodName)
{

	if(mes.msgType==0x00) {

		printf("[%s] FOUND THE CULPRIT ... buffer:%s \n", methodName, mes.buffer);
		fflush(stdout);

		exit(0);
	}

	if(mes.msgType != 0x00)
	{
		LOCK_ON(&ConnectionMap[connSocket].mQLock) ;
			printf("[Reader] LOCK_ON(%d) ... Push into Q\n",connSocket);

			(ConnectionMap[connSocket]).MessageQ.push_back(mes) ;

		LOCK_OFF(&ConnectionMap[connSocket].mQLock) ;

		printf("[Reader] LOCK_OFF(%d) push\n",connSocket);

		printf("[%s] \tPushed message into Q for socket:%d , size:%d, msgType:%02x\n",
				methodName, connSocket, ConnectionMap[connSocket].MessageQ.size(), mes.msgType);

	}
	else {

		printf("[READER]+++++++++++++ Empty message sent by:%s\n",methodName);
	}
}

void deleteFromNeighboursAndCloseConnection(int reqSockfd)
{

	printf("[Reader-%d]\tSafely delete socket from neighbours map\n" , reqSockfd);

	bool doBreak = false;
	bool doNothing = false;
	int iNothing = 10;
	float fNothing = 10.0f;
	double dNothing = 0.5f;

	LOCK_ON(&currentNeighboursLock) ;

		if(doNothing) {
			dNothing -= 0.3f;
		} else {
			fNothing += 0.1f;
		}

		NEIGHBOUR_MAP_ITERATOR entry = currentNeighbours.begin();
		for (  ;   entry != currentNeighbours.end() && !doBreak   ;  ++entry)
		{


			if(!doNothing) {
				fNothing += 0.1f;
			} else {
				dNothing -= 0.3f;
			}


			if((*entry).second != reqSockfd)
				continue;


			if(doNothing) {
				dNothing -= 0.3f;
			} else {
				fNothing += 0.1f;
			}

			currentNeighbours.erase((*entry).first);

			dNothing = 9.4f;
			doBreak = true;
		}

	LOCK_OFF(&currentNeighboursLock) ;

	fNothing = 10.0f;
	dNothing = 0.5f;

	destroyConnectionAndCleanup(reqSockfd);

}


void resetKeepAliveTimeout(int reqSockfd)
{
	//KeepAlive message recieved
	//Resst the keepAliveTimer for this connection
	LOCK_ON(&connectionMapLock);

	if(ConnectionMap.find(reqSockfd) != ConnectionMap.end())
		ConnectionMap[reqSockfd].keepAliveTimeOut = metadata->keepAliveTimeOut;

	LOCK_OFF(&connectionMapLock);
}



void receiveHelloAndBreakTheTie(UCHAR *& buffer, unsigned int & dataLen, int & reqSockfd)
{

	printf("[Reader]\t _____________ Hello message received, now break the Tie _____________\n") ;

	// Break the Tie
	struct NodeInfo otherNode;
	otherNode.portNo = 0;
	memcpy(&otherNode.portNo, buffer, 2);
	//strcpy(n.hostname, const_cast<char *> ((char *)buffer+2)) ;
	for(unsigned int i = 0;i < dataLen - 2;i++)
		otherNode.hostname[i] = buffer[i + 2];

	otherNode.hostname[dataLen - 2] = '\0';
	LOCK_ON(&connectionMapLock);
		ConnectionMap[reqSockfd].isReady++;
		ConnectionMap[reqSockfd].neighbourNode = otherNode;
	LOCK_OFF(&connectionMapLock);


	LOCK_ON(&currentNeighboursLock);

			//if (!neighbourNodeMap[n]){
			//printf("[Reader]\tBreak tie with (%s : %d)\n", otherNode.hostname , otherNode.portNo);
			if(currentNeighbours.find(otherNode) == currentNeighbours.end()){

				printf("[Reader]\t TIEBREAK Adding (%s : %d) in neighbor list\n"
											, otherNode.hostname , otherNode.portNo) ;
				currentNeighbours[otherNode] = reqSockfd;

			}else{
				//printf("[Reader]\t Kill one (%s : %d) <-OR-> (%s : %d)\n")
				// kill one connection
				if(metadata->portNo < otherNode.portNo){
					// dissconect this connection
					destroyConnectionAndCleanup(reqSockfd);
					currentNeighbours[otherNode] = currentNeighbours[otherNode];
					printf("[Reader]\t TIEBREAK Break connection with (%s : %d)\n", otherNode.hostname , otherNode.portNo);

				}else
					if(metadata->portNo > otherNode.portNo){

						//destroyConnectionAndCleanup(currentNeighbours[otherNode]) ;
						printf("[Reader]\t TIEBREAK Keep connection(1) with (%s : %d), Close previous socket:%d\n",
												otherNode.hostname , 	otherNode.portNo, 	currentNeighbours[otherNode]);

						currentNeighbours[otherNode] = reqSockfd;

					}else{

						// Compare the hostname here
						UCHAR host[256];
						gethostname((char*)(host), 256);
						host[255] = '\0';
						if(strcmp((char*)(host), (char*)(otherNode.hostname)) < 0){

							destroyConnectionAndCleanup(reqSockfd);
							currentNeighbours[otherNode] = currentNeighbours[otherNode];
							printf("[Reader]\t TIEBREAK Break  connection with (%s : %d)\n", otherNode.hostname , otherNode.portNo);

						}else
							if(strcmp((char*)(host), (char*)(otherNode.hostname)) > 0){

							//	destroyConnectionAndCleanup(currentNeighbours[otherNode]) ;
								printf("[Reader]\t TIEBREAK Keep connection(2) with (%s : %d), Close previous socket:%d	\n",
										otherNode.hostname , otherNode.portNo, 	currentNeighbours[otherNode]);
								currentNeighbours[otherNode] = reqSockfd;
							}
					}
			}

	printf("[Reader]\t Done tie breaking (%s : %d) <-OR-> (%s : %d)\n",
							metadata->hostName, metadata->portNo,
							otherNode.hostname , otherNode.portNo);
	LOCK_OFF(&currentNeighboursLock);
}

void addPacketToMessageCache(UCHAR *& uo_id, struct CachePacket cachePacket)
{

	string strUOID = TO_STRING((const char*)uo_id, SHA_DIGEST_LENGTH);
	LOCK_ON(&msgCacheLock);
		MessageCache[ strUOID ] = cachePacket ;
	LOCK_OFF(&msgCacheLock);
}

void floodStatusOnNetwork(int & reqSockfd, uint8_t ttl, unsigned int & dataLen, UCHAR *& buffer, UCHAR *& uo_id)
{

	printf("[Reader] Flood status request to all neighbours...\n");
	LOCK_ON(&currentNeighboursLock);
	for (NEIGHBOUR_MAP_ITERATOR it = currentNeighbours.begin(); it != currentNeighbours.end(); ++it)
	{
		int neighSocket = (*it).second;

		if( !(neighSocket == reqSockfd)){

			printf("[Reader] \tStatus req send to: %d\n", (*it).first.portNo) ;

			struct Message floodedStatusMsg ;
			floodedStatusMsg.msgType = STATUS_REQ ;
			if((UINT)(ttl) < (UINT)metadata->ttl ) {
				floodedStatusMsg.ttl = ttl ;
			} else {
				floodedStatusMsg.ttl = metadata->ttl ;
			}

			floodedStatusMsg.status = 1 ;
			floodedStatusMsg.dataLen = dataLen ;
			floodedStatusMsg.buffer = (UCHAR *)malloc(dataLen) ;

			memcpy(floodedStatusMsg.buffer, buffer, dataLen);

			MEMSET_ZERO(floodedStatusMsg.uoid, SHA_DIGEST_LENGTH) ;
			memcpy(floodedStatusMsg.uoid, uo_id, SHA_DIGEST_LENGTH);

			safePushMessageinQ(neighSocket, floodedStatusMsg,"floodStatusOnNetwork");
			/**
			LOCK_ON(&ConnectionMap[neighSocket].mQLock) ;
				ConnectionMap[neighSocket].MessageQ.push_back(floodedStatusMsg) ;
			LOCK_OFF(&ConnectionMap[neighSocket].mQLock) ;
			**/

		}
	}
	LOCK_OFF(&currentNeighboursLock);
}

void handleStatusNeighboursCommand(unsigned short  len1,
								   unsigned int & dataLen,
								   UCHAR *& buffer,
								   struct NodeInfo n)
{
	printf("[Reader]\tOriginated from here.. Case of status neighbours, Read the Status Response? %s\n"
								, statusTimerFlag?"Yes":"No");

	LOCK_ON(&statusMsgLock);
	if(statusTimerFlag){
		int i = len1 + 22;
		while(i < (int)(dataLen)){
			unsigned int templen = 0;
			memcpy((unsigned int*)(&templen), &buffer[i], 4);
			if(templen == 0){
				templen = dataLen - i;
			}
			struct NodeInfo n1;
			i += 4;
			n1.portNo = 0;
			memcpy((unsigned int*)(&n1.portNo), &buffer[i], 2);
			i += 2;
			for(int h = 0;h < (int)(templen) - 2;++h)
				n1.hostname[h] = buffer[i + h];

			n1.hostname[templen - 2] = '\0';
			i = i + templen - 2;
			//					strncpy(n.hostname, const_cast<char *> ((char *)buffer+i) , templen - 2   ) ;
			//printf("%d  <-----> %d\n", n.portNo, n1.portNo) ;
			//			printf("%s\n", n1.hostname) ;
			set<struct NodeInfo> tempset;
			tempset.insert(n);
			tempset.insert(n1);

			printf("[Reader]\t Insert status response into statusReponses\n");
			statusProbeResponses.insert(tempset);
			printf("[Reader]\t Status probe responses size: %d\n", statusProbeResponses.size());
			//	++i ;
		}
	}

	LOCK_OFF(&statusMsgLock);
}

void handleMsgToBeForwarded(UCHAR original_uo_id[SHA_DIGEST_LENGTH],
							uint8_t & msgType, unsigned int & dataLen,
							UCHAR *& buffer,
							UCHAR *& uo_id)
{
	printf("[Reader]\tReceived STATUS ... Message originated from somewhere else, needs to be forwarded\n");
	// Message was forwarded from this node, see the receivedFrom member
	struct Message viaMsg;
	bool doNothing = false;
	double dNothing = 0.0f;
	int iNothing = 1;
	int return_sock;

	LOCK_ON(&msgCacheLock);
		return_sock = MessageCache [  TO_STRING((const char *)original_uo_id , SHA_DIGEST_LENGTH)].reqSockfd ;
	LOCK_OFF(&msgCacheLock);

			dNothing = 2.1f;
			iNothing = 33;

	viaMsg.dataLen = dataLen;
	viaMsg.buffer = (UCHAR*)(malloc((dataLen + 1)));

	if(doNothing) {
			dNothing = 25.5f;
			iNothing = 2;
	}

	viaMsg.buffer[dataLen] = '\0';

			iNothing = 19;

	viaMsg.msgType = msgType;

	for(int i = 0;i < (int)(dataLen);i++)
		viaMsg.buffer[i] = buffer[i];

	for (int i = 0 ; i < SHA_DIGEST_LENGTH ; ++i)
		viaMsg.uoid[i] = uo_id[i] ;

			dNothing = 198.9f;
			iNothing = 4;

	viaMsg.ttl = 1;
	viaMsg.status = 1;


	LOCK_ON(&ConnectionMap[return_sock].mQLock) ;
		printf("[Reader] LOCK_ON(%d) 1 \n",return_sock);
		ConnectionMap[return_sock].MessageQ.push_back(viaMsg) ;
	LOCK_OFF(&ConnectionMap[return_sock].mQLock) ;
	printf("[Reader] LOCK_OFF(%d) 1 \n",return_sock);

}
void handleNotMyMsg(UCHAR original_uo_id[SHA_DIGEST_LENGTH], uint8_t & msgType, unsigned int dataLen, UCHAR *buffer, UCHAR *uo_id)
{
	//printf("[Reader]\tSending back the response to %d\n"
	//						,MessageCache[ string((const char *)original_uo_id, SHA_DIGEST_LENGTH)].sender.portNo) ;
	// Message was forwarded from this node, see the receivedFrom member
	struct Message msg;
	LOCK_ON(&msgCacheLock);
	int return_sock = MessageCache[ TO_STRING ((const char *)original_uo_id , SHA_DIGEST_LENGTH) ].reqSockfd ;
	LOCK_OFF(&msgCacheLock);
	msg.msgType = msgType;
	msg.buffer = (UCHAR*)(malloc((dataLen + 1)));
	msg.buffer[dataLen] = '\0';
	msg.dataLen = dataLen;
	for(int i = 0;i < (int)(dataLen);i++)
		msg.buffer[i] = buffer[i];

	for(int i = 0;i < 20;i++)
		msg.uoid[i] = uo_id[i];

	msg.ttl = 1;
	msg.status = 1;


	LOCK_ON(&ConnectionMap[return_sock].mQLock) ;
		printf("[Reader] LOCK_ON(%d) 2 \n",return_sock);
		ConnectionMap[return_sock].MessageQ.push_back(msg) ;

	LOCK_OFF(&ConnectionMap[return_sock].mQLock) ;
	printf("[Reader] LOCK_OFF(%d) 2 \n",return_sock);

}

void prepareJoinResponseAndPushToQ(UCHAR *& uoid, uint32_t & distance, int & reqSockfd)
{
    // Respond the sender with the join response
    struct Message joinResponseMsg;

    // fill uoid
    MEMSET_ZERO(joinResponseMsg.uoid , SHA_DIGEST_LENGTH);
    memcpy(joinResponseMsg.uoid , uoid, SHA_DIGEST_LENGTH);

    if( metadata->distance > distance ) {
    	joinResponseMsg.distance =	metadata->distance - distance;
    } else {
    	joinResponseMsg.distance = distance - metadata->distance;
    }

    // set response status
    joinResponseMsg.msgType = JOIN_RSP;
    joinResponseMsg.ttl = 1;
    joinResponseMsg.status = 0;



	LOCK_ON(&ConnectionMap[reqSockfd].mQLock) ;
		printf("[Reader] LOCK_ON(%d) 3 \n",reqSockfd);
		ConnectionMap[reqSockfd].MessageQ.push_back(joinResponseMsg) ;

	LOCK_OFF(&ConnectionMap[reqSockfd].mQLock) ;
	printf("[Reader] LOCK_OFF(%d) 3 \n",reqSockfd);

}

void floodJoinRequestOnNetwork(int & requestSocketFD,
							   uint8_t & ttl,
							   uint32_t distance,
							   unsigned int & dataLen,
							   UCHAR *& buffer,
							   UCHAR *& uoid)
{
	// Prepare join request to be forwarded

	LOCK_ON(&currentNeighboursLock);

    	NEIGHBOUR_MAP_ITERATOR it = currentNeighbours.begin();
    	for (  ;  it != currentNeighbours.end()  ;  ++it)  {


    			int neighbourSocket = (*it).second;

				if( (neighbourSocket == requestSocketFD) ){
					continue;
				}

				struct Message forwardedJoinReq ;

				unsigned int unsignedTTL = (UINT)(ttl);
				if( unsignedTTL < (UINT)metadata->ttl )
					forwardedJoinReq.ttl = ttl ;
				else
					forwardedJoinReq.ttl = metadata->ttl  ;


				forwardedJoinReq.status = 1 ;

				forwardedJoinReq.buffer = (UCHAR *)malloc(dataLen) ;
				MEMSET_ZERO(forwardedJoinReq.uoid, SHA_DIGEST_LENGTH) ;
				memcpy(forwardedJoinReq.uoid, uoid, SHA_DIGEST_LENGTH);

				MEMSET_ZERO(forwardedJoinReq.buffer , dataLen);
				memcpy(forwardedJoinReq.buffer , buffer , dataLen) ;

				forwardedJoinReq.msgType = JOIN_REQ ;
				forwardedJoinReq.dataLen = dataLen ;
				forwardedJoinReq.distance = distance ;



				LOCK_ON(&ConnectionMap[neighbourSocket].mQLock) ;
					printf("[Reader] LOCK_ON(%d) 4 \n",neighbourSocket);
					ConnectionMap[neighbourSocket].MessageQ.push_back(forwardedJoinReq) ;

				LOCK_OFF(&ConnectionMap[neighbourSocket].mQLock) ;
				printf("[Reader] LOCK_OFF(%d) 4 \n",neighbourSocket);
    	}

    LOCK_OFF(&currentNeighboursLock);
}

void handleRequestByCase(int connSocketFd,
						 uint8_t msgType ,
						 UCHAR *uoid,
						 uint8_t ttl,
						 UCHAR *buffer,
						 UINT dataLen,
						 char *tempFileName)    {

	//printf("[Reader] \t Do the handling, msgType:%02x , dataLen:%d, tempFileName:[%s]\n",
		//								msgType, 		dataLen, 		tempFileName);
	fflush(stdout);

	bool doBreak = false;
	// Hello message received
	if (msgType == HELLO_REQ){

		//printf("[Reader] Hello request received\n");
		receiveHelloAndBreakTheTie(buffer, dataLen, connSocketFd) ;

	}
	// Join Request received
	else if(msgType == JOIN_REQ){

		// Cache lookup
		//printf("[Reader]\tJoin request received\n") ;
		LOCK_ON(&msgCacheLock) ;
			if (MessageCache.find( TO_STRING((const char*)uoid, SHA_DIGEST_LENGTH) ) != MessageCache.end()){
				//printf("[Reader]\tMessage has already been received. UOID = %s\n" , (const char *)uo_id) ;
				doBreak = true;
			}
		LOCK_OFF(&msgCacheLock) ;

		if(doBreak) {
			return;
		}

		// read the location portno and hostname

		uint32_t distance = 0 ;
		memcpy(&distance, buffer, 4) ;

		int PORT_OFFSET = 4;
		int HOSTNAME_OFFSET = 6;
		struct NodeInfo senderNode ;
		memcpy(&senderNode.portNo, buffer + PORT_OFFSET, 2) ;

		int remainingLen = dataLen - HOSTNAME_OFFSET;
		MEMSET_ZERO(senderNode.hostname, remainingLen);
		memcpy(senderNode.hostname, buffer + HOSTNAME_OFFSET, remainingLen);
		senderNode.hostname[remainingLen] = '\0';

		LOCK_ON(&connectionMapLock) ;
			ConnectionMap[connSocketFd].joinFlag = 1;
			ConnectionMap[connSocketFd].neighbourNode = senderNode;
		LOCK_OFF(&connectionMapLock) ;


		struct CachePacket cachePacket;
		cachePacket.msgLifetimeInCache = metadata->lifeTimeOfMsg;
		cachePacket.status = 1 ;
		cachePacket.sender = senderNode ;
		addPacketToMessageCache(uoid, cachePacket) ;

		prepareJoinResponseAndPushToQ(uoid, distance, connSocketFd) ;

		ttl = ttl - 1 ;

		cachePacket.reqSockfd = connSocketFd ;
		// Push the request message in neighbors queue
		if (ttl >= 1 && metadata->ttl > 0){

			floodJoinRequestOnNetwork(connSocketFd, ttl, distance, dataLen, buffer, uoid);
		}

	}else if(msgType == STATUS_REQ){

		//printf("[Reader] Received STATUS REQ ..... socket:%d\n" , connSocketFd);

		//printf("[Reader] STATUS1 ..... socket:%d\n" , connSocketFd);
		// Cache lookup
		LOCK_ON(&msgCacheLock) ;
			if (MessageCache.find( TO_STRING( (const char*)uoid, SHA_DIGEST_LENGTH) ) != MessageCache.end()){
				//printf("[Reader]\tMessage has already been received. UOID = %s\n" , (const char *)uoid) ;
				doBreak = true;
			}
		LOCK_OFF(&msgCacheLock) ;

		//printf("[Reader] STATUS2 ..... socket:%d\n" , connSocketFd);

		struct CachePacket cachePacket;
		cachePacket.reqSockfd = connSocketFd ;
		cachePacket.msgLifetimeInCache = metadata->lifeTimeOfMsg;
		cachePacket.status = 1 ;

		addPacketToMessageCache(uoid, cachePacket) ;

		//printf("[Reader] STATUS3 ..... socket:%d\n" , connSocketFd);

		// Respond the sender with the status response

		//printf("[Reader] Sending back STATUS RESPONSE\n");
		struct Message statResponseMSG ;
		memcpy(statResponseMSG.uoid , uoid , SHA_DIGEST_LENGTH);

/**
		LOCK_ON(&ConnectionMap[connSocketFd].mQLock) ;

			ConnectionMap[connSocketFd].MessageQ.push_back(statResponseMSG) ;

		LOCK_OFF(&ConnectionMap[connSocketFd].mQLock) ;
**/

		//printf("[Reader] Status4..... socket:%d\n" , connSocketFd);

		statResponseMSG.status = 0;
		statResponseMSG.ttl = 1 ;
		ttl = ttl-1 ;
		uint8_t status_type = 0 ;

		memcpy(&status_type, buffer, 1) ;
		statResponseMSG.statusType = status_type ;
		// Push the request message in neighbors queue
		statResponseMSG.msgType = STATUS_RSP ;

		//printf("[Reader] About to push status response..... socket:%d\n" , connSocketFd);

//		safePushMessageinQ(connSocketFd, statResponseMSG);
		LOCK_ON(&ConnectionMap[connSocketFd].mQLock) ;
			//printf("[Reader] LOCK_ON(%d) 5 \n",connSocketFd);
			//printf("[Reader] *** Inside critical section... socket:%d\n" , connSocketFd);
			ConnectionMap[connSocketFd].MessageQ.push_back(statResponseMSG) ;

		LOCK_OFF(&ConnectionMap[connSocketFd].mQLock) ;
		//printf("[Reader] LOCK_OFF(%d) 5 \n",connSocketFd);
		//printf("[Reader] Check metadata.ttl=%d , ttl=%d\n", metadata->ttl, ttl);

		if (metadata->ttl > 0 && ttl >= 1){

			//printf("[Reader] About to flood to neighbours\n");
			//floodStatusOnNetwork(connSocketFd, ttl, dataLen, buffer, uoid);

		}

	}
	else if (msgType == STATUS_RSP){

		//printf("[Reader] Received STATUS RESPONSE  .... socket:%d\n" , connSocketFd);

		UCHAR originalMsg_UOID[SHA_DIGEST_LENGTH] ;
		//		memcpy((UCHAR *)original_uo_id, buffer, SHA_DIGEST_LENGTH) ;
		memcpy(originalMsg_UOID , buffer, SHA_DIGEST_LENGTH);
/*
		for (int i = 0 ; i < SHA_DIGEST_LENGTH ; i++)
			originalMsg_UOID[i] = buffer[i] ;
*/

		struct NodeInfo n ;


		//printf("[Reader] Received STATUS ... populate node info \n" );
		unsigned short tempLength = 0 ;
		memcpy((unsigned short *)&tempLength, buffer + 20, 2) ;
		memcpy(n.hostname , buffer + 24 , tempLength-2);
		n.hostname[tempLength-2] = '\0' ;
		n.portNo = 0 ;
		memcpy((unsigned short int *)&n.portNo, buffer + 22, 2) ;

		//printf("[Reader] Received STATUS ... cache check \n" );
		/**
		printf("[Reader]\t ...... MessageCache ............");
		for(CACHEPACKET_MAP::iterator = MessageCache.begin(); iterator!= MessageCache.end(); iterator++) {

			string sUoid = (*iterator).first;
			struct CachePacket pkt = (*iterator).second;
			printf("[Reader]\t\tuoid=%s , Packet{sock=%d, status=%d}\n",
					sUoid.c_str(), pkt.reqSockfd, pkt.status );
		}
		**/

		if (MessageCache.find( TO_STRING((const char *)originalMsg_UOID , SHA_DIGEST_LENGTH)) != MessageCache.end()){

			//message origiated from here
			//printf("[Reader-%d] Received STATUS ... Message originated from this node...\n", connSocketFd) ;
			if(MessageCache [ TO_STRING((const char *)originalMsg_UOID, SHA_DIGEST_LENGTH)   ].status == 1){

				//printf("[Reader] Received STATUS ... handleMsgToBeForwarded \n" );
				handleMsgToBeForwarded(originalMsg_UOID, msgType, dataLen, buffer, uoid) ;

			} else if (MessageCache [ TO_STRING((const char *)originalMsg_UOID , SHA_DIGEST_LENGTH) ].status == 0){

				//printf("[Reader] Received STATUS status == 0 \n" );
				// Case of status neighbors
				//if(MessageCache [ TO_STRING((const char *)originalMsg_UOID, SHA_DIGEST_LENGTH)].status_type == 0x01){

					//printf("[Reader] Received STATUS ... insert into status responses \n" );
					handleStatusNeighboursCommand(tempLength, dataLen, buffer, n) ;
				//}
			}
		} else {

			//printf("[Reader] Received STATUS ... Message not from here.. return\n");
			return;
		}
	}

	else if (msgType == JOIN_RSP){


		UCHAR original_uo_id[SHA_DIGEST_LENGTH] ;
		//		memcpy((UCHAR *)original_uo_id, buffer, SHA_DIGEST_LENGTH) ;
		memcpy(original_uo_id  ,  buffer  ,   SHA_DIGEST_LENGTH);

		/**
		for (int i = 0 ; i < SHA_DIGEST_LENGTH ; i++){
			original_uo_id[i] = buffer[i] ;
			//printf("%02x-", original_uo_id[i]) ;
		}
		**/
		//printf("\n") ;


		int LOC_OFFSET = 20;
		uint32_t distance = 0 ;
		memcpy(&distance, buffer + LOC_OFFSET, 4) ;

		int PORT_OFFSET = 24;
		int HOST_OFFSET = 26;
		struct NodeInfo responseNode ;
		memcpy(&responseNode.portNo, buffer + PORT_OFFSET, 2) ;

		int remainLen = dataLen - HOST_OFFSET;
		memcpy(responseNode.hostname , buffer + HOST_OFFSET , remainLen);
		responseNode.hostname[remainLen] = '\0';


		LOCK_ON(&connectionMapLock) ;
			ConnectionMap[connSocketFd].neighbourNode = responseNode;
		LOCK_OFF(&connectionMapLock) ;


		//Message not found in the cache
		if (MessageCache.find( TO_STRING((const char *)original_uo_id, SHA_DIGEST_LENGTH) ) == MessageCache.end()){

			//printf("[Reader]\tJOIN request was never forwarded from this node.\n") ;
			return ;
		}
		else{

			//message started here
			if (MessageCache[ TO_STRING((const char *)original_uo_id, SHA_DIGEST_LENGTH)].status == 0){

				//printf("[Reader-%d]\t +++++++++++++ JOIN request originated from node (%s : %d). Add to joinResponses list \n"
				//					, reqSockfd, responseNode.hostname, responseNode.portNo) ;

				struct JoinResponseInfo jResp;
				jResp.neighbourPortNo = responseNode.portNo;
				for(int i=0;i<256;i++)
					jResp.neighbourHostname[i] = responseNode.hostname[i];
				jResp.location = distance ;

				//pair<set<struct JoinResponseInfo>::iterator,bool> ret = joinResponses.insert(jResp)  ;
				joinResponses.push_back(jResp);

			}
			//message originated from somewhere else
			else if(MessageCache[ TO_STRING((const char *)original_uo_id, SHA_DIGEST_LENGTH)   ].status == 1){

				handleNotMyMsg(original_uo_id, msgType, dataLen, buffer, uoid) ;

			}
		}
	} else if(msgType == KEEPALIVE){

//		printf("[Reader] \t handle KEEPALIVE \n");
		fflush(stdout);

	} // Notify message received
	else if(msgType == NOTIFY) {

		deleteFromNeighboursAndCloseConnection(connSocketFd);

	}
	else if(msgType == STORE_REQ) 
	{

		// Check if packet was already received
		printf("[Reader] \t Handle STORE request \n");
		fflush(stdout);

		string uoidStr = TO_STRING((const char *)uoid , SHA_DIGEST_LENGTH);
		LOCK_ON(&msgCacheLock);

		printf("[Reader]\t lookup uoid:%s\n", uoidStr.c_str());
		if(MessageCache.find(uoidStr) == MessageCache.end()) {

			printf("[Reader] \t NEW Store request, couldn't find in MsgCache\n");

			LOCK_OFF(&msgCacheLock);

			CachePacket packet;
			packet.msgLifetimeInCache =metadata->msgLifeTime;
			packet.reqSockfd = connSocketFd;
			packet.status = 1;


			LOCK_ON(&msgCacheLock);
				MessageCache[uoidStr] = packet;
			LOCK_OFF(&msgCacheLock);


			double coinFlip = drand48();
			if(coinFlip <= metadata->storeProb) {

				printf("[Reader] \tStore... accept and keep this store message locally\n");
				//TODO:write file to cache
				string strMetaData((char *)&buffer[4], dataLen);

				struct FileMetadata fileMetadata ;//= readMetaDataFromStr(strMetaData.c_str());

				//TODO: check the indexes if file exists
				bool doesFileExist = false; // lookupIndices();

				if(doesFileExist) {

					// update the LRU

				} 
				else 
				{

					int fileNumber = incfNumber();  //update global file number
					bool success=false;
					// success = storeInLRU();

					if(success) {

						// write data
						FILE* tempFile = fopen(tempFileName, "rb");
						FILE* dataFile = fopen((char *)fileMetadata.fName, "wb");
						char letter;
						while(fread(&letter, 1, 1, tempFile)) {

							fwrite(&letter, 1, 1, dataFile);
						}
						fclose(tempFile);
						int closeRet = fclose(dataFile);
						if(closeRet != 0) {

							printf("[Reader] ERROR:Closing data file FAILED! Try emptying space!!\n");

						} else {

							printf("[Reader] \tStore... write metadata to file\n");
							writeMetadataToFile(fileMetadata, fileNumber);

							populateIndexes(fileMetadata , fileNumber);
						}
					}
				}

				printf("[Reader] \tStore... Flood to neighbours\n");
				fflush(stdout);
				//send to neighbours
				LOCK_ON(&currentNeighboursLock);

					NEIGHBOUR_MAP_ITERATOR iter = currentNeighbours.begin() ;
					for(; iter!=currentNeighbours.end(); iter++) {

						if((*iter).second != connSocketFd ) {
						
							//if((double) drand48() > metadata->neighborStoreProb)
								//continue;
								
							printf("[Reader]\t Send to neighbour :%d\n", (*iter).second);	
							struct Message storeMessage;
							storeMessage.dataLen=  dataLen;


							// set file name
							int tmpFNameLength = strlen(tempFileName);
							strncpy((char *)storeMessage.fileName , tempFileName, tmpFNameLength);
							storeMessage.fileName[ tmpFNameLength ] = '\0';

							if(ttl < metadata->ttl)
								storeMessage.ttl= ttl;
							else
								storeMessage.ttl= metadata->ttl;

							// message type and uoid
							storeMessage.msgType = STORE_REQ;
							MEMSET_ZERO(storeMessage.uoid, SHA_DIGEST_LENGTH);
							memcpy(storeMessage.uoid , uoid, SHA_DIGEST_LENGTH);


							storeMessage.buffer = (UCHAR *)malloc(dataLen);
							MEMSET_ZERO(storeMessage.buffer, dataLen);
							memcpy(storeMessage.buffer , buffer, dataLen);


							// enqueue to be sent to neighbour
							safePushMessageinQ(connSocketFd, storeMessage,"Reader");
						} else {
							printf("[Reader] \t Not sending to SELF\n");
						}
					}

				LOCK_OFF(&currentNeighboursLock);

			}

		} else {

			printf("[Reader]\t Store... request already present in cache, IGNORE\n");
			LOCK_OFF(&msgCacheLock);
			return;
		}
		printf("[Writer]\t Store... req handling done!!!\n");
	}

	//printf("[Reader] \t Reset keepalive\n");
	fflush(stdout);
	resetKeepAliveTimeout(connSocketFd) ;
	//	if(connectionMap[reqSockfd].keepAliveTimer!=0)

}


//This fnction pushes the NOTIFY message at the very begining of the message queue, so as to send it at priority
void sendNotificationOfShutdown(int resSock, uint8_t errorCode)
{
	//printf("[Notify] Send ------->  %d\n", resSock);
	struct Message notifyMsg ;
	notifyMsg.errorCode = errorCode ;
	notifyMsg.status = 0 ;
	notifyMsg.msgType = NOTIFY ;

	//pushing the message and signaling the write thread

	LOCK_ON(&ConnectionMap[resSock].mQLock) ;
		printf("[Reader] LOCK_ON(%d) 6 \n",resSock);
		ConnectionMap[resSock].MessageQ.push_back(notifyMsg) ;
	LOCK_OFF(&ConnectionMap[resSock].mQLock) ;
	printf("[Reader] LOCK_OFF(%d) 6 \n",resSock);

}




bool checkIfReaderLoopShouldBreak(long  connSocket, bool doBreak)
{
    LOCK_ON(&connectionMapLock) ;
		//close connection if either of the condition occurs, jointime out, shutdown
		if( globalShutdownToggle || ConnectionMap[connSocket].keepAliveTimeOut == -1 )//|| joinTimeOutFlag
		{
			//printf("[Reader-%d]\t breaking out of Reader loop\n", reqSockfd);
			doBreak = true;
		}
    LOCK_OFF(&connectionMapLock) ;

    return doBreak;
}

/**
 *  - Check the ConnMap if this socket exists, if not exit thread
 *  while(!shutdown) {
 *  	- break if (ConnectionMap[reqSockfd].keepAliveTimeOut == -1 || globalShutdownToggle)
 *  	- Read header
 * 	 	- Read body
 * 	 	- handle request case by case
 *  }
 **/
void *connectionReaderThread(void *args) {


	UCHAR *body;
	UCHAR header[HEADER_SIZE];

	long connSocket  =  (long) args;

	printf("[Reader] ++++++ NEW Thread started[%ld] ++++++ \n", connSocket);

	for(  ;  !ConnectionMap[connSocket].shutDown ;  )
	{

		int bytesReadTillNow = 0;

		//LOCK_OFF(&ConnectionMap[connSocket].mQLock);

		bool doBreak = false;
		doBreak = checkIfReaderLoopShouldBreak (connSocket, doBreak) ;
		if(doBreak)
		{
			printf("[Reader]**********Going to do break*********************\n");
			deleteFromNeighboursAndCloseConnection (connSocket);
			break;
		}


		// read header
		MEMSET_ZERO(header, HEADER_SIZE);
		int bytesRead = (int)read(connSocket, header, HEADER_SIZE);
		
		printf("[Reader] ======= Got NEW message, Reading HEADER ..Number of bytes read:%d\n",bytesRead);
		uint8_t messageType = 0;
		uint8_t ttl = 0;
		uint32_t bufferDataLength = 0;
		UCHAR uoid[20] ;

		MEMSET_ZERO(uoid, 20);

		// populate header
		memcpy(&messageType, 		header, 	 1);
		memcpy(uoid , 		 		header + 1,  SHA_DIGEST_LENGTH);
		memcpy(&ttl,  		 		header + 21, 1);
		memcpy(&bufferDataLength,  	header + 23, 4);
		
		//printf("[Reader] \t Read HEADER bytes:%d , Message type :%02x , data length:%d ..... %s\n", bytesRead, messageType, bufferDataLength, header);
		if(bytesRead != HEADER_SIZE) 
		{
			printf("[Reader] \tERROR:couldn't read HEADER_SIZE , BREAK\n");
			deleteFromNeighboursAndCloseConnection (connSocket);
			break;
		}

		bytesReadTillNow += bytesRead;

		doBreak = checkIfReaderLoopShouldBreak (connSocket, doBreak) ;
		if(doBreak) 
		{
			printf("[Reader] \tBREAK:Loop should break\n");
			deleteFromNeighboursAndCloseConnection (connSocket);
			break;
		}

		char tempFileName[256];
		
		if(messageType == STORE_REQ) 
		{

			// read metadata length
			printf("[Reader] \tStore_req caught\n");
			uint32_t metadataLen ;
			int retCode = read(connSocket, &metadataLen, 4);

			bytesReadTillNow += retCode;
			printf("[Reader] \tStore ... metadata len:%d, numBytes:4, till now: %d \n", metadataLen, bytesReadTillNow);

			// read metadata
			body = (UCHAR *)malloc(sizeof(UCHAR) * (metadataLen+1));
			retCode = read(connSocket, body, metadataLen);

			bytesReadTillNow += retCode;
			body[metadataLen] = '\0';
			printf("[Reader] \tStore ... Read metadata : %s\n till now: %d\n", body, bytesReadTillNow);
			
			// create file to store content
			char fNameSuffix[L_tmpnam];// = "1.data";

			tmpnam(fNameSuffix);
			sprintf(tempFileName, "%s/file%s.tmp", metadata->currWorkingDir, fNameSuffix) ;
			printf("[Reader] \tStore ... tempFileName : %s\n", tempFileName);

			// pull data chunk-wise

			int bytesToBeRead = bufferDataLength - metadataLen - 4;
			UCHAR dataChunk[8192];
			int totalBytesRead = 0;
			FILE* tempDataFile = fopen(tempFileName , "wb");

			printf("[Reader]\tStore ... Reading file data in chunks, bytesToBeRead:%d, Write to : %s\n", bytesToBeRead, tempFileName);
			for ( ; totalBytesRead < bytesToBeRead ; ) {

				int readBytes = read(connSocket, dataChunk, 8192);
				bytesReadTillNow += readBytes;
				printf("[Reader]\t\t ... read %d bytes , data: {%s}, till now: %d \n", readBytes, dataChunk, bytesReadTillNow);
				fflush(stdout);

				if(readBytes == 0) {
					printf("[Reader]\t\t ... BREAK chunk loop!!");
					break;
				}

				fwrite(dataChunk,	1,   readBytes, tempDataFile);
				totalBytesRead += readBytes;
			}

			printf("[Reader] \tStore ... Read data bytes : %d, till now: %d \n", totalBytesRead, bytesReadTillNow);
			fflush(stdout);
			
			fflush(tempDataFile);
			fclose(tempDataFile);

			// set fileDataLen, dataLen = 4 + metadataLength
			int fileDataLen  = totalBytesRead;
			bytesRead = metadataLen + 24;
			bufferDataLength = metadataLen + 24;

		}
		else 
		{
			// read message body
			body = (UCHAR *)malloc(sizeof(UCHAR)*(bufferDataLength+1)) ;
			bytesRead = (int)read(connSocket, body, bufferDataLength);
			body[bufferDataLength] = '\0' ;
		}

		if(bytesRead != bufferDataLength) 
		{
			printf("Complete data couldnt be read\n");
			deleteFromNeighboursAndCloseConnection (connSocket);
			break;
		}
		
		doBreak = checkIfReaderLoopShouldBreak (connSocket, doBreak) ;
		if(doBreak) 
		{
			deleteFromNeighboursAndCloseConnection (connSocket);
			break;
		}

		//printf("[Reader] \tHandle Case by Case... messageType:%02x , bufferDataLength:%d, tempFileName:%s\n",
			//			messageType, 	bufferDataLength, 	tempFileName);
		// look at request and handle case by case
		handleRequestByCase(connSocket,
							messageType,
							uoid,
							ttl,
							body,
							bufferDataLength,
							tempFileName);

		// Todo: Log message received unless it's hello
		printf("[Reader]  Finished sending message\n");
		fflush(stdout);

		free(body);
	}

	printf("[Reader-%d]\t********** Leaving Read thread \n", connSocket);

	pthread_exit(0);

	return 0;

}


void putConnectionInfoInMap(int & acceptSocketfd, struct ConnectionInfo connInfo)
{
	//			printf("[Server]\tAdd to ConnectionMap[%d] = (%s : %d)\n", acceptSocketfd, connInfo.neighbourNode.hostname);
	LOCK_ON(&connectionMapLock);
	ConnectionMap[acceptSocketfd] = connInfo;
	LOCK_OFF(&connectionMapLock);
}

void waitForServerChildThreadsToFinish(PTHREAD_LIST & serverListenerChildThreads, void *thread_result)
{
	//Accept thread waits for child thread to exit, joins here
	PTHREAD_LIST::iterator it = serverListenerChildThreads.begin();
	while(  it != serverListenerChildThreads.end() ) {

		int res = pthread_join((*it), &thread_result);
		if(res == 0) {
			++it;
			continue;
		}
		//doLog((UCHAR *)"//In Accept: Thread Join failed\n");
		exit(EXIT_FAILURE);
	}
}


/**
 * Node server thread that listens for connections
 */
void *nodeServerListenerThread(void *) {


	//printf("[Server]\tListener thread started....\n");

	struct addrinfo addr_hint, *servinfo, *p;
	//int nSocket=0 ; // , portGiven = 0 , portNum = 0;
	UCHAR port_buf[10] ;
	memset(port_buf, '\0', 10) ;

	int ret = 0;

	//maintains the child threads ID, used for joining
	PTHREAD_LIST serverListenerChildThreads;

	memset(&addr_hint, 0, sizeof addr_hint) ;
	addr_hint.ai_family = AF_INET;
	addr_hint.ai_socktype = SOCK_STREAM;
	addr_hint.ai_flags = AI_PASSIVE;
	sprintf((char*)(port_buf), "%d", metadata->portNo);
	// Code until connection establishment has been taken from the Beej's guide
	if ((ret = getaddrinfo(NULL, (char *)port_buf, &addr_hint, &servinfo)) != 0) {
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

		//printf("[Server]\tBIND to address\n");

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
		//doLog((UCHAR *)"//server: failed to bind socket\n");
		exit(1) ;
	}

	freeaddrinfo(servinfo); // all done with this structure
	if(listen(acceptServerSocket, 5) == -1){
		//		fprintf(stderr, "server: Error in listen\n");
		close(acceptServerSocket);
		//doLog((UCHAR*)("//Error in listen!!!\n"));
		exit(1);
	}
	// Accept handler code begins here
	for(;;) {

		//printf("[Server]\t Server waiting for requests..\n");

		int cli_len = 0, acceptSocketfd = 0;
		struct sockaddr_in cli_addr ;


		if(globalShutdownToggle)
		{
			//printf("[Server]\tShutdown detected, break!\n");
			//shutdown(acceptServerSocket, SHUT_RDWR);
			break;
			//close(acceptServerSocket);
		}

		acceptSocketfd = accept(acceptServerSocket,
				(struct sockaddr *) &cli_addr,
				(socklen_t *)&cli_len);

		// Server received a connection from someone
		if(acceptSocketfd > 0) {



			//printf("[Server]\tI got a connection from (%s , %d)\n", inet_ntoa(cli_addr.sin_addr),(cli_addr.sin_port));



			// Create connection info object
			struct ConnectionInfo connInfo ;
			int mres = pthread_mutex_init(&connInfo.mQLock, NULL) ;
			if (mres != 0){
				//perror("[Server]\tMutex initialization failed");
				//doLog((UCHAR *)("//Mutex initialization failed\n"));
			}
			/*int cres = pthread_cond_init(&cn.mQCv, NULL) ;
			if (cres != 0){
				perror("CV initialization failed") ;
				writeLogEntry((UCHAR *)"//CV initialization failed\n");
			}*/
			connInfo.shutDown = 0;
			connInfo.keepAliveTimer = metadata->keepAliveTimeOut / 2;
			connInfo.keepAliveTimeOut = metadata->keepAliveTimeOut;
			connInfo.isReady = 0;


			putConnectionInfoInMap(acceptSocketfd, connInfo) ;


			// Spawn Reader thread
			int res ;
			pthread_t readRequestThread ;
			res = pthread_create(&readRequestThread, NULL, connectionReaderThread , (void *)acceptSocketfd);
			if (res != 0) {
				perror("Thread creation failed");
				//doLog((UCHAR *)"//In Accept: Read Thread creation failed\n");
				exit(EXIT_FAILURE);
			}
			ConnectionMap[acceptSocketfd].myReadId = readRequestThread;
			serverListenerChildThreads.push_front(readRequestThread);








			// send Hello ??
			struct Message m;
			m.msgType = HELLO_REQ ;
			m.status = 0;
			//m.fromConnect = 0;
			//printf("[Server]\tSending HELLO message\n");

			LOCK_ON(&ConnectionMap[acceptSocketfd].mQLock) ;
				printf("[Reader] LOCK_ON(%d) 7 \n",acceptSocketfd);
				ConnectionMap[acceptSocketfd].MessageQ.push_back(m) ;

			LOCK_OFF(&ConnectionMap[acceptSocketfd].mQLock) ;
			printf("[Reader] LOCK_OFF(%d) 7 \n",acceptSocketfd);



			// Spawn Writer thread
			pthread_t writerThread ;
			res = pthread_create(&writerThread, NULL, connectionWriterThread , (void *)acceptSocketfd);
			if (res != 0) {
				//printf("[Server]\tIn Accept: Write Thread creation failed\n");
				//doLog((UCHAR *)"//In Accept: Write Thread creation failed\n");
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
	//printf("[Server]\tWait for server child threads to finish, size : %d\n", serverListenerChildThreads.size());
	void *thread_result;

	waitForServerChildThreadsToFinish(serverListenerChildThreads, thread_result);

	//printf("[Server]\t Server terminated normally\n") ;
	serverListenerChildThreads.clear();

	//printf("[Server]\tExited!\n");
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

	//printf("[Server]\t Fill info to connection map :%d\n", newreqSockfd);
	// Populate ConnectionMap for this reqSockfd
	struct ConnectionInfo connectionInfoObj ;

	int mret = pthread_mutex_init(&connectionInfoObj.mQLock, NULL) ;
	if (mret != 0){
		perror("Mutex initialization failed");
		//doLog((UCHAR *)"//Mutex initialization failed\n");
	}

	connectionInfoObj.shutDown = shutDown ;
	connectionInfoObj.keepAliveTimer = keepAliveTimer;
	connectionInfoObj.keepAliveTimeOut = keepAliveTimeOut;
	connectionInfoObj.isReady = isReady;
	connectionInfoObj.neighbourNode = n;

	LOCK_ON(&connectionMapLock) ;
		ConnectionMap[newreqSockfd] = connectionInfoObj ;
	LOCK_OFF(&connectionMapLock) ;

}



