/**
 *main.h
 **/

#include <string.h>
#include <iostream>
#include <errno.h>
#include <sys/socket.h>
#include <signal.h>
#include <list>
#include <openssl/sha.h>
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

#define HEADER_SIZE 27
#define SHA_DIGEST_LENGTH 20

using namespace std ;

/**
 * Message structure that goes into the messgae queues
 */

struct MetaData
{
        uint32_t location;
        unsigned char hostName[256];
        unsigned short int portNo;

        unsigned char homeDir[512];
        unsigned char logFileName[256];
        unsigned int autoShutDown;
        unsigned int autoShutDownFixed;

        uint8_t ttl;
        unsigned int joinTimeOutFixed;
        unsigned int joinTimeOut;
        unsigned int keepAliveTimeOut;

        unsigned int msgLifeTime;
        unsigned int getMsgLifeTime;
        unsigned int initNeighbor;

        unsigned int minNeighbor;
        unsigned int noCheck;
        unsigned int cacheSize;
        double cacheProb;
        double storeProb;
        double neighborStoreProb;
        unsigned int beaconRetryTime;
        list<struct NodeInfo *> *beaconList;

        bool isBeacon;                                          // to check if the current node is beacon or not
        unsigned char node_id[265] ;                                    // To store the unique node ID of this node
        unsigned char node_instance_id[300];                            // To store the node instance ID, node_id + time when node started
        uint8_t status_ttl ;
        unsigned char status_file[256] ;
        unsigned int statusResponseTimeout ;
        unsigned int checkResponseTimeout ;
};

struct NodeInfo {

        unsigned short int portNo;
        unsigned char hostname[256];

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

	struct NodeInfo sender ;
	/**
	 *  0 - originated from here
	 *  1 - The message was created by the node which
	 **/
	int status ;
	int reqSockfd ;
	int msgLifetimeInCache ;
	int status_type ;
};

struct Message{

        uint8_t msgType;
        unsigned char *buffer ;
        uint8_t ttl ;
        uint32_t location ;
        int status ;                                                            // 0 - originated from here
        unsigned char uoid[SHA_DIGEST_LENGTH] ;
//        bool fromConnect ;                                                      // 1 - The message was created by the node which 
                                                                                // initiated the connection
        int dataLen ;
        uint8_t status_type ;
        uint8_t errorCode;
};

struct ConnectionInfo{
        int shutDown  ;
        unsigned int keepAliveTimer;
        int keepAliveTimeOut;
        int myReadId;
        int myWriteId;
        int isReady;
        bool joinFlag;
        struct NodeInfo neighbourNode;

        list<struct Message> MessageQ ;
        pthread_mutex_t mQLock ;
        pthread_cond_t mQCv ;
};


struct JoinResponseInfo{

        unsigned short int neighbourPortNo;
        unsigned char neighbourHostname[256];
        uint32_t location ;

        bool operator<(const struct JoinResponseInfo& aNode) const{
                return location < aNode.location ;
        }

        bool operator==(const struct JoinResponseInfo& aNode) const{

        	return ((aNode.neighbourPortNo == neighbourPortNo)
        					&& !strcmp((char *)aNode.neighbourHostname, (char *)neighbourHostname)
        					&& (aNode.location == location)) ;
        	/**
                return ((aNode.neighbourPortNo == neighbourPortNo)
                			&& !strcmp((char *)aNode.neighbourHostname, (char *)neighbourHostname)
//                			&& (aNode.location == location)) ; **/
        }
};








//Flags
extern bool shutDownFlag;
extern bool joinNetworkPhaseFlag;

extern int keepAlivePid;
extern list<struct JoinResponseInfo> joinResponses ;
extern unsigned char initNeighboursFilePath[512];
extern unsigned char logFilePath[512];
extern map<string, struct CachePacket> MessageCache ;
extern pthread_mutex_t msgCacheLock ;
extern pthread_mutex_t logEntryLock ;
extern map<struct NodeInfo, int> neighbourNodeMap ;
extern pthread_mutex_t neighbourNodeMapLock ;
extern map<int, struct ConnectionInfo> ConnectionMap ;
extern pthread_mutex_t connectionMapLock ;
extern list<pthread_t > childThreadList;
extern MetaData *metadata;
extern int acceptServerSocket ;
extern int nodeProcessId;
extern int acceptProcessId;
//extern FILE *loggerRef = NULL;


// For timer thread
extern int statusTimerFlag ;
extern pthread_mutex_t statusMsgLock ;
extern pthread_cond_t statusMsgCV;
extern int checkTimerFlag;
extern int softRestartEnable;
extern int joinTimeOutFlag;


// Function declaration
extern int connectTo(char *context,unsigned char *hostName, unsigned int portNum );
extern void performJoinNetworkPhase() ;
extern void safePushMessageinQ(char *, int, struct Message ) ;
extern void fillInfoToConnectionMap(   int newreqSockfd,
								int shutDown ,
								unsigned int keepAliveTimer ,
								int keepAliveTimeOut ,
								int isReady ,
								bool joinFlag,
								NodeInfo n) ;
extern unsigned char *GetUOID(char *obj_type, unsigned char *uoid_buf, long unsigned int uoid_buf_sz);
extern void closeConnection(int reqSockfd);
extern set< set<struct NodeInfo> > statusResponse;

// Thread implementation functions
extern void *timerThread(void *);
extern void *nodeServerListenerThread(void *);
extern void *connectionReaderThread(void *args);
extern void *connectionWriterThread(void *args);
extern void *beaconConnectThread(void *args);
extern void *keepAliveSenderThread(void *dummy);
extern void *keyboardReader(void *dummy);


extern void fillDefaultMetaData();
extern void trim(unsigned char* line);
extern void parseINI(char *fileName);
extern void printMetaData();
extern void setNodeInstanceId();
extern void signalHandler(int sig);
extern void waitForChildThreadsToFinish(char *context);
extern void writeLogEntry(unsigned char *tobewrittendata);
extern unsigned char *createLogRecord(unsigned char mode, int sockfd, unsigned char header[], unsigned char *buffer);
extern void writeToStatusFile();
extern int isBeaconNode(struct NodeInfo me);
extern void getStatus();
extern void notifyMessageSend(int resSock, uint8_t errorCode);
extern void initiateNotify(int errorCode);
