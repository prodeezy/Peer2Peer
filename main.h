/**
 *main.h
 **/

#include <string.h>
#include <iostream>
#include <errno.h>
#include <sys/socket.h>
#include <signal.h>
#include <list>
#include <sys/time.h>
//#include <openssl/sha.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <set>
#include <queue>
#include <unistd.h>
#include <ctype.h>
#include <map>


#define HEADER_SIZE 								27
#define SHA_DIGEST_LENGTH 							20

#define JOIN_REQ 	0xFC
#define JOIN_RSP 	0xFB
#define HELLO_REQ 	0xFA
#define NOTIFY 		0xF7
#define KEEPALIVE 	0xF8
#define CHECK_RSP 	0xF5
#define CHECK_REQ 	0xF6
#define DEFAULT 	0x00
#define STATUS_REQ	0xAC
#define STATUS_RSP	0xAB
#define STORE_REQ	0xCC
#define GET_REQ		0xDC
#define GET_RESP	0xDB


using namespace std ;

#define LOCK_ON(mutex_addr)								pthread_mutex_lock(mutex_addr)
#define LOCK_OFF(mutex_addr)							pthread_mutex_unlock(mutex_addr)
#define MEMSET_ZERO(arr , size)							memset(arr, 0, size)
#define TO_STRING(const_char_arr, len)					string(const_char_arr, len)


typedef map< struct NodeInfo, int >						NEIGHBOUR_MAP;
typedef NEIGHBOUR_MAP::iterator							NEIGHBOUR_MAP_ITERATOR;
typedef map< string , struct CachePacket >				CACHEPACKET_MAP;
typedef struct NodeInfo *								NODEINFO_REF;
typedef list<NODEINFO_REF>								NODEINFO_LIST;
typedef unsigned char  									UCHAR;
typedef unsigned int									UINT;
typedef unsigned short int								USHORTINT;
typedef list< pthread_t >								PTHREAD_LIST;
typedef list< struct Message >							MESSAGE_LIST;
typedef list<struct JoinResponseInfo>					JOINRESPINFO_LIST;

/**
 * Message structure that goes into the messgae queues
 */

struct FileMetadata {

	list<string> keywords;
	UCHAR bitVector[128];
	UCHAR fName[256];
	UINT fSize;
	UINT fileNumber;
	UCHAR NONCE[20];
	UCHAR SHA1[20];

};

struct MetaData
{
		UCHAR currWorkingDir[512];
		UINT autoShutDown;
		UCHAR loggerFile[256];

        UINT msgLifeTime;
        UINT lifeTimeOfMsg;
        UINT autoShutDownFixed;
        uint8_t status_ttl ;

        bool iAmBeacon;                                          // to check if the current node is beacon or not
        UINT beaconRetryTime;
        UINT initNeighbor;
        NODEINFO_LIST *beaconList;

        UCHAR statusOutFileName[256] ;
        UINT statusFloodTimeout ;
        UINT checkResponseTimeout ;
        UINT joinTimeOutFixed;

        UCHAR idNode[265] ;                                    // To store the unique node ID of this node
        UCHAR idNodeInstance[300];                            // To store the node instance ID, node_id + time when node started
        UINT cacheSize;

        uint32_t distance;
        UCHAR hostName[256];
        USHORTINT portNo;
        double storeProb;
        UINT keepAliveTimeOut;


        UINT noCheck;
        double neighborStoreProb;
        double cacheProb;
        uint8_t ttl;
        UINT minimumNumNeighbours;
        UINT joinTimeOut;
};

struct NodeInfo {

        USHORTINT portNo;
        UCHAR hostname[256];

        bool operator==(const struct NodeInfo& node1) const{

                return ((node1.portNo == portNo)
                		  && !strcmp((char *)node1.hostname,
                		 		     (char *)hostname) ) ;
        }

        bool operator<(const struct NodeInfo& node1) const{

                return node1.portNo < portNo ;
        }
};

struct CachePacket {
		/**
		 *  0 - originated from here
		 *  1 - The message was created by the node which
		 **/
		int reqSockfd ;
		int msgLifetimeInCache ;
		int status_type ;
		int status ;
		struct NodeInfo sender ;
};

struct Message{

		UCHAR *metadata;
        uint8_t msgType;
        UCHAR fileName[256];
        uint32_t fileNameLength;
        uint32_t metadatLen;

