#include "main.h"

using namespace std;

void initiateNotify(int errorCode)
{

	//printf("[Notify]\t initiate Notify sequence, errorCode:%d\n", errorCode);

    //send notify msg to all neighbors to inform shutdown of itself
    pthread_mutex_lock(&currentNeighboursLock);
    for(map<struct NodeInfo,int>::iterator i = currentNeighbours.begin();i != currentNeighbours.end();++i){

        //printf("[Keyboard]\t\tNotify (%s : %d)\n" , (*i).first.hostname, (*i).first.portNo);
        sendNotificationOfShutdown((*i).second, errorCode);
    }
    pthread_mutex_unlock(&currentNeighboursLock);

    sleep(3);
}

void writeStatusFileOutput()
{
	printf("Entered writeStatusFileOutput method NOW!!!\n");
	LOCK_ON(&statusMsgLock);
	list <int> myFileList;
	
	//find own status first
	struct NodeInfo info;
	struct FileMetadata fData;
	
	myFileList=getAllFileInfo();
	printf("Got all the files on the node\n");
	int i;
	info.portNo=metadata->portNo;
	i=0;
	for (; i < 256 ; )
	{
		info.hostname[i] = metadata->hostName[i] ;
		i++;
	}
	
	statusFilesResponsesOfNodes[info];
	string strMeta("");
	list<int>::iterator iter=myFileList.begin();
	for(;iter!=myFileList.end();)
	{
		fData=createFileMetadata(*iter);
		printf("The bitvector from file is====***********>");
		for(int w=0;w<128;w++)
			printf("%02x",fData.bitVector[w]);
		printf("\n");
		
		fileMap[string((char*)fData.fileID,20)]=(*iter);
		strMeta=toStringMetaData(fData);
		printf("The string obtained is:%s\n",strMeta.c_str());
		statusFilesResponsesOfNodes[info].push_front(strMeta);
		iter++;
	}
	
	FILE *fptr=fopen((char*)metadata->statusOutFileName,"a");
	
	map<struct NodeInfo,list<string> >::iterator x=statusFilesResponsesOfNodes.begin();
	
	if(fptr==NULL)
	{
		doLog((UCHAR*)"//The status file couldnt be opened\nThe node will shut down\n");
		exit(1);
	}
	for(;x!=statusFilesResponsesOfNodes.end();)
	{
		if((*x).second.size()>1)
			fprintf(fptr, "%s:%d has %d files\n", (*x).first.hostname,  (*x).first.portNo, (int)(*x).second.size());
		
		if((*x).second.size()==0)
			fprintf(fptr, "%s:%d has no file\n", (*x).first.hostname,  (*x).first.portNo);
		
		else
			fprintf(fptr, "%s:%d has 1 file\n", (*x).first.hostname,  (*x).first.portNo);
			
		
		list<string>::iterator it2=(*x).second.begin();
		for(;it2!=(*x).second.end();)
		{
			fputs((*it2).c_str(),fptr);
			it2++;
			fputs("\n",fptr);
		}
		x++;
	}
	
	fclose(fptr);
	statusFilesResponsesOfNodes.clear();
	LOCK_OFF(&statusMsgLock);
}

