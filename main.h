/**
 *main.h
 **/

#include <string.h>
#include <iostream>
#include <errno.h>
#include <sys/socket.h>
#include <signal.h>
#include <list>
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
//        unsigned int autoShutDown_permanent;
        uint8_t ttl;
        unsigned int msgLifeTime;
        unsigned int getMsgLifeTime;
        unsigned int initNeighbor;
        unsigned int joinTimeOut;
        unsigned int joinTimeOut_permanent;
        unsigned int keepAliveTimeOut;
        unsigned int minNeighbor;
        unsigned int noCheck;
        unsigned int cacheSize;
        unsigned int retry;
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
        int status;                               
        int reqSockfd ;
        int msgLifeTime;
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
        struct NodeInfo n;

        list<struct Message> MessageQ ;
        pthread_mutex_t mQLock ;
        pthread_cond_t mQCv ;
};

set<struct JoinResponseInfo> joinResponses ;


struct JoinResponseInfo{

        unsigned short int neighbourPortNo;
        unsigned char neighbourHostname[256];
        uint32_t location ;

        bool operator<(const struct JoinResponseInfo& aNode) const{
                return aNode.location > location ;
        }

        bool operator==(const struct JoinResponseInfo& aNode) const{
                return ((aNode.neighbourPortNo == neighbourPortNo) && !strcmp((char *)aNode.neighbourHostname, (char *)neighbourHostname) && (aNode.location == location)) ;
        }
};


map<string, struct CachePacket> MessageCache ;
pthread_mutex_t msgCacheLock ;

map<struct NodeInfo, int> neighbourNodeMap ;
pthread_mutex_t neighbourNodeMapLock ;

map<int, struct ConnectionInfo> ConnectionMap ;
pthread_mutex_t connectionMapLock ;

list<pthread_t > childThreadList;
MetaData *metadata;


//Accept connections thread's socket
int nSocket_accept = 0;
bool shutDown = 0 ;
