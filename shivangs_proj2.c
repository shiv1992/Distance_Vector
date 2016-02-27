/*
 * shivangs_pro2.c : Distance Vector Protocol
 * Starts off with reading Topology file for the specific router
 * Created for CSE 589 Fall 2015 Programming Assignment 2
 * @author Shivang Trivedi
 * @created 26 October 2015
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/timerfd.h>

#define mCount 5

//Declartions
FILE *fp;
int routerSocket,neighbourSocket[5];
char buf[1000];
char *tok,*tok1;
int routerCount,neighbourCount;
int neighbourPacketCount[5]={0}; // Array for keeping received Packet count of individual Neighbouring routers
char serverData[5][50]; // Server(Router) Data storage
char neighbourData[4][50]; // Neighbour Data
char neighbourCostData[5][5];
int neighbourCost[5]={0};
int timeUpdate; // Update Time
int distanceTable[6];
//Server Seperate Data
char serverID[5]; // Server(Router) ID
char nexthopID[5]; // Router ID for the next hop to reach the Router ID from the Host Router
char serverIP[5][20]; // Server(Router) IP
char serverPort[5][5]; // Server(Router) Port 
int serverDistance[5]; // Cost to server
//
struct addrinfo router,*routero;
int mainrouterID,neighbourID[5]={0};
int masterRouter;
struct timeval timeout; // Timeout Interval
struct sockaddr_in tmpIP,tmpIP1; // Temporary IP Address
char updatesendID;
fd_set routerfds; // Router FDS


/*Data Structure for Packing Message*/

//For Data of Neighbours
struct neighbour{
	short nPort;
	unsigned long nIP;
	short nID;
	int nCost;
};

//Main Message Packet Structure
struct message{
	short updateNum;
	short sPort;
	unsigned long sIP;
	struct neighbour updateNode[mCount];
}mainNode,recNode;


//Topology File Read
void readTopologyFile(char *file)
{
	char c;
	char str[2];
	fp=fopen(file,"r");
	int i,n;

	while(1)
	{
		c = fgetc(fp);
		if( feof(fp) )
		{ 
		
		break ;
		}
		else
		{
			str[0]=c;
			str[1]='\0';
			strcat(buf,str);	
		}	
	}

	tok=strtok(buf,"\n");
	printf("\nTopology File content is :\n%s",tok);
	routerCount=atoi(tok); // Count of Total routers
	tok=strtok(NULL,"\n");
	printf("\n%s",tok);
	neighbourCount=atoi(tok); // Count of Total Neighbours
	n=routerCount+neighbourCount;

	for(i=0;i<n;i++)
	{
		tok=strtok(NULL,"\n");
		if(i<routerCount)
		{		
			strcpy(serverData[i],tok);
			printf("\n%s",serverData[i]);
		}
		else
		{		
		strcpy(neighbourCostData[i-routerCount],tok);
		printf("\n%s",neighbourCostData[i-routerCount]);

		}
	}
}


//Display Distance Vector Table ( [-1] value means Infinity Distance)
void displayDistanceTable()
{	int i;
	printf("\nThe Distance Table for RouterID %d is :",mainrouterID);
	printf("\nDestination ID\tNext Hop ID\tCost of Path");
	
	for(i=0;i<mCount;i++)
	{
		if(serverDistance[i]==-1)
		printf("\n    %d\t\t    %c\t\t    inf",i+1,nexthopID[i]);
		else			
		printf("\n    %d\t\t    %c\t\t    %d",i+1,nexthopID[i],serverDistance[i]);
	}

	printf("\nNeighbours : ");
	for(i=0;i<neighbourCount;i++)
	{
		printf("%d ",neighbourID[i]);
	}

	printf("\nPhysical Neighbour Link Cost : ");
	for(i=0;i<neighbourCount;i++)
	{
		printf("%d ",neighbourCost[i]);
	}
	
}

