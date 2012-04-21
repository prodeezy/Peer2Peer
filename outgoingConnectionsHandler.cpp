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

#define CONN_SOCKET_MSG_Q(socket) 		ConnectionMap[socket].MessageQ

int makeTCPPipe(UCHAR *hostName, unsigned int portNum ){

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
		//printf("[%s]\tError in getaddrinfo() function\n", context);
		exit(0);
	}
	if ((connSocket = socket(server_info->ai_family, server_info->ai_socktype,server_info->ai_protocol)) == -1) {
		perror("\tClient: socket");
		exit(0);
	}
	status = connect(connSocket, server_info->ai_addr, server_info->ai_addrlen);
	if (status >= 0) {
		doLog((UCHAR*)"TCP Connection created\n");
		return connSocket;

	} else {

		close(connSocket);
		return -1 ;

	}

}


 string toStringMetaData(struct FileMetadata metadata) {

    string strMetadata;

    strMetadata.append("[metadata]");
    strMetadata.append("\nFileName=");    
    strMetadata.append((char *)metadata.fName);

    strMetadata.append("\nFileSize=");
    int strLen = strMetadata.size();
    strMetadata.resize(strLen+4) ;
//    sprintf(&strMetadata[strLen], "%d", metadata.fSize);

    
    sprintf(&strMetadata[strLen], "%d\n",  metadata.fSize);
    strMetadata.resize(strlen(strMetadata.c_str())) ;
                
        printf("[Writer] metadata str ........ %s", strMetadata.c_str());

                strMetadata.append("SHA1=") ;
                strLen = strMetadata.size() ;
                strMetadata.resize(strLen+40) ;
//printf("\n\n");               
                for(int i=0;i<20;i++)
                {
                        sprintf(&strMetadata[strLen+i*2], "%02x", metadata.SHA1[i]);
                        //printf("%02x", metadata.sha1[i]);
                }

//printf("\n\n");
                strMetadata.append("\nNonce=") ;
                strLen = strMetadata.size() ;
                strMetadata.resize(strLen+40) ;
                for(int i=0;i<20;i++) 
                        sprintf(&strMetadata[strLen+i*2], "%02x", metadata.NONCE[i]);
                                
                strMetadata.append("\nKeywords=") ;
                strLen = strMetadata.size() ;
                for (list<string >::iterator it = metadata.keywords->begin(); it != metadata.keywords->end(); ++it){
                        strMetadata.append((*it)) ;
                        strMetadata.append(" ") ;
                }
                strMetadata.resize(strMetadata.size() -1) ;

        printf("[Writer] metadata str ........ %s", strMetadata.c_str());
/**
    strMetadata.append("\nKeywords=");
    list<string> keywordList = metadata.keywords;
    for(list<string>::iterator iter=keywordList.begin(); iter!=keywordList.end(); iter++) {

        strMetadata.append((*iter));
        if( iter++ != keywordList.end()) {
            strMetadata.append(" ");
        }
        iter--;
    }
    **/
    strMetadata.append("\nBit-vector=");
    
    strLen = strMetadata.length();
	strMetadata.resize(strLen+256) ;
	for(int i=0;i<128;i++)
		sprintf(&strMetadata[strLen+i*2], "%02x", metadata.bitVector[i]);
	strMetadata.resize(strMetadata.size()+1) ;
	strMetadata[strLen+256] = '\0' ;
	printf("Bit-vector=======>\n");
	for(int f=0;f<128;f++)
		printf("%02x", metadata.bitVector[f]);
    memcpy(&strMetadata[strLen], metadata.bitVector, 128);
    
    printf("[Writer] metadata str : %02x, strlen(metadata):%d\n", strMetadata.c_str(), strlen(strMetadata.c_str()));

    return strMetadata;
}

/**
 * - Decide whether or not to send STORE message to neighbour
 * - create message from metadata and filename (NOT file data)
 * - Create a string version of metadata,
 **/
void fireSTORERequest(struct FileMetadata fileMetadata, char *fileName) {


	string strMetaData = toStringMetaData(fileMetadata);
	int metadataLen = strMetaData.length();
	int fileNameLength = strlen(fileName);

	printf("[Writer] FIRE=> STORE to neighbours, fileName:%s, \n", fileName);
	printf("[Writer] \t metadataLen:%d , strMetaLen:%d , strMetadata : %s\n", metadataLen, strMetaData.length(), strMetaData.c_str());
	printf(" === Bit Vector ===\n");
	for(int i=0;i<128;i++)
		printf("%02x", fileMetadata.bitVector[i]);
	printf("\n");
	
	struct CachePacket packet;
	packet.msgLifetimeInCache = metadata->msgLifeTime;
	packet.status=0;

	// generate UOID
	UCHAR uoid[SHA_DIGEST_LENGTH] ;
	GetUOID( const_cast<char *>("msg"), uoid, SHA_DIGEST_LENGTH,"fireSTORERequest") ;



	string cacheKey = string((const char *)uoid, SHA_DIGEST_LENGTH);
	LOCK_ON(&msgCacheLock);

		MessageCache[cacheKey] = packet;

	LOCK_OFF(&msgCacheLock);



	LOCK_ON(&connectionMapLock);

		for(NEIGHBOUR_MAP_ITERATOR iter = currentNeighbours.begin(); iter != currentNeighbours.end() ; iter++) {

			int connSocket = (*iter).second;
			double coinFlip = (double)drand48();

			if(coinFlip <= metadata->neighborStoreProb) {

				printf("[Writer] \t Send STORE to neighbour : %d\n", connSocket);
				// create message and send to neighbour
				struct Message msg;

				// set uoid
				memcpy(msg.uoid, uoid, SHA_DIGEST_LENGTH);

				// set file name and metadata
				msg.metadata = (UCHAR *)malloc(metadataLen+1);
				strncpy((char *)msg.metadata , strMetaData.c_str(), metadataLen);
				strncpy((char *)msg.fileName , fileName, fileNameLength);

				msg.metadata[metadataLen] 	 = '\0';
				msg.fileName[fileNameLength] = '\0';

				msg.status 	= 3;
				msg.msgType	= STORE_REQ;
				msg.ttl    	= metadata->status_ttl;

				msg.fileNameLength = fileNameLength+1;
				msg.metadatLen = metadataLen+1;

				printf("[STORE] metadataLen=%d\n", metadataLen);

				safePushMessageinQ(connSocket , msg,"fireSTORERequest");

			} else {

				printf("[STORE] Skipping sending STORE to neighbour, not Probable coinFlip:%f, neighStoreProb:%f \n",
								coinFlip, metadata->neighborStoreProb);
			}
		}

	LOCK_OFF(&connectionMapLock);
}

