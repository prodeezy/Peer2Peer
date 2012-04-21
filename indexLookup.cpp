#include "main.h"
#include <fstream>



/*
 *0==>SHA1
 *1==>MD5
 */
void toSHA1_MD5(UCHAR *str,int choice, UCHAR *buffer)
{
        //UCHAR *encoded_str = (UCHAR *)malloc(sizeof(unsigned char)*20);
        if(choice==0)
                SHA1(str, strlen((const char *)str), buffer);
        else
                MD5(str, strlen((const char *)str), buffer);
//      return encoded_str;
}

/**
 *  bitVector - unsigned char array of size 128
 *  word          - keyword
 */

void generateBitVector(UCHAR *word , UCHAR *bitVector)
{

	printf("________ Begin:%s _________\n", (char *) word);
	fflush(stdout);

	unsigned char tempSHA1[SHA_DIGEST_LENGTH];
	unsigned char tempMD5[MD5_DIGEST_LENGTH];

	MEMSET_ZERO(tempSHA1, SHA_DIGEST_LENGTH);
	MEMSET_ZERO(tempMD5 , MD5_DIGEST_LENGTH);

	//  printf("\t memset ZERO, strlen(word) : %d\n", strlen((char*)word));
	fflush(stdout);

	// Normalize to lowercase
	UCHAR lowerCaseWord[strlen((char *)word)];
	for(int i=0 ; i<strlen((char*)word) && word[i] != '\0' ; i++) {

		//                printf("\t\t ...toLower :%c \n", word[i]);
		lowerCaseWord[i]= tolower(word[i]);
	}
	strncpy((char *)word, (char *)lowerCaseWord, strlen((char *)word));

	//printf("\t toSHA1_MD5 \n");
	fflush(stdout);

	toSHA1_MD5(word, 0, tempSHA1);
	toSHA1_MD5(word, 1, tempMD5);

	//        printf("\t Done SHA1\n");
	fflush(stdout);

	// SHA1 Index  - right most 9 bits.
	int sha1Bits = (tempSHA1[SHA_DIGEST_LENGTH - 2] & 1)* 256
			+  (unsigned int)(tempSHA1[SHA_DIGEST_LENGTH - 1]);

	// MD5 Index  - right most 9 bits.
	int md5Bits = (tempMD5[MD5_DIGEST_LENGTH - 2] & 1)* 256
			+  (unsigned int)(tempMD5[MD5_DIGEST_LENGTH - 1]) ;


	//      printf("\t minus 9 rightmost \n");
	fflush(stdout);

	// Push to left side
	sha1Bits += 512;
	int numSha1Chars = sha1Bits / 8;
	int numMd5Chars = md5Bits / 8;

	int sha1WithoutChars = sha1Bits % 8;
	int md5WithoutChars = md5Bits % 8;

	bitVector[127 - numSha1Chars] |= (0x01 << (sha1WithoutChars));
	printf("SHA1 : %d\n", sha1Bits);


	// set the hex equivalent bits of md5 and sha1 indices.
	bitVector[127 - numMd5Chars] |= (0x01 << (md5WithoutChars));
	printf("MD5 : %d\n", md5Bits);

	//    printf("________ END _________\n");
}


void populateIndexes(struct FileMetadata f,unsigned int gfn)
{
	printf("The ORIGINAL sizes are:%d %d %d\n",BitVectorIndexMap.size(),FileNameIndexMap.size(),SHA1IndexMap.size());
	BitVectorIndexMap[string((char*)f.bitVector,128)].push_back((int)gfn);
	FileNameIndexMap[string((char*)f.fName)].push_back((int)gfn);
	SHA1IndexMap[string((char*)f.SHA1)].push_back((int)gfn);
	printf("The following has been pushed in the index with global file number:%d\n",gfn);
	printf("The bit-vector=\n");
	for(int i=0;i<128;i++)
		printf("%02x", f.bitVector[i]);
	printf("\nThe file name=%s\n",(char*)f.fName);
	printf("The SHA1=\n");
	for(int x=0;x<19;x++)
		printf("%02x",f.SHA1[x]);
	
	printf("The NEW sizes are:%d %d %d\n",BitVectorIndexMap.size(),FileNameIndexMap.size(),SHA1IndexMap.size());
}

