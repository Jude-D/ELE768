/* time_server.c - main */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>


#define	PACKETSIZE	101
#define	DATASIZE	100
#define	NAMESIZE	10
#define	ADDRSIZE	80
#define	MAXCONTENTPORTS	5
#define	MAXPEERS	5
#define	MAXCONTENTLEN	50
/*------------------------------------------------------------------------
 * main - Index Server
 *------------------------------------------------------------------------
 */
struct content {
	char name[NAMESIZE];
	char user[NAMESIZE];
	int port;
	struct	sockaddr_in addr;
};

struct pdu {
	char type;
	char data[DATASIZE];
};
 
struct registerPDU {
	char type;
	char peerName[NAMESIZE];
	char contentName[NAMESIZE];
	struct	sockaddr_in addr;
};

int getUserIndex(struct content contents[MAXPEERS], char name[]);
int getContentIndex(struct content contents[MAXPEERS], char name[], int startIndex);
void getContent(struct content contents[MAXPEERS], int length, char *msg);
void removeContent(struct content contents[MAXPEERS], int length, int index);

int main(int argc, char *argv[])
{
	struct  sockaddr_in fsin;	/* the from address of a client	*/
	int	sock;			/* server socket		*/
	int	alen;			/* from-address length		*/
	struct  sockaddr_in sin; /* an Internet endpoint address         */
        int     s, type, n;        /* socket descriptor and socket type    */
	int 	port=3000;
	
	/* PDUs */
	struct  registerPDU 	rpdu, dpdu;	// Standard PDU structure
	struct	pdu	spdu;	// Acknowledgement-Errors
	
	/* Peer-Content Information */
	struct	content *contents = malloc(MAXPEERS * sizeof(struct content));
	int	nextNameIndex = 0, nextContentIndex = 0;
	int	pcIndex = 0;
	
	/* Messages */
	char	nameExistsMsg[] = "Peer name already exists\n";
	char	noContentMsg[] = "No Registered Content Available\n";
	char	allContentMsg[MAXCONTENTLEN+5];
	
	int i;
	
	switch(argc){
		case 1:
			break;			// ./server
		case 2:
			port = atoi(argv[1]);	// ./server 15000
			break;
		default:
			fprintf(stderr, "Usage: %s [port]\n", argv[0]);
			exit(1);
	}

        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = INADDR_ANY;
        sin.sin_port = htons(port);
                                                                                                 
    /* Allocate a socket */
        s = socket(AF_INET, SOCK_DGRAM, 0);
        if (s < 0)
		fprintf(stderr, "can't creat socket\n");
                                                                                
    /* Bind the socket */
        if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
		fprintf(stderr, "can't bind to %d port\n",port);
        listen(s, 5);	
	alen = sizeof(fsin);
	
	while (1) {
		if ((n = recvfrom(s, &rpdu, sizeof(rpdu), 0, (struct sockaddr *)&fsin, &alen)) < 0)	// Get PDU
			fprintf(stderr, "recvfrom error\n");
		
		switch(rpdu.type){
			case 'R':
				printf("Case R\n"); //REMOVE THIS
				spdu.type = 'A';
				
				/* Check for already existing peer name */
				if(nextNameIndex > 0 && getUserIndex(contents, rpdu.peerName) >= 0)
				{
					printf("%s", nameExistsMsg);
					spdu.type = 'E';
					memcpy(spdu.data, nameExistsMsg, sizeof(nameExistsMsg));
					
				}
				
				/* Send acknowledgement or error if peer name already exists */
				if(sendto(s, &spdu, PACKETSIZE, 0, (struct sockaddr *)&fsin, sizeof(fsin)) < 0){
			  		fprintf(stderr, "Error sending data\n");
			  		exit(1);
				}
				if (spdu.type == 'E')
					break;
				
				printf("Registered Name: '%s'\n", rpdu.peerName);
				printf("Registered Content: '%s'\n", rpdu.contentName);
				printf("Registered Port: '%d'\n", rpdu.addr.sin_port);
				
				strcpy(contents[nextContentIndex].name, rpdu.contentName);	// Add new content
				contents[nextContentIndex].name[NAMESIZE] = '\0';
				strcpy(contents[nextContentIndex].user, rpdu.peerName);
				contents[nextContentIndex].user[NAMESIZE] = '\0';
				contents[nextContentIndex].addr = rpdu.addr;
				nextContentIndex++;
				nextNameIndex++;
				break;
			case 'O':
				/* If no content registered yet, send no content available otherwise send all registered content */
				printf("Request Received. Currently Available Content: \n");
				if (nextContentIndex == 0){
					spdu.type = 'E';
					memcpy(spdu.data, noContentMsg, sizeof(noContentMsg));
					if(sendto(s, &spdu, PACKETSIZE, 0, (struct sockaddr *)&fsin, sizeof(fsin)) < 0){
				  		fprintf(stderr, "Error sending data\n");
				  		exit(1);
					}
				}
				else {
					getContent(contents, nextContentIndex, allContentMsg);
					spdu.type = 'O';
					memcpy(spdu.data, allContentMsg, sizeof(allContentMsg));
					if(sendto(s, &spdu, PACKETSIZE, 0, (struct sockaddr *)&fsin, sizeof(fsin)) < 0){
				  		fprintf(stderr, "Error sending data\n");
				  		exit(1);
					}
				}
				break;
			case 'T':
				/* If no content registered yet, send no content available otherwise send all registered content */
				pcIndex = getUserIndex(contents, rpdu.peerName); 	// Registered content is monitored on peer side
				removeContent(contents, nextContentIndex, pcIndex);	// Remove content from list of registered content
				nextContentIndex--;
				nextNameIndex--;
				spdu.type = 'A';
				if(sendto(s, &spdu, PACKETSIZE, 0, (struct sockaddr *)&fsin, sizeof(fsin)) < 0){
			  		fprintf(stderr, "Error sending data\n");
			  		exit(1);
				}
				break;
			case 'S':
				printf("Searching for '%s'...\n", rpdu.contentName);
				pcIndex = getContentIndex(contents, rpdu.contentName, nextContentIndex); // Check if content registered
				if (pcIndex == -1){
					dpdu.type = 'E';
				}
				else {
					printf("'%s' found\n", rpdu.contentName);
					dpdu.type = 'S';
					dpdu.addr = contents[pcIndex].addr;
				}
				/* Send address of content server otherwise send error */
				if(sendto(s, &dpdu, PACKETSIZE, 0, (struct sockaddr *)&fsin, sizeof(fsin)) < 0){
			  		fprintf(stderr, "Error sending data\n");
			  		exit(1);
				}
				break;
			default:
				printf("Case not recognized\n"); // No functionality for received PDU type
				break;
		}
		bzero(spdu.data, DATASIZE);
	}
}

