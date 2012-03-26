/*
 * iniParser.cpp
 *
 *  Created on: Mar 24, 2012
 *      Author: gkowshik
 */

#include "main.h"

using namespace std;

void fillDefaultMetaData()
{
	printf("inside fill default\n");
	metadata = (struct MetaData *)malloc(sizeof(MetaData));
	metadata->beaconList = new list<struct NodeInfo *> ;
	metadata->portNo = 0;
	gethostname((char *)metadata->hostName, 256);
	metadata->location = 0;
	memset(metadata->homeDir, '\0', 512);
	strncpy((char *)metadata->logFileName, "servant.log", strlen("servant.log"));
	metadata->logFileName[strlen("servant.log")]='\0';
	metadata->autoShutDown = 900;
	metadata->ttl = 30;
	metadata->msgLifeTime = 30;
	metadata->getMsgLifeTime = 300;
	metadata->initNeighbor = 3;
	metadata->joinTimeOut = 15;
	metadata->joinTimeOutFixed = metadata->joinTimeOut;
	metadata->autoShutDownFixed = metadata->autoShutDown;
	metadata->checkResponseTimeout = 15;
	metadata->keepAliveTimeOut = 60;
	metadata->minNeighbor = 2;
	metadata->noCheck = 0;
	metadata->cacheProb = 0.1;
	metadata->storeProb = 0.1;
	metadata->neighborStoreProb = 0.2;
	metadata->cacheSize = 500;
	metadata->isBeacon = false;
	metadata->beaconRetryTime = 30;
	memset(metadata->node_id, '\0', 265) ;
	memset(metadata->node_instance_id, '\0', 300) ;
	printf("fill default done\n");
}

//Creates the node instacne id, using the timestamp
void setNodeInstanceId(){
	// Should be called once node_id of the node is set
	time_t sec;
	sec = time (NULL) ;
	sprintf((char *)metadata->node_instance_id, "%s_%ld", metadata->node_id, (long)sec) ;
//	printf("%s\n", myInfo->node_instance_id) ;
}


void trim(unsigned char* line)
{
	unsigned char temp[512];
	int noOfChar=0;

	for(int i=0;line[i]!='\0';i++)
	{
		if(line[i]=='\"')
		{
			while(line[i]!='\"')
			{
				if(line[i]=='\0')
				{
					i--;
					break;
				}
				temp[noOfChar++]=line[i];
				i++;
			}
		}
		else
		{
			if(line[i]=='\n' || line[i]==' ' || line[i]=='\r' || line[i]=='\t')
				continue;
			else
				temp[noOfChar++]=line[i];
		}
	}
	temp[noOfChar]='\0';
	strncpy((char *)line, (char *)temp, strlen((char *)temp));
	line[strlen((char *)temp)]='\0';
}