void writeIndex()
{
	unsigned char keyword_fileName[256];
	sprintf((char*)keyword_fileName,"%s/kwrd_index",metadata->currWorkingDir);
	FILE *ptr=fopen((char*)keyword_fileName,"w");
	//unsigned char bV[128];
	if(ptr==NULL)
	{
		printf("File couldnt be opened\n");
		exit(0);
	}
	for(map<string, list<int > >::iterator x=BitVectorIndexMap.begin();x!=BitVectorIndexMap.end();x++)
	{
		for(int i=0;i<128;i++)
			fprintf(ptr,"%02x",(((*x).first).c_str())[i]);
		
		for(list<int>:: iterator l=(*x).second.begin();l!=(*x).second.end();l++)
			fprintf(ptr," %d",*l);
		
		fprintf(ptr,"\n");
	}
	
	fclose(ptr);
	
	unsigned char fNameIndex_fileName[256];
	sprintf((char*)fNameIndex_fileName,"%s/name_index",metadata->currWorkingDir);
	ptr=fopen((char*)fNameIndex_fileName,"w");
	if(ptr==NULL)
	{
		printf("File couldnt be opened\n");
		exit(0);
	}
	for(map<string, list<int > >::iterator x=FileNameIndexMap.begin();x!=FileNameIndexMap.end();x++)
	{
		fprintf(ptr,"%s",((*x).first).c_str());
		for(list<int>:: iterator l=(*x).second.begin();l!=(*x).second.end();l++)
			fprintf(ptr," %d",*l);
			
		fprintf(ptr,"\n");
	}
	
	fclose(ptr);
	
	unsigned char SHA1Index_fileName[256];
	sprintf((char*)SHA1Index_fileName,"%s/sha1_index",metadata->currWorkingDir);
	ptr=fopen((char*)SHA1Index_fileName,"w");
	if(ptr==NULL)
	{
		printf("File couldnt be opened\n");
		exit(0);
	}
	for(map<string, list<int > >::iterator x=SHA1IndexMap.begin();x!=SHA1IndexMap.end();x++)
	{
	
		for(int i=0;i<20;i++)
			fprintf(ptr, "%02x",((*x).first).c_str()[i]);
		for(list<int>::iterator l=(*x).second.begin();l!=(*x).second.end();l++)
			fprintf(ptr," %d",*l);
			
		fprintf(ptr,"\n");
	}
	fclose(ptr);
}

