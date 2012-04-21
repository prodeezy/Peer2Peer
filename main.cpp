#include "main.h"

#define min(X,Y)		(((X)>(Y))	?	(Y)		:		(X))

#define BACKLOG 5

MetaData *metadata;
map<int, struct ConnectionInfo> ConnectionMap;
int nodeProcessId;
NEIGHBOUR_MAP currentNeighbours;
bool joinNetworkPhaseFlag= false;
UCHAR logFilePath[512];
UCHAR initNeighboursFilePath[512];
int acceptProcessId = -1;
int joinTimeOutFlag = 0;
pthread_mutex_t connectionMapLock ;
int acceptServerSocket = 0;
map<string, struct CachePacket> MessageCache;
list<int> cacheLRU;
set< set<struct NodeInfo> > statusProbeResponses;
list<struct JoinResponseInfo> joinResponses;
list<pthread_t > childThreadList;
int statusTimerFlag = 0;
pthread_cond_t statusMsgCV;
pthread_mutex_t statusMsgLock ;
pthread_mutex_t logEntryLock;
int checkTimerFlag = 0 ;
pthread_mutex_t msgCacheLock ;
pthread_mutex_t currentNeighboursLock ;
bool globalShutdownToggle = 0 ;
map<string, list<int> > BitVectorIndexMap;
map<string, list<int> > FileNameIndexMap;
map<string, list<int> > SHA1IndexMap;
map<string, int> fileIDMap;
struct NodeInfo n;
int cacheSizeReached = 0;
pthread_mutex_t searchMsgLock;
pthread_cond_t searchMsgCV;
pthread_mutex_t countOfSearchResLock;
int countOfSearchRes=0;
map<int, struct FileMetadata> fileDisplayIndexMap;
map<struct NodeInfo, list<string> >statusFilesResponsesOfNodes;
map<string,int>fileMap;


/***
 ***********************************
 * 			  Logger
 ***********************************
 **/

UCHAR msg_type[5];
UCHAR data[256];
uint32_t data_len=0;
unsigned char log_entry[512];
FILE *loggerRef = NULL;


void getNeighbor(int sockfd)
{
	bool doNothing = false;
	int iNothing = 10;
	LOCK_ON(&connectionMapLock);

		if(doNothing)
			iNothing = 0;
		n = ConnectionMap[sockfd].neighbourNode;
	LOCK_OFF(&connectionMapLock);
}

void decideBasedOnMode(UCHAR mode, struct timeval tv, uint8_t ttl, UCHAR uoid[4]) {

	bool doNothing = false;
	int iNothing = 10;
	float fNothing = 0.5f;

	if (mode == 's') {

			fNothing = 0.5f;
			//log for messages sent
			sprintf((char *)log_entry, "%c %10ld.%03d %s_%d %s %d %d %02x%02x%02x%02x %s\n",
							mode, tv.tv_sec, (int)(tv.tv_usec/1000), (char *)n.hostname, n.portNo,
							(char *)msg_type, (data_len + HEADER_SIZE),  ttl, uoid[0], uoid[1],
							uoid[2], uoid[3], (char *)data);
	}else if (mode == 'r') {

			iNothing = 10;
			// logging for read mode
			sprintf((char *)log_entry, "%c %10ld.%03d %s_%d %s %d %d %02x%02x%02x%02x %s\n",
							mode, tv.tv_sec, (int)(tv.tv_usec/1000), (char *)n.hostname, n.portNo,
							(char *)msg_type, (data_len + HEADER_SIZE),  ttl, uoid[0], uoid[1],
							uoid[2], uoid[3], (char *)data);
	}else {

			doNothing=true;
			// log for messages forwarded
			sprintf((char *)log_entry, "%c %10ld.%03d %s_%d %s %d %d %02x%02x%02x%02x %s\n",
							mode, tv.tv_sec, (int)(tv.tv_usec/1000), (char *)n.hostname, n.portNo,
							(char *)msg_type, (data_len + HEADER_SIZE),  ttl, uoid[0], uoid[1],
							uoid[2], uoid[3], (char *)data);
	}

	fflush(loggerRef);
}