void parseINI(char *fileName)
{
	FILE *f=fopen((char *)fileName,"r");
	int initSection=0;
	int beaconSection=0;
	int visitedFlag[20];
	memset(&visitedFlag, 0, sizeof(visitedFlag));

	if(f==NULL)
	{
		printf("Error in file opening\n");
		writeLogEntry((unsigned char *)"Error in Ini file opening\n");
		exit(1);
	}


	//File found and opened
	unsigned char line[512];
	unsigned char *key, *value;

	//reading the ini file to parse it
	printf("file opened\n");
	while(fgets((char *)line, 511,f)!=NULL)
	{
		//removes trailing space, tabs, new line, CR
		trim(line);
		if(!(*line))
			continue;
		//printf("2. line is: %s\n", line);
		if(strcasecmp((char *)line, "[init]")==0)
		{
			if(visitedFlag[0]==0)	//Not parsed
				visitedFlag[0]=1;	//Parsed
			else
			{
				visitedFlag[0]=2;	//Duplicate found
				continue;
			}

			initSection = 1;
			beaconSection=0;
			continue;
		}
		else
		{
			if(strcasecmp((char *)line, "[beacons]")==0)
			{
				if(visitedFlag[1]==0)	//Not parsed
					visitedFlag[1]=1;	//Parsed
				else
				{
					visitedFlag[1]=2;	//Duplicate detected
					continue;
				}

				beaconSection=1;
				initSection=0;
				continue;
			}
			else
			{
				key=(unsigned char *)strtok((char *)line,"=");

				if(initSection)
				{
					if(!strcasecmp((char *)key, "port"))
					{
						if(visitedFlag[2]==0)
						{
							if(visitedFlag[0]==2)	//In duplicate section
								continue;
							value=(unsigned char *)strtok(NULL,"=");
							metadata->portNo=atoi((char *)value);
							visitedFlag[2]=1;
						}
						else
							continue;				//Duplicate found
					}
					else if(!strcasecmp((char *)key, "location"))
					{
						if(visitedFlag[3]==0)
						{
							if(visitedFlag[0]==2)
								continue;
							value=(unsigned char *)strtok(NULL,"=");
							metadata->location=atol((char *)value);
							visitedFlag[3]=1;
						}
						else
							continue;
					}
					else if(!strcasecmp((char *)key, "homedir"))
					{
						if(visitedFlag[4]==0)
						{
							if(visitedFlag[0]==2)
								continue;
							value=(unsigned char *)strtok(NULL,"=");
							strncpy((char *)metadata->homeDir,(char *)value, strlen((char *)value));
							metadata->homeDir[strlen((char *)value)]='\0';
							visitedFlag[4]=1;
						}
						else
							continue;
					}
					else if(!strcasecmp((char *)key, "logfilename"))
					{
						if(visitedFlag[5]==0)
						{
							if(visitedFlag[0]==2)
								continue;
							value=(unsigned char *)strtok(NULL,"=");
							strncpy((char *)metadata->logFileName,(char *)value, strlen((char *)value));
							metadata->logFileName[strlen((char *)value)]='\0';
							visitedFlag[5]=1;
						}
						else
							continue;
					}
					else if(!strcasecmp((char *)key, "autoshutdown"))
					{
						if(visitedFlag[6]==0)
						{
							if(visitedFlag[0]==2)
								continue;
							value=(unsigned char *)strtok(NULL,"=");
							metadata->autoShutDown=atoi((char *)value);
							visitedFlag[6]=1;
						}
						else
							continue;
					}
					else if(!strcasecmp((char *)key, "ttl"))
					{
						if(visitedFlag[7]==0)
						{
							if(visitedFlag[0]==2)
								continue;
							value=(unsigned char *)strtok(NULL,"=");
							metadata->ttl=atoi((char *)value);
							visitedFlag[7]=1;
						}
						else
							continue;
					}
					else if(!strcasecmp((char *)key, "msglifetime"))
					{
						if(visitedFlag[8]==0)
						{
							if(visitedFlag[0]==2)
								continue;
							value=(unsigned char *)strtok(NULL,"=");
							metadata->msgLifeTime=atoi((char *)value);
							visitedFlag[8]=1;
						}
						else
							continue;
					}
					else if(!strcasecmp((char *)key, "getmsglifetime"))
					{
						if(visitedFlag[9]==0)
						{
							if(visitedFlag[0]==2)
								continue;
							value=(unsigned char *)strtok(NULL,"=");
							metadata->getMsgLifeTime=atoi((char *)value);
							visitedFlag[9]=1;
						}
						else
							continue;
					}
					else if(!strcasecmp((char *)key, "initneighbors"))
					{
						if(visitedFlag[10]==0)
						{
							if(visitedFlag[0]==2)
								continue;
							value=(unsigned char *)strtok(NULL,"=");
							metadata->initNeighbor=atoi((char *)value);
							visitedFlag[10]=1;
						}
						else
							continue;
					}
					else if(!strcasecmp((char *)key, "jointimeout"))
					{
						if(visitedFlag[11]==0)
						{
							if(visitedFlag[0]==2)
								continue;
							value=(unsigned char *)strtok(NULL,"=");
							metadata->joinTimeOut=atoi((char *)value);
							metadata->checkResponseTimeout=atoi((char *)value);
							visitedFlag[11]=1;
						}
						else
							continue;
					}
					else if(!strcasecmp((char *)key, "keepalivetimeout"))
					{
						if(visitedFlag[12]==0)
						{
							if(visitedFlag[0]==2)
								continue;
							value=(unsigned char *)strtok(NULL,"=");
							metadata->keepAliveTimeOut=atoi((char *)value);
							visitedFlag[12]=1;
						}
						else
							continue;
					}
					else if(!strcasecmp((char *)key, "minneighbors"))
					{
						if(visitedFlag[13]==0)
						{
							if(visitedFlag[0]==2)
								continue;
							value=(unsigned char *)strtok(NULL,"=");
							metadata->minNeighbor=atoi((char *)value);
							visitedFlag[13]=1;
						}
						else
							continue;
					}
					else if(!strcasecmp((char *)key, "nocheck"))
					{
						if(visitedFlag[14]==0)
						{
							if(visitedFlag[0]==2)
								continue;
							value=(unsigned char *)strtok(NULL,"=");
							metadata->noCheck=atoi((char *)value);
							visitedFlag[14]=1;
						}
						else
							continue;
					}
					else if(!strcasecmp((char *)key, "cacheprob"))
					{
						if(visitedFlag[15]==0)
						{
							if(visitedFlag[0]==2)
								continue;
							value=(unsigned char *)strtok(NULL,"=");
							metadata->cacheProb=atof((char *)value);
							visitedFlag[15]=1;
						}
						else
							continue;
					}
					else if(!strcasecmp((char *)key, "storeprob"))
					{
						if(visitedFlag[16]==0)
						{
							if(visitedFlag[0]==2)
								continue;
							value=(unsigned char *)strtok(NULL,"=");
							metadata->storeProb=atof((char *)value);
							visitedFlag[16]=1;
						}
						else
							continue;
					}
					else if(!strcasecmp((char *)key, "neighborstoreprob"))
					{
						if(visitedFlag[17]==0)
						{
							if(visitedFlag[0]==2)
								continue;
							value=(unsigned char *)strtok(NULL,"=");
							metadata->neighborStoreProb=atof((char *)value);
							visitedFlag[17]=1;
						}
						else
							continue;
					}
					else if(!strcasecmp((char *)key, "cachesize"))
					{
						if(visitedFlag[18]==0)
						{
							if(visitedFlag[0]==2)
								continue;
							value=(unsigned char *)strtok(NULL,"=");
							metadata->cacheSize=atoi((char *)value);
							visitedFlag[18]=1;
						}
						else
							continue;
					}
				}
				else	//Part of beacons section
				{
					if(!strcasecmp((char *)key, "retry"))
					{
						if(visitedFlag[19]==0)
						{
							if(visitedFlag[1]==2)
								continue;
							value=(unsigned char *)strtok(NULL, "=");
							metadata->beaconRetryTime = atoi((char *)value);
							visitedFlag[19]=1;
						}
						else
							continue;
					}
					else
					{
						value=(unsigned char *)strtok((char *)key, ":");
						struct NodeInfo *beaconNodeTemp = (struct NodeInfo*)malloc(sizeof(struct NodeInfo));
						//strncpy((char *)beaconNodeTemp->hostname, (char *)value, strlen((char *)value));
						for(unsigned int i=0;i<strlen((char *)value);i++)
							beaconNodeTemp->hostname[i] = value[i];
						beaconNodeTemp->hostname[strlen((char *)value)] = '\0';
						value=(unsigned char *)strtok(NULL,":");
						beaconNodeTemp->portNo = (unsigned int)atoi((char *)value);
						metadata->beaconList->push_back(beaconNodeTemp);
						printf("Host name is : %s\tHost Port no: %d\n", beaconNodeTemp->hostname, beaconNodeTemp->portNo);
					}

				}
			}
		}
	}
	fclose(f) ;
	printf("End of parsing\n");
}