void *connectionWriterThread(void *args) {

	UCHAR *buffer ;
	uint32_t bufferLength = 0 ;
	FILE *fileRef;
	struct Message outgoingMsg ;
	//struct stat st ;
	struct stat fileStat;
	UCHAR header[HEADER_SIZE] ;
	MEMSET_ZERO(header,HEADER_SIZE);
	long connSocket = (long)args ;


	printf("[Writer] ++++++ NEW Thread started[%ld] ++++++ \n", connSocket);

	while(!globalShutdownToggle){

		int bytesWrittenTillNow=0;

		LOCK_OFF(&ConnectionMap[connSocket].mQLock);

		bool isConnectionShutting = false;
		int msgQSize =0;
		LOCK_ON(&connectionMapLock) ;
			isConnectionShutting = ConnectionMap[connSocket].shutDown;
			msgQSize = CONN_SOCKET_MSG_Q(connSocket).size();
		LOCK_OFF(&connectionMapLock) ;

		if(isConnectionShutting) 
		{

			//printf("[Writer-%d]\tShutdown connection [%d] break from connection writer thread\n",connSocket, connSocket);
			break;
		}

		if(msgQSize > 0){


			LOCK_ON(&ConnectionMap[connSocket].mQLock) ;

				//printf("[Writer] LOCK_ON1(%d)\n",connSocket);
				//printf("[Writer] pick next outgoing message ...\n");
				outgoingMsg = CONN_SOCKET_MSG_Q(connSocket).front() ;
				CONN_SOCKET_MSG_Q(connSocket).pop_front() ;

			LOCK_OFF(&ConnectionMap[connSocket].mQLock) ;
			//printf("[Writer] LOCK_OFF1(%d)\n",connSocket);

		} else {

			//printf("[Writer] messageQ is empty!\n");
			bool doBreak = false;
			bool doContinue = false;


			LOCK_ON(&ConnectionMap[connSocket].mQLock) ;
				//printf("[Writer] LOCK_ON2(%d)\n",connSocket);

			//	outgoingMsg = CONN_SOCKET_MSG_Q(connSocket).front() ;
				if(CONN_SOCKET_MSG_Q(connSocket).size() == 0) {

					//sleep(1);
					doContinue = true;
				} else {

					outgoingMsg = CONN_SOCKET_MSG_Q(connSocket).front() ;
					CONN_SOCKET_MSG_Q(connSocket).pop_front() ;
				}
			LOCK_OFF(&ConnectionMap[connSocket].mQLock) ;
			//printf("[Writer] LOCK_OFF2(%d)\n",connSocket);

			if(doContinue){
				//printf("[Writer] ... Continue ...\n");
				continue;
			}

		}
		if (ConnectionMap[connSocket].shutDown) {

			printf("[Writer]shutdown break\n");
			printf("[Writer] LOCK_OFF(%d)\n",connSocket);
			LOCK_OFF(&ConnectionMap[connSocket].mQLock) ;
			break;
		}

		printf("[Writer]--------------- Write a message from Q\n");
		fflush(stdout);

		// Message originated from here
		switch(outgoingMsg.status) {
			case 0 :

				//printf("[Writer] .... Here 0.5\n");
				fflush(stdout);

				UCHAR uoid[SHA_DIGEST_LENGTH] ;
				GetUOID( const_cast<char *> ("msg"), uoid, sizeof(uoid),"connectionWriterThread") ;

				//printf("[Writer] .... Here after GetUOID , uoid:%s \n", uoid);
				fflush(stdout);

				struct CachePacket packet;
				packet.msgLifetimeInCache = metadata->lifeTimeOfMsg;
				packet.status = 0 ;

				//printf("[Writer] .... Here after set msgLifeTime\n");
				fflush(stdout);

				//string strUOID = ;
				//printf("[Writer] .... strUOID : %s\n", strUOID.c_str());
				fflush(stdout);

				printf("[Writer] \t save packet in cache\n");
				LOCK_ON(&msgCacheLock);
					MessageCache[string((const char *)uoid, SHA_DIGEST_LENGTH)] = packet;
				LOCK_OFF(&msgCacheLock);

				// put uoid into header
				MEMSET_ZERO(header, HEADER_SIZE) ;
				memcpy(header+1, uoid, SHA_DIGEST_LENGTH);

				//printf("[Writer] .... Here1\n");
				fflush(stdout);

				break;

			case 1 :

				//printf("[Writer] .... Here1.5\n");
				fflush(stdout);
				MEMSET_ZERO(header, HEADER_SIZE) ;
				memcpy(header+1, outgoingMsg.uoid, SHA_DIGEST_LENGTH);

				//printf("[Writer] .... Here2\n");
				fflush(stdout);

				break;

			case 2 :

				//printf("[Writer] .... Here2.5\n");
				fflush(stdout);
				MEMSET_ZERO(header, HEADER_SIZE) ;
				memcpy(header+1, uoid, SHA_DIGEST_LENGTH);

				//printf("[Writer] .... Here3\n");
				fflush(stdout);

				break;

			case 3 :

				//printf("[Writer] .... Here3.5\n");
				fflush(stdout);
				MEMSET_ZERO(header, HEADER_SIZE) ;
				memcpy(header+1, uoid, SHA_DIGEST_LENGTH);

				//printf("[Writer] .... Here4\n");
				fflush(stdout);

				break;

		}

		//printf("[Writer] Outgoing msg type switch case ...\n");
		fflush(stdout);

		if (outgoingMsg.msgType == HELLO_REQ)
		{

			UCHAR host[256] ;
			header[21] = 0x01 ;
			header[0] = HELLO_REQ;


			gethostname((char *)host, 256) ;
			host[255] = '\0' ;
			bufferLength = strlen((char *)host) + 2 ;
			memcpy((char *)&header[23], &(bufferLength), 4) ;

			buffer = (UCHAR *)malloc(bufferLength) ;
			memset(buffer, '\0', bufferLength) ;
			memcpy((char *)buffer, &(metadata->portNo), 2) ;
			memcpy(buffer + 2 , host, bufferLength-2);


			//Incrementing Ready STatus
			LOCK_ON(&connectionMapLock) ;
				ConnectionMap[connSocket].isReady +=1;
			LOCK_OFF(&connectionMapLock) ;
		}
		// Sending JOIN Request
		else if (outgoingMsg.msgType == JOIN_REQ)
		{

			////printf("[Writer-%d]\tSending JOIN Request , status : %d\n", connSocket, outgoingMsg.status) ;
			if (outgoingMsg.status == 1){

				buffer = outgoingMsg.buffer ;
				bufferLength = outgoingMsg.dataLen ;
			}
			else{

				UCHAR host[256] ;
				gethostname((char *)host, 256) ;
				host[255] = '\0' ;
				bufferLength = strlen((char *)host) + 6 ;

				buffer = (UCHAR *)malloc(bufferLength) ;
				memset(buffer, '\0', bufferLength) ;
				memcpy((UCHAR *)buffer, &(metadata->distance), 4) ;
				memcpy((UCHAR *)&buffer[4], &(metadata->portNo), 2) ;

				sprintf((char *)&buffer[6], "%s",  host);
			}

			header[0] = 0xfc;
			memcpy((char *)&header[21], &(outgoingMsg.ttl), 1) ;
			header[22] = 0x00 ;
			memcpy((char *)&header[23], &(bufferLength), 4) ;

		}
		// Sending Join Response
		else if (outgoingMsg.msgType == 0xfb)
		{

			//originated from somewhere else, just need to forward the message
			if (outgoingMsg.status == 1){

				////printf("[Writer-%d]\tForwarding join response\n" , connSocket) ;
				buffer = (UCHAR *)malloc(outgoingMsg.dataLen) ;
				for (int i = 0 ; i < outgoingMsg.dataLen ; i++)
					buffer[i] = outgoingMsg.buffer[i] ;
				bufferLength = outgoingMsg.dataLen ;
			}
			else{

				// Originated from here, needs to send to destinations
				UCHAR host[256] ;
				gethostname((char *)host, 256) ;
				host[255] = '\0' ;
				bufferLength = strlen((char *)host) + 26 ;
				buffer = (UCHAR *)malloc(bufferLength) ;
				memset(buffer, '\0', bufferLength) ;

				for(unsigned int i = 0;i<20;i++){
					buffer[i] = outgoingMsg.uoid[i];

				}

				memcpy(&buffer[20], &(outgoingMsg.distance), 4) ;
				memcpy(&buffer[24], &(metadata->portNo), 2) ;
				sprintf((char *)&buffer[26], "%s",  host);
			}

			header[0] = JOIN_RSP;
			memcpy((char *)&header[21], &(outgoingMsg.ttl), 1) ;
			header[22] = 0x00 ;
			memcpy((char *)&header[23], &(bufferLength), 4) ;
		}
		else if (outgoingMsg.msgType == KEEPALIVE)
		{
			// Keepalive request being sent

			bufferLength = 0;
			memcpy((char *)&header[23], &(bufferLength), 4) ;

			header[21] = 0x01;
			header[0]  = KEEPALIVE;
			header[22] = 0x00 ;

			//buffer = (UCHAR *)malloc(0) ;
			//MEMSET_ZERO(buffer, 0) ;

			//printf("[Writer-%d]\t Sending KeepAlive request from : %d\n", (int)connSocket, (int)connSocket) ;
		}
		// Status Message request
		else if (outgoingMsg.msgType == STATUS_REQ)
		{

			printf("[Writer] Sending STATUS REQ to %d\n" , connSocket);

			if (outgoingMsg.status == 1){

				printf("[Writer] ...status = 1 \n");
				//originated from elsewhere
				bufferLength = outgoingMsg.dataLen ;
				buffer = outgoingMsg.buffer ;
			}
			else{

				printf("[Writer] ...status = %d \n", outgoingMsg.status);
				buffer = (UCHAR *)malloc(1) ;
				MEMSET_ZERO(buffer, 1) ;
				buffer[0] = outgoingMsg.statusType ;
				bufferLength = 1 ;
			}

			printf("[Writer] .... prepare header\n");
			header[22] = 0x00 ;
			header[0] = STATUS_REQ;
			memcpy((char *)header + 23, &(bufferLength), 4) ;
			memcpy((char *)header + 21, &(outgoingMsg.ttl), 1) ;
			printf("*****The buffer length in header is:%c",header[23]);

		}
		else if (outgoingMsg.msgType == NOTIFY)
		{

			buffer = (UCHAR *)malloc(1) ;
			MEMSET_ZERO(buffer, 1) ;

			//buffer[0] = outgoingMsg.errorCode;
			header[0] = NOTIFY;
			header[21] = 0x01 ;

			bufferLength = 1;
			memcpy((char *)(header + 23), &(bufferLength), 4) ;

		}
		else if (outgoingMsg.msgType == STATUS_RSP)
		{

			printf("[Writer] Sending STATUS RESPONSE to %d\n" , connSocket);
			if (outgoingMsg.status == 1){

				printf("[Writer] status = 1\n");
				buffer = (UCHAR *)malloc(outgoingMsg.dataLen) ;
				memcpy(buffer, outgoingMsg.buffer, outgoingMsg.dataLen);
				bufferLength = outgoingMsg.dataLen ;

			}
			else
			{

				printf("[Writer] status != 1\n");

				UCHAR host[256] ;
				gethostname((char *)host, 256) ;
				host[255] = '\0' ;

				UINT lenth = strlen((char *)host) + 24 ;
				bufferLength  =  lenth ;

				buffer = (UCHAR *)malloc(lenth) ;
				MEMSET_ZERO(buffer, lenth) ;
				memcpy(buffer, outgoingMsg.uoid, 20);

				uint16_t templen = lenth - 22 ;
				memcpy(buffer + 24 , host, lenth);
				memcpy(buffer + 22 , &(metadata->portNo), 2) ;
				memcpy(buffer + 20, &(templen), 2) ;

				for (int i = 24 ; i < (int)lenth ; ++i)
					buffer[i] = host[i-24] ;

				if(outgoingMsg.statusType == 0x01)
				{

					printf("[Writer]Send STATUS RESPONSE to all neighbours....\n");
					for(NEIGHBOUR_MAP::iterator currNeighIter = currentNeighbours.begin()
							; currNeighIter != currentNeighbours.end()
							; ++currNeighIter) {


						unsigned int lengthTwo = strlen((char *)((*currNeighIter).first.hostname)) + 2  ;
						++currNeighIter ;
						bufferLength += lengthTwo+4 ;
						buffer = (UCHAR *)realloc(buffer, bufferLength) ;


						if ( currNeighIter != currentNeighbours.end() ) {

							memcpy(&buffer[bufferLength-lengthTwo-4], &(lengthTwo), 4);
						}
						else{

							UINT tempLengthTwo = 0 ;
							memcpy(&buffer[bufferLength-lengthTwo-4], &(tempLengthTwo), 4);
						}

						--currNeighIter ;

						memcpy(&buffer[bufferLength-lengthTwo], &((*currNeighIter).first.portNo ), 2) ;

						for (int i = bufferLength - lengthTwo + 2 ; i < (int)bufferLength ; ++i) {
							buffer[i] = host[i -(bufferLength-lengthTwo+2)] ;
						}
					}
				}
				else if(outgoingMsg.statusType == 0x02)
				{
					list <int> allMyFileInfo;
					struct FileMetadata fMetadata;
					allMyFileInfo = getAllFileInfo();
					string strFileMetadata("");
					
					list <int>::iterator x = allMyFileInfo.begin();
					
					for(;x!=allMyFileInfo.end();)
					{
						fMetadata = createFileMetadata(*x);
						fileMap[string((char*)fMetadata.fileID,20)]=(*x);
						x++;
						UINT tempLengthTwo = 0 ;
						strFileMetadata=toStringMetaData(fMetadata);
						int newStrSize=strFileMetadata.size();
						lenth=lenth+newStrSize+4;
						buffer=(UCHAR*)realloc(buffer,lenth);
						
						if(x!=allMyFileInfo.end())
						{
							memcpy(&buffer[lenth-newStrSize-4],&(newStrSize),4);
						}
						else
						{
							tempLengthTwo=0;
							memcpy(&buffer[lenth-newStrSize-4], &(tempLengthTwo), 4);
						}
						x--;
						int z=bufferLength-newStrSize;
						for(;z<(int)lenth;)
						{
							buffer[z]=strFileMetadata[z-lenth+newStrSize];
							z++;
						}
					}
				}
			}
			printf("[Writer] build status response header \n");
			header[0] = STATUS_RSP;
			header[22] = 0x00 ;
			memcpy((char *) ( header + 23), &(bufferLength), 4) ;
			memcpy((char *) ( header + 21), &(outgoingMsg.ttl), 1) ;

		} 
		else if (outgoingMsg.msgType == STORE_REQ)
		{

			printf("[Writer]\t STORE outgoing request\n");
			// check if file exists
			
			header[0] = STORE_REQ;
			header[22] = 0x00;			
			
			char *fileName = (char *)outgoingMsg.fileName;
			printf("[Writer]\t Store... opening file:%s\n", fileName);
			fileRef = fopen(fileName, "rb");
			if(fileRef == NULL) {

				printf("[Writer]\t File to store could not be opened\n");
				doLog((UCHAR *)"//File to store could not be opened\n");
				continue;
			}

			//read file and rewing
			fseek (fileRef , 0 , SEEK_END);
			long int lSize = ftell (fileRef);
			rewind (fileRef);

			printf("[Writer]\t Store... seek file size:%ld\n", lSize);
//			char fileBuf[101];
//			int retBytes = fread(fileBuf, 1, 100, fileRef);
//			fileBuf[101]='\0';
//			printf("[Writer]\t Store... file contents{%s}\n", fileBuf);

			// rewing file reference ptr
//			rewind(fileRef);



			if(outgoingMsg.status != 1) {
				// originated here
				printf("[Writer]\t Sending STORE ... Originated here , status:%d \n", outgoingMsg.status);
				// write metadata length
				bufferLength = outgoingMsg.metadatLen + 4;
				uint32_t metadataLen = outgoingMsg.metadatLen;
				MEMSET_ZERO(buffer, bufferLength) ;
				memcpy(buffer, &metadataLen, 4);

				// write metadata
				printf("[Writer]\t Sending STORE .... add to buffer:%d, metadataLen:%d\n",
									strlen((char *)outgoingMsg.metadata), outgoingMsg.metadatLen);
				memcpy((UCHAR *)buffer+4 , outgoingMsg.metadata, outgoingMsg.metadatLen);

				// get file size
				stat(fileName, &fileStat);
				//bufferLength += fileStat.st_size;


			} 
			else 
			{


				// forwarding the message, buffer was already filled by someone
				printf("[Writer]\t Sending STORE ... Forwarding  , status:%d ^^^^^^^^^^^^^^^^^^^^^^ \n", outgoingMsg.status);

				bufferLength = outgoingMsg.dataLen ;
		        buffer = outgoingMsg.buffer ;

		        stat(fileName, &fileStat) ;
		        // special status for writing FILE on network
				outgoingMsg.status = 3;
				//bufferLength += fileStat.st_size ;

			}
			
			int totalStoreMsgLen =  bufferLength + (int)fileStat.st_size;

			memcpy((char *) &header[21], &(outgoingMsg.ttl), 1);
			memcpy((char *) &header[23], &(totalStoreMsgLen), 4);
			
			printf("[Writer]\t Sending Store...  just set header, metadataLen:%d, bufferLength:%d, totalStoreMsgLen=%d\n",
												outgoingMsg.metadatLen, 	bufferLength, 		totalStoreMsgLen);
			
		}
		else if (outgoingMsg.msgType == 0xEC)
		{
			if (outgoingMsg.status == 1)
			{
				buffer = outgoingMsg.buffer ;
				bufferLength = outgoingMsg.metadatLen ;
			}
			else
			{
				bufferLength = outgoingMsg.metadatLen + 1 ;
				buffer = (unsigned char *)malloc(bufferLength) ;
				memset(buffer, '\0', bufferLength) ;
				buffer[0] = outgoingMsg.q_type ;
				for (unsigned int i = 1 ; i < bufferLength ; ++i)
					buffer[i] = outgoingMsg.query[i-1] ;
			}


			header[0] = SEARCH_REQ;


			memcpy((char *)&header[21], &(outgoingMsg.ttl), 1) ;
			header[22] = 0x00 ;
			memcpy((char *)&header[23], &(bufferLength), 4) ;
		}

		printf("[Writer]\t .... Reset KEEP_ALIVE\n");
		fflush(stdout);

		//KeepAlive message sending
		//Resst the keepAliveTimer for this connection
		if( ConnectionMap.find(connSocket) != ConnectionMap.end())
		{
			LOCK_ON(&connectionMapLock) ;
				ConnectionMap[connSocket].keepAliveTimer = metadata->keepAliveTimeOut/2;
			LOCK_OFF(&connectionMapLock) ;
		}

		printf("[Writer]\t .... Here6\n");
		fflush(stdout);


		if (ConnectionMap[connSocket].shutDown)
		{

			printf("[Writer]\t shutdown BREAK\n");
			//printf("[Writer] LOCK_OFF(%d)\n",connSocket);
			LOCK_OFF(&ConnectionMap[connSocket].mQLock);
//			doBreak = true;
			break;
		}

		/*if(doBreak) {
			printf("doBreak occured,hence message not written to socket\n");
			break ;
		}*/

		int return_code = 0 ;

		printf("[Writer] \t... Write header, msgType:%02x \n", header[0]);
		fflush(stdout);

		if(header[0] == 0x00) {

			printf("[Writer] ********* FOUND THE BITCH...... msgType:%02x , status:%d CONTINUE! \n", outgoingMsg.msgType, outgoingMsg.status);
			fflush(stdout);

			continue;
		}

		return_code = (int)write(connSocket, header, HEADER_SIZE) ;
		bytesWrittenTillNow += return_code;

		if (return_code != HEADER_SIZE)
		{
			fprintf(stderr, "Socket Write Error") ;
		}

		//printf("[Writer-%d]\t\tWrote %d bytes of header\n", connSocket, return_code);

		// Send the data on network
		if(outgoingMsg.status == 3){

			// FILE data is chunked
			
			printf("[Writer] \tSending STORE ... bufferLength = (4 + metaData) = %d , file size : %d\n",
					bufferLength, (int)fileStat.st_size);

			int retCode = write(connSocket, buffer, bufferLength) ;
			bytesWrittenTillNow += retCode;


			printf("[Writer] \tSending STORE ... CHUNKING , status:%d, bytes to be read:%d , till now: %d\n",
									outgoingMsg.status, 	(int)fileStat.st_size, 		bytesWrittenTillNow);

			UCHAR fileChunk[1] ;
			int totalBytes = 0;

			//printf("[Writer]\t..... rewind fileRef\n");
//			rewind(fileRef);

            unsigned char chunk[8192] ;
            // read the content of the file and write on the socket
            //while(!feof(fp)){
            while(1){
                    memset(chunk, '\0' , 8192) ;
                    int numBytes = fread(chunk, 1, 8192, fileRef) ;
                    totalBytes += numBytes;
                    printf("[Writer]\t.... read bytes:%d, chunk : (%s)\n", numBytes, chunk);
                    fflush(stdout);

                    if(numBytes == 0)
                            break;
                    write(connSocket, chunk, numBytes) ;
            }
/***

			for(;;) {

				MEMSET_ZERO(fileChunk, 1) ;
				printf("[Writer]\t..... read CHUNK, till now: %d\n", bytesWrittenTillNow);
				fflush(stdout);
				if(fileRef == NULL) {

					printf("[Writer]\t..... fileRef was NULL!");
					fflush(stdout);
					break;
				}
				printf("[Writer]..... about to read 1\n");
				fflush(stdout);
				int numBytes = fread(fileChunk, 1, 1, fileRef) ;
				totalBytes += numBytes;
				if(numBytes == 0) {

					printf("[Writer]..... read 0 bytes BREAK\n");
					fflush(stdout);
					break;
				}
				printf("[Writer]..... write CHUNK, till now: %d\n", bytesWrittenTillNow);
				fflush(stdout);
				retCode = write(connSocket, fileChunk, numBytes) ;
				bytesWrittenTillNow += retCode;
				printf("[Writer]\t..... Writing the [%s] file, file size: %d, Bytes wrote from file on socket: %d , till now:%d\n",
									fileChunk, (int)fileStat.st_size,numBytes, bytesWrittenTillNow);
				fflush(stdout);
			}
**/
			if(fileRef != NULL) {

				printf("[Writer] \t..... CLOSE fileRef\n");
				fflush(fileRef);
				fclose(fileRef);
			}
			printf("[Writer-%d]\tWrote entire STORE message, total bytes:%d, till now:%d\n", connSocket, totalBytes, bytesWrittenTillNow);
			fflush(stdout);
		}
		else
		{

			//printf("[Writer] ... write buffer\n");
			return_code = (int)write(connSocket, buffer, bufferLength) ;
			if (return_code != (int)bufferLength)
			{
				fprintf(stderr, "[Writer]\t Failed to write buffer bytes") ;
			}
			printf("[Writer-%d]\t\tWrote %d bytes of buffer\n", connSocket, return_code);
		}

		
		//logging the message sent from this node or forwarded from this node
		UCHAR *logEntryRecord = NULL;

		if(!(outgoingMsg.msgType == 0xfa && (joinNetworkPhaseFlag || ConnectionMap[connSocket].joinFlag == 1)))
		{

			printf("[Writer]\tcreating LOG ENTRY for sock:%d, header:%s, buf:%s\n", connSocket, header, buffer);

			LOCK_ON(&logEntryLock) ;

				if(! ( outgoingMsg.status== 0 || outgoingMsg.status == 2) )
					logEntryRecord = prepareLogRecord('f', connSocket, header, buffer);
				else
					logEntryRecord = prepareLogRecord('s', connSocket, header, buffer);

				if(logEntryRecord!=NULL) 
				{
					//		//printf("[Reader]\t\tWriting LOG ENTRY\n");
					doLog(logEntryRecord);
				}

			LOCK_OFF(&logEntryLock) ;
		}
		
		// Do some logging
		//printf("[Writer]\t About to free strlen(buffer):%d, bufferLength:%d \n", strlen((char *)buffer), bufferLength);
		fflush(stdout);

		if(bufferLength > 0 && buffer != NULL) {

			free(buffer) ;
		}

		printf("[Writer-%d]\t ...... End of writer loop\n", connSocket);

		sleep(1);

		////printf("[Writer-%d]\t ...... Done sleeping\n", connSocket);
	}

	printf("[Writer] LOCK_OFF(%d)\n",connSocket);
	LOCK_OFF(&ConnectionMap[connSocket].mQLock);

	printf("[Writer-%d]\t********** Leaving write thread \n", connSocket);
	pthread_exit(0);

	////printf("[Writer-%d]\tExited!\n", connSocket);
	return 0;
}

