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
            string sampleRec("SAMPLE1");

            tempMetadata.keywords->push_back(sampleRec);

            sampleRec = "SAMPLE2";
            tempMetadata.keywords->push_back(sampleRec);



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
		}
		
		else if(strstr((char*)input,"search ")!=NULL)
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
			printf("the value is:%s\n",value);
			list <string> keyword_search;
			string SToken((char*)value);
			
			//Clear the search parameters first.
			fileDisplayIndexMap.clear();
			LOCK_ON(&countOfSearchResLock);
			countOfSearchRes=0;
			LOCK_OFF(&countOfSearchResLock);
			
			if(!strcasecmp((char*)pattern,"keywords"))
			{
				printf("the keywords are:\n");
				int start=1;
				int count=0;
				for(int x=1;x<SToken.length()-1;x++)
				{
					if(SToken[x]==' ')
					{
						string part(SToken,start,count);
						printf("The part being pushed is:%s\n",part.c_str());
						keyword_search.push_back(part);
						count=0;
						start=x+1;
						continue;
					}
					count++;
				}
				printf("Call initiate keyword search method\n");
				constructSearchMsg(value,0x03);
				initiateLocalKeywordSearch(keyword_search);
				//break;
			}
			
			if(!strcasecmp((char*)pattern,"sha1hash"))
			{
				unsigned char* hash=convertToHex(value,20);
				printf("Call initiate sha1 search method\n");
				constructSearchMsg(value,0x02);
				initiateLocalSha1Search(hash);
			}
			
			if(!strcasecmp((char*)pattern,"filename"))
			{
				printf("Call initiate filename search method\n");
				printf("Call initiate filename search method for filename=%s\n",value);
				constructSearchMsg(value,0x01);
				initiateLocalFilenameSearch(value);
			}
			//Use locks to prevent the keyboard from looping back-up
			pthread_mutex_lock(&searchMsgLock) ;
			pthread_cond_wait(&searchMsgCV, &searchMsgLock);
			pthread_mutex_unlock(&searchMsgLock) ;
		}
	}

	free(input);
	printf("[Keyboard]\t********** Leaving keyboard thread \n");
}
