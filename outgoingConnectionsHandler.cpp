/**
 *  Outgoing connection handler
 **/ 

#include "main.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
//#include "iniParser.h"
//#include "signalHandler.h"
//#include "indexSearch.h"

using namespace std;

int connectTo(char *context, unsigned char *hostName, unsigned int portNum ){

	//printf("[%s]\tConnect to %s : %d\n", context, hostName, portNum);
	struct addrinfo hints, *server_info;
	int connSocket=0;
	int status;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	int r_no;

	char portStr[10];
	memset(portStr, '\0', 10);
	sprintf(portStr, "%d", portNum);

	if ((r_no = getaddrinfo((char *)hostName, (char*)portStr, &hints, &server_info)) != 0) {

		printf("[%s]\tError in getaddrinfo() function\n", context);
		exit(0);
	}

	if ((connSocket = socket(server_info->ai_family, server_info->ai_socktype,server_info->ai_protocol)) == -1) {

		perror("\tClient: socket");
		exit(0);
	}

	status = connect(connSocket, server_info->ai_addr, server_info->ai_addrlen);
	if (status < 0) {

		close(connSocket);
		perror("\tClient: connect");
		return -1 ;

	} else {

		printf("[%s]\tConnected successfully to %s : %d\n", context, hostName, portNum);
		return connSocket;
	}

}


/**
 *  Send messages in the dispatcher queue for a socket connection.
 *   Keep the thread running until :
 *    - the connection is asked to shutdown
 *    - the node itself shutdown
 */