//set buffer according to message type
void setData(uint8_t m_type, UCHAR *buffer)
{
	//printf("[Log]\t\t ... Set data");

	//UCHAR portNo[2];
	unsigned short int portNo;
	memset(&portNo, '\0', sizeof(portNo));
	UCHAR uoid[4];
	memset(&uoid, '\0', sizeof(uoid));
	uint32_t distance;
	memset(&distance, '\0', sizeof(distance));
	UCHAR hostName[256];
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



void messageType(uint8_t m_type)
{

	MEMSET_ZERO(&msg_type , 4);

	//printf("[Log]\t\t..begin message type, m_type:%#x\n", m_type);

	const char *temporaryMessageType;

	if(m_type == 0xf7) {

			temporaryMessageType ="NTFY";

	}else if(m_type == 0xf8) {

			temporaryMessageType ="KPAV";

	}else if(m_type == 0xac) {

			temporaryMessageType ="STRQ";

	}else if(m_type == 0xfb ) {

			temporaryMessageType ="JNRS";

	}else if(m_type == 0xfa) {


			temporaryMessageType ="HLLO";

	}else if(m_type == 0xfc) {

			temporaryMessageType = "JNRQ";

	}else if(m_type == 0xf5) {

			temporaryMessageType ="CKRS";

	}else if(m_type == 0xab) {

			temporaryMessageType ="STRS";

	}else if(m_type == 0xf6) {

			temporaryMessageType ="CKRQ";

	}


	for(unsigned int i=0;i<4;i++)
		msg_type[i]=temporaryMessageType[i];

	msg_type[4] = '\0';


}

UCHAR *prepareLogRecord(UCHAR mode,
								int sockfd,
								UCHAR header[],
								UCHAR *logBufferObj)
{
	bool doNothing = false;
	int intNothing=0;


	UCHAR uoid[4];
	uint8_t message_type=0;

	MEMSET_ZERO(&uoid, 4);
	MEMSET_ZERO(&msg_type, 4);

	float floatNothing = 1.0f;
	double doubleNothing = 0.5f;

	uint8_t ttl=0;
	UCHAR hostname[256];

	MEMSET_ZERO(&data, sizeof(data));
	MEMSET_ZERO(&hostname, sizeof(hostname));

	//printf("[Log]\t\tCreate log record buffer:isNull?%s, header:isNull?%s\n"
	//					, (buffer==NULL)?"YES":"NO", (header==NULL)?"YES":"NO"  );

	MEMSET_ZERO(&log_entry, sizeof(log_entry));

	struct timeval tv;
	memset(&tv, NULL, sizeof(tv));

	//struct node *n;
	MEMSET_ZERO( &n, sizeof(n) );
	gettimeofday(&tv, NULL);
	memcpy(&message_type, header, 1);

	if(doNothing)
		doubleNothing++;
	else

		intNothing--;
	memcpy(uoid, header+17, 4);
	doubleNothing++;
	memcpy(&data_len,  &header[23], 4);
	intNothing--;
	memcpy(&ttl,       &header[21], 1);


	messageType(message_type);

	//gets the hostname and port no of neighbor
	//printf("[Log]\t\tGet neighbour, uoid:%s , message_type:%ud\n", uoid, message_type);
	getNeighbor(sockfd);

	if(!doNothing)
		doubleNothing+=0.5;
	else
		intNothing-=1;

	setData(message_type, logBufferObj);


	if(!doNothing)
		doubleNothing+=0.5;
	else
		intNothing-=1;


	if(data != NULL) {


		if(!doNothing)
			doubleNothing+=0.5;
		else
			intNothing-=10;


		decideBasedOnMode(mode, tv, ttl, uoid);

	} else {

		return NULL;
	}

	if(!doNothing)
		doubleNothing -=0.5;
	else
		intNothing +=1;

    //printf("[Log]\t\tReturn log entry\n");
	return log_entry;
}



//this functions writes the passed string into the log entry
void doLog(UCHAR *tobewrittendata)
{
	bool doFlag = false;

	//printf("[Log]\treached here %s\n",tobewrittendata);
	fprintf(loggerRef, "%s", tobewrittendata);

	int iNoth=10;
	if(doFlag) {
		iNoth--;
	}

	fflush(loggerRef);
}

//**********************************************

//get the UOID of the messages
UCHAR *GetUOID(char *obj_type, unsigned char *uoid_buf, long unsigned int uoid_buf_sz, const char *context){

		static unsigned long seq_no=(unsigned long)1;
        unsigned char sha1_buf[SHA_DIGEST_LENGTH], str_buf[104];

        snprintf((char *)str_buf, sizeof(str_buf), "%s_%s_%1ld", metadata->idNodeInstance, obj_type, (long)seq_no++);
        SHA1(str_buf, strlen((const char *)str_buf), sha1_buf);
        memset(uoid_buf, 0, uoid_buf_sz);
        memcpy(uoid_buf, sha1_buf,min(uoid_buf_sz,sizeof(sha1_buf)));

        printf("*********** NEW UOID :[%s] , type=[%s] , creator=[%s] \n" , uoid_buf , obj_type, context);

        return uoid_buf;
}

/***
UCHAR *GetUOID(char *obj_type,
			   UCHAR *uoid_buf,
			   long unsigned int uoid_buf_sz,
			   const char *context)
{

	UCHAR strBuf[104];
	UCHAR sha1_buf[SHA_DIGEST_LENGTH];

	static unsigned long seqNum=(unsigned long)1;

	MEMSET_ZERO(strBuf, 104);
	snprintf((char *)strBuf, sizeof(strBuf), "%s_%s_%1ld", metadata->idNodeInstance, obj_type, (long)seqNum++);
	SHA1(strBuf, strlen((const char *)strBuf), sha1_buf);

	MEMSET_ZERO(uoid_buf, uoid_buf_sz);
	memcpy(uoid_buf, sha1_buf, min(uoid_buf_sz,sizeof(sha1_buf)));


	printf("*********** NEW UOID :[%s] , type=[%s] , creator=[%s] \n" , uoid_buf , obj_type, context);

	return uoid_buf;
}
**/

void initializeLocks()
{
	int ret = pthread_mutex_init(&connectionMapLock, NULL) ;
	if(ret != 0){
		perror("Mutex initialization failed");
		//doLog((UCHAR *)"//Mutex initialization failed\n");
	}
	ret = pthread_mutex_init(&msgCacheLock, NULL) ;
	if(ret != 0){
		perror("Mutex initialization failed");
		//doLog((UCHAR *)"//Mutex initialization failed\n");
	}

	ret = pthread_mutex_init(&logEntryLock, NULL) ;
	if (ret != 0){
		//perror("Mutex initialization failed") ;
		//doLog((UCHAR *)"//Mutex initialization failed\n");
	}

	ret = pthread_mutex_init(&statusMsgLock, NULL) ;
	if (ret != 0){
		//perror("Mutex initialization failed") ;
		//doLog((UCHAR *)"//Mutex initialization failed\n");
	}

	ret = pthread_cond_init(&statusMsgCV, NULL) ;
	if (ret != 0){
		//perror("CV initialization failed") ;
		//doLog((UCHAR *)"//CV initialization failed\n");
	}

	ret = pthread_mutex_init(&searchMsgLock, NULL) ;
	if (ret != 0){
		//perror("Mutex initialization failed") ;
		//doLog((UCHAR *)"//Mutex initialization failed\n");
	}
	
	ret = pthread_cond_init(&searchMsgCV, NULL) ;
	if (ret != 0){
		//perror("Mutex initialization failed") ;
		//doLog((UCHAR *)"//Mutex initialization failed\n");
	}

	ret = pthread_mutex_init(&countOfSearchResLock, NULL) ;
	if(ret != 0){
		perror("Mutex initialization failed");
	}
}

void condense(char* readline)
{
	char temp[512];
	int noOfChar=0;

	for(int i=0;readline[i]!='\0';i++)
	{
		if(readline[i]=='\"')
		{
			while(readline[i]!='\"')
			{
				if(readline[i]=='\0')
				{
					i--;
					break;
				}
				temp[noOfChar++]=readline[i];
				i++;
			}
		}
		else
		{
			if(readline[i]=='\n' || readline[i]==' ' || readline[i]=='\r' || readline[i]=='\t')
				continue;
			else
				temp[noOfChar++]=readline[i];
		}
	}
	temp[noOfChar]='\0';
	strncpy((char *)readline, (char *)temp, strlen((char *)temp));
	readline[strlen((char *)temp)]='\0';
	//printf("The condensed line is %s\n",readline);
	//printf("End of condense reached\n");
}

void parseINIfile(char * fptr)
{
	FILE *fp=fopen((char *)fptr,"r");
	int flags[20];
	memset(&flags, 0, sizeof(flags));

	if(fp!=NULL)
	{
		//writeLog("//.ini file opened successfully\n);
		//printf(".ini file opened successfully\n");
	}
	else
	{
		//writeLog("//.ini file couldnt be opened successfully\n);
		printf(".ini file couldnt opened successfully\n");
		exit(0);
	}

	char readline[512];
	int initphase=0;
	int beaconphase=0;
	char *key, *value;

	while(fgets((char *)readline, 511,fp)!=NULL)
	{
		condense(readline);
		//printf("the line read is %s\n",readline);

		if(!(*readline))
		{
			printf("blank line encountered\n");
			continue;
		}

		//printf("The line is not blank and it is %s\n",readline);
		if(strcasecmp((char*)readline,"[init]")==0)
		{
			//printf("in [init]\n");
			flags[0]++;
			if(flags[0]==1)
			{
				//first [init] section encountered.
				//printf("[init] encountered for first time\n");
				initphase=1;
				beaconphase=0;
				continue;
			}
			else
			{
				//printf("duplicate [init] found\n ignore it");
				initphase=0;
				continue;
			}
			//printf("End of [init]\n");
		}

		else if(strcasecmp((char*)readline,"[beacons]")==0)
		{
			flags[1]++;
			if(flags[1]==1)
			{
				//first [init] section encountered.
				printf("[init] encountered for first time\n");
				initphase=0;
				beaconphase=1;
				continue;
			}
			else
			{
				printf("duplicate [beacon] found\n ignore it");
				beaconphase=0;
				continue;
			}
		}

		else
		{
			//printf("Reached the else part\n");
			//body of either of the two sections.
			//break into tokens
			key=(char*)strtok((char *)readline,"=");

			//printf("key is %s.\n",key);
			if((strcasecmp((char*)key,"Port")==0) && flags[2]==0 && initphase==1 && beaconphase==0)
			{
				flags[2]=1;
				value=(char*)strtok((char *)NULL,"=");
				metadata->portNo=atoi((char*)value);
				continue;
			}
			else if((strcasecmp((char*)key,"Location")==0) && flags[3]==0 && initphase==1 && beaconphase==0)
			{
				flags[3]=1;
				value=(char*)strtok((char *)NULL,"=");
				metadata->distance=atol((char*)value);
				continue;
			}
			else if((strcasecmp((char*)key,"HomeDir")==0) && flags[4]==0 && initphase==1 && beaconphase==0)
			{
				flags[4]=1;
				value=(char*)strtok((char *)NULL,"=");
				strncpy((char*)metadata->currWorkingDir,(char*)value, strlen((char*)value));
				metadata->currWorkingDir[strlen((char*)value)]='\0';
				continue;
			}
			else if((strcasecmp((char*)key,"LogFilename")==0) && flags[5]==0 && initphase==1 && beaconphase==0)
			{
				flags[5]=1;
				value=(char*)strtok((char *)NULL,"=");
				strncpy((char *)metadata->loggerFile,(char*)value,strlen((char*)value));
				metadata->loggerFile[strlen((char*)value)]='\0';
				continue;
			}
			else if((strcasecmp((char*)key,"AutoShutdown")==0) && flags[6]==0 && initphase==1 && beaconphase==0)
			{
				flags[6]=1;
				value=(char*)strtok((char *)NULL,"=");
				metadata->autoShutDown=atoi((char*)value);
				continue;
			}
			else if((strcasecmp((char*)key,"TTL")==0) && flags[7]==0 && initphase==1 && beaconphase==0)
			{
				flags[7]=1;
				value=(char*)strtok((char *)NULL,"=");
				metadata->ttl=atoi((char*)value);
				continue;
			}
			else if((strcasecmp((char*)key,"MsgLifetime")==0) && flags[8]==0 && initphase==1 && beaconphase==0)
			{
				flags[8]=1;
				value=(char*)strtok((char *)NULL,"=");
				metadata->lifeTimeOfMsg=atoi((char*)value);
				continue;
			}
			else if((strcasecmp((char*)key,"GetMsgLifetime")==0) && flags[9]==0 && initphase==1 && beaconphase==0)
			{
				flags[9]=1;
				value=(char*)strtok((char *)NULL,"=");
				metadata->msgLifeTime=atoi((char*)value);
				continue;
			}
			else if((strcasecmp((char*)key,"InitNeighbors")==0) && flags[10]==0 && initphase==1 && beaconphase==0)
			{
				flags[10]=1;
				value=(char*)strtok((char *)NULL,"=");
				metadata->initNeighbor=atoi((char*)value);
				continue;
			}
			else if((strcasecmp((char*)key,"JoinTimeout")==0) && flags[11]==0 && initphase==1 && beaconphase==0)
			{
				flags[11]=1;
				value=(char*)strtok((char *)NULL,"=");
				metadata->joinTimeOut=atoi((char*)value);
				continue;
			}
			else if((strcasecmp((char*)key,"KeepAliveTimeout")==0) && flags[12]==0 && initphase==1 && beaconphase==0)
			{
				flags[12]=1;
				value=(char*)strtok((char *)NULL,"=");
				metadata->keepAliveTimeOut=atoi((char*)value);
				continue;
			}
			else if((strcasecmp((char*)key,"MinNeighbors")==0) && flags[13]==0 && initphase==1 && beaconphase==0)
			{
				flags[13]=1;
				value=(char*)strtok((char *)NULL,"=");
				metadata->minimumNumNeighbours=atoi((char*)value);
				continue;
			}
			else if((strcasecmp((char*)key,"NoCheck")==0) && flags[14]==0 && initphase==1 && beaconphase==0)
			{
				flags[14]=1;
				value=(char*)strtok((char *)NULL,"=");
				metadata->noCheck=atoi((char*)value);
				continue;
			}
			else if((strcasecmp((char*)key,"CacheProb")==0) && flags[15]==0 && initphase==1 && beaconphase==0)
			{
				flags[15]=1;
				value=(char*)strtok((char *)NULL,"=");
				metadata->cacheProb=atof((char*)value);
				continue;
			}
			else if((strcasecmp((char*)key,"StoreProb")==0) && flags[16]==0 && initphase==1 && beaconphase==0)
			{
				flags[16]=1;
				value=(char*)strtok((char *)NULL,"=");
				metadata->storeProb=atof((char*)value);
				continue;
			}
			else if((strcasecmp((char*)key,"NeighborStoreProb")==0) && flags[17]==0 && initphase==1 && beaconphase==0)
			{
				flags[17]=1;
				value=(char*)strtok((char *)NULL,"=");
				metadata->neighborStoreProb=atof((char*)value);
				continue;
			}
			else if((strcasecmp((char*)key,"CacheSize")==0) && flags[18]==0 && initphase==1 && beaconphase==0)
			{
				flags[18]=1;
				value=(char*)strtok((char *)NULL,"=");
				metadata->cacheSize=atoi((char*)value);
				continue;
			}
			else if((strcasecmp((char*)key,"Retry")==0) && flags[18]==0 && initphase==0 && beaconphase==1)
			{
				printf("Reached retry section\n");
				flags[19]=1;
				value=(char*)strtok((char *)NULL,"=");
				metadata->beaconRetryTime=atoi((char*)value);
				continue;
			}
			else if((strcasecmp((char*)key,"Retry")!=0) && initphase==0 && beaconphase==1)
			{
				//printf("reached the beacon list section\n");
				value=strtok(key, ":");
				//printf("The value is %s.\n",value);
				NODEINFO_REF beaconNodeTemp = (struct NodeInfo*)malloc(sizeof(struct NodeInfo));
				strncpy((char *)beaconNodeTemp->hostname, (char *)value, strlen((char *)value));
				//for(unsigned int i=0;i<strlen((char *)value);i++)
					//beaconNodeTemp->hostname[i] = value[i];
				beaconNodeTemp->hostname[strlen((char *)value)] = '\0';
				value=(char *)strtok(NULL,":");
				beaconNodeTemp->portNo = (unsigned int)atoi((char *)value);
				metadata->beaconList->push_back(beaconNodeTemp);
				printf("Host name is : %s\tHost Port no: %d\n", beaconNodeTemp->hostname, beaconNodeTemp->portNo);
			}
			/*else
			{NodeInfo
				printf("Incorrect value in .ini file\n exit now\n");
				//writeLog("\\Incorrect value in .ini value \n exit now\n");
				exit(0);
			}*/
			//printf("End of while loop\n");
		}
	}

	//printf("End of file read function\n");

	if(flags[0]==0 || flags[1]==0 || flags[2]==0 || flags[3]==0 || flags[4]==0)
	{
		printf("All the compulsory sections were not included\nThe node will exit\n");
		//writeLog("//All the compulsory sections were not included\nThe node will exit\n");
		exit(1);
	}
}

void fillDefaultMetaData()
{
	//printf("inside fill default\n");
	metadata = (struct MetaData *)malloc(sizeof(MetaData));

	gethostname((char *)metadata->hostName, 256);
	metadata->distance = 0;
	memset(metadata->currWorkingDir, '\0', 512);
	strncpy((char *)metadata->loggerFile, "servant.log", strlen("servant.log"));

	metadata->loggerFile[strlen("servant.log")]='\0';
	metadata->lifeTimeOfMsg = 30;
	metadata->msgLifeTime = 300;

	metadata->beaconList = new list<NODEINFO_REF> ;
	metadata->portNo = 0;

	metadata->autoShutDown = 900;
	metadata->initNeighbor = 3;
	metadata->ttl = 30;

	metadata->checkResponseTimeout = 15;
	metadata->joinTimeOut = 15;
	//metadata->joinTimeOut_permanent = metadata->joinTimeOut;

	metadata->noCheck = 0;
	metadata->keepAliveTimeOut = 60;
	metadata->minimumNumNeighbours = 2;

	memset(metadata->idNodeInstance, '\0', 300) ;
	metadata->storeProb = 0.1f;
	metadata->cacheProb = 0.1f;

	memset(metadata->idNode, '\0', 265) ;

	metadata->cacheSize = 500;
	metadata->neighborStoreProb = 0.2;

	metadata->beaconRetryTime = 30;
	metadata->iAmBeacon = false;


	//printf("fill default done\n");
}

void printMetaData()
{
	printf("Value for metadata is:--\n\n");
	printf("Port No: %d\n",metadata->portNo);
	printf("hostName: %s\n",metadata->hostName);
	printf("Location: %lu\n",metadata->distance);
	printf("HomeDir: %s\n",metadata->currWorkingDir);
	printf("logFileName: %s\n",metadata->loggerFile);
	printf("autoShutDown: %d\n",metadata->autoShutDown);
	printf("TTL: %d\n",metadata->ttl);
	printf("msgLifeTime: %d\n",metadata->lifeTimeOfMsg);
	printf("getMsgLifeTime: %d\n",metadata->msgLifeTime);
	printf("initNeighbor: %d\n",metadata->initNeighbor);
	printf("joinTimeOut: %d\n",metadata->joinTimeOut);
	printf("keepAliveTimeOut: %d\n",metadata->keepAliveTimeOut);
	printf("minNeighbor: %d\n",metadata->minimumNumNeighbours);
	printf("noCheck: %d\n",metadata->noCheck);
	printf("cacheProb: %lf\n",metadata->cacheProb);
	printf("storeProb: %lf\n",metadata->storeProb);
	printf("neighborStoreProb: %lf\n",metadata->neighborStoreProb);
	printf("cacheSize: %d\n",metadata->cacheSize);
	printf("retry: %d\n",metadata->beaconRetryTime);
	printf("isBeacon: %d\n",metadata->iAmBeacon);
}

void *server_thread(void *dummy)
{

	list<pthread_t > serverChildThreads;

    int nSocket;
	struct sockaddr_in serv_addr;
    nSocket = socket(AF_INET,SOCK_STREAM,0);
    if(nSocket==-1)
	{
        perror("Server: socket\n");
		//writeLog("//server_thread :Error in creating socket");
		exit(1);
    }

    memset(&serv_addr,0,sizeof(struct sockaddr_in));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr =htonl(INADDR_ANY);
    serv_addr.sin_port = htons(metadata->portNo);

    if (setsockopt(nSocket, SOL_SOCKET, SO_REUSEADDR,(void*)(&serv_addr), sizeof(int)) == -1) {
        printf("Server: setsocketopt\n");
        exit(0);
    }

    if(bind(nSocket,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) == -1)
	{
        perror("Server: bind\n");
        exit(0);
    }

    if(listen(nSocket,BACKLOG) == -1)
	{
        perror("Server: listen\n");
        exit(0);
    }

	printf("---->Server listening\n");
	while(true)
	{
		int cli_len = 0, newsockfd = 0;
		struct sockaddr_in cli_addr;

		if(globalShutdownToggle)
			break;

		newsockfd = accept(nSocket, (struct sockaddr *)&cli_addr, (socklen_t *)&cli_len );

		//printf("-------->accept occured\n");
		if(globalShutdownToggle)
			break;

		if(newsockfd==-1)
		{
			printf("\n Error in server_thread:accept");
			//writeLog("\\ Error in server_thread:accept");
			exit(1);
		}
		else
		{
			struct ConnectionInfo ci;
			int minit = pthread_mutex_init(&ci.mQLock, NULL) ;
			if (minit != 0){
				perror("Mutex initialization failed");
				//writeLog("//In server thread :Mutex initialization failed\n");
			}
			int cinit = pthread_cond_init(&ci.mQCv, NULL) ;
			if (cinit != 0){
				perror("CV initialization failed") ;
				//writeLog("//In server thread :CV initialization failed\n");
			}

			ci.shutDown = 0 ;
			ci.keepAliveTimer = metadata->keepAliveTimeOut/2;
			ci.keepAliveTimeOut = metadata->keepAliveTimeOut;
			ci.isReady = 0;															//Check if this parameter is required
			LOCK_ON(&connectionMapLock) ;
			ConnectionMap[newsockfd] = ci ;
			LOCK_OFF(&connectionMapLock) ;

			pthread_t rThread, wThread;

			if ((pthread_create(&rThread, NULL, connectionReaderThread, (void *)newsockfd))!=0) {

				printf("//The read_thread couldnt be created in the server thread.\nHence the node will shutdown");
				//writeLog("//The read_thread couldnt be created in the server thread.\nHence the node will shutdown");
				exit(1);
			}

			ConnectionMap[newsockfd].myReadId = rThread;
			serverChildThreads.push_front(rThread);

			struct Message msg;
			msg.status = 0;
			//msg.fromConnect = 0;
			msg.msgType = 0xfa ;
			//safePushMessageinQ(newsockfd, msg) ;

			LOCK_ON(&ConnectionMap[newsockfd].mQLock) ;

			ConnectionMap[newsockfd].MessageQ.push_back(msg) ;

			LOCK_OFF(&ConnectionMap[newsockfd].mQLock) ;

			if(pthread_create(&wThread, NULL, connectionWriterThread, (void *)newsockfd)!=0)
			{
				//writeLog("//The write_thread couldnt be created in the server thread.\nHence the node will shutdown");
				exit(1);
			}

			ConnectionMap[newsockfd].myWriteId = wThread;
			serverChildThreads.push_front(wThread);
		}
	}
	//Join all threads
	void *thread;
	for(list<pthread_t>::iterator i=serverChildThreads.begin(); i!=serverChildThreads.end();i++)
	{
		if((pthread_join((*i),&thread))!=0)
		{
			printf("In server_thread: couldnt join the threads\n");
			//writeLog("//In server_thread: Couldnt join the threads");
			exit(1);
		}
	}
	serverChildThreads.clear();

	printf("[Server]\t********** Leaving server thread \n");
}

void constructLogAndInitneighbourFileNames()
{

	//init_neighbors_file
	memset(&initNeighboursFilePath, '\0', 512);
	strncpy((char*)(initNeighboursFilePath), (char*)(metadata->currWorkingDir), strlen((char*)(metadata->currWorkingDir)));
	initNeighboursFilePath[strlen((char*)(metadata->currWorkingDir))] = '\0';
	strcat((char*)(initNeighboursFilePath), "/init_neighbor_list");

	//Log file
	memset(&logFilePath, '\0', 512);
	strncpy((char *)logFilePath, (char *)metadata->currWorkingDir, strlen((char*)metadata->currWorkingDir));
	logFilePath[strlen((char*)metadata->currWorkingDir)] = '\0';
	strcat((char *)logFilePath, "/");
	strcat((char *)logFilePath, (char *)metadata->loggerFile);


	printf("[Main]\tInit neighbours file path :%s\n", initNeighboursFilePath);
	printf("[Main]\tLog file path :%s\n", logFilePath);
}

pthread_t spawnServerListenerThread()
{
	//pthread_sigmask(SIG_BLOCK, &new_t, NULL);

	printf("[Main]\t Start Node Server listener thread\n");

	pthread_t serverThread;
	int createThreadRet;
	createThreadRet = pthread_create(&serverThread, NULL, server_thread , (void *)NULL);

	if (createThreadRet != 0) {
		perror("Thread creation failed");
		//doLog((UCHAR *)"//Accept Thread creation failed\n");
		exit(EXIT_FAILURE);
	}

	return serverThread;
}

/*
void doCompleteJoinPhase()
{
	// Perform join sequence
	joinNetworkPhaseFlag = true;
	//printf("[Main]\tStart join phase\n");


			//performJoinNetworkPhase();


	//printf("Neighbour List does not exist... perform Join sequence\n");
	//                                writeLogEntry((UCHAR *)"//Initiating the Join process, since Init_Neighbor file not present\n");
	//printf("Join successfull\n");
	//printf("Terminating the Join process, Init Neighbors identified\n");
	//                                writeLogEntry((UCHAR *)"//Terminating the Join process, Init Neighbors identified\n");
	//                                metadata->joinTimeOut = metadata->joinTimeOut_permanent;

	joinNetworkPhaseFlag = false;
}
*/

void doParticipationPhase(list<struct NodeInfo*> *neighbourNodesList, int & nodeConnected, int connSocket)
{
	/**
	 *  Participation phase:
	 *   If a node is participating in the network, when it makes a connection
	 *    to another node, the first message it sends must be a hello message.
	**/

	printf("[Main]\tParticipation phase begins\n");
	list<NODEINFO_REF>::iterator it = neighbourNodesList->begin();
	for(  ;  it != neighbourNodesList->end() && globalShutdownToggle!=1  ;  it++)
	{

		struct NodeInfo neighbourNode;
		neighbourNode.portNo = (*it)->portNo ;
		strncpy((char *)neighbourNode.hostname, (const char *)(*it)->hostname, 256) ;


		LOCK_ON(&currentNeighboursLock) ;
		//Neighbour already in list
		if (currentNeighbours.find(neighbourNode)!=currentNeighbours.end())
		{

			printf("[Main]\t Neighbour (%s : %d) already in node connections list\n"
			, neighbourNode.hostname, neighbourNode.portNo);
			nodeConnected++;
			it = neighbourNodesList->erase(it) ;
			--it ;
			LOCK_OFF(&currentNeighboursLock) ;

			// Do not connect and check next node from the list
			continue ;
		}

		LOCK_OFF(&currentNeighboursLock) ;

		printf("[Main]\t    Regular node connecting to %s:%d\n", (*it)->hostname, (*it)->portNo) ;
		if(globalShutdownToggle)
		{
			printf("[Main]\t Shutdown set \n") ;
			shutdown(connSocket, SHUT_RDWR);
			close(connSocket);
			break;
		}

		//trying to connect the host name and port no
		connSocket = makeTCPPipe( (*it)->hostname,(*it)->portNo) ;
		if(globalShutdownToggle)
		{
			shutdown(connSocket, SHUT_RDWR);
			close(connSocket);
			break;
		}
		if (connSocket == -1 )
		{
			// Connection could not be established
			// now we have to reset the network, call JOIN
			printf("[Main]\t........... CONTINUE loop, since couldnt connect to the node %d",(*it)->portNo);
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
		if (mres != 0)
		{
			perror("Mutex initialization failed");
			//doLog((UCHAR *)"//Mutex Initializtaion failed\n");

		}


		//Shutdown init to zero
		connInfo.shutDown = 0 ;
		connInfo.keepAliveTimer = metadata->keepAliveTimeOut/2;
		connInfo.keepAliveTimeOut = metadata->keepAliveTimeOut;
		connInfo.isReady = 0;
		connInfo.neighbourNode = n;
		//signal(SIGUSR2, my_handler);


		// Add to dispatcher map and add this outgoing message to it's mQ
		LOCK_ON(&connectionMapLock) ;
		ConnectionMap[connSocket] = connInfo ;
		LOCK_OFF(&connectionMapLock) ;

		// Push a Hello type message in the writing queue
		//printf("[Main]\tSend 'Hello' to neighbour node!\n");
		struct Message outgoingMsg ;
		outgoingMsg.msgType = 0xfa ;
		outgoingMsg.status = 0 ;
		//					m.fromConnect = 1 ;
		safePushMessageinQ(connSocket, outgoingMsg,"doParticipationPhase") ;

		// Create a reader thread for the connection
		pthread_t connReaderThread ;
		int ret = pthread_create(&connReaderThread, NULL, connectionReaderThread , (void *)connSocket);
		if (ret != 0) {
			perror("[Main:init:participation phase] connectionReaderThread Thread creation failed");
			//doLog((UCHAR *)"//Read Thread creation failed\n");
			exit(EXIT_FAILURE);
		}
		LOCK_ON(&connectionMapLock) ;
		ConnectionMap[connSocket].myReadId = connReaderThread;
		childThreadList.push_front(connReaderThread);
		LOCK_OFF(&connectionMapLock) ;




		// Create a write thread for connection
		pthread_t connWriterThread ;
		ret = pthread_create(&connWriterThread, NULL, connectionWriterThread , (void *)connSocket);
		if (ret != 0) {
			perror("[Main:init:participation phase] connectionWriterThread Thread creation failed");
			//doLog((UCHAR *)"//Write Thread creation failed\n");
			exit(EXIT_FAILURE);
		}

		childThreadList.push_front(connWriterThread);
		LOCK_ON(&connectionMapLock) ;
		ConnectionMap[connSocket].myWriteId = connWriterThread;
		LOCK_OFF(&connectionMapLock) ;

		nodeConnected++;

		if(nodeConnected == (int)metadata->minimumNumNeighbours)
		{

			//printf("[Main]\t Successfully connected to minimum neighbours, BREAK!\n");
			break;
		}


	}
}

void resetNodeTimeoutsAndFlags()
{
	globalShutdownToggle = 0;
	metadata->joinTimeOut = metadata->joinTimeOutFixed;
	metadata->autoShutDown = metadata->autoShutDownFixed;
	metadata->checkResponseTimeout = metadata->joinTimeOutFixed;
	checkTimerFlag = 0;
	acceptServerSocket = 0;
}

void parseNeighboursListFromInitFile(UCHAR nodeLineBuf[256], NODEINFO_REF nodeInfo, list<struct NodeInfo*> *& neighbourNodesList)
{
	//fgets(nodeName, 255, f);
	UCHAR *neighbourHostname = (unsigned char*)(strtok((char*)(nodeLineBuf), ":"));
	UCHAR *neighbourPortno = (UCHAR *)strtok(NULL, ":");
	nodeInfo = (struct NodeInfo*)(malloc(sizeof (struct NodeInfo)));
	strncpy((char*)(nodeInfo->hostname), (char*)((neighbourHostname)), strlen((char*)(neighbourHostname)));
	nodeInfo->hostname[strlen((char*)(neighbourHostname))] = '\0';
	nodeInfo->portNo = atoi((char*)(neighbourPortno));
	neighbourNodesList->push_back(nodeInfo);
}

void blockForChildThreadsToFinish()
{
	// wait for child threads to finish
	//printf("[%s]\tWaiting for all threads to finish , size:%d\n", context, childThreadList.size());
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

void init()
{
	acceptProcessId=getpid();
	if(metadata->iAmBeacon)
	{
		printf("[Main] I am a Beacon \n");

		LOCK_ON(&msgCacheLock);
			MessageCache.clear();
		LOCK_OFF(&msgCacheLock);

		//create server thread which continuesly accepts connections
		pthread_t serverThread;

		printf("[Main]\tstart server\n");
		if((pthread_create(&serverThread, NULL, server_thread, (void *)NULL))!=0)
		{
			//writeLog("//Server thread creation failed,hence the node will exit\n");
			exit(1);
		}
		childThreadList.push_front(serverThread);

		//create clients, that in turn connect to other beacons' server.
		struct NodeInfo tempBeaconNode[10];
		int index=0;
		printf("[Main]\tConnect to beacons size :%d\n", metadata->beaconList->size());

		for(list<struct NodeInfo*>::iterator it = metadata->beaconList->begin(); it != metadata->beaconList->end(); it++)
		{

			memcpy(&tempBeaconNode[index].portNo,&(*it)->portNo, sizeof((*it)->portNo));

			printf("[Main] \tport number is %d\n",(*it)->portNo);

			for(UINT i = 0;i < strlen((const char*)((*it)->hostname));i++)
				tempBeaconNode[index].hostname[i] = (*it)->hostname[i];

			tempBeaconNode[index].hostname[strlen((const char*)((*it)->hostname))]='\0';



			if(metadata->portNo == tempBeaconNode[index].portNo) {

				index++;
				printf("[Main] SKIP ... Move to next beacon!\n");
				continue;
			}

			printf("[Main] \tMe %s:%d , Neighbour Beacon %s:%d \n",
									metadata->hostName, metadata->portNo,
									tempBeaconNode[index].hostname,tempBeaconNode[index].portNo);

			printf("[Main]\t Kickoff connect thread %s:%d\n" ,
					tempBeaconNode[index].hostname,tempBeaconNode[index].portNo);

			pthread_t connectThread;
			if(pthread_create(&connectThread, NULL,clientThread, (void *)&tempBeaconNode[index])!=0)
			{
				printf("error in creating thread\nWill exit now");
				//writeLog("//error in creating thread\nWill exit now\n");
				exit(1);
			}

			childThreadList.push_front(connectThread);
			index++;
		}

		printf("[Main]\tChild connect threads :%d\n", childThreadList.size());
	}
	else
	{
		printf("Init section:Regular node\n");
		//Current node is regular node.
		//Hence start server and one client and connect to one of the beacons randomly.

		// Regular node
		printf("___________________ Start Regular Node\n");

		FILE *neighboursFile;
		while(!globalShutdownToggle)
		{

			//check if the init_neighbor_list exsits or not
			//printf("[Main]\t Loop Begin : Check if neighbours file exists\n");
			neighboursFile = fopen((const char *)(initNeighboursFilePath), "r");
			if(neighboursFile == NULL)
			{

				printf("[Main]\tInit neighbours doesn't exist, Quiting!\n");
				//memset(&acceptProcessId, 0, sizeof(acceptProcessId));
				signal(SIGTERM, signal_handler);

				//doCompleteJoinPhase();

				//metadata->joinTimeOut = metadata->joinTimeOutFixed;
				//continue ;
			}

			metadata->joinTimeOut = -1;
			pthread_t serverThread = spawnServerListenerThread();
			printf("In init() server thread launched for regular node\n");
			childThreadList.push_front(serverThread);

			// Connect to neighbors in the list
			UCHAR nodeLineBuf[256];
			memset(&nodeLineBuf, '\0', 256);
			list<struct NodeInfo*> *neighbourNodesList;
			neighbourNodesList = new list<struct NodeInfo*>;
			NODEINFO_REF nodeInfo;
			/**
			 *  Read from the init_neighbours_file
			 **/
			while(fgets((char *)nodeLineBuf, 255, neighboursFile) != NULL   )
			{
				parseNeighboursListFromInitFile(nodeLineBuf, nodeInfo, neighbourNodesList) ;
			}


			int nodeConnected = 0;
			int connSocket;

			//Now try connecting to the nodes from the list.
			doParticipationPhase(neighbourNodesList, nodeConnected, connSocket);


			//succesfully connected to minNeighbors
			if(nodeConnected == (int)(metadata->minimumNumNeighbours) || globalShutdownToggle) {

				printf("[Main]\t Connected successfully to min neighbours || shutDown occurred ... Breaking out..\n");

				// set global shutdown so that the serverListenerThread dies
				fclose(neighboursFile);
				//shutDownFlag = 1;
				//shutdown(acceptServerSocket, SHUT_RDWR);
				//close(acceptServerSocket);
				//waitForChildThreadsToFinish("Main");

				sleep(1);

				break;
			}
			else
			{

				printf("A regular node couldnt connect to the nodes from init neighbor list\n");
				//Coulndn't connect to the Min Neighbors, need to soft restart
				fclose(neighboursFile);
				//remove((char *)tempInitFile);

				// set global shutdown so that the serverListenerThread dies
				globalShutdownToggle = 1;
				shutdown(acceptServerSocket, SHUT_RDWR);
				close(acceptServerSocket);


				//Send NOTIFY message with error code(3) : Self Restart
				initiateNotify(3);

				sleep(1);

				/**
				 * Close all connections from this node
				 **/
				//printf("[Main]\tClose all connections\n");
				LOCK_ON(&currentNeighboursLock) ;
				for (map<struct NodeInfo, int>::iterator it = currentNeighbours.begin(); it != currentNeighbours.end(); ++it) {
					destroyConnectionAndCleanup((*it).second);
				}
				currentNeighbours.clear();
				LOCK_OFF(&currentNeighboursLock) ;


				blockForChildThreadsToFinish();

				printf("Clean everything created and memory used...\n");

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

int isBeaconNode(struct NodeInfo x)
{
	// check if I am among the beacon nodes
	list<NODEINFO_REF>::iterator it = metadata->beaconList->begin();
	for( ; it != metadata->beaconList->end() ; )
	{
		NodeInfo* beaconNode = *it;
		//printf("[Main]\t \t=> Me=[%s : %d] , Beacon[%s : %d]\n" , me.hostname, me.portNo, beaconNode->hostname, beaconNode->portNo);

		//if((strcasecmp((char *)n.hostname, (char *)(*it)->hostName)==0) && (n.portNo == (*it)->portNo))
		if((strcasecmp((char *)x.hostname, (char *)beaconNode->hostname) == 0)&&(x.portNo == beaconNode->portNo))
			return true;

		it++;
	}
	return false;
}

void process()
{
	void *thread_result ;

	//printf("[Main]\t Cleanup\n");

	//KeepAlive Timer Thread, Sends KeepAlive Messages
	pthread_t keepAliveThread ;
	printf("[Main:Process]\tKickoff keepalive thread..\n");
	if((pthread_create(&keepAliveThread, NULL, keepAliveTimer_thread , (void *)NULL))!=0)
	{
		perror("Thread creation failed");
		//doLog((UCHAR *)"//In Main: Thread creation failed\n");
		exit(EXIT_FAILURE);
	}
	childThreadList.push_front(keepAliveThread);

	pthread_t k_thread ;
	printf("[Main:Process]\tKickoff keyboard thread..\n");
	if((pthread_create(&k_thread, NULL, keyboard_thread , (void *)NULL))!=0)
	{

		perror("Thread creation failed");
		//doLog((UCHAR *)"//In Main: Thread creation failed\n");
		exit(EXIT_FAILURE);
	}
	childThreadList.push_front(k_thread);

	// Call the timer thread
	// Thread creation and join code taken from WROX Publications book
	printf("[Main:Process]\tKickoff timer thread..\n");
	pthread_t timer_thread ;
	if((pthread_create(&timer_thread, NULL, general_timer, (void *)NULL))!=0)
	{
		perror("Thread creation failed");
		//doLog((UCHAR *)"//In Main: Thread creation failed\n");
		exit(EXIT_FAILURE);
	}
	childThreadList.push_front(timer_thread);


	printf("[Main]\tWait for all child threads to finish,join all threads to main(), size:%d\n", childThreadList.size());
	int threadsLeft = childThreadList.size();
	for (list<pthread_t >::iterator it = childThreadList.begin(); it != childThreadList.end(); ++it){

		printf("Value is : %d and SIze: %d\n", (int)(*it), threadsLeft);
		if((pthread_join((*it), &thread_result))!=0)
		{
			perror("Thread join failed");
			//writeLogEntry((UCHAR *)"//In Main: Thread joining failed\n");
			exit(EXIT_FAILURE);
			//continue;
		}
		threadsLeft--;
		//printf("[Main]\t\t Child thread finished, now size:%d, threads left : %d\n", childThreadList.size(), threadsLeft);
	}
}

void cleanup()
{
	LOCK_ON(&currentNeighboursLock) ;
	currentNeighbours.clear();
	LOCK_OFF(&currentNeighboursLock) ;

	LOCK_ON(&connectionMapLock) ;
	ConnectionMap.clear();
	LOCK_OFF(&connectionMapLock) ;

	childThreadList.clear();
	joinResponses.clear();
}

int main(int argc, char *argv[])
{
	time_t seconds;
	nodeProcessId = getpid();
	seconds = time (NULL) ;
	if(argc!=2)
	{
		printf("Incorrect commandline arguements program will exit\n");
		//writeLog("//Incorrect arguements,will exit now\n");
		exit(1);
	}
	else
	{
		//writeLog("//Correct number of arguements\n");

		//Create filename and create a pointer for the same.
		char *fileptr;
		int len=strlen((char*)*(++argv));
		fileptr=(char*)malloc(sizeof(char)*(len+1));
		strncpy(fileptr,(char*)*argv,len);
		fileptr[len]='\0';
		printf("The name of file is %s\n",fileptr);
		fillDefaultMetaData();
		parseINIfile(fileptr);
		printMetaData();
		constructLogAndInitneighbourFileNames();
	}



	loggerRef = fopen((char *)logFilePath, "a");

	metadata->joinTimeOutFixed = metadata->joinTimeOut;
	metadata->autoShutDownFixed = metadata->autoShutDown;
	metadata->cacheSize *= 1024;

	for(list<NODEINFO_REF>::iterator it = metadata->beaconList->begin(); it != metadata->beaconList->end(); it++)
	{
		printf("check if the node is beacon or regular\n");
		if((strcasecmp((char *)metadata->hostName, (char *)(*it)->hostname)==0) && (metadata->portNo == (*it)->portNo))
		{
			metadata->iAmBeacon=true;
			break;
		}
		printf("%s and %d\n",(char *)metadata->hostName,metadata->portNo);
	}

	signal(SIGTERM, signal_handler);

	/*struct NodeInfo me;
	bool val;
	//strcpy((char *)n.hostname, (char *)metadata->hostName);
	for(int i = 0;i < (int)(strlen((char*)(metadata->hostName)));i++)
		me.hostname[i] = metadata->hostName[i];

	me.hostname[strlen((char*)(metadata->hostName))] = '\0';
	me.portNo = metadata->portNo;

	val=isBeaconNode(me);
	metadata->iAmBeacon=val;
	*/

	if(metadata->iAmBeacon)
		printf("is beacon true\n");
	else
		printf("is beacon false\n");

	//Set node id now.
	sprintf((char *)metadata->idNode, "%s_%hd_%ld",metadata->hostName,metadata->portNo, (long)seconds) ;
	printf("the node id is %s\n",metadata->idNode);

	initializeLocks();
	//make connections to all the other beacons or just one beacon
	while(!globalShutdownToggle)
	{
		init();
		process();
		cleanup();
	}

	printf("Main is exiting....\n");
	return 0;
}
