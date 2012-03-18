#include<commons.h>


class Node {

	char* constructJoinRespMessage(long uoid, 
				      long distance,
				      char[] hostName,
                      char[] hostPort, 
				      Header hdr, 
				      u_int msgType) {


		int msgLen = 52 + strlen(hostName);
		char* msg = (char*)malloc(msgLen * sizeof(char));
		
		// TODO: construct				
		
		return msg;
	}

	char* constructJoinReqMessage(Header hdr,
					 uint32_t hostLoc, 
				     char[] hostName, 
				     char[] hostPort) {

		int msgLen = 32 + strlen(hostName);
		char* msg = (char*)malloc(msgLen * sizeof(char));
		hdr.dataLength = strlen(hostName)+6;
		memcpy((void*)msg ,(void*)hdr ,27)		//Copy the common header
		
		memcpy((void*)(msg+27),(void*)&hostLoc,4);
		
		memcpy((void*)(msg+31),(void*)hostPort,2);
		
		memcpy((void*)(msg+33),(void*)hostName,strlen(hostName));
		
		return msg;
	}
}


class StateTransitionFSM {

	int rowSize = 3;
	int colSize = 8;
	int[] row = {STATUS_SHUTDOWN, STATUS_DISCOVERY, STATUS_NETWORK_READY};
	int[] col = {JOIN_REQ, JOIN_RSP, HELLO_REQ, NOTIFY, KEEPALIVE, CHECK_RSP, CHECK_REQ, DEFAULT};

	int[][] states = { {} , {}, {}, {}, {}, {}, {} };
	
	int getState(int inputState, int currState) {
		
		int iInput=0; iState=0;
		for(int i=0; i<rowSize; i++) {

			if(row[i] == inputState) {
				iInput = i
			}
		}
		for(int i=0; i<colSize; i++) {

			if(colk[i] == currState) {
				iState = i;
			}
		}
		return states[iInput][iState];
	}
}