void *connectionWriterThread(void *args) {


	long connSocket = (long)args ;

	//printf("[Writer-%d]\t********** Started\n" , connSocket);

	struct Message outgoingMsg ;
	unsigned char header[HEADER_SIZE] ;
	unsigned char *buffer ;
	uint32_t len = 0 ;
	FILE *fp;
	struct stat st ;

	while(!shutDownFlag){


//		printf("[Writer-%d]\t ...\n" , connSocket);
/**
		if(ConnectionMap.find(connSocket) == ConnectionMap.end()) {

				printf("[Writer-%d]\t Socket not in ConnectionMap... breaking!\n");
				break;
		}
**/
		bool isConnectionShutting = false;
		int msgQSize =0;
		pthread_mutex_lock(&connectionMapLock) ;
			isConnectionShutting = ConnectionMap[connSocket].shutDown;
			msgQSize = ConnectionMap[connSocket].MessageQ.size();
		pthread_mutex_unlock(&connectionMapLock) ;

		//printf("[Writer-%d]\t Checking for messages to send, mQ size:%d\n", connSocket, msgQSize);

		if(isConnectionShutting) {

			printf("[Writer-%d]\tShutdown connection [%d] break from connection writer thread\n",connSocket, connSocket);
			break;
		}

		if(msgQSize <= 0){

			//printf("[Writer-%d]\t Message queue is now empty\n", connSocket);

			pthread_mutex_lock(&ConnectionMap[connSocket].mQLock) ;
			//pthread_cond_wait(&ConnectionMap[connSocket].mesQCv, &ConnectionMap[connSocket].mQLock) ;
			if (ConnectionMap[connSocket].shutDown)
			{
				printf("[Writer-%d]\t Connection was marked for shutdown, breaking out of loop!\n", connSocket);
				pthread_mutex_unlock(&ConnectionMap[connSocket].mQLock) ;
				break ;
			}

			outgoingMsg = ConnectionMap[connSocket].MessageQ.front() ;
			if(ConnectionMap[connSocket].MessageQ.size() == 0) {

				//printf("[Writer-%d]\toutgoingMsg == NULL .. Waiting for a sec!\n", connSocket);
				pthread_mutex_unlock(&ConnectionMap[connSocket].mQLock) ;

				sleep(1);
				continue;
			} else {

				ConnectionMap[connSocket].MessageQ.pop_front() ;
			}
			pthread_mutex_unlock(&ConnectionMap[connSocket].mQLock) ;


		} else {

			//printf("[Writer]\t Pick next message\n");
			pthread_mutex_lock(&ConnectionMap[connSocket].mQLock) ;
				outgoingMsg = ConnectionMap[connSocket].MessageQ.front() ;
				ConnectionMap[connSocket].MessageQ.pop_front() ;
			pthread_mutex_unlock(&ConnectionMap[connSocket].mQLock) ;
		}

		if (ConnectionMap[connSocket].shutDown) {

			pthread_mutex_unlock(&ConnectionMap[connSocket].mQLock) ;
			printf("[Writer-%d]\tShutdown connection [%d] break from connection writer thread\n",connSocket, connSocket);
			break ;
		}

		memset(header, '\0', HEADER_SIZE) ;

		// Message originated from here
		if (outgoingMsg.status == 0){

			unsigned char uoid[SHA_DIGEST_LENGTH] ;
			GetUOID( const_cast<char *> ("msg"), uoid, sizeof(uoid)) ;
			struct CachePacket packet;
			packet.status = 0 ;
			packet.msgLifetimeInCache = metadata->msgLifeTime;

			pthread_mutex_lock(&msgCacheLock) ;
				MessageCache[string((const char *)uoid, SHA_DIGEST_LENGTH) ] = packet ;
			pthread_mutex_unlock(&msgCacheLock) ;

			// put uoid into header
			for (int i=0 ; i < SHA_DIGEST_LENGTH ; i++)
				header[1+i] = uoid[i] ;
		}
		else if (outgoingMsg.status == 1){
			//

			// set uoid on header
			for (int i=0 ; i < SHA_DIGEST_LENGTH ; i++)
				header[1+i] = outgoingMsg.uoid[i] ;
		}
		// Done for status message
		else if (outgoingMsg.status == 2 || outgoingMsg.status == 3){
			for (int i=0 ; i < SHA_DIGEST_LENGTH ; i++)
				header[1+i] = outgoingMsg.uoid[i] ;
		}


		// Message of type Hello
		if (outgoingMsg.msgType == 0xfa){

			printf("[Writer-%d]\tSending Hello Message\n" , connSocket) ;
			header[0] = 0xfa;
			header[21] = 0x01 ;
			unsigned char host[256] ;
			gethostname((char *)host, 256) ;
			host[255] = '\0' ;
			len = strlen((char *)host) + 2 ;
			memcpy((char *)&header[23], &(len), 4) ;

			buffer = (unsigned char *)malloc(len) ;
			memset(buffer, '\0', len) ;
			memcpy((char *)buffer, &(metadata->portNo), 2) ;

			//			sprintf((char *)&buffer[2], "%s",  host);
			for (int i = 0 ; i < (int)len - 2 ; ++i)
				buffer[2+i] = host[i] ;

			//Incrementing Ready STatus
			pthread_mutex_lock(&connectionMapLock) ;
			ConnectionMap[connSocket].isReady++;
			pthread_mutex_unlock(&connectionMapLock) ;
		}
		// Sending JOIN Request
		else if (outgoingMsg.msgType == 0xfc){

			printf("[Writer-%d]\tSending JOIN Request , status : %d\n", connSocket, outgoingMsg.status) ;
			if (outgoingMsg.status == 1){

				buffer = outgoingMsg.buffer ;
				len = outgoingMsg.dataLen ;
			}
			else{

				unsigned char host[256] ;
				gethostname((char *)host, 256) ;
				host[255] = '\0' ;
				len = strlen((char *)host) + 6 ;
				buffer = (unsigned char *)malloc(len) ;
				memset(buffer, '\0', len) ;
				memcpy((unsigned char *)buffer, &(metadata->location), 4) ;
				memcpy((unsigned char *)&buffer[4], &(metadata->portNo), 2) ;
				sprintf((char *)&buffer[6], "%s",  host);
			}

			header[0] = 0xfc;
			memcpy((char *)&header[21], &(outgoingMsg.ttl), 1) ;
			header[22] = 0x00 ;
			memcpy((char *)&header[23], &(len), 4) ;

		}
		// Sending Join Response
		else if (outgoingMsg.msgType == 0xfb){

			//originated from somewhere else, just need to forward the message
			if (outgoingMsg.status == 1){

				printf("[Writer-%d]\tForwarding join response\n" , connSocket) ;
				buffer = (unsigned char *)malloc(outgoingMsg.dataLen) ;
				for (int i = 0 ; i < outgoingMsg.dataLen ; i++)
					buffer[i] = outgoingMsg.buffer[i] ;
				len = outgoingMsg.dataLen ;
			}
			else{

				printf("[Writer-%d]\tSending JOIN Response originated here\n", connSocket) ;
				//originated from here, needs to send to destinations
				unsigned char host[256] ;
				gethostname((char *)host, 256) ;
				host[255] = '\0' ;
				len = strlen((char *)host) + 26 ;
				buffer = (unsigned char *)malloc(len) ;
				memset(buffer, '\0', len) ;

				for(unsigned int i = 0;i<20;i++){
					buffer[i] = outgoingMsg.uoid[i];

				}
				//printf("\n") ;
				memcpy(&buffer[20], &(outgoingMsg.location), 4) ;
				memcpy(&buffer[24], &(metadata->portNo), 2) ;
				sprintf((char *)&buffer[26], "%s",  host);
			}

			header[0] = 0xfb;
			memcpy((char *)&header[21], &(outgoingMsg.ttl), 1) ;
			header[22] = 0x00 ;
			memcpy((char *)&header[23], &(len), 4) ;
		}
		// Sending Keepalive request
		else if (outgoingMsg.msgType == 0xf8){

			printf("[Writer-%d]\t Sending KeepAlive request from : %d\n", (int)connSocket, (int)connSocket) ;

			len = 0;
			buffer = (unsigned char *)malloc(len) ;
			memset(buffer, '\0', len) ;

			header[0]  = 0xf8;
			header[21] = 0x01;
			header[22] = 0x00 ;
			memcpy((char *)&header[23], &(len), 4) ;

		}
		// Status Message request
		else if (outgoingMsg.msgType == 0xac){


			printf("[Writer-%d]\t Sending Status message %d\n", (int)connSocket, (int)connSocket) ;

			//originted from somewhere else
			if (outgoingMsg.status == 1){
				buffer = outgoingMsg.buffer ;
				len = outgoingMsg.dataLen ;
			}
			else{
				len = 1 ;
				buffer = (unsigned char *)malloc(len) ;
				memset(buffer, '\0', len) ;
				buffer[0] = outgoingMsg.status_type ;
			}

			header[0] = 0xac;

			memcpy((char *)&header[21], &(outgoingMsg.ttl), 1) ;
			header[22] = 0x00 ;
			memcpy((char *)&header[23], &(len), 4) ;

		}else if (outgoingMsg.msgType == 0xf7)
		{
			printf("Sending Notify message\n");
			len = 1;
			buffer = (unsigned char *)malloc(len) ;
			memset(buffer, '\0', len) ;

			buffer[0]=outgoingMsg.errorCode;
			header[0] = 0xf7;
			header[21] = 0x01 ;

			memcpy((char *)&header[23], &(len), 4) ;

		}
		else if (outgoingMsg.msgType == 0xab){

			printf("[Writer-%d]\tSending Status Response..\n", connSocket) ;

			if (outgoingMsg.status == 1){

				printf("[Writer-%d]\t\t status =1\n", connSocket) ;
				buffer = (unsigned char *)malloc(outgoingMsg.dataLen) ;
				for (int i = 0 ; i < outgoingMsg.dataLen ; i++)
					buffer[i] = outgoingMsg.buffer[i] ;
				len = outgoingMsg.dataLen ;
			}
			else{

				printf("[Writer-%d]\t\t status !=1\n", connSocket);

				unsigned char host[256] ;
				gethostname((char *)host, 256) ;
				host[255] = '\0' ;
				unsigned int len1 = strlen((char *)host) + 24 ;
				buffer = (unsigned char *)malloc(len1) ;
				memset(buffer, 0, len1) ;

				for(unsigned int i=0;i<20;i++)
					buffer[i] = outgoingMsg.uoid[i];
				uint16_t templen = len1 - 22 ;
				memcpy(&buffer[20], &(templen), 2) ;
				memcpy(&buffer[22], &(metadata->portNo), 2) ;
				for (int i = 24 ; i < (int)len1 ; ++i)
					buffer[i] = host[i-24] ;
				//				sprintf((char *)&buffer[24], "%s",  host);
				len  =  len1 ;


				if(outgoingMsg.status_type == 0x01){

					for(map<struct NodeInfo, int>::iterator it = neighbourNodeMap.begin(); it != neighbourNodeMap.end() ; ++it){
						unsigned int len2 = strlen((char *)((*it).first.hostname)) + 2  ;
						len += len2+4 ;
						buffer = (unsigned char *)realloc(buffer, len) ;
						++it ;
						if ( it == neighbourNodeMap.end() ){
							unsigned int temlen2 = 0 ;
							memcpy(&buffer[len-len2-4], &(temlen2), 4);
						}
						else{
							memcpy(&buffer[len-len2-4], &(len2), 4);
						}
						--it ;
						memcpy(&buffer[len-len2], &((*it).first.portNo ), 2) ;
						for (int i = len - len2 + 2 ; i < (int)len ; ++i)
							buffer[i] = host[i -(len-len2+2)] ;
						//					sprintf((char *)&buffer[len-len2+2], "%s",  host);
					}
				}
				/**else if(outgoingMsg.status_type == 0x02){
			//		printf("Sending Status Files response\n") ;
					list<int> tempList ;
					tempList  = getAllFiles() ;

					struct metaData metadata ;
					string metaStr("")  ;
					for(list<int>::iterator it = tempList.begin(); it != tempList.end(); it++){
						metadata = populateMetaData(*it) ;
						fileIDMap[string((char *)metadata.fileID, 20)] = (*it) ;
						metaStr = MetaDataToStr(metadata) ;
						//						printf("%s\n", metaStr) ;
						unsigned int len2 = metaStr.size()  ;
						len += len2+4 ;
						buffer = (unsigned char *)realloc(buffer, len) ;
						++it ;
						if ( it == tempList.end() ){
							unsigned int temlen2 = 0 ;
							memcpy(&buffer[len-len2-4], &(temlen2), 4);
						}
						else{
							memcpy(&buffer[len-len2-4], &(len2), 4);
						}
						--it ;
						for (int i = len - len2  ; i < (int)len ; ++i)
							buffer[i] = metaStr[i -(len-len2)] ;
					}

				}**/

			}

			header[0] = 0xab;
			memcpy((char *)&header[21], &(outgoingMsg.ttl), 1) ;
			header[22] = 0x00 ;
			memcpy((char *)&header[23], &(len), 4) ;
		}



		//KeepAlive message sending
		//Resst the keepAliveTimer for this connection
		if(ConnectionMap.find(connSocket)!=ConnectionMap.end())
		{
			pthread_mutex_lock(&connectionMapLock) ;
				ConnectionMap[connSocket].keepAliveTimer = metadata->keepAliveTimeOut/2;
			pthread_mutex_unlock(&connectionMapLock) ;
		}

		int return_code = 0 ;
		return_code = (int)write(connSocket, header, HEADER_SIZE) ;
		if (return_code != HEADER_SIZE){
			fprintf(stderr, "Socket Write Error") ;
		}
//		printf("[Writer-%d]\t\tWrote %d bytes of header\n", connSocket, return_code);

		if(outgoingMsg.status == 3){
			write(connSocket, buffer, len - st.st_size) ;
			unsigned char chunk[8192] ;
			// read the content of the file and write on the socket
			//while(!feof(fp)){
			while(1){
				memset(chunk, '\0' , 8192) ;
				int numBytes = fread(chunk, 1, 8192, fp) ;
				if(numBytes == 0)
					break;
				write(connSocket, chunk, numBytes) ;
			}
		}
		else{
			return_code = (int)write(connSocket, buffer, len) ;
			if (return_code != (int)len){
				fprintf(stderr, "[Writer]\t Failed to write buffer bytes") ;
			}
//			printf("[Writer-%d]\t\tWrote %d bytes of buffer\n", connSocket, return_code);
		}


		//logging the message sent from this node or forwarded from this node
		unsigned char *logEntry = NULL;

		if(!(outgoingMsg.msgType == 0xfa && (joinNetworkPhaseFlag || ConnectionMap[connSocket].joinFlag == 1)))
		{
			//printf("[Writer]\t\tcreating LOG ENTRY for sock:%d, header:%s, buf:%s\n", connSocket, header, buffer);
			pthread_mutex_lock(&logEntryLock) ;
			if(outgoingMsg.status== 0 || outgoingMsg.status == 2 )
				logEntry = createLogRecord('s', connSocket, header, buffer);
			else
				logEntry = createLogRecord('f', connSocket, header, buffer);

			if(logEntry!=NULL) {
	//			printf("[Reader]\t\tWriting LOG ENTRY\n");
				writeLogEntry(logEntry);
			}
			pthread_mutex_unlock(&logEntryLock) ;
		}

		// Do some logging
//		printf("[Writer]\t About to free buffer len=%d \n", len);
		if(len > 0 ) {

			free(buffer) ;
		}

		//printf("[Writer-%d]\t ...... End of writer loop\n", connSocket);

		sleep(1);

		//printf("[Writer-%d]\t ...... Done sleeping\n", connSocket);
	}

	//printf("[Writer-%d]\t********** Leaving write thread \n", connSocket);
	pthread_exit(0);

	printf("[Writer-%d]\tExited!\n", connSocket);
	return 0;
}