//Create Distance Vector Table ( [-1] value means Infinity Distance)
void createTopologyTable()
{
	int i,d,ng;
	int val[3];
	int ctr=0;

	for(i=0;i<=mCount;i++)
	{
		distanceTable[i]=-1;
	}

	for(i=0;i<neighbourCount;i++)
	{
		tok=strtok(neighbourCostData[i]," ");
	
		distanceTable[0]=atoi(tok);
		mainrouterID=distanceTable[0];//Main Router
		distanceTable[distanceTable[0]]=0;
	
		tok=strtok(NULL," ");
		ng=atoi(tok);
		neighbourID[ctr]=ng;
		ctr++;
		tok=strtok(NULL,"\0");	
		if(i==(neighbourCount-1))	
		d=atoi(tok);
		else
		d=atoi(tok)/10;
	
		distanceTable[ng]=d;
	}
}

//Creating master Socket
void createMasterSocketRouter(){

	char routerIP[20],routerPort[5];
	int ctr,s,i;
	
	printf("\nSelf Value %d",mainrouterID);	
	printf("\nRouter Port %s",serverPort[mainrouterID-1]);
	

	memset(&router, 0, sizeof(router));
	router.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
	router.ai_socktype = SOCK_DGRAM;	
		
	//Server Socket addrinfo
	if(getaddrinfo(serverIP[mainrouterID-1],serverPort[mainrouterID-1], &router, &routero) == -1)
	{
		printf("\nGetaddrinfo Error");
		exit(0);
	}
	else
	{
		printf("\nGetaddrinfo Done with Port %s",serverPort[mainrouterID-1]);
	}

	//Create Socket
	s = socket(routero->ai_family, routero->ai_socktype, routero->ai_protocol);
	if (s == -1)
	{
		printf("\nSocket Error");
		exit(0);	
	}
	else
	{
		printf("\nSocket Created");
		masterRouter=s;
	}

	
	//Bind socket to Self(Router ip and port)
	if (bind(s, routero->ai_addr, routero->ai_addrlen)==-1)
	{
		printf("\nBind Issue!!");
		exit(0);
	}
	else
	{
		printf("\nBind Done");
		
	}
	freeaddrinfo(routero);

}


//Packing Message in Structure Message
void packMessage(int flag){

	int i,j;
	short updateN=routerCount;
	char ch[2];
	
	ch[1]='\0';
	//printf("\nPacked Data : - ");

	if(flag==1)
	{	printf("\nUpdate Message");
		mainNode.updateNum=1; // For identifying Update Package
	}	
	else{
	mainNode.updateNum = updateN; 
	//printf("\nUpdate Number : %hu",mainNode.updateNum);
	}
	mainNode.sPort = (short)atoi(serverPort[mainrouterID-1]);
	//printf("\tServer Port : %hu",mainNode.sPort); 

	inet_aton(serverIP[mainrouterID-1], &tmpIP.sin_addr);
	mainNode.sIP=tmpIP.sin_addr.s_addr;
	//printf("\nUpdate IP : %lu",mainNode.sIP);

	for(i=0;i<mCount;i++)
	{
		inet_aton(serverIP[i], &tmpIP.sin_addr);
		mainNode.updateNode[i].nIP=tmpIP.sin_addr.s_addr;
		//printf("\nUpdateIP %i : %lu",i+1,mainNode.updateNode[i].nIP);		
		
		mainNode.updateNode[i].nPort = (short)atoi(serverPort[i]); 
		//printf("\nUpdatePort %i : %hu",i+1,mainNode.updateNode[i].nPort); 

		ch[0]=serverID[i];	
		mainNode.updateNode[i].nID=(short)atoi(ch);
		//printf("\nUpdateID %i : %hu",i+1,mainNode.updateNode[i].nID);
	
		if(flag==1)
		{	
			if(updatesendID==serverID[i])
			{
				mainNode.updateNode[i].nCost=neighbourCost[j];
				
			}
						
		}		
		else{
		mainNode.updateNode[i].nCost=serverDistance[i];
		//printf("\tUpdateCost %i : %d",i+1,mainNode.updateNode[i].nCost);	
		}
	}
}