void safelyEmptyAllNeighbours()
{

	//printf("[JoinPhase] safely empty all neighbours\n");
	// Empty all the neighbours
	pthread_mutex_lock(&currentNeighboursLock);
	currentNeighbours.erase(currentNeighbours.begin(), currentNeighbours.end());
	pthread_mutex_unlock(&currentNeighboursLock);
}


void waitForChildThreadsToFinish() {

	void *thread_result;

	// Join the read thread here
	////printf("[Writer]\tJoin spawned threads...");
	for (list<pthread_t >::iterator it = childThreadList.begin(); it != childThreadList.end(); ++it){

		int res = pthread_join((*it), &thread_result);
		if (res != 0) {
			perror("Thread join failed");
			doLog((UCHAR *)"//In Join Network: Thread Joining Failed\n");
			exit(EXIT_FAILURE);
		}
	}
}

void writeRespondingNeighboursToInitFile(FILE *& initNeighboursFile)
{
	//printf("[JoinPhase]\tWrite responded neighbours to init neighbours file\n");

	unsigned int counter = 0;
	for(list<struct JoinResponseInfo>::iterator it = joinResponses.begin();it != joinResponses.end();it++, counter++){
		//                if(counter==metadata->minNeighbor)
		//                        break;
		JoinResponseInfo jresp = (*it);
		UCHAR portBuf[10];
//		printf("[JoinPhase]\tHostname: %s, Port: %d, location: %d\n", jresp.neighbourHostname, jresp.neighbourPortNo, jresp.location);
		fputs((char*)(((jresp.neighbourHostname))), initNeighboursFile);
		fputs(":", initNeighboursFile);
		sprintf((char*)(((portBuf))), "%d", jresp.neighbourPortNo);
		fputs((char*)(((portBuf))), initNeighboursFile);
		fputs("\n", initNeighboursFile);
	}
}

