  #include <pthread.h>
  #include <sys/types.h>
  #include <openssl/sha.h> /* please read this */
  #include <map>
  #include <string.h>
  #include <string>
  #include <stdio.h>

  using namespace std;

  #define min(A,B) (((A)>(B)) ? (B) : (A))

  struct Item {
	
	int timeout;
	char *itemValue;
  };
  typedef   map<string, Item*>   LookupTable;
  LookupTable routingTable;

  char *GetUOID(
	        char *node_inst_id,
          	char *obj_type,
          	char *uoid_buf,
          	int uoid_buf_sz)
  {
      static unsigned long seq_no=(unsigned long)1;
      char sha1_buf[SHA_DIGEST_LENGTH], str_buf[104];

      snprintf(str_buf, sizeof(str_buf), "%s_%s_%1ld",
              node_inst_id, obj_type, (long)seq_no++);
      SHA1((unsigned char*)str_buf,      strlen(str_buf), (unsigned char*)sha1_buf);
      memset(uoid_buf,   0,               uoid_buf_sz);
      memcpy(uoid_buf,   sha1_buf,        min(uoid_buf_sz,sizeof(sha1_buf)));

      return uoid_buf;
  }
  


  void *timer_thread(void *arg) {

	while(true) {

		sleep(1);
//		break;
		printf("wakey\n");
		string removeKeys[10];	
		int iRem=0;
		for(map<string,Item*>::iterator entry = routingTable.begin(); entry != routingTable.end(); entry++) {
			
		   	string msgId = entry->first;
		   	Item *item = entry->second;	
			item->timeout--;
			printf("  \ttimeout--\n");
		   	// std::cout << i->first << " " << i->second << std::endl;
			if( item->timeout <= 0 ) {
				printf("********** Entry timedout, remove : %s\n", msgId.c_str());
				removeKeys[iRem] = msgId;
				iRem++;	
			}
  		}
		for(int i = 0; i<iRem; i++) {

			routingTable.erase(removeKeys[i]);
			printf("  \tDeleted -> %s\n" , removeKeys[i].c_str());
		}
	}
  }

  void startTimer(pthread_t *taskThread) {

	printf("Starting timer thread..\n");
        int res ;
        res = pthread_create(taskThread, NULL, timer_thread, (void *)NULL);
        if (res != 0) {
                fprintf(stderr, "Thread creation failed\n");
                exit(0);
        }
        //return taskThread;
  }


  int main(int arc, char** argv)   {

  	// Store "Message UOID" -> "Node Id"


  	char msgId_1[SHA_DIGEST_LENGTH];
  	char msgId_2[SHA_DIGEST_LENGTH];
  	char msgId_3[SHA_DIGEST_LENGTH];
  	char msgId_4[SHA_DIGEST_LENGTH];


	char* nodeId_1 = "merlot.usc.edu_16011";
	char* nodeId_2 = "merlot.usc.edu_16012";
	char* nodeId_3 = "merlot.usc.edu_16013";
	char* nodeId_4 = "merlot.usc.edu_16014";


	string putStr;

  	GetUOID(nodeId_1, "msg", msgId_1, sizeof(msgId_1));
	putStr = string(msgId_1);
	Item *item_1;
	item_1->timeout = 10;
	item_1->itemValue = nodeId_1;
	printf("Putting... str:%s , char[]=%s\n", putStr.c_str(), msgId_1);
	
	routingTable[putStr] = item_1;
	

//	putStr = string(msgId_1);
	Item *item = routingTable[putStr];
	printf(" size=%d \n" , routingTable.size());
	printf(" msgId=%s , timeout=%d, nodeId=%s \n" , msgId_1 , item->timeout , item->itemValue);

	pthread_t timerThread;
	startTimer(&timerThread);

        void *threadResult;
        int ret = pthread_join(timerThread, &threadResult);

	return 1;
  }



