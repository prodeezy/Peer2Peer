#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>	
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <errno.h>
#include <netdb.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <time.h>
#include <math.h>
#include "commons.h"
#include <sys/time.h>
#include <pthread.h>
#include <openssl/sha.h>

#ifndef min
#define min(A,B) (((A)>(B)) ? (B) : (A))
#endif /* ~min */

#define HEADER_SIZE 27
#include <openssl/sha.h>
#define BACKLOG 5

struct client_thread_data
{
	char *host_name;
	char **s_port;
};

struct client_thread_data d1;

int my_server_port;
char *serverport;
int beacon;
char* nodeID;

unsigned char *GetUOID(char *obj_type, unsigned char *uoid_buf, long unsigned int uoid_buf_sz){

        static unsigned long seq_no=(unsigned long)1;
        unsigned char sha1_buf[SHA_DIGEST_LENGTH], str_buf[104];

        snprintf((char *)str_buf, sizeof(str_buf), "%s_%s_%1ld", nodeID, obj_type, (long)seq_no++);
	printf(".....REAL = %s\n" , str_buf);

        SHA1(str_buf, strlen((const char *)str_buf), sha1_buf);
        memset(uoid_buf, 0, uoid_buf_sz);
        memcpy(uoid_buf, sha1_buf,min(uoid_buf_sz,sizeof(sha1_buf)));
	
	printf(".....ENCODED = %s\n" , uoid_buf);
        return uoid_buf;
}

	unsigned char* constructHelloMessage(	char* hostPort,
				     		unsigned char *header, 
			 		     	unsigned char *buffer) {

		Header* h1 = (Header*)malloc(27);
		unsigned char uoid_buf[SHA_DIGEST_LENGTH] ;
		uint32_t len = 0 ;

		GetUOID( const_cast<char *> ("msg"), uoid_buf, sizeof(uoid_buf)) ;

		// fill message body buffer
                unsigned char host[256];
                gethostname((char *)host, 256) ;
                host[255] = '\0' ;
                len = strlen((char *)host) + 2 ;
                buffer = (unsigned char *)malloc(len) ;

                memset(buffer, '\0', len) ;
                /*for(unsigned int i = 0;i<20;i++){
                        buffer[i] = mes.uoid[i];
                        //printf("%02x-", mes.uoid[i]) ;
                }*/
                memcpy(buffer, &(hostPort), 2) ;
                sprintf((char *)&buffer[2], "%s",  host);

		// fill header
		header[0] = 0xfa;
		memcpy((char*)&header[1] , uoid_buf , SHA_DIGEST_LENGTH);
		header[21] = 0x01;
		header[22] = 0x00;
		memcpy((char *)&header[23], &(len), 4);

		printf("[client] => header len : %d \n", strlen((char *)header));
//		printf("[client] => header[24-27] : %d \n", data_len);
		printf("[client] => buffer len : %d \n", strlen((char *)buffer));
		printf("[client] => message body len : %d \n", len);
	
		free(h1);		

		return buffer;

	}

	unsigned char* constructJoinRespMessage(unsigned char requestor_uoid[],
						uint32_t distance,
						char* hostPort,
				     		unsigned char *header, 
			 		     	unsigned char *buffer) 
	{

		Header* h1 = (Header*)malloc(27);
		unsigned char uoid_buf[SHA_DIGEST_LENGTH] ;
		uint32_t len = 0 ;

		GetUOID( const_cast<char *> ("msg"), uoid_buf, sizeof(uoid_buf)) ;
		h1->MessageType=0xFB;
		h1->uoid = (char *)uoid_buf;
		h1->reserved = 0;
		h1->ttl = 3;		//needs to be read from config file
 
/*
                if (mes.status == 1){
                        buffer = mes.buffer ;
                        len = mes.buffer_len ;
                }
                else{	
*/
                        unsigned char host[256];
                        gethostname((char *)host, 256) ;
                        host[255] = '\0' ;
                        len = strlen((char *)host) + 26 ;
                        buffer = (unsigned char *)malloc(len) ;

                        memset(buffer, '\0', len) ;
                        memcpy(buffer, requestor_uoid, 20) ;
                        /*for(unsigned int i = 0;i<20;i++){
                                buffer[i] = mes.uoid[i];
                                //printf("%02x-", mes.uoid[i]) ;
                        }*/
                        memcpy(&buffer[20], &(distance), 4) ;
                        memcpy(&buffer[24], &(hostPort), 2) ;
                        sprintf((char *)&buffer[26], "%s",  host);
//  		}

                  header[0] = 0xfb;
 		  memcpy((char*)&header[1] , h1->uoid , SHA_DIGEST_LENGTH);
                  memcpy((char *)&header[21], &(h1->ttl), 1);
                  header[22] = 0x00;
                  memcpy((char *)&header[23], &(len), 4);

		printf("[client] => header len : %d \n", strlen((char *)header));
//		printf("[client] => header[24-27] : %d \n", data_len);
		printf("[client] => buffer len : %d \n", strlen((char *)buffer));
		printf("[client] => message body len : %d \n", len);
	
		free(h1);		

		return buffer;

	} 	


	unsigned char *constructJoinReqMessage(uint32_t hostLoc,
				     char* hostPort,
				     unsigned char *header, 
		 		     unsigned char *buffer
				     ) 
	{

		Header* h1 = (Header*)malloc(27);
		unsigned char uoid_buf[SHA_DIGEST_LENGTH] ;
		uint32_t len = 0 ;

		GetUOID( const_cast<char *> ("msg"), uoid_buf, sizeof(uoid_buf)) ;
		h1->MessageType=0xFC;
		h1->uoid = (char *)uoid_buf;
		h1->reserved = 0;
		h1->ttl = 3;		//needs to be read from config file

/*              if (mes.status == 1){
                        buffer = mes.buffer ;
                        len = mes.buffer_len ;
                }
                else{		*/
                        unsigned char host[256] ;
                        gethostname((char *)host, 256) ;
                        host[255] = '\0' ;
                        len = strlen((char *)host) + 6 ;
                        buffer = (unsigned char *)malloc(len) ;
                        memset(buffer, '\0', len) ;
                        memcpy((unsigned char *)buffer, &(hostLoc), 4) ;
                        memcpy((unsigned char *)&buffer[4], &(hostPort), 2) ;
                        sprintf((char *)&buffer[6], "%s",  host);
  //              }

                header[0] = 0xfc;
		memcpy((char*)&header[1] , h1->uoid , SHA_DIGEST_LENGTH);
                memcpy((char *)&header[21], &(h1->ttl), 1) ;
                header[22] = 0x00 ;
                memcpy((char *)&header[23], &(len), 4) ;

		printf("[client] => header len : %d \n", strlen((char *)header));
//		printf("[client] => header[24-27] : %d \n", data_len);
		printf("[client] => buffer len : %d \n", strlen((char *)buffer));
		printf("[client] => message body len : %d \n", len);
	
		free(h1);		

		return buffer;

///////////////////////////////////////
/**
	int msgLen = 32 + strlen(hostName);
	msg = (char*)malloc(msgLen * sizeof(char));
	hdr->dataLength = strlen(hostName)+6;

		memset(buffer, '\0', msgLen) ;
		memcpy(msg , &hdr->MessageType , sizeof(uint8_t));		//Copy the common header
		memcpy(msg+1 , hdr->uoid , SHA_DIGEST_LENGTH);		
		memcpy(msg+21 ,&hdr->ttl , sizeof(uint8_t));		
		memcpy(msg+22 ,&hdr->reserved , sizeof(uint8_t));		
		memcpy(msg+23 ,&hdr->dataLength , sizeof(uint32_t));		
	
		memcpy(msg+27, &hostLoc,4);		
		memcpy(msg+31, &hostPort,2);
		
		memcpy(msg+33, hostName, strlen(hostName));
//		msg[34]='\0';

		printf("\t=============\n");
		printf("MessageType = %#x\n", hdr->MessageType);
		printf("uoid = %s\n", hdr->uoid);
		printf("ttl = %u\n", hdr->ttl);
		printf("reserved = %u\n", hdr->reserved);
		printf("dataLength = %d\n", hdr->dataLength);
		printf("hostLoc = %d\n", hostLoc);
		printf("hostport = %d\n", hostPort);
		printf("hostname = %s\n", hostName);
		printf("\t===========\n");	
	
		printf(".....Message DONE:%s , Len:%d\n",msg, strlen(msg));

		return msg;
**/
	}