void safelyEmptyAllNeighbours()
{
	printf("[JoinPhase] safely empty all neighbours\n");
	// Empty all the neighbours
	pthread_mutex_lock(&neighbourNodeMapLock);
		neighbourNodeMap.erase(neighbourNodeMap.begin(), neighbourNodeMap.end());
	pthread_mutex_unlock(&neighbourNodeMapLock);
}

/**
void waitForChildThreadsToFinish()
{
    // wait for child threads to finish
    printf("[Main]\tWaiting for all threads to finish , size:%d\n", childThreadList.size());
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
**/


void waitForChildThreadsToFinish() {

	void *thread_result;

	// Join the read thread here
	printf("[Writer]\tJoin spawned threads...");
	for (list<pthread_t >::iterator it = childThreadList.begin(); it != childThreadList.end(); ++it){


		int res = pthread_join((*it), &thread_result);
		if (res != 0) {
			perror("Thread join failed");
			writeLogEntry((unsigned char *)"//In Join Network: Thread Joining Failed\n");
			exit(EXIT_FAILURE);
		}
	}
}

void writeRespondingNeighboursToInitFile(FILE *& initNeighboursFile)
{
	printf("[JoinPhase]\tWrite responded neighbours to init neighbours file\n");

    unsigned int counter = 0;
    for(list<struct JoinResponseInfo>::iterator it = joinResponses.begin();it != joinResponses.end();it++, counter++){
        //                if(counter==metadata->minNeighbor)
        //                        break;
        JoinResponseInfo jresp = (*it);
        unsigned char portBuf[10];
        printf("[JoinPhase]\tHostname: %s, Port: %d, location: %d\n", jresp.neighbourHostname, jresp.neighbourPortNo, jresp.location);
        fputs((char*)(((jresp.neighbourHostname))), initNeighboursFile);
        fputs(":", initNeighboursFile);
        sprintf((char*)(((portBuf))), "%d", jresp.neighbourPortNo);
        fputs((char*)(((portBuf))), initNeighboursFile);
        fputs("\n", initNeighboursFile);
    }
}

