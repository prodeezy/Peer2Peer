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

#include <openssl/sha.h>
#define BACKLOG 5

struct client_thread_data
{
	char *host_name;
	char **s_port;
};

struct client_thread_data d1;

//class Node {

	char* constructJoinRespMessage(long uoid,long distance,char* hostName,char* hostPort,Header hdr,u_int msgType) 
	{


		int msgLen = 52 + strlen(hostName);
		char* msg = (char*)malloc(msgLen * sizeof(char));
		
		// TODO: construct				
		
		return msg;
	}

	char* constructJoinReqMessage(Header hdr,uint32_t hostLoc,char* hostName,char* hostPort) 
	{

		int msgLen = 32 + strlen(hostName);
		char* msg = (char*)malloc(msgLen * sizeof(char));
		hdr.dataLength = strlen(hostName)+6;
		memcpy((void*)msg ,(void*)&hdr ,27);		//Copy the common header
		
		memcpy((void*)(msg+27),(void*)&hostLoc,4);
		
		memcpy((void*)(msg+31),(void*)hostPort,2);
		
		memcpy((void*)(msg+33),(void*)hostName,strlen(hostName));
		
		return msg;
	}
// }


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

int my_server_port;
char *serverport;

int beacon;

char* nodeID;

char* GenNodeID(char *hostname, char *portNo)
{
	timeval time;
	gettimeofday(&time, NULL);
	long t1=time.tv_sec*1000+(time.tv_usec/1000.0);
	char *nodeID;
	strcat(nodeID,hostname);
	strcat(nodeID,"_");
	strcat(nodeID,portNo);
	strcat(nodeID,"_");
	sprintf(nodeID,"%l",t1);
	return nodeID;
}

char* GetUOID(char *node_inst_id,char *obj_type,char *uoid_buf,int uoid_buf_sz)
{
  static unsigned long seq_no=(unsigned long)1;
  char sha1_buf[SHA_DIGEST_LENGTH], str_buf[104];

  snprintf(str_buf, sizeof(str_buf), "%s_%s_%1ld",node_inst_id, obj_type, (long)seq_no++);
  SHA1((unsigned char*)str_buf,strlen(str_buf),(unsigned char*)sha1_buf);
  memset(uoid_buf,0,uoid_buf_sz);
  memcpy(uoid_buf,sha1_buf,min(uoid_buf_sz,sizeof(sha1_buf)));

  return uoid_buf;
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
	int return_value;
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
		//send join request to that beacon.
		//construct header struct
		Header h1;
		char *uoid_buf;
		uoid_buf=new char[20];
		GetUOID(nodeID,"msg",uoid_buf,20);
		h1.MessageType=0xFC;
		h1.UOID = uoid_buf;
		h1.reserved = 0;
		h1.ttl = 3;		//needs to be read from config file
	}
	printf("required connections done\n");
	sleep(50);
	exit(0);
}


void *server_connection(void *dummy)		//Server (int my_port) //need port no, it listens to only one port
{
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
	nodeID=GenNodeID("nunki.usc.edu",serverport);

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
		
	server = pthread_create( &thread1, NULL, server_connection, (void*) dummy);
	if( server == 0)
		printf("Server thread creation successful\n");
		
	client = pthread_create( &thread2, NULL, client_connection, (void*) &d1);
	if( client == 0)
		printf("Client thread creation successful\n");
		
	pthread_join( thread1, NULL);
	pthread_join( thread2, NULL);
	
	return 0;
}
