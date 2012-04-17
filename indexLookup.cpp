#include "main.h"
#include <fstream>

/*
 *0==>SHA1
 *1==>MD5
 */
 
unsigned char *toSHA1_MD5(unsigned char *str,int choice)
{
	unsigned char *encoded_str = (unsigned char *)malloc(sizeof(unsigned char)*20);
	if(choice==0)
		SHA1(str, strlen((const char *)str), encoded_str);
	else
		MD5(str, strlen((const char *)str), encoded_str);
	return encoded_str;
}

void generateBitVector(unsigned char*bv, unsigned char*kw)
{
	for(int i=0;i<strlen((char*)kw);i++)
		kw[i]=tolower(kw[i]);
	
	
}

void populateIndexes(struct FileMetadata f,unsigned int gfn)
{
	BitVectorIndexMap[string((char*)f.bitVector)].push_back((int)gfn);
	FileNameIndexMap[string((char*)f.fName)].push_back((int)gfn);
	SHA1IndexMap[string((char*)f.SHA1)].push_back((int)gfn);
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
	for(map<string, list<int > >::iterator x=BitVectorIndexMap.begin();x!=BitVectorIndexMap.end();x++)
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
	for(map<string, list<int > >::iterator x=BitVectorIndexMap.begin();x!=BitVectorIndexMap.end();x++)
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
		unsigned char str[129];
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

unsigned char* convertToHex(unsigned char *str, int len)
{
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
	for(list<string >::iterator i=fMetadata.keywords.begin();
	i!=fMetadata.keywords.end();
	i++)
		fprintf(fptr,"%s ",(*i).c_str());
		
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
	  printf("The character is:%c\n",c);
	  c = getc (f1);
    }
	fclose(f1);
	fclose(f2);
}