//Disable Function
void disableServer(int val){
	
	int i;
	
	serverDistance[neighbourID[val]-1]=-1;
	for(i=val;i<neighbourCount-1;i++)
	{
		neighbourID[val]=neighbourID[val+1];
		neighbourCost[val]=neighbourCost[val+1];
		neighbourPacketCount[val]=neighbourPacketCount[val+1];
	}		
		
	neighbourCount--;
			
}

//Unpack Message
void unpackMessage(){
	
	int i,j;
	int uNum;
	int mPort;
	int mID;	
	char mIP[20];
	char nIP[5][30];
	char nPort[5][5];
	int nID[5];
	int nCost[5];
	int currCost;
	int updateFlag=0;	
		
	uNum=(int)recNode.updateNum;
	if(uNum==1)
	{	printf("\nUpdate Message Received");
		updateFlag=1;
		uNum=mCount;
	}	
	mPort=(int)recNode.sPort;
	
	tmpIP.sin_addr.s_addr=recNode.sIP;
	strcpy(mIP,inet_ntoa(tmpIP.sin_addr));
		
	for(i=0;i<mCount;i++)
	{
		tmpIP.sin_addr.s_addr=recNode.updateNode[i].nIP;
		strcpy(nIP[i],inet_ntoa(tmpIP.sin_addr));
	
		j=(int)recNode.updateNode[i].nPort;
		sprintf(nPort[i],"%d",j);	
	
		nID[i]=(int)recNode.updateNode[i].nID;
		
		nCost[i]=recNode.updateNode[i].nCost;
	
	}

	//Get Sending Server ID
	for(i=0;i<mCount;i++)
	{
		if(strcmp(mIP,nIP[i])==0 )
		{	mID=nID[i];
			break;		
		}
	}
	
	/*Update Link Cost coming from Updated Neighbour  <Update Command> */
	if(updateFlag==1)
	{
		for(j=0;j<neighbourCount;j++)
			{
				if(neighbourID[j]==mID)
				{	neighbourPacketCount[j]++;
					break;
				}
			}
		neighbourCost[j]=nCost[mainrouterID-1];
		if(serverDistance[neighbourID[j]-1]>neighbourCost[j])
		{
			serverDistance[neighbourID[j]-1]=neighbourCost[j];
			nexthopID[neighbourID[j]-1]=serverID[neighbourID[j]-1];
		}
		
	}
	
	/*Update Distance Table */
	else
	{	
		// Get Current Cost from Existing Table
		for(i=0;i<neighbourCount;i++)
		{	if(neighbourID[i] == mID)
			{
				neighbourPacketCount[i]++;
				break;
			}	
		}
		
		if(serverDistance[mID-1] > neighbourCost[i])
		{
			currCost=neighbourCost[i];
		}
		else
		{
			currCost=serverDistance[mID-1];
		}
		
		if(currCost==-1)
		{
			currCost=5000; //Set High Cost for infinite value
		}

		printf(" ");
		
		for(i=0;i<routerCount;i++)
		{	printf(" ");

			if( i!=(mainrouterID-1) )
			{ 
				if(serverDistance[i]==-1) //For Infinite
				{
					if(nCost[i]!=-1)
					{
						if(currCost == 5000)
						{								
							serverDistance[i]=nCost[i];
						}
						else
						{								
							serverDistance[i]=(currCost + nCost[i]);
						}
						nexthopID[i]=serverID[i];
					}
				}			
				else if( serverDistance[i] > (currCost + nCost[i]) ) //Normal Updation [Case 3]
				{
					
					if(currCost == 5000)
						{								
							serverDistance[i]=nCost[i];
						}
						else
						{								
							serverDistance[i]=(currCost + nCost[i]);
						}			
					nexthopID[i]=mID+'0';
				}	 
			
			}	
		
		}
	}

	printf("\nRECEIVED A MESSAGE FROM SERVER %d",mID);	

}

