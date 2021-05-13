/* time_client.c - main */

#include <sys/types.h>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>                                                                            
#include <netinet/in.h>
#include <arpa/inet.h>
                                                                                
#include <netdb.h>

#define	BUFSIZE 64
#define	PACKETSIZE	101
#define	DATASIZE	100

#define	MSG		"Any Message \n"


/*------------------------------------------------------------------------
 * main - UDP client for TIME service that prints the resulting time
 *------------------------------------------------------------------------
 */
 
 struct pdu {
 	char type;
 	char data[DATASIZE];
 };
 
int receiveFile(int sd, char fileName[])
{
	char	fileBuf[PACKETSIZE];
	int	n;
	FILE	*file;
	struct pdu packet;
	
	file = fopen(fileName, "w");	// Create file
	if (file == NULL) {
		fprintf(stderr, "Can't create file \n");
		return 1;
	}
	
	while (1) {
		n = recv(sd, &packet, PACKETSIZE, 0);
		packet.data[DATASIZE] = '\0';
		
		if (packet.type  == 'E'){		// When error type PDU, delete file and break loop
			printf("[ERROR] %s\n\n", packet.data);
			remove(fileName);
			break;
		}
		
		fprintf(file, "%s", packet.data);	// Write to file
		if (packet.type  == 'F'){		// When final pdu received, break loop
			printf("File Transferred\n\n");
			break;
		}
	
	}
	fclose(file);
	return 0;
}
 
int main(int argc, char **argv)
{
	struct pdu spdu, rpdu;
	char	*host = "localhost";
	char	buf[100];
	int	port = 3000;
	char	now[100];		/* 32-bit integer to hold time	*/ 
	struct hostent	*phe;	/* pointer to host information entry	*/
	struct sockaddr_in sin;	/* an Internet endpoint address		*/
	int	s, n, type;	/* socket descriptor and socket type	*/

	switch (argc) {
	case 1:
		break;
	case 2:
		host = argv[1];
	case 3:
		host = argv[1];
		port = atoi(argv[2]);
		break;
	default:
		fprintf(stderr, "usage: UDPtime [host [port]]\n");
		exit(1);
	}

	memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;                                                                
        sin.sin_port = htons(port);
                                                                                        
    /* Map host name to IP address, allowing for dotted decimal */
        if ( phe = gethostbyname(host) ){
                memcpy(&sin.sin_addr, phe->h_addr, phe->h_length);
        }
        else if ( (sin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE )
		fprintf(stderr, "Can't get host entry \n");
                                                                                
    /* Allocate a socket */
        s = socket(AF_INET, SOCK_DGRAM, 0);
        if (s < 0)
		fprintf(stderr, "Can't create socket \n");
	
                                                                                
    /* Connect the socket */
        if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
		fprintf(stderr, "Can't connect to %s %s \n", host, "Time");

	while(1) {
		spdu.type = 'C';
		printf("Enter a filename (enter 'exit' to terminate program):\n");
		n = read(0, spdu.data, 100);	// Get filename from user
		spdu.data[n-1] = '\0';
		
		if (strcmp(spdu.data, "exit") == 0) // If user enters "exit", break loop
			break;
			
		write(s, &spdu, n+1);		// Send filename
		receiveFile(s, spdu.data);	// Read from server
	}
	
	exit(0);
}