        UCHAR *buffer ;
        uint8_t ttl ;
        uint32_t distance ;
        int status ;                                                            // 0 - originated from here
        UCHAR uoid[SHA_DIGEST_LENGTH] ;
        bool fromConnect ;                                                      // 1 - The message was created by the node which
                                                                                // initiated the connection
        int dataLen ;
        uint8_t statusType ;
        uint8_t errorCode;
};

struct ConnectionInfo{

		MESSAGE_LIST MessageQ ;
		int myWriteId;
        int isReady;
        UINT keepAliveTimer;
        int keepAliveTimeOut;
        int shutDown  ;
        int myReadId;
        bool joinFlag;

        pthread_mutex_t mQLock ;
        pthread_cond_t mQCv ;

        struct NodeInfo neighbourNode;
};


struct JoinResponseInfo{

		bool operator<(const struct JoinResponseInfo& aNode) const{
				return location < aNode.location ;
		}

        USHORTINT neighbourPortNo;
        UCHAR neighbourHostname[256];
        uint32_t location ;


        bool operator==(const struct JoinResponseInfo& aNode) const{

        	return (!strcmp((char *)aNode.neighbourHostname, (char *)neighbourHostname)
        				&& (aNode.location == location)
        					&&(aNode.neighbourPortNo == neighbourPortNo)) ;
        }
};



//Flags
extern int keepAlivePid;
extern UCHAR logFilePath[512];


extern bool joinNetworkPhaseFlag;
extern bool globalShutdownToggle;
extern CACHEPACKET_MAP MessageCache ;

extern JOINRESPINFO_LIST joinResponses ;
extern UCHAR initNeighboursFilePath[512];
extern pthread_mutex_t msgCacheLock ;
extern pthread_mutex_t logEntryLock ;
extern NEIGHBOUR_MAP currentNeighbours ;
extern pthread_mutex_t currentNeighboursLock ;
extern map<int, struct ConnectionInfo> ConnectionMap ;
extern pthread_mutex_t connectionMapLock ;
extern PTHREAD_LIST childThreadList;
extern MetaData *metadata;
extern int acceptServerSocket ;
extern int nodeProcessId;
extern int acceptProcessId;
extern pthread_mutex_t getvalue;
//extern FILE *loggerRef = NULL;


// For timer thread
extern int statusTimerFlag ;
extern pthread_mutex_t statusMsgLock ;
extern pthread_cond_t statusMsgCV;
extern int checkTimerFlag;
extern int isSoftRestartEnabled;
extern int joinTimeOutFlag;


// Function declaration
extern int makeTCPPipe(UCHAR *hostName, UINT portNum );
extern void performJoinNetworkPhase() ;
extern void safePushMessageinQ(int, struct Message ) ;
extern void fillInfoToConnectionMap(   	int newreqSockfd,
										int shutDown ,
										UINT keepAliveTimer ,
										int keepAliveTimeOut ,
										int isReady ,
										bool joinFlag,
										NodeInfo n) ;




extern set< set<struct NodeInfo> > statusProbeResponses;
UCHAR *GetUOID(char *obj_type, UCHAR *uoid_buf, long unsigned int uoid_buf_sz);
extern void destroyConnectionAndCleanup(int reqSockfd);

// Thread implementation functions
extern void *connectionReaderThread(void *args);
extern void *keepAliveTimer_thread(void *dummy);
extern void *general_timer(void *);
extern void *keyboard_thread(void *dummy);
extern void *connectionWriterThread(void *args);
//extern void *beaconConnectThread(void *args);
extern void *server_thread(void *);
extern void *clientThread(void *info);

extern UCHAR *prepareLogRecord(UCHAR mode, int sockfd, UCHAR header[], UCHAR *buffer);
extern void serializeStatusResponsesToFile();
extern void sendNotificationOfShutdown(int resSock, uint8_t errorCode);
extern void initiateNotify(int errorCode);
extern void setDefaultValuesForInputMetadata();
extern void setNodeInstanceId();
extern void signal_handler(int sig);
extern void trim(unsigned char* line);
extern void parseINI(char *fileName);
extern void printMetaData();
extern void blockForChildThreadsToFinish();
extern int isBeaconNode(struct NodeInfo me);
extern void floodStatusRequestsIntoNetwork();
extern void doLog(UCHAR *tobewrittendata);