void readIndex()
{
	unsigned char tempbV[256];
	list <int> list1;
	string line;
	char * pch,* cstr;

	unsigned char keyword_fName[256];
	sprintf((char*)keyword_fName,"%s/kwrd_index",metadata->currWorkingDir);

	ifstream ptr((char*)keyword_fName);
	if(!ptr)
	{
		printf("File couldnt be opened\n");
		exit(0);
	}
	while(getline(ptr,line))
	{
		/*
		for(int x=0;x<256;x++)
			tempbV[x]=(char)line[x];
			
		for(int x=256;x<line.length();x++)
		{
			if(line[x]==' ')
			{				
		}
		*/
		
		strcpy (cstr, line.c_str());
		pch = strtok (cstr," ");
		unsigned char str[128];
		strcpy((char*)tempbV,pch);
		unsigned char *temp = (unsigned char *)convertToHex(tempbV, 128);
		for(int i=0;i<128;i++)
			str[i] = temp[i];
			
		pch=strtok (NULL," ");
		
		int no;
		while(pch!=NULL)
		{
			no=atoi(pch);
			list1.push_back(no);
			pch=strtok (NULL," ");
		}
		
		BitVectorIndexMap[string((char*)str,128)]=list1;
		list1.clear();
	}
	ptr.close();
	
	//Now read File Index Map
	
	unsigned char fileIndexName[256];
	unsigned char fileNameFromFile[256];
	sprintf((char*)fileIndexName,"%s/name_index/",metadata->currWorkingDir);
	ifstream ptr1((char*)fileIndexName);
	if(!ptr1)
	{
		printf("File couldnt be opened\n");
		exit(0);
	}
	while(getline(ptr1,line))
	{
		strcpy (cstr, line.c_str());
		pch = strtok (cstr," ");
		//unsigned char str[129];
		strcpy((char*)fileNameFromFile,pch);
		
		pch=strtok (NULL," ");
		
		int no;
		while(pch!=NULL)
		{
			no=atoi(pch);
			list1.push_back(no);
			pch=strtok (NULL," ");
		}
		
		FileNameIndexMap[string((char*)fileNameFromFile,strlen((char*)fileNameFromFile))]=list1;
		list1.clear();
	}
	ptr1.close();
	
	//Now read sha1 index
	
	unsigned char sha1IndexFileName[256];
	sprintf((char*)sha1IndexFileName,"%s/sha1_index/",metadata->currWorkingDir);
	ifstream ptr2((char*)sha1IndexFileName);
	unsigned char tempsha1[40];
	unsigned char finalsha1[20];
	if(!ptr2)
	{
		printf("File couldnt be opened\n");
		exit(0);
	}
	while(getline(ptr2,line))
	{
		strcpy (cstr, line.c_str());
		pch = strtok (cstr," ");
		//unsigned char str[129];
		strcpy((char*)tempsha1,pch);
		
		strncpy((char *)finalsha1, (char *)convertToHex(tempsha1, 20), 20);
		
		pch=strtok (NULL," ");
		
		int no;
		while(pch!=NULL)
		{
			no=atoi(pch);
			list1.push_back(no);
			pch=strtok (NULL," ");
		}
		
		SHA1IndexMap[string((char*)finalsha1,strlen((char*)finalsha1))]=list1;
		list1.clear();
	}
	ptr2.close();
}

int incfNumber()
{
	unsigned char fName[256];
	int globalfNumber =0;
	string number;
	sprintf((char*)fName,"%s/.fileNumber",metadata->currWorkingDir);
	
	//FILE *fptr = fopen((char*)fName,"r+");
	
	ifstream fptr;
	fptr.open((char*)fName);
	if(!fptr.is_open())
	{
		printf("The file number file was not found or couldnt be found\nHence creating one ... %s\n",fName);
		fflush(stdout);
		//doLog("//The file number file was not found\nThe node will exit\n");
		//exit(0);
		FILE *fptr1 = fopen((char*)fName,"w+");
		if(fptr==NULL)
		{
			printf("File was not created by fopen\n");
		}
		int num=0;
		fprintf(fptr1,"%d",num);
		fclose(fptr1);
		fptr.open((char*)fName);
	}
	
	getline(fptr,number);
	globalfNumber=atoi(number.c_str());
	printf("The global file number is:%d\n",globalfNumber);
	fflush(stdout);
	
	globalfNumber++;
	fptr.close();
	
	ofstream fptr1;
	fptr1.open((char*)fName);
	if(!fptr1.is_open())
	{
		printf("The file number file was not found or couldnt be found\n");
		//doLog("//The file number file was not found\nThe node will exit\n");
		exit(0);
	}
	
	fptr1 << globalfNumber;
	fptr1.close();
	return globalfNumber;
}

int hexchar2int(unsigned char hexchar) 
{ 
	switch (hexchar) 
	{ 
		case '0': return 0; 
		case '1': return 1; 
		case '2': return 2; 
		case '3': return 3;
		case '4': return 4;
		case '5': return 5;
		case '6': return 6;
		case '7': return 7;
		case '8': return 8;
		case '9': return 9;
		case 'a': return 10;
		case 'b': return 11;
		case 'c': return 12;
		case 'd': return 13;
		case 'e': return 14;
		case 'f': return 15; 
	} 
} 