//Writing the status response to the file, creating the nam file
void writeToStatusFile(){

	printf("[Status]\tWriting to status file : %s\n" , (char *)metadata->status_file) ;

	pthread_mutex_lock(&statusMsgLock) ;
	FILE *fp = fopen((char *)metadata->status_file, "a") ;
	if (fp == NULL){
		//fprintf(stderr, "File open failed\n") ;
		writeLogEntry((unsigned char *)"//Failed to open STATUS FILE\n");
		exit(0) ;
	}

	set<struct NodeInfo> distinctNodes ;
	set<struct NodeInfo>::iterator it1 ;

	for (set< set<struct NodeInfo> >::iterator it = statusResponse.begin(); it != statusResponse.end() ; ++it){
		it1 = (*it).begin() ;
		distinctNodes.insert( *it1 ) ;
		++it1 ;
		distinctNodes.insert( *it1 ) ;
	}

	if(distinctNodes.size() == 0)
	{
		fputs("n -t * -s ", fp) ;

		unsigned char portS[20] ;
		sprintf((char *)portS, "%d", metadata->portNo) ;
		fputs((char *)portS, fp) ;
		//Beacon node is represnted by BLUE NODES
		if (metadata->isBeacon){
			fputs(" -c blue -i blue\n", fp) ;
		}
		//NON-Beacon ndoe is represented by BLACK nodes
		else{
			fputs(" -c black -i black\n", fp) ;
		}
	}

	for (set< struct NodeInfo >::iterator it = distinctNodes.begin(); it != distinctNodes.end() ; ++it){
		fputs("n -t * -s ", fp) ;
		unsigned char portS[20] ;
		sprintf((char *)portS, "%d", (*it).portNo) ;
		fputs((char *)portS, fp) ;
		//Beacon node is represnted by BLUE NODES
		if (isBeaconNode(*it) ){
			fputs(" -c blue -i blue\n", fp) ;
		}
		else{
			//NON-Beacon node is represnted by BLACK NODES
			fputs(" -c black -i black\n", fp) ;
		}
	}

	for (set< set<struct NodeInfo> >::iterator it = statusResponse.begin(); it != statusResponse.end() ; ++it){
		fputs("l -t * -s ", fp) ;
		struct NodeInfo n1;
		struct NodeInfo n2;
		it1 = (*it).begin() ;
		n1 = *it1 ;
		++it1 ;
		n2 = *it1 ;

		unsigned char portS[20] ;
		sprintf((char *) portS, "%d", n1.portNo) ;
		fputs((char *)portS, fp) ;
		fputs(" -d ", fp) ;

		memset(portS, '\0', 20) ;
		sprintf((char *)portS, "%d", n2.portNo) ;
		fputs((char *)portS, fp) ;


		if (isBeaconNode(n1) && isBeaconNode(n2) ){
			fputs(" -c blue\n", fp) ;
		}
		else{
			fputs(" -c black\n", fp) ;
		}
	}



	fflush(fp) ;
	fclose(fp) ;
	statusResponse.erase( statusResponse.begin(), statusResponse.end()) ;
	pthread_mutex_unlock(&statusMsgLock) ;

	printf("[Status] Status file written\n") ;
}