// class StateTransitionFSM 
// {

	// int rowSize = 3;
	// int colSize = 8;
	// int[] row = {STATUS_SHUTDOWN, STATUS_DISCOVERY, STATUS_NETWORK_READY};
	// int[] col = {JOIN_REQ, JOIN_RSP, HELLO_REQ, NOTIFY, KEEPALIVE, CHECK_RSP, CHECK_REQ, DEFAULT};

	// int[][] states = { {} , {}, {}, {}, {}, {}, {} };
	
	// int getState(int inputState, int currState) {
		
		// int iInput=0; iState=0;
		// for(int i=0; i<rowSize; i++) {

			// if(row[i] == inputState) {
				// iInput = i
			// }
		// }
		// for(int i=0; i<colSize; i++) {

			// if(colk[i] == currState) {
				// iState = i;
			// }
		// }
		// return states[iInput][iState];
	// }
// }



void GenNodeID(char *hostname, char *portNo)
{
	timeval time;	
	gettimeofday(&time, NULL);
	double t1=time.tv_sec * 1000 + (time.tv_usec / 1000.0);
	char * temp;
	temp=new char[10];
	strcat(nodeID,hostname);
	strcat(nodeID,"_");
	strcat(nodeID,portNo);
	strcat(nodeID,"_");
	sprintf(temp,"%012.3f",t1);
	strcat(nodeID,temp);
	printf(" => NodeID:%s\n" , nodeID);
}



