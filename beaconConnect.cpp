/**
 * beaconConnect.cpp
 **/
#include<main.h>

void *beacon_Connect(char *hostname , char *server_port) {

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
    
	while(true)
	{
		status = connect(nSocket, server_info->ai_addr, server_info->ai_addrlen);        
		if (status > 0)
		break;
	}
    	exit(0);

}