void *keyboard_thread(void *dummy)
{
	char *input=(char*)malloc(1024*sizeof(char));
	while(!globalShutdownToggle)
	{
		sleep(1);
		int flag=1;
		signal(SIGINT,signal_handler);
		
		printf("\nservent:%d> ",metadata->portNo);
		
		fgets((char*)input,511,stdin);
		//printf("msg from keyboard:%s",input);
		if(globalShutdownToggle)
			break;
		
		if(strstr((char*)input,"status neighbors")!=NULL)
		{
			unsigned char *tokens[4];
			tokens[0]=(unsigned char*)strtok((char*)input," ");
			for(int x=1;x<4;x++)
			{
				tokens[x]=(unsigned char*)strtok(NULL," ");
				printf("[Status] \tTOKEN:'%s'\n",tokens[x]);
				if(tokens[x]==NULL)
				{
					printf("Incorrect commandline usage\n");
					//writeLog("Incorrect commandline usage in status neighbors\nRe enter\n");
					flag=0;
					break;
				}
			}
			if(flag==0)
				break;
			
			metadata->status_ttl=(uint8_t)atoi((char *)tokens[2]);
			
			int fNameLen = strlen((char *)tokens[3]);
			printf("status file length is:\t%d\n" , fNameLen);

			strncpy((char *)metadata->statusOutFileName, (char *)tokens[3], fNameLen - 1) ;
			metadata->statusOutFileName[fNameLen] = '\0';
			
			printf("[Keyboard]\t Status file:'%s'\n",metadata->statusOutFileName);
			metadata->statusFloodTimeout = metadata->lifeTimeOfMsg + 10 ;
			printf("[Keyboard]\t  keyboard OPEN status file \n");
			FILE *fp = fopen((char *)metadata->statusOutFileName, "w") ;
			
			if (fp == NULL)
			{
				//writeLogEntry((unsigned char *)"In Keyborad thread: Status File open failed\n");
				printf("[status file: Failure to open\n");
				exit(0) ;
			}
			
			fputs("V -t * -v 1.0a5\n", fp);
			fclose(fp);
			
			printf("[Status]\t  keyboard CLOSE status file \n");


			floodStatusRequestsIntoNetwork();

			// waiting for status time out, so that command prompt can be returned
			printf("[Status] waiting for status timeout...\n");
			pthread_mutex_lock(&statusMsgLock);
				statusTimerFlag = 1;
				printf("[Keyboard] reset statusTimerFlag=1 , statusFloodTimeout:%d", metadata->statusFloodTimeout);
				pthread_cond_wait(&statusMsgCV, &statusMsgLock);
			pthread_mutex_unlock(&statusMsgLock);

			serializeStatusResponsesToFile();	
		}
		
		else if(strstr((char*)input,"shutdown") != NULL)
		{
			printf("shutdown caught\n");
			globalShutdownToggle = 1;
			pthread_mutex_lock(&currentNeighboursLock);
			printf("Got the lock\n");
			for(map<struct NodeInfo, int>::iterator it = currentNeighbours.begin(); it != currentNeighbours.end(); ++it)
				sendNotificationOfShutdown((*it).second, 1);
				
			pthread_mutex_unlock(&currentNeighboursLock);
			sleep(1);
			printf("will call SIGTERM\n");
			kill(nodeProcessId,SIGTERM);
			break;
		}
		
		else if(strstr((char*)input,"store ")!=NULL)
		{
			printf("$$$$$store command caught\n");
			fflush(stdout);
			unsigned char* tokens;
			tokens=(unsigned char*)strtok((char*)input," ");
			printf("token is:%s\n",tokens);
			fflush(stdout);

			struct FileMetadata tempMetadata;
			MEMSET_ZERO(&tempMetadata, sizeof(tempMetadata));

            MEMSET_ZERO(&tempMetadata.NONCE, 128);
            tempMetadata.keywords =  new list<string >();
            MEMSET_ZERO(&tempMetadata.bitVector, 128);


			fflush(stdout);
			tokens=(unsigned char*)strtok(NULL," ");
			
			printf("token is:%s\n",tokens);
			fflush(stdout);
			//Set the file name to be stored
			if(tokens==NULL)
			{
				printf("Incorrect store command entered\nRe-enter\n");
				fflush(stdout);
				continue;
			}

			MEMSET_ZERO(tempMetadata.fName, 256);
			strncpy((char *)tempMetadata.fName, (char *)tokens, strlen((char *)tokens));
			printf("The file name is:%s\n",(char *)tempMetadata.fName);
			fflush(stdout);
			//Fill the file size in the filemetadata struct
			struct stat s;
			stat((char *)tempMetadata.fName, &s);
			tempMetadata.fSize=(unsigned int)s.st_size;
			printf("File size obtained\n");
			fflush(stdout);
			//Obtain the ttl from the commandline
			tokens=(unsigned char*)strtok(NULL," ");
			for (int i = 0; i < strlen((char*)tokens); i++)
			{
				if (!isdigit(tokens[i]))
				{
					printf("Incorrect store command entered\nRe-enter\n");
					fflush(stdout);
					continue;
				}
			}
			metadata->status_ttl=atoi((char*)tokens);
			printf("here1\n");
			fflush(stdout);
			unsigned char buffer;
			FILE *f = fopen((char *)tempMetadata.fName, "rb");
			if(f==NULL)
			{
				printf("The file is not present in cwd\n");
				fflush(stdout);
				exit(0);
			}
			SHA_CTX *c = (SHA_CTX *)malloc(sizeof(SHA_CTX));
			//memset(c, 0, sizeof(c));
			memset(c, 0, sizeof(c));
			printf("SHA1 here2\n");
			fflush(stdout);
			SHA1_Init(c);
			while(fread(&buffer,1,1,f)!=0)
				SHA1_Update(c, &buffer, 1);

			MEMSET_ZERO(tempMetadata.SHA1, 20);
			SHA1_Final(tempMetadata.SHA1, c);
			fclose(f);
			printf("SHA1 calculated\n");
			fflush(stdout);
			tokens=(unsigned char*)strtok(NULL,"\n");
			printf("after all tokens\n");
			string strToken((char*)tokens);
			printf("All key words are:%s",strToken.c_str());
			fflush(stdout);
			fflush(stdout);
			/*
			int strLength=0;
			for(int i=0;i<strToken.length();i++)
			{
				if(strToken[i]=='"')
					strToken[i]='';
				strLength++;
			}
			strToken[++strLength]='\0';
			*/
			
			//printf("After removing \":%s",strToken);
			int count=0;
			int start=0;
			printf("before strtokens loop\n");
			fflush(stdout);
			
			for(int j=0;j<strToken.length();j++)
			{
				if(((strToken[j]=='"'&&strToken[j-1]!='=')&&(strToken[j]=='"'&&strToken[j-1]!=' ')) || strToken[j]==' ' || strToken[j]=='=')
				{
					string part(strToken,start,count);
					if(count!=0)
					{
						printf("keyword being pushed:%s\n",part.c_str());
						tempMetadata.keywords->push_back(part);
					}
					count=0;
					start=j+1;
					continue;
				}
				if((strToken[j]=='"'&&strToken[j-1]=='=')||(strToken[j]=='"'&&strToken[j-1]==' '))
				{
					start=j+1;
					continue;
				}
				count++;
			}
			
			printf("here3.5\n");
			fflush(stdout);
			for(list<string >::iterator it = tempMetadata.keywords->begin(); it != tempMetadata.keywords->end(); ++it)
				generateBitVector((unsigned char *)(*it).c_str() , tempMetadata.bitVector);
			printf(" === Bit Vector ===\n");
			for(int i=0;i<128;i++)
				printf("%02x", tempMetadata.bitVector[i]);
			printf("\n");
			printf("here4\n");
			fflush(stdout);
			unsigned char psswdfName[256];
			unsigned char psswd[SHA_DIGEST_LENGTH];
			int globalfNumber=incfNumber();
			printf("global file updated:%d\n",globalfNumber);
			GetUOID(const_cast<char *> ("password"), psswd, sizeof(psswd));
			printf("The password obtained is:%s\n",psswd);
			sprintf((char*)psswdfName,"%s/file/%d.pass",metadata->currWorkingDir,globalfNumber);
			FILE *psswdfptr=fopen((char*)psswdfName,"w+");
			fprintf(psswdfptr,"%s",psswd);
			fclose(psswdfptr);
			
			printf("here5\n");
			fflush(stdout);
			UCHAR psswdSHA1[20];
			MEMSET_ZERO(psswdSHA1, 20);
			//void toSHA1_MD5(UCHAR *str,int choice, UCHAR *buffer)
			toSHA1_MD5(psswd,0, psswdSHA1);
			strncpy((char*)tempMetadata.NONCE, (char*)psswdSHA1, 20);
			writeDataToFile(tempMetadata,globalfNumber);
			writeMetadataToFile(tempMetadata,globalfNumber);
			populateIndexes(tempMetadata,globalfNumber);
			
			if(metadata->status_ttl > 0)
			{
				printf("[keyboard]\t Start flooding store msg\n");
				fireSTORERequest(tempMetadata, (char *)tempMetadata.fName);
			}
			printf("Local store handling DONE!!!\n");
		}
		
		else if(strstr((char*)input,"search")!=NULL)
		{
			unsigned char* token;
			
			printf("The search command caught is:%s\n",input);
			
			token=(unsigned char*)strtok((char*)input," ");
			
			//token will now be key
			
			token=(unsigned char*)strtok(NULL,"=");
			
			unsigned char* pattern;
			pattern=token;
			printf("The value that is to be searched against is:%s",pattern);
			
			token=(unsigned char*)strtok(NULL,"\n");
			unsigned char* value;
			value=token;
			printf("\t the value is:%s\n",value);
			list <string> keyword_search;
			//string SToken((char*)value);			
			//Clear the search parameters first.
			fileDisplayIndexMap.clear();
			LOCK_ON(&countOfSearchResLock);
			countOfSearchRes=0;
			LOCK_OFF(&countOfSearchResLock);
			
			printf("The parameters are cleared\n");
			if(!strcasecmp((char*)pattern,"keywords"))
			{
				/*
				printf("the keywords are:\n");
				int start=1;
				int count=0;
				for(int x=1;x<SToken.length();x++)
				{
					if(SToken[x]==' ')
					{
						string part(SToken,start,count);
						printf("The part being pushed is:%s the start:%d count:%d\n",part.c_str(),start,count);
						keyword_search.push_back(part);
						count=0;
						start=x+1;
					}
					else
					{
						count++;
					}
				}
				if(count!=0)
				{
					string part(SToken,start,count-1);
					printf("The part being pushed is:%s the start:%d count:%d\n",part.c_str(),start,count);
					keyword_search.push_back(part);
				}*/
				
				printf("Calling local keyword search method\n");
				initiateLocalKeywordSearch(value);
				constructSearchMsg(value,0x03);
				//break;
			}
			
			if(!strcasecmp((char*)pattern,"sha1hash"))
			{
				unsigned char* hash=convertToHex(value,20);
				printf("Calling local sha1 search method\n");
				int y=0;
				for(;y<SHA_DIGEST_LENGTH;)
				{
					printf("%02x",hash[y]);
					y++;
				}
				initiateLocalSha1Search(value);
				constructSearchMsg(value,0x02);
			}
			
			if(!strcasecmp((char*)pattern,"filename"))
			{
				printf("Calling local filename search method for filename=%s\n",value);
				//initiateLocalFilenameSearch(value);
				bool ifExists = FileNameIndexMap.find((char*)value)!=FileNameIndexMap.end();
				if(ifExists) {
					
						list<int> fNumbers = FileNameIndexMap[(char*)value];
					for(list<int>::iterator listIter = fNumbers.begin();
						listIter!=fNumbers.end();
						listIter++) {

								printf("\tFileNumber:%d\n", (*listIter));
								
								struct FileMetadata populatedFileMeta = createFileMetadata(*listIter);
								printf("\tDone with createFileMetadata\n");
								
								pthread_mutex_lock(&countOfSearchResLock);
								printf("\tThe value of countOfSearchRes is:%d\n",countOfSearchRes);
								countOfSearchRes++;
								printf("\tThe value of countOfSearchRes is:%d\n",countOfSearchRes);
								fileDisplayIndexMap[countOfSearchRes]=populatedFileMeta;
								printf("\tFile added to the fileDisplayIndexMap whose size is now:%d\n",fileDisplayIndexMap.size());
								displayFileMetadata(populatedFileMeta);
								pthread_mutex_unlock(&countOfSearchResLock);
								
								printf("\tNEXT FILE NUMBER\n");
								fflush(stdout);
						
						}			
											
				}
				constructSearchMsg(value,0x01);
			}
			//Use locks to prevent the keyboard from looping back-up
			pthread_mutex_lock(&searchMsgLock) ;
			pthread_cond_wait(&searchMsgCV, &searchMsgLock);
			pthread_mutex_unlock(&searchMsgLock) ;
		}
		
		else if(strstr((char*)input,"status files")!=NULL)
		{
			printf("The status files command is caught!!!\n");
			
			UCHAR *token;
			token=(UCHAR*)strtok((char*)input," ");
			token=(UCHAR*)strtok(NULL," ");
			token=(UCHAR*)strtok(NULL," ");
			
			UCHAR fName[256];
			if(token==NULL)
			{
				printf("the status file command not entered appropriately\nRe-enter\n");
				fflush(stdout);
				continue;
			}
			
			for (int i = 0; i < strlen((char*)token); i++)
			{
				if (!isdigit(token[i]))
				{
					printf("Incorrect store command entered\nRe-enter\n");
					fflush(stdout);
					continue;
				}
			}
			metadata->status_ttl=atoi((char*)token);
			printf("The ttl from status command is:%d\n",metadata->status_ttl);
			
			token=(UCHAR*)strtok(NULL," ");
			
			if(token==NULL)
			{
				printf("the status file command not entered appropriately\nRe-enter\n");
				fflush(stdout);
				continue;
			}
			MEMSET_ZERO(fName,256);
			
			strncpy((char *)metadata->statusOutFileName, (char*)token, strlen((char *)token)-1) ;
			metadata->statusOutFileName[strlen((char *)token)]='\0';
			printf("[Keyboard]\t Status file:'%s'\n",metadata->statusOutFileName);
			metadata->statusFloodTimeout = metadata->lifeTimeOfMsg + 10 ;
			printf("[Keyboard]\t  keyboard OPENING status file \n");
			FILE *fp = fopen((char *)metadata->statusOutFileName, "w") ;
			
			if (fp == NULL)
			{
				//writeLogEntry((unsigned char *)"In Keyborad thread: Status File open failed\n");
				printf("[status file: Failure to open\n");
				exit(0) ;
			}
			
			fclose(fp);
			
			printf("Now checking for TTL and flooding if required\n");
			if(metadata->status_ttl >= 1)
			{
				struct CachePacket cp;
				printf("Flood the status files command into the network\n");
				cp.status_type=0x02;
				cp.status=0;
				UCHAR msg_uoid[SHA_DIGEST_LENGTH];
				cp.msgLifetimeInCache=metadata->msgLifeTime;
				LOCK_ON(&msgCacheLock);
				GetUOID( const_cast<char *> ("msg"), msg_uoid, sizeof(msg_uoid));
				MessageCache[string((const char*)msg_uoid,SHA_DIGEST_LENGTH)]=cp;
				LOCK_OFF(&msgCacheLock);
				
				printf("Cache packet added to message cache with UOID:%s\n",(char*)msg_uoid);
				LOCK_ON(&currentNeighboursLock);
				NEIGHBOUR_MAP_ITERATOR iter=currentNeighbours.begin();
				for(;iter!=currentNeighbours.end();)
				{
					struct Message msg;
					msg.ttl=metadata->status_ttl;
					memcpy(msg.uoid,msg_uoid,SHA_DIGEST_LENGTH);
					msg.statusType=0x02;
					msg.status=2;
					msg.msgType=0xAC;
					safePushMessageinQ((*iter).second,msg,"keyborad:status files");
					iter++;
					printf("Pushed message for socket no%d",(*iter).second);
				}
				LOCK_OFF(&currentNeighboursLock);
				
			}
			
			LOCK_ON(&statusMsgLock);
			statusTimerFlag = 1 ;
			pthread_cond_wait(&statusMsgCV, &statusMsgLock);
			LOCK_OFF(&statusMsgLock);
			//After getting signal,write the status messages!!!
			writeStatusFileOutput();
		}
	}

	free(input);
	printf("[Keyboard]\t********** Leaving keyboard thread \n");
}