/**
 * Writing the status response to the file, creating the name file
 *
 **/
void serializeStatusResponsesToFile(){

	printf("[Status]\tSerialize to status file \n" ) ;

	bool doNothing = true;
	int intNothing = 0;
	float floatNothing = 0.0f;

	fflush(stdout);


	LOCK_ON(&statusMsgLock) ;

	FILE *statusOutFile = fopen((char *)metadata->statusOutFileName, "a") ;
	if (statusOutFile == NULL){
		//fprintf(stderr, "File open failed\n") ;
		if(doNothing)
			 floatNothing = -7.0f + intNothing;
		else
			 intNothing++;

		printf("[Status] Failed to open STATUS FILE\n");
		doLog((UCHAR *)"//Failed to open STATUS FILE\n");
		intNothing = -1;
		exit(0) ;
	}

	set<struct NodeInfo>::iterator nodeInfoSetIter ;
	set< set<struct NodeInfo> >::iterator probeRespIter = statusProbeResponses.begin();
	set<struct NodeInfo> distinctNodes ;

	double doubleNothing = 5.1525f;

	printf("[Status] Looping through status responses, build distinct nodes\n");
	for (; probeRespIter != statusProbeResponses.end() 	; ) {

		floatNothing = -110.0f;
		nodeInfoSetIter = (*probeRespIter).begin() ;

		doNothing = false;
		if(doNothing)
			 floatNothing = -9 + intNothing;
		else
			 intNothing += 10;

		distinctNodes.insert( *nodeInfoSetIter ) ;

		nodeInfoSetIter++ ;

		floatNothing = -10.0f;
		distinctNodes.insert( *nodeInfoSetIter ) ;

		++probeRespIter;
	}

	printf("[Status]distinct nodes size ...%d \n" , distinctNodes.size());
	intNothing = 0;
	if(distinctNodes.size() == 0)
	{
		printf("");
		fputs("n -t * -s ", statusOutFile) ;

		UCHAR portS[20] ;

		printf("");
		intNothing = 1;
		sprintf((char *)portS, "%d", metadata->portNo) ;

		if(doNothing)
			floatNothing = -9 + intNothing;
		else
			intNothing--;

		doNothing = false;
		fputs((char *)portS, statusOutFile) ;

		if (!metadata->iAmBeacon){

			intNothing = -1;

			doNothing = false;
			fputs(" -c black -i black\n", statusOutFile) ;

			if(doNothing)
				floatNothing = -9 + intNothing;
		} else{

			doNothing = false;
			fputs(" -c blue -i blue\n", statusOutFile) ;
		}
	}

	set< struct NodeInfo >::iterator it = distinctNodes.begin();
	printf("[Status] Looping distinct nodes \n");
	for (; it != distinctNodes.end() ; ){

		printf("[Status]\t write node data \n");
		intNothing = -1;
		doNothing = false;

		fputs("n -t * -s ", statusOutFile) ;

		floatNothing += -1.0f;
		doNothing = false;

		UCHAR portS[20] ;
		sprintf((char *)portS, "%d", (*it).portNo) ;

		doNothing = true;
		fputs((char *)portS, statusOutFile) ;

		doubleNothing = 5.25f;
		if (!isBeaconNode(*it) ){

			fputs(" -c black -i black\n", statusOutFile) ;

		} else{

			fputs(" -c blue -i blue\n", statusOutFile) ;
		}

		++it;
	}

	set< set<struct NodeInfo> >::iterator responseIter = statusProbeResponses.begin();
	while(	responseIter != statusProbeResponses.end() ){

		fputs("l -t * -s ", statusOutFile) ;

		if(doNothing)
			 floatNothing = -9 + intNothing;
		else
			 intNothing += 10;

		struct NodeInfo n2;
		struct NodeInfo n1;

		nodeInfoSetIter = (*responseIter).begin() ;

		n1 = *nodeInfoSetIter ;

		floatNothing = -10.0f;
		intNothing--;

		++nodeInfoSetIter ;
		n2 = *nodeInfoSetIter ;

		UCHAR statusPort[20] ;
		sprintf((char *) statusPort, "%d", n1.portNo) ;

		if(doNothing)
			 floatNothing = -9 + intNothing;
		else
			 intNothing += 10;

		doubleNothing = 3.15f;
		fputs((char *)statusPort, statusOutFile) ;

		if(doNothing)
				floatNothing = -1 + intNothing;

		fputs(" -d ", statusOutFile) ;

		memset(statusPort, '\0', 20) ;
		sprintf((char *)statusPort, "%d", n2.portNo) ;

		if(!doNothing)
			 floatNothing = -9 + intNothing;
		else
			 intNothing += 10;

		fputs((char *)statusPort, statusOutFile) ;


		if (isBeaconNode(n1) && isBeaconNode(n2) ){

			fputs(" -c blue\n", statusOutFile) ;

			if(doNothing)
				doubleNothing = 5.1525f;
		}
		else{

			fputs(" -c black\n", statusOutFile) ;

			if(doNothing)
				floatNothing = -1 + intNothing;
		}

		intNothing++;
		floatNothing--;
		++responseIter;
	}

	if(doNothing)
		 floatNothing = intNothing - 0.05;
	else
		 intNothing += 10;

	printf("[Status] Flush and Close status file\n");
	fflush(statusOutFile) ;
	fclose(statusOutFile) ;


	floatNothing = intNothing - 0.05;
	statusProbeResponses.erase( statusProbeResponses.begin(), statusProbeResponses.end()) ;

	LOCK_OFF(&statusMsgLock) ;

	printf("[Status] ********** Status file written *********\n") ;
}