unsigned char hexstring2bin(unsigned char hi_char, unsigned char lo_char) 
{ 
	int hi_value = 0; 
	int lo_value = 0; 
	hi_value = hexchar2int(hi_char); 
	lo_value = hexchar2int(lo_char); 
	return (unsigned char) ((hi_value * 16 + lo_value) & 0x0ff); 
}

unsigned char* convertToHex(unsigned char *str, int len)
{
	UCHAR *out=(UCHAR*)malloc(sizeof(UCHAR)*len); 
    for (int i=0; i < len; i++) 
	{ 
        unsigned char hi_char = str[i*2]; 
        unsigned char lo_char = str[i*2+1]; 
        /* convert 2 hexstring characters to 1 byte at a time */ 
        out[i] = hexstring2bin(hi_char, lo_char); 
    }
	return out;
} 

void writeMetadataToFile(struct FileMetadata fMetadata,int globalfNo)
{
	unsigned char fName[256];
	sprintf((char*)fName,"%s/file/%d.meta",metadata->currWorkingDir,globalfNo);
	FILE *fptr=fopen((char*)fName,"w");
	
	if(fptr==NULL)
	{
		printf("File couldnt be opened\n");
		exit(0);
	}
	
	printf("Going to write to %d.meta file\n",globalfNo);
	
	fprintf(fptr,"%s\n","[metadata]");
	fprintf(fptr,"%s=%s\n","File Name",fMetadata.fName);
	fprintf(fptr,"%s=%ld\n","File Size",fMetadata.fSize);
	fprintf(fptr,"%s","SHA1=");
	for(int x=0;x<19;x++)
		fprintf(fptr,"%02x",fMetadata.SHA1[x]);
	fprintf(fptr,"%02x\n",fMetadata.SHA1[19]);
	
	fprintf(fptr,"%s","NONCE=");
	for(int x=0;x<19;x++)
		fprintf(fptr,"%02x",fMetadata.NONCE[x]);
	fprintf(fptr,"%02x\n",fMetadata.NONCE[19]);

	fprintf(fptr,"%s","Keywords=");
	for(list<string >::iterator i=fMetadata.keywords->begin();
	i!=fMetadata.keywords->end();
	i++)
		fprintf(fptr,"%s ",(*i).c_str());

	fprintf(fptr,"%s","\nBit-vector=");
	for(int i=0;i<128;i++)
		fprintf(fptr, "%02x", fMetadata.bitVector[i]);
		
	fclose(fptr);
}

void writeDataToFile(struct FileMetadata fMetadata,int globalfNo)
{
	unsigned char fName[256];
	sprintf((char*)fName,"%s/file/%d.data",metadata->currWorkingDir,globalfNo);
	
	FILE *f1=fopen((char*)fMetadata.fName,"r");
	char c;
	FILE *f2=fopen((char*)fName,"w");
	
	c = getc (f1);
    while (c != EOF)
	{
      putc(c,f2);
	  //printf("The character is:%c\n",c);
	  c = getc (f1);
    }
	fclose(f1);
	fclose(f2);
}