// Method to flood the status requests
void getStatus(){

	printf("[Status] +++++++> Gather status info..\n");
	unsigned char uoid[SHA_DIGEST_LENGTH] ;
	GetUOID( const_cast<char *> ("msg"), uoid, sizeof(uoid)) ;
	//			memcpy((char *)&header[1], uoid, 20) ;


	struct CachePacket pk;
	pk.status = 0 ;
	pk.msgLifetimeInCache = metadata->msgLifeTime;
	pk.status_type = 0x01 ;


	pthread_mutex_lock(&msgCacheLock) ;
		MessageCache[string((const char *)uoid, SHA_DIGEST_LENGTH) ] = pk ;
	pthread_mutex_unlock(&msgCacheLock) ;

	//sending the status request message to all of it's neighbor
	pthread_mutex_lock(&neighbourNodeMapLock) ;
	for(map<struct NodeInfo, int>::iterator it = neighbourNodeMap.begin(); it != neighbourNodeMap.end() ; ++it){
		struct Message statusMsg ;
		statusMsg.msgType = 0xac ;
		statusMsg.status = 2 ;
		statusMsg.ttl = metadata->ttl ;
		statusMsg.status_type = 0x01 ;
		for (int i=0 ; i < SHA_DIGEST_LENGTH ; i++)
			statusMsg.uoid[i] = uoid[i] ;
		safePushMessageinQ("Status", (*it).second, statusMsg) ;
	}
	pthread_mutex_unlock(&neighbourNodeMapLock) ;

	printf("[Status] +++++++> Done gathering status info..\n");
}