int client_connect(void *arg,int no) //1 is returned for unsuccessful connection
{
	struct addrinfo hints, *server_info;    
        int nSocket=0;
        int status;
	int port_temp;
	int con;
	char *hostname;
	char *server_port;
	
	hostname = (char*)((struct client_thread_data*)arg)->host_name;
	server_port = (char*) (((struct client_thread_data*)arg)->s_port[no]);
	port_temp = atoi(((struct client_thread_data*)arg)->s_port[no]);
	if(port_temp <= my_server_port && beacon)
	{
		con = 0;
		printf("There will be no connection with %s\n",server_port);
		return 0;
	}
	
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	int r_no;

	if ((r_no = getaddrinfo(hostname, server_port, &hints, &server_info)) != 0)
	{
			printf("\nError in getaddrinfo() function\n");
			return 2;
	}

	if ((nSocket = socket(server_info->ai_family, server_info->ai_socktype,server_info->ai_protocol)) == -1)
	{
			perror("Client socket creation error");
			return 2;
	}
	
	int ret = connect(nSocket, server_info->ai_addr, server_info->ai_addrlen);
	if( ret < 0)
	{
		printf("Unsuccessful connection to port: %s \n",server_port);
		printf("Will now sleep for 5 seconds \n");
		close(nSocket);
		return 1;
	}
	else 
	{
		printf("Successful connection to port: %s \n",server_port);
		return 0;
		//client_processing(nSocket);
	}
}