void initiateLocalFilenameSearch(unsigned char *fileToBeSearched)
{
	//list<int> fileIDs;
	printf("Inside initiateLocalFilenameSearch for file name:%s\n",fileToBeSearched);
	
	
	for(map<string, list<int > >::iterator mapIter=FileNameIndexMap.begin();
			mapIter!=FileNameIndexMap.end();
			mapIter++)
//	for(map<string, list<int > >::iterator f=FileNameIndexMap.begin();f!=FileNameIndexMap.end();f++)
	{
		if((strcasecmp(((*mapIter).first.c_str()),(char*)fileToBeSearched))==0)
		{
			printf("FILE match found with :%s\n",(*mapIter).first.c_str());
			printf("Size of the list is:%d\n",(*mapIter).second.size());
	
			list<int> fNumbers = (*mapIter).second;
			for(list<int>::iterator listIter = fNumbers.begin();
					listIter!=fNumbers.end();
					listIter++)
//			for (list<int>::iterator h = (*f).second.begin(); 
	//				h != (*f).second.end(); 
		//			h++)
			{
			
			}
		}
		printf("LOOPING NOW\n");
	}
	printf("************************The File name search is complete!!!!\n");
	
	/*
	//Check if the filename was obtained.
	if(fileIDs.size()==0)
	{
		//File name was not present in the index.
		//Hence we dont display any results, directly call function to construct search msg.
		constructSearchMsg();
		return;
	}
	
	//The filename was found in the index,create metadata and display it.
	for(list<int>::iterator i=fileIDs.begin();i!=fileIDs.end();i++)
	{
		struct FileMetadata populatedFileMeta = createFileMetadata(*i);
		displayFileMetadata(populatedFileMeta);
		
	}
	*/
	
	//Call function to construct search msg.
}

void initiateLocalKeywordSearch(UCHAR *v)
{
	string SToken((char*)v);
	list <string> keyword_search;
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
	}
	
	unsigned char generatedBitVector[128];
	unsigned char *bitvectorFromMap;
	unsigned char result;
	//list<int> matchedFileNos;
	int match=1;
	MEMSET_ZERO(generatedBitVector,128);
	printf("Checking locally for keyword match\n");
	for(list<string>::iterator i=keyword_search.begin();i!=keyword_search.end();i++)
		generateBitVector((unsigned char*)(*i).c_str() , generatedBitVector);
	
	for(int i=0;i<128;i++)
		printf("%02x", generatedBitVector[i]);
	for(map<string, list<int> >::iterator it = BitVectorIndexMap.begin(); it != BitVectorIndexMap.end(); ++it)
	{
		bitvectorFromMap=(unsigned char*)((*it).first).c_str();
		printf("The bit-vector obtained from the Map is\n");
		for(int i=0;i<128;i++)
				printf("%02x", bitvectorFromMap[i]);
		
		//And the generated and stored bit vector
		int checkKeywords=1;
		for(int x=0;x<128;x++)
		{
			//printf(" {%02x} vs {%02x}\n", generatedBitVector[x] , bitvectorFromMap[x]);
			if(generatedBitVector[x] != bitvectorFromMap[x])
			{
				
				checkKeywords=0;
				break;
			} 
		}
			//printf("check keywords:%d\n", checkKeywords);
		//compare keywords individually of files having matching bit-vector
		if(checkKeywords==1)
		{
			printf("The bit-vector matched for this file\nPerform keyword search\n");
			//This is the for-loop for files
			printf("The size of the list is:%d",(*it).second.size());
			for(list<int>::iterator y=(*it).second.begin();y!=(*it).second.end();y++)
			{
				printf("the file no is:");
				struct FileMetadata metadataMatchedFile=createFileMetadata(*y);
				
				//match=0;
				printf("Now iterate over the keywords from the metadata\n");
				for(list<string>::iterator z=keyword_search.begin();z!=keyword_search.end();z++)
				{
					match=0;
					for(list<string>::iterator a=metadataMatchedFile.keywords->begin();a!=metadataMatchedFile.keywords->end();a++)
					{
						printf("{%s} vs {%s}\n",(char*)(*a).c_str(),(char*)(*z).c_str());
						if(strcasecmp((char*)(*a).c_str(),(char*)(*z).c_str())==0)
						{
							match=1;
							break;
						}
					}
					if(match==1)
						continue;
					else
						break;
				}
				if(match==1)
				{
					printf("All keywords match\n");
					pthread_mutex_lock(&countOfSearchResLock);
					countOfSearchRes++;
					displayFileMetadata(metadataMatchedFile);
					pthread_mutex_unlock(&countOfSearchResLock);
					//matchedFileNos.push_back((*y));
				}
			}
			printf("For loop exited\n");
		}
		else
			printf("The bit-vector did not match for this file\n");
	}
}