void printMetaData()
{
	printf("Value for metadata is:--\n\n");
	printf("Port No: %d\n",metadata->portNo);
	printf("hostName: %s\n",metadata->hostName);
	printf("Location: %lu\n",metadata->location);
	printf("HomeDir: %s\n",metadata->homeDir);
	printf("logFileName: %s\n",metadata->logFileName);
	printf("autoShutDown: %d\n",metadata->autoShutDown);
	printf("TTL: %d\n",metadata->ttl);
	printf("msgLifeTime: %d\n",metadata->msgLifeTime);
	printf("getMsgLifeTime: %d\n",metadata->getMsgLifeTime);
	printf("initNeighbor: %d\n",metadata->initNeighbor);
	printf("joinTimeOut: %d\n",metadata->joinTimeOut);
	printf("keepAliveTimeOut: %d\n",metadata->keepAliveTimeOut);
	printf("minNeighbor: %d\n",metadata->minNeighbor);
	printf("noCheck: %d\n",metadata->noCheck);
	printf("cacheProb: %lf\n",metadata->cacheProb);
	printf("storeProb: %lf\n",metadata->storeProb);
	printf("neighborStoreProb: %lf\n",metadata->neighborStoreProb);
	printf("cacheSize: %d\n",metadata->cacheSize);
	printf("retry: %d\n",metadata->beaconRetryTime);
	printf("isBeacon: %d\n",metadata->isBeacon);
}