/* Get index of existing user */
int getUserIndex(struct content contents[MAXPEERS], char name[])
{
	int i;
	for (i = 0; i < MAXPEERS; i++)
	{
		if (strcmp(contents[i].user, name) == 0)
				return i;
	}
	return -1;
}

/* Get index of latest content (in case content is not unique) */
int getContentIndex(struct content contents[MAXPEERS], char name[], int startIndex)
{
	int i;
	for (i = startIndex; i >= 0; i--)
	{
		if (strcmp(contents[i].name, name) == 0)
				return i;
	}
	return -1;
}

/* Get all currently registered content with unique names */
void getContent(struct content contents[MAXPEERS], int length, char *msg)
{
	char buffer[MAXCONTENTLEN+5];
	int i,j,k;
	char uniqueNames[length][NAMESIZE];
	int nameCount = 1;
	int duplicate = 0;
	
	strcpy(uniqueNames[0], contents[0].name);
	for(i = 1; i < length; i++) // Start at cell[1] and check for duplicates in the rest of array
	{
		for(j = 0; j < nameCount; j++)
		{
			if (strcmp(contents[i].name, uniqueNames[j]) == 0)
				duplicate = 1;
		}
		if (duplicate == 0){	// If unique, copy into array
			strcpy(uniqueNames[nameCount], contents[i].name);
			nameCount++;
		}
		duplicate = 0;
	}
	
	for (k = 0; k < nameCount; k++)
	{
		strcat(buffer, uniqueNames[k]);
		strcat(buffer, "\n");
	}
	printf("%s\n", buffer);
	strcpy(msg, buffer);
}

/* Remove and resize content array */
void removeContent(struct content contents[MAXPEERS], int length, int index)
{
	int i;
	printf("De-Registering Name: '%s'\n", contents[index].user);
	printf("De-Registering Content: '%s'\n", contents[index].name);
	printf("De-Registering Port: '%d'\n", contents[index].addr.sin_port);
	for (i = index; i < length; i++)
	{
		if (i > 0 && i + 1 >= length){
			strcpy(contents[i].name, "");
			strcpy(contents[i].user, "");
			break;
		}
		contents[i] = contents[i+1];
	}
	printf("De-Registered\n");
}
