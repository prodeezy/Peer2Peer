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



void connectTo(char *hostname,char *server_port){

    struct addrinfo hints, *server_info;    
    int nSocket=0;
    int status;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int r_no;

    if ((r_no = getaddrinfo(hostname, server_port, &hints, &server_info)) != 0) {
            printf("\nError in getaddrinfo() function\n");
            exit(0);
    }

    if ((nSocket = socket(server_info->ai_family, server_info->ai_socktype,server_info->ai_protocol)) == -1) {
            perror("Client: socket");
            exit(0);
    }
    
    status = connect(nSocket, server_info->ai_addr, server_info->ai_addrlen);        
    if (status < 0) {
        close(nSocket);
        perror("Client: connect");
        exit(0);
    } 

    close(nSocket);
    exit(0);

}

/**
 *  Called when the init_neighbours file is not present
 **/
void joinNetwork() {

	int nSocket = -1;
	for(list<struct NodeInfo *>::iterator it = metadata->beaconList->begin(); it != metadata->beaconList->end(); it++){

		NodeInfo* beaconInfo = *it;

		nSocket = connectTo(beaconInfo->hostName, beaconInfo->portNo);

		if(nSocket != -1) {

			// connected to beacon

			// load Connection Node Info and beacon node info
                        struct NodeInfo beaconNode;
                        beaconNode.portNo = beaconInfo->portNo ;
                        for(unsigned int i =0 ;i<strlen((const char *)(beaconInfo)->hostName);i++)
                                beaconNode.hostname[i] = beaconInfo->hostName[i];
                        beaconNode.hostname[strlen((const char *)beaconInfo->hostName)]='\0';
/**
                        int mret = pthread_mutex_init(&cn.mQLock , NULL) ;
                        if (mret != 0){
                                perror("Mutex initialization failed");
                                //writeLogEntry((unsigned char *)"//Mutex initialization failed\n");

                        }
                        int cret = pthread_cond_init(&cn.mQCv, NULL) ;
                        if (cret != 0){
                                //perror("CV initialization failed") ;
                                writeLogEntry((unsigned char *)"//CV initialization failed\n");
                        }
**/

			fillInfoToConnectionMap( nSocket , 0, keepAliveTimer ,
						 keepAliveTimeOut , 0, 
						 true, beaconNode);


                        // Push a Join Req type message in the writing queue
                        struct Message m ;
                        m.type = 0xfc ;
                        m.status = 0 ;
                        m.ttl = metadata->ttl ;
                        pushMessageinQ(nSocket, m) ;

/**
                        // Create a read thread for this connection
                        int res = pthread_create(&re_thread, NULL, read_thread , (void *)nSocket);
                        if (res != 0) {
                                //perror("Thread creation failed");
                                writeLogEntry((unsigned char *)"//In Join Network: Read Thread Creation Failed\n");
                                exit(EXIT_FAILURE);
                        }
                        childThreadList.push_front(re_thread);
                        // Create a write thread
                        pthread_t wr_thread ;
                        res = pthread_create(&wr_thread, NULL, write_thread , (void *)nSocket);
                        if (res != 0) {
                                //perror("Thread creation failed");
                                writeLogEntry((unsigned char *)"//In Join Network: Write Thread Creation Failed\n");
                                exit(EXIT_FAILURE);
                        }
                        childThreadList.push_front(wr_thread);
**/
                        break ;
			
		}

	// if there was a successfull beacon connection start a JoinRequest Timer
	if(nSocket == -1) {

                fprintf(stderr,"No Beacon node up\n") ;
                //writeLogEntry((unsigned char *)"//NO Beacon Node is up, shuting down\n");
                exit(0) ;		
	}

	// set off timer for join request 
/**
        pthread_t t_thread ;
        res = pthread_create(&t_thread, NULL, timer_thread , (void *)NULL);
        if (res != 0) {
                //perror("Thread creation failed");
                writeLogEntry((unsigned char *)"//In Join Network: Timer Thread Creation Failed\n");
                exit(EXIT_FAILURE);
        }
        childThreadList.push_front(t_thread);
**/

        // Join the read thread here
        // Thread Join code taken from WROX Publications
        for (list<pthread_t >::iterator it = childThreadList.begin(); it != childThreadList.end(); ++it){
                //printf("Value is : %d\n", (pthread_t)(*it).second.myReadId);
                int res = pthread_join((*it), &thread_result);
                if (res != 0) {
                        perror("Thread join failed");
//                        writeLogEntry((unsigned char *)"//In Join Network: Thread Joining Failed\n");
                        exit(EXIT_FAILURE);
                }
        }




	//joinTimeOutFlag = 0;
        printf("Join process done, Exiting..\n") ;

        // Sort the output and write them in the file
        FILE *file = fopen((char *)tempInitFile, "w");
        if (file==NULL){
		printf("In Join Network: Failed to open Init_Neighbor_list file\n");
//                writeLogEntry((unsigned char *)"//In Join Network: Failed to open Init_Neighbor_list file\n");
                exit(EXIT_FAILURE);
        }


        if (joinResponses.size() < metadata->initNeighbor){
 //               writeLogEntry((unsigned char *)"//Failed to locate Init Neighbor number of nodes\n");
//                fclose(f_log);
                exit(0) ;
        }

        unsigned int counter = 0;
        unsigned char tempPort[10] ;
        for (set<struct JoinResponseInfo>::iterator it = joinResponses.begin(); it != joinResponses.end() ; it++, counter++){

//                if(counter==metadata->minNeighbor)
//                        break;

		JoinResponseInfo jresp = (*it);
                printf("Hostname: %s, Port: %d, location: %d\n", jresp.neighbourHostname, jresp.neighbourPortNo, jresp.location) ;

                fputs((char *)jresp.neighbourHostname , file) ;
                fputs(":", file) ;
                sprintf((char *)tempPort, "%d", jresp.neighbourPortNo) ;
                fputs((char *)tempPort, file) ;
                fputs("\n", file) ;
        }

        fflush(file) ;
        fclose(file) ;

        pthread_mutex_lock(&neighbourNodeMapLock) ;
	        neighbourNodeMap.erase(neighbourNodeMap.begin(), neighbourNodeMap.end()) ;
        pthread_mutex_unlock(&neighbourNodeMapLock) ;
        childThreadList.clear();
}