void initiateLocalSha1Search(unsigned char* hashvalue)
{
	for(map<string, list<int > >::iterator x=SHA1IndexMap.begin();x!=SHA1IndexMap.end();x++)
	{
		if((strcasecmp(((*x).first).c_str(),(char*)hashvalue))==0)
		{
			for (list<int >::iterator it = (*x).second.begin(); it != (*x).second.end(); ++it)
			{
				struct FileMetadata populatedFileMeta = createFileMetadata(*it);
				pthread_mutex_lock(&countOfSearchResLock);
				countOfSearchRes++;
				displayFileMetadata(populatedFileMeta);
				pthread_mutex_unlock(&countOfSearchResLock);
			}
		}	
	}
}

struct FileMetadata createFileMetadata(int fNo)
{
	unsigned char *fileName[256];	
	sprintf((char *)fileName, "%s/file/%d.meta", metadata->currWorkingDir, fNo);
	struct FileMetadata fMeta;
	MEMSET_ZERO(&fMeta, sizeof(fMeta));

	MEMSET_ZERO(&fMeta.NONCE, 128);
	fMeta.keywords =  new list<string >();
	MEMSET_ZERO(&fMeta.bitVector, 128);
	
	GetUOID( const_cast<char *> ("FileID"), fMeta.fileID, sizeof(fMeta.fileID));
	
	printf("The file ID obtained is:%s\n",fMeta.fileID);
	