void *client_connection(void *arg)	//Client (char *hostname,char *server_port)
{
	printf("[client] inside thread\n");
	int return_value;
	char *msg;
	msg=new char[100];
    if(beacon)
	{
		for(int j=0;j<sizeof(((struct client_thread_data*)arg)->s_port);j++)
		{
			return_value=client_connect(arg,j);
			if(return_value)
			{
				sleep(5);
				j--;
			}
			else if(return_value == 2)
			{
				exit(0);
			}
		}
	}
	else
	{
		int no=-1;;
		do
		{
			no++;
			return_value=client_connect(arg,(no%4));
		}while(return_value);

		//send join request to that beacon to which it got connected.
		//construct header struct

		unsigned char *buffer;
		unsigned char header[HEADER_SIZE];
		buffer = constructJoinReqMessage(3134382376 , "12024" , header , buffer);
		printf(".....After header :%d\n",strlen((char *)header));
		printf(".....After buffer :%d\n",strlen((char *)buffer));
/**
		unsigned char requestor_uoid[],
		long distance,
		char* hostName,
		char* hostPort,
**/
		unsigned char *buffer2;
		unsigned char uoid_buf[SHA_DIGEST_LENGTH];
		uint32_t len = 0;
		GetUOID( const_cast<char *> ("msg"), uoid_buf, sizeof(uoid_buf));
		unsigned char header2[HEADER_SIZE];
		buffer2 = constructJoinRespMessage(uoid_buf , 10000 , "12022" , header2 , buffer2);
		printf(".....After header2 :%d\n",strlen((char *)header2));
		printf(".....After buffer2 :%d\n",strlen((char *)buffer2));

		unsigned char *buffer3;
		unsigned char header3[HEADER_SIZE];
		buffer3 = constructHelloMessage(	"12023", header3,  buffer3  );
		printf(".....After header3 :%d\n",strlen((char *)header3));
		printf(".....After buffer3 :%d\n",strlen((char *)buffer3));

	}
	printf("required connections done\n");
	sleep(50);
	exit(0);
}


void *server_connection(void *dummy)		//Server (int my_port) //need port no, it listens to only one port
{
    printf("[server] inside thread\n");
	struct sockaddr_in serv_addr;
	int nSocket;
	int my_port;
    nSocket = socket(AF_INET,SOCK_STREAM,0);
    if(nSocket==-1)
	{
        perror("Server: socket\n");
    }
	
	my_port = my_server_port;
    memset(&serv_addr,0,sizeof(struct sockaddr_in));    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr =htonl(INADDR_ANY);
    serv_addr.sin_port = htons(my_port);

    if (setsockopt(nSocket, SOL_SOCKET, SO_REUSEADDR,(void*)(&serv_addr), sizeof(int)) == -1) 
	{
        printf("Server: setsocketopt\n");
        exit(0);
    }
    
    if(bind(nSocket,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) == -1)
	{
        perror("Server: bind\n");
        exit(0);
    }
    
	//get nodeID
	nodeID=new char[100];
	printf("[server] serverPort = %s\n" , serverport);
    
	GenNodeID("nunki.usc.edu" , serverport);
        printf("[server] Generated Node Id :%s\n",nodeID);
	if(listen(nSocket,BACKLOG) == -1)
	{
		perror("Server: listen\n");
	        exit(0);
    	}
	sleep(50);
}

int main(int argc, char* argv[])
{
	int client,server;
	pthread_t thread1, thread2;
	
	char *dummy;
	
	//Decide if its beacon or regular and obtain port from command line
		if(strcmp(argv[1],"r")==0)
		{
			beacon=0;
		}
		else
		{
			beacon=1;
		}
		
		serverport=argv[2];

	client = 1;
	server = 1;
	

	//serverport = "12024";
	my_server_port = atoi(serverport);
	d1.host_name = "nunki.usc.edu";
	d1.s_port = new char *[4];
	//for(int i=0;i<4;i++)
		d1.s_port[0]="12024";
		d1.s_port[1]="12025";
		d1.s_port[2]="12026";
		d1.s_port[3]="12027";
	printf("creating server thread\n");	
	server = pthread_create( &thread1, NULL, server_connection, (void*) dummy);
	if( server == 0)
		printf("Server thread creation successful\n");
	printf("creating client thread\n");		
	client = pthread_create( &thread2, NULL, client_connection, (void*) &d1);
	if( client == 0)
		printf("Client thread creation successful\n");
		
	pthread_join( thread1, NULL);
	pthread_join( thread2, NULL);
	
	return 0;
}
