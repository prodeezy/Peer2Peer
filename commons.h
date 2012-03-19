#include<iostream>

#define JOIN_REQ 	0xFC
#define JOIN_RSP 	0xFB
#define HELLO_REQ 	0xFA
#define NOTIFY 		0xF7
#define KEEPALIVE 	0xF8
#define CHECK_RSP 	0xF5
#define CHECK_REQ 	0xF6
#define DEFAULT 	0x00

#define STATUS_SHUTDOWN		10
#define STATUS_DISCOVERY	11
#define STATUS_NETWORK_READY	12

#define ERROR_UNKNOWN 			0
#define ERROR_USER_SHUTDOWN 		1
#define ERROR_UNEXPECTED_KILL		2
#define ERROR_SELF_RESTART	 	3

struct Header {

	uint8_t MessageType;
	char* uoid;
	uint8_t ttl;
	uint8_t reserved;
	uint32_t dataLength;
};

//int[][] StateTransitionFSM = {};