// Method to flood the status requests
void floodStatusRequestsIntoNetwork(){

	printf("[Status] Gather status info, flood status request..\n");
	UCHAR uoid[SHA_DIGEST_LENGTH] ;
	GetUOID( const_cast<char *> ("msg"), uoid, sizeof(uoid),"floodStatusRequestsIntoNetwork") ;
	//			memcpy((char *)&header[1], uoid, 20) ;


	struct CachePacket pk;
	pk.status = 0 ;
	pk.msgLifetimeInCache = metadata->lifeTimeOfMsg;
	pk.status_type = 0x01 ;

	pthread_mutex_lock(&msgCacheLock);
		MessageCache[string((const char *)uoid, SHA_DIGEST_LENGTH) ] = pk ;
	pthread_mutex_unlock(&msgCacheLock);

	//sending the status request message to all of it's neighbor
	LOCK_ON(&currentNeighboursLock) ;

		printf("[Status] Flood to neighbours ... size:%d\n" , currentNeighbours.size());
		for(NEIGHBOUR_MAP::iterator it = currentNeighbours.begin(); it != currentNeighbours.end() ; ++it){

			printf("[Status]\t send to neighbor -> %d\n", (*it).second);

			struct Message statusMsg ;
			statusMsg.msgType = 0xac ;
			statusMsg.status = 2 ;
			statusMsg.ttl = metadata->ttl ;
			statusMsg.statusType = 0x01 ;
			for (int i=0 ; i < SHA_DIGEST_LENGTH ; i++)
				statusMsg.uoid[i] = uoid[i] ;

			printf("[Status]\t Add to queue to be sent, ttl=%d\n", statusMsg.ttl);
			safePushMessageinQ((*it).second, statusMsg,"floodStatusRequestsIntoNetwork") ;
		}

	LOCK_OFF(&currentNeighboursLock) ;

	printf("[Status] Done flooding status request.\n");
}




