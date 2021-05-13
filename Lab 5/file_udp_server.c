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
/*------------------------------------------------------------------------
 * main - Iterative UDP server for TIME service
 *------------------------------------------------------------------------
 */
 struct pdu {
	char type;
	char data[DATASIZE];
 };

void sendFile(int s, FILE *p, int fileByteSize, struct sockaddr_in fsin)
{
	struct	pdu packet;
	char	fileData[DATASIZE] = {0};
	int	n, bytesSent, totalBytesSent = 0;

	while((n = fread(fileData, sizeof(char), DATASIZE, p)) > 0) {	// NULL if EOF reached or error occurs
		if (totalBytesSent + DATASIZE >= fileByteSize)	// If last packet to be sent, set type to F
			packet.type = 'F';
		else
			packet.type = 'D';
		
		memcpy(packet.data, fileData, DATASIZE);	// Copy file data to packet
		
		if((bytesSent = sendto(s, &packet, PACKETSIZE, 0, (struct sockaddr *)&fsin, sizeof(fsin))) < 0){
		  fprintf(stderr, "Error sending data\n");
		  exit(1);
		}
		totalBytesSent += n;	// Monitor number of bytes sent
		bzero(fileData, DATASIZE);	//Erase data
	}
}
 
int main(int argc, char *argv[])
{
	struct  sockaddr_in fsin;	/* the from address of a client	*/
	char	buf[100];		/* "input" buffer; any size > 0	*/
	char    *pts;
	int	sock;			/* server socket		*/
	time_t	now;			/* current time			*/
	int	alen;			/* from-address length		*/
	struct  sockaddr_in sin; /* an Internet endpoint address         */
        int     s, type, n;        /* socket descriptor and socket type    */
	int 	port=3000;
	struct  pdu 	spdu;	// Filename pdu
	struct	pdu	fileNotFound;	// Error pdu
	char	fileNotFoundMsg[] = "FILE NOT FOUND";
	FILE	*file;		// File                                                

	switch(argc){
		case 1:
			break;
		case 2:
			port = atoi(argv[1]);
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
		if ((n = recvfrom(s, &spdu, sizeof(spdu), 0, (struct sockaddr *)&fsin, &alen)) < 0)	// Get file name from client
			fprintf(stderr, "recvfrom error\n");
		
		file = fopen(spdu.data, "r");	
		if (file == NULL) {			// File does not exist
			fileNotFound.type = 'E';
			memcpy(fileNotFound.data, fileNotFoundMsg, sizeof(fileNotFoundMsg));
			
			printf("%s\n\n", fileNotFound.data);
			if(sendto(s, &fileNotFound, PACKETSIZE, 0, (struct sockaddr *)&fsin, sizeof(fsin)) < 0){
		  		fprintf(stderr, "Error sending data\n");
		  		exit(1);
			}
		}
		else {					// File exists
			printf("File Found\n");
			struct stat fileInfo;
			stat(spdu.data, &fileInfo);		// Used to get file size
			
			char sendMsg[] = "Sending file...\n";
			printf("%s", sendMsg);
			
			sendFile(s, file, fileInfo.st_size, fsin);
			printf("Successfuly sent file\n\n");
			fclose(file);
		}		
	}
}