//Update Link Cost
void updateCost(int r,char *val)
{
	int i,bt,j,jFlag=0;
	printf(" ");
	if(strcmp(val,"inf")==0)
	{
		//serverDistance[r-1]=-1;
		for(j=0;j<neighbourCount;j++)
			{
				if(neighbourID[j]==r)
				{	//printf("\n%d",neighbourID[j]);
					break;
				}
			}
		neighbourCost[j]=-1;	
		//nexthopID[r-1]=serverID[r-1];
		disableServer(j);
		jFlag=1;	// Flag for not sending packet since Infinite Update
	}
	else
	{
		
		/* Edit*/
		for(j=0;j<neighbourCount;j++)
			{
				if(neighbourID[j]==r)
				{	//printf("\n%d",neighbourID[j]);
					break;
				}
			}
		neighbourCost[j]=atoi(val);
		printf("\n%d",neighbourCost[j]);
		updatesendID=neighbourID[j]+'0';
		displayDistanceTable();
		if(serverDistance[r-1]>neighbourCost[j])
		{
			serverDistance[r-1]=neighbourCost[j];
			nexthopID[r-1]=serverID[r-1];
		}
		packMessage(1); // Pack Message Before sending
	}
	
	
	if(jFlag==0)
	{	
		/* Sending Routing table to Selected Neighbour*/
		for(i=0;i<neighbourCount;i++)
		{
			if(neighbourID[i] == r) //Selecting neighbour to send
			{
				//Neighbour Socket addrInfo
				if(getaddrinfo(serverIP[neighbourID[i]-1], serverPort[neighbourID[i]-1], &router, &routero) == -1)
				{
					printf("\nGetaddrinfo Error");
					exit(0);
				}
				else
				{
					//printf("\nGetaddrinfo Done with Port %s",serverPort[neighbourID[i]-1]);
				}

				//Create Socket for New Router
				neighbourSocket[i] = socket(routero->ai_family, routero->ai_socktype, routero->ai_protocol);
				if (neighbourSocket[i] == -1)
				{
					printf("\nSocket Error");
					exit(0);	
				}
				else
				{
				
					//printf("\nSocket Created %d",neighbourSocket[i]);
				
					if((bt=sendto(neighbourSocket[i],&mainNode,sizeof(struct message),0,routero->ai_addr,routero->ai_addrlen))==-1)
					{
						printf("\nSend Error");
					}
					else
					{
						//printf("\nData Sent %d of %d bytes",i+1,bt);
						close(neighbourSocket[i]);
					}
					
				}
				break; // Break from loop after sending Updated Routing table
			}
		}
	}
	jFlag=0;	
}


// Server Data Seperation for Easy Access
void seperateServerData()
{
	int i,t,p;
	char ch[2];
	for(i=0;i<routerCount;i++)
	{
		tok=strtok(serverData[i]," ");
		
		strcpy(ch,tok);
		t=atoi(ch)-1;
			
		serverID[t]=ch[0];
				
		tok=strtok(NULL," ");
		strcpy(serverIP[t],tok);
		
		tok=strtok(NULL,"\r");
		strcpy(serverPort[t],tok);
		
		serverDistance[t]=distanceTable[i+1];
	}
	
	for(i=0;i<routerCount;i++)
	nexthopID[i]=serverID[i];

	nexthopID[mainrouterID-1]=serverID[mainrouterID-1];
	for(i=0;i<neighbourCount;i++)
	{
		nexthopID[neighbourID[i]-1]=neighbourID[i] + '0';
		neighbourCost[i]=serverDistance[neighbourID[i]-1];
	}
		
	displayDistanceTable();
}