void createJReqMsqAndPutInPushQ(int & beaconConnectSocket)
{
	// Push a Join Req type message in the writing queue
	struct Message joinRequestMsg;
	joinRequestMsg.msgType = 0xfc;
	joinRequestMsg.status = 0;
	joinRequestMsg.ttl = metadata->ttl;
	safePushMessageinQ(beaconConnectSocket, joinRequestMsg,"createJReqMsqAndPutInPushQ");
}

/**
 *  Called when the init_neighbours file is not present
 **/
void performJoinNetworkPhase() {

	//printf("[JoinPhase]\tStarting JOIN phase\n");

	int beaconConnectSocket = -1;
	for(NODEINFO_LIST::iterator it = metadata->beaconList->begin(); it != metadata->beaconList->end(); it++){

		//printf("[JoinPhase]\t....Begin of join phase loop\n");
		pthread_t readerThread ;

		//		void *thread_result ;
		NodeInfo* beaconInfo = *it;

		beaconConnectSocket = makeTCPPipe(beaconInfo->hostname, beaconInfo->portNo);

		if(beaconConnectSocket != -1) {

			// connected to beacon

			// load Connection Node Info and beacon node info
			struct ConnectionInfo connInfo;
			struct NodeInfo beaconNode;
			beaconNode.portNo = beaconInfo->portNo ;
			for(unsigned int i =0 ;i<strlen((const char *)((beaconInfo)->hostname));i++)
				beaconNode.hostname[i] = beaconInfo->hostname[i];

			beaconNode.hostname[strlen((const char*)(beaconInfo->hostname))] = '\0';

			fillInfoToConnectionMap(beaconConnectSocket, 0,
									ConnectionMap[beaconConnectSocket].keepAliveTimer,
									metadata->keepAliveTimeOut, 0, true, beaconNode);

			createJReqMsqAndPutInPushQ(beaconConnectSocket) ;


			// Create reader thread for this connection
			int res = pthread_create(&readerThread, NULL, connectionReaderThread , (void *)beaconConnectSocket);
			if (res != 0) {
				//perror("Thread creation failed");
				doLog((UCHAR *)"//In Join Network: Read Thread Creation Failed\n");
				exit(EXIT_FAILURE);
			}
			childThreadList.push_front(readerThread);


			// Create writer thread for this connection
			pthread_t writerThread ;
			res = pthread_create(&writerThread, NULL, connectionWriterThread , (void *)beaconConnectSocket);
			if (res != 0) {
				//perror("Thread creation failed");
				doLog((UCHAR *)"//In Join Network: Write Thread Creation Failed\n");
				exit(EXIT_FAILURE);
			}
			childThreadList.push_front(writerThread);

			break ;

		}
	}

	// if there was a successful beacon connection start a JoinRequest Timer
	if(beaconConnectSocket == -1) {

		fprintf(stderr,"No Beacon node up\n") ;
		doLog((UCHAR *)"//NO Beacon Node is up, shutting down\n");
		exit(0) ;
	}



	// set off timer for join request
	pthread_t t_timer_thread ;
	int res = pthread_create(&t_timer_thread , NULL , general_timer , (void *)NULL);
	if (res != 0) {
		perror("Thread creation failed");
		doLog((UCHAR *)"//In Join Network: Timer Thread Creation Failed\n");
		exit(EXIT_FAILURE);
	}
	childThreadList.push_front(t_timer_thread);



	blockForChildThreadsToFinish();


	joinTimeOutFlag = 0;


	//printf("[JoinPhase]\tJoin process done, Exiting..\n");
	// Sort the output and write them in the file
	FILE *initNeighboursFile = fopen((char*)(((initNeighboursFilePath))), "w");
	if (initNeighboursFile == NULL){

		//printf("[JoinPhase]\tIn Join Network: Failed to open Init_Neighbor_list file\n");
		doLog((UCHAR *)"//In Join Network: Failed to open Init_Neighbor_list file\n");
		exit(EXIT_FAILURE);
	}



	// exit if the join responses are less than # of init neighbours
	//printf("[JoinPhase] About to write to file ... joinResponses size : %d, initNeighbours : %d\n",
	//					joinResponses.size(), metadata->initNeighbor);

	if(joinResponses.size() < metadata->initNeighbor){

		//printf("[JoinPhase]\t  Not enough join responses, Exiting! \n");
		doLog((UCHAR *)"//Failed to locate Init Neighbor number of nodes\n");
		                //fclose(f_log);
		exit(0);
	}



	writeRespondingNeighboursToInitFile(initNeighboursFile);


	fflush(initNeighboursFile);
	fclose(initNeighboursFile);


	safelyEmptyAllNeighbours() ;


	childThreadList.clear();
	//printf("[JoinPhase]\t..... End of join phase loop\n");
}