	string line;
	ifstream myfile((char*)fileName,ifstream::in);
	unsigned char* key;
	unsigned char* value;
	printf("start reading file\n");
	if (myfile.is_open())
	{
		while ( myfile.good() )
		{
			getline (myfile,line);
			printf("The string obtained is:%s\n",line.c_str());
			fflush(stdout);
			
			if(strstr((char*)line.c_str(),"[metadata]"))
				continue;
			else
			{
				key=(unsigned char*)strtok((char*)line.c_str(),"=");
				printf("The key is:%s\t",key);
				fflush(stdout);
				value=(unsigned char*)strtok(NULL,"\n");
				printf("The Value is:%s\n",value);
				
				if((strcasecmp((char*)key,"File Name"))==0)
				{
						printf("parse FILE NAME\n");
						fflush(stdout);
						strncpy((char *)fMeta.fName, (char *)value, strlen((char *)value));
					printf("In create metadata,copy one:%s the file name is:%s\n",(char *)value,(char *)fMeta.fName);
				}
				
				if((strcasecmp((char*)key,"File Size"))==0){
						printf("parse file size\n");
						fflush(stdout);
						fMeta.fSize = atoi((char *)value);
					}
				if((strcasecmp((char*)key,"SHA1"))==0)
				{
						printf("parse SHA1\n");
						fflush(stdout);
						printf("SHA1\n");
						unsigned char *str = convertToHex(value, 20);
						memcpy((char*)fMeta.SHA1,(char*)str,SHA_DIGEST_LENGTH);
				}
					
				if((strcasecmp((char*)key,"NONCE"))==0)
				{
						printf("parse NONCE\n");
						fflush(stdout);		
						unsigned char *str = convertToHex(value, 20);
					memcpy((char*)fMeta.NONCE,(char*)str,SHA_DIGEST_LENGTH);
				}
				
				if((strcasecmp((char*)key,"Keywords"))==0)
				{
						printf("parse keywords\n");
						fflush(stdout);

						int start=0;
					int count=0;	
					string strValue((char*)value);
					for(int x=0;x<strValue.length();x++)
					{
						if(strValue[x]==' ')
						{
							string part(strValue,start,count);
							printf("The part being pushed is:%s the start:%d count:%d\n",part.c_str(),start,count);
							fMeta.keywords->push_back(part);
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
						string part(strValue,start,count);
						printf("The part being pushed is:%s the start:%d count:%d\n",part.c_str(),start,count);
						fMeta.keywords->push_back(part);
					}
				}
				
				if((strcasecmp((char*)key,"bit-vector"))==0)
				{
						printf("parse bit-vector\n");
						fflush(stdout);
						unsigned char *hexBitVector=convertToHex(value,128);
					strncpy((char*)fMeta.bitVector,(char*)hexBitVector,strlen((char*)fMeta.bitVector));
				}
			}
		}
		myfile.close();
	}
	printf("The file metadata is populated\n");
	return fMeta;
}

void displayFileMetadata(struct FileMetadata fMetadata)
{
	printf("****The file data is being displayed****\n");
	printf("[%d] FileId=",countOfSearchRes);
	int x=0;
	for(;x<20;)
	{
		printf("%02x",fMetadata.fileID[x]);
		x++;
	}
	printf("\n");
	printf("    FileName=%s\n",fMetadata.fName);
	printf("    FileSize=%d\n",fMetadata.fSize);
	printf("    SHA1=");
	int y=0;
	for(;y<SHA_DIGEST_LENGTH;)
	{
		printf("%02x",fMetadata.SHA1[y]);
		y++;
	}
	printf("\n");
	printf("    Nonce=");
	int z=0;
	for(;z<20;)
	{
		printf("%02x",fMetadata.NONCE[z]);
		z++;
	}
	printf("\n");
	printf("    Keywords=");
	list<string>::iterator it=fMetadata.keywords->begin();
	for(;it!=fMetadata.keywords->end();it++)
		printf("%s ",(*it).c_str());
	printf("\nThe file is finished is displayed\n");
}

void constructSearchMsg(UCHAR *dataForMsg,UCHAR type)
{
	printf("Constructing the search message\n");
	struct CachePacket c_packet;
	c_packet.status=0;
	c_packet.msgLifetimeInCache=metadata->msgLifeTime;
	
	LOCK_ON(&currentNeighboursLock);
	NEIGHBOUR_MAP_ITERATOR x=currentNeighbours.begin();
	for(;x!=currentNeighbours.end();)
	{
		struct Message msg;
		msg.query=(UCHAR *)malloc(strlen((char*)dataForMsg));
		msg.q_type=type;
		msg.msgType=0xEC;
		msg.dataLen=strlen((char*)dataForMsg);
		strncpy((char*)msg.query,(char*)dataForMsg,msg.dataLen);
		/*
		int x=0;
		for(;x<msg.dataLen;)
		{
			msg.query[x]=dataForMsg[x];
			x++;
		}
		*/
		GetUOID( const_cast<char *> ("msg"), msg.uoid, sizeof(msg.uoid));
		msg.ttl=metadata->ttl;
		msg.status=2;
		safePushMessageinQ((*x).second,msg,"constructSearchMsg");
		printf("Message pushed in queue for neighbor:%d",(*x).first.portNo);
		x++;
	
		LOCK_ON(&msgCacheLock);
		MessageCache[string((char*)msg.uoid)]=c_packet;
		LOCK_OFF(&msgCacheLock);
	}
	LOCK_OFF(&currentNeighboursLock);
	printf("The search message is constructed and pushed in Q");
}

void updateLRU(int fNumber) {

	for(list<int>::iterator entry = cacheLRU.begin(); entry!=cacheLRU.end(); entry++) {
		if((*entry) == fNumber) {
			cacheLRU.erase(entry);
		}
	}
	cacheLRU.push_back(fNumber);
}

list<int> getAllFileInfo()
{
	list<int> allFiles;
	map<string, list<int> >::iterator w = FileNameIndexMap.begin();
	for(;w != FileNameIndexMap.end();)
	{
		list<int>::iterator z=(*w).second.begin();
		for(;z!=(*w).second.end();)
		{
			allFiles.push_back((*z));
			z++;
		}
		w++;
	}
	return allFiles;
}