//Function to Send Routing table to Neighbours
void sendRoutingTable(){

	int i,bt;

	//Creating sockets for Neighbours and Storing in RouterFDS 
	for(i=0;i<neighbourCount;i++)
	{
		//Neighbour Socket addrInfo
		if(getaddrinfo(serverIP[neighbourID[i]-1], serverPort[neighbourID[i]-1], &router, &routero) == -1)
		{
			printf("\nGetaddrinfo Error");
			exit(0);
		}
		else
		{
			//printf("\nGetaddrinfo Done with Port %s",serverPort[neighbourID[i]-1]);
		}

		//Create Socket for New Router
		neighbourSocket[i] = socket(routero->ai_family, routero->ai_socktype, routero->ai_protocol);
		if (neighbourSocket[i] == -1)
		{
			printf("\nSocket Error");
			exit(0);	
		}
		else
		{
			
			if((bt=sendto(neighbourSocket[i],&mainNode,sizeof(struct message),0,routero->ai_addr,routero->ai_addrlen))==-1)
			{
				printf("\nSend Error");
			}
			else
			{
				//printf("\nData Sent %d of %d bytes",i+1,bt);
				close(neighbourSocket[i]);
			}
				
		}

	}

	
}



//Main Function
int main(int argc, char *argv[]){

	char input[20];
	char string[10];
	char buff[50];
	int fdMax;
	int s,valread;
	int r1,r2,i,j,flag,abc=0,activity,len;
	int packetCount=0;
	int fd;
	int dFlag=0;	
	int disServer=0; // Server Diable Parameter
	int absenceCount=0;	
	struct itimerspec new_value,curr_value;	

	timeUpdate=atoi(argv[4]);	
	printf("\nThe time-interval is %d",timeUpdate);
	readTopologyFile(argv[2]);
	
	createTopologyTable();
	seperateServerData();	
	createMasterSocketRouter();
	
	curr_value.it_value.tv_sec=0;
	// Main While loop consisting Select 
	while(1)
	{
		
		/* Timer Set for Timeout */
		fd = timerfd_create(CLOCK_REALTIME, 0);
		new_value.it_interval.tv_sec = 0;
		new_value.it_interval.tv_nsec = 0;
		new_value.it_value.tv_sec = timeUpdate - (curr_value.it_value.tv_sec);
		
		new_value.it_value.tv_nsec = 0;
		
		timerfd_settime(fd,0, &new_value, NULL); // Setting time
		
		FD_ZERO(&routerfds); // Clear RouterFDS
		FD_SET(masterRouter, &routerfds); //Set Master Router
		FD_SET(fileno(stdin), &routerfds); // Set Stdin
		FD_SET(fd,&routerfds); //Set Timerfd
		
		if( fileno(stdin) > masterRouter && fileno(stdin) > fd)
			fdMax=fileno(stdin);
		else if( fileno(stdin) < masterRouter && masterRouter > fd)
			fdMax=masterRouter;
		else
			fdMax=fd;

		printf("\n[PA2]$ : ");
		fflush(stdout);
		//Wait for an activity on one of the sockets, timeout
		
		activity = select(fdMax + 1, &routerfds , NULL , NULL , NULL);
		
				if(FD_ISSET(fd,&routerfds)) // Timeout Event, Send Update
				{
					packMessage(0);
					sendRoutingTable();
					curr_value.it_value.tv_sec=0;
					absenceCount++; //Increment for Neighbour Server Timeout
					if(absenceCount==3) // Check for 3 consecutive timeouts
					{
						for(j=0;j<neighbourCount;j++)
						{
							if(neighbourPacketCount[j]==0)
							{	//printf("\nDisabling...");
								disableServer(j);
							}
						}

						for(j=0;j<neighbourCount;j++)
						{
							neighbourPacketCount[j]=0;	
						}

						absenceCount=0;
					}
				}
				else if(FD_ISSET(masterRouter,&routerfds)) // Incoming Packets
				{
					valread = recvfrom(masterRouter, &recNode,sizeof(struct message), 0, NULL,0);
					packetCount++; //Increment Count of Packets Received						
					unpackMessage(); //Unpack Function call to Update Distance table
					timerfd_gettime(fd, &curr_value); // Get Current time Elapsed
					
				}
				
				else if(FD_ISSET(fileno(stdin),&routerfds)) // Input Command from user
				{
					timerfd_gettime(fd, &curr_value); // Get Current time Elapsed
					system("clear all");
					scanf("%[^\n]%*c",buff);					
					len = strlen(buff) - 1;
        				if (buff[len] == '\n')
				            buff[len] = '\0';
					
					tok=strtok(buff," ");

					//Update command	
					if(strcmp(tok,"update") == 0)
					{
						tok=strtok(NULL," ");
						r1=atoi(tok);
						tok=strtok(NULL," ");
						r2=atoi(tok);
						tok=strtok(NULL,"\n");
						
						if(r1==mainrouterID)
						{	flag=0;
							for(i=0;i<routerCount;i++)
							{
								if(r2==neighbourID[i])
								{
									flag=1;
									break;	
								}
							}	
			
							if(flag==1)
							{ 
								//if(strcmp(tok,"inf"))
								updateCost(r2,tok);
								
								printf("\nupdate SUCCESS");
							}
							else
							{
								 printf("\nupdate ERROR : %d is mainrouter, but %d is not a neighbour",r1,r2); 
							}
						}
						else if(r2==mainrouterID)
						{	flag=0;
						
							for(i=0;i<routerCount;i++)
							{
								if(r1==neighbourID[i])
								{
									flag=1;
									break;	
								}
		
							}	
			
							if(flag==1)
							{	
								updateCost(r1,tok);
								printf("\nupdate SUCCESS");
							}
							else
							{
								printf("\nupdate ERROR : %d is mainrouter, but %d is not a neighbour",r2,r1); 
							}	
						}
						else
						{
								printf("\nupdate ERROR : Neither of %d or %d is mainrouter",r1,r2); 
						}
						
					
					}//if Update

					//Display command to display Routing Table	
					if(strcmp(tok,"display") == 0)
					{
						displayDistanceTable();
						printf("\ndisplay SUCCESS");	
						
					}//if Display

					//Step Command to send Update Table					
					if(strcmp(tok,"step") == 0)
					{
						packMessage(0);
						sendRoutingTable();
						printf("\nstep SUCCESS");	
					}//if Step
					
					//Packets Command to Display Num of Packets received by Router					
					if(strcmp(tok,"packets") == 0)
					{
						printf("\nNumber of Packets Received by Router ID : %d are %d",mainrouterID,packetCount);
						packetCount=0;
						printf("\npackets SUCCESS");
					}//if Packets

					//Disable Command to Diable router/server					
					if(strcmp(tok,"disable") == 0)
					{
						
						tok=strtok(NULL,"\n");
						disServer=atoi(tok);	
						printf("\n%d",disServer);		
						
						dFlag=0;
						for(i=0;i<neighbourCount;i++)
						{
							if(neighbourID[i]==disServer)
							{	
								dFlag=1;
								disableServer(i);
								break;
							}
							
						}
						
						if(dFlag==1)
						{
							printf("\ndisable SUCCESS");
						}
						else
						{
							printf("\ndisable ERROR - Server ID not a neighbour");
							dFlag=0;
						}	
						
		
					}//if Disable
			
					//Crash Command to Simulate Crash					
					if(strcmp(tok,"crash") == 0)
					{
								
						/* Close all Connections on this link by closing the routerSocket*/
						printf("\ncrash SUCCESS");
						for(i=0;i<routerCount;i++)
						{
							disableServer(i);
						}
						close(masterRouter);
						
						break;
						
		
					}//if Disable
				
			}//If
	
	
			
	}
	
	
	while(1)
	{
		
	}	

	return 0;

}