/**
 *  Called when the init_neighbours file is not present
 **/
void performJoinNetworkPhase() {

	printf("[JoinPhase]\tStarting JOIN phase\n");

	int beaconConnectSocket = -1;
	for(list<struct NodeInfo *>::iterator it = metadata->beaconList->begin(); it != metadata->beaconList->end(); it++){

		printf("[JoinPhase]\t....Begin of join phase loop\n");
		pthread_t readerThread ;

		void *thread_result ;
		NodeInfo* beaconInfo = *it;

		beaconConnectSocket = connectTo("JoinPhase", beaconInfo->hostname, beaconInfo->portNo);

		if(beaconConnectSocket != -1) {

			// connected to beacon

			// load Connection Node Info and beacon node info
			struct ConnectionInfo connInfo;
			struct NodeInfo beaconNode;
			beaconNode.portNo = beaconInfo->portNo ;
			for(unsigned int i =0 ;i<strlen((const char *)(beaconInfo)->hostname);i++)
				beaconNode.hostname[i] = beaconInfo->hostname[i];
			beaconNode.hostname[strlen((const char *)beaconInfo->hostname)]='\0';

			/**
			int mret = pthread_mutex_init(&connInfo.mQLock , NULL) ;
			if (mret != 0){
					perror("Mutex initialization failed");
					writeLogEntry((unsigned char *)"//Mutex initialization failed\n");

			} **/

             /**
                        int cret = pthread_cond_init(&cn.mQCv, NULL) ;
                        if (cret != 0){
                                //perror("CV initialization failed") ;
                                writeLogEntry((unsigned char *)"//CV initialization failed\n");
                        }
			 **/

			fillInfoToConnectionMap( 	beaconConnectSocket , 0,
					ConnectionMap[beaconConnectSocket].keepAliveTimer ,
					metadata->keepAliveTimeOut , 0 ,
					true,
					beaconNode);




			// Push a Join Req type message in the writing queue
			struct Message joinRequestMsg ;
			joinRequestMsg.msgType = 0xfc ;
			joinRequestMsg.status = 0 ;
			joinRequestMsg.ttl = metadata->ttl ;
			safePushMessageinQ("[JoinPhase]", beaconConnectSocket, joinRequestMsg) ;


			// Create reader thread for this connection
			int res = pthread_create(&readerThread, NULL, connectionReaderThread , (void *)beaconConnectSocket);
			if (res != 0) {
				//perror("Thread creation failed");
				writeLogEntry((unsigned char *)"//In Join Network: Read Thread Creation Failed\n");
				exit(EXIT_FAILURE);
			}
			childThreadList.push_front(readerThread);


			// Create writer thread for this connection
			pthread_t writerThread ;
			res = pthread_create(&writerThread, NULL, connectionWriterThread , (void *)beaconConnectSocket);
			if (res != 0) {
				//perror("Thread creation failed");
				writeLogEntry((unsigned char *)"//In Join Network: Write Thread Creation Failed\n");
				exit(EXIT_FAILURE);
			}
			childThreadList.push_front(writerThread);

			break ;

		}
	}

		// if there was a successful beacon connection start a JoinRequest Timer
		if(beaconConnectSocket == -1) {

			fprintf(stderr,"No Beacon node up\n") ;
			writeLogEntry((unsigned char *)"//NO Beacon Node is up, shutting down\n");
			exit(0) ;
		}



		// set off timer for join request
		pthread_t t_timer_thread ;
		int res = pthread_create(&t_timer_thread , NULL , timerThread , (void *)NULL);
		if (res != 0) {
			//perror("Thread creation failed");
			writeLogEntry((unsigned char *)"//In Join Network: Timer Thread Creation Failed\n");
			exit(EXIT_FAILURE);
		}
		childThreadList.push_front(t_timer_thread);



		waitForChildThreadsToFinish("JoinPhase");


		joinTimeOutFlag = 0;


		printf("[JoinPhase]\tJoin process done, Exiting..\n");
		// Sort the output and write them in the file
		FILE *initNeighboursFile = fopen((char*)(((initNeighboursFilePath))), "w");
		if (initNeighboursFile == NULL){

			printf("[JoinPhase]\tIn Join Network: Failed to open Init_Neighbor_list file\n");
			writeLogEntry((unsigned char *)"//In Join Network: Failed to open Init_Neighbor_list file\n");
			exit(EXIT_FAILURE);
		}



		// exit if the join responses are less than # of init neighbours
		printf("[JoinPhase] About to write to file ... joinResponses size : %d, initNeighbours : %d\n",
					joinResponses.size(), metadata->initNeighbor);

		if(joinResponses.size() < metadata->initNeighbor){

			printf("[JoinPhase]\t  Not enough join responses, Exiting! \n");
			writeLogEntry((unsigned char *)"//Failed to locate Init Neighbor number of nodes\n");
			//                fclose(f_log);
			exit(0);
		}



		writeRespondingNeighboursToInitFile(initNeighboursFile);


		fflush(initNeighboursFile);
		fclose(initNeighboursFile);


		safelyEmptyAllNeighbours() ;


		childThreadList.clear();
		printf("[JoinPhase]\t..... End of join phase loop\n");
}
