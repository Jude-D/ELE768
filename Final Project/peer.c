/* time_client.c - main */

#include <sys/types.h>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>                                                                            
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
                                                                                
#include <netdb.h>
#include <dirent.h>

#define	BUFSIZE 64
#define	PACKETSIZE	101
#define	DATASIZE	100

#define	NAMESIZE	10


/*------------------------------------------------------------------------
 * main - PEER
 *------------------------------------------------------------------------
 */
 
 
 struct pdu {
 	char type;
	char peerName[NAMESIZE];
	char contentName[NAMESIZE];
	struct	sockaddr_in addr;
 };
 
 struct standardPDU {
 	char type;
 	char data[DATASIZE];
 };

void handle_user(int s, struct sockaddr_in reg_addr, char *peerName, int *registered);
void handle_client(int sd);
void sendFile(int s, FILE *p, int fileByteSize);
int getContentServer(int s, char peerName[], char downloadName[], struct sockaddr_in addr);
int download_request(int sd, char downloadName[]);
int receiveFile(int sd, char fileName[]);
int getLocalFiles(void);
int setupPDU(struct pdu *spdu, char type, char peerName[], char contentName[]);
 
int main(int argc, char **argv)
{
	char	*host = "localhost";
	int	port = 3000;
	struct hostent	*phe;	/* pointer to host information entry	*/
	struct sockaddr_in indexServer, client, reg_addr;	/* an Internet endpoint address		*/
	int	client_len,	reg_len;
	int	s, type, sd, new_sd;	/* socket descriptor and socket type	*/
	int	ret_sel;
	char	peerName[NAMESIZE];
	int	registered = 0;
	int	*regPtr = &registered;
	int	s_counter;	// Socket counter

	switch (argc) {
	case 1:
		break;
	case 2:
		host = argv[1];	// ./peer locahost
	case 3:
		host = argv[1];	// ./peer locahost 15000
		port = atoi(argv[2]);
		break;
	default:
		fprintf(stderr, "usage: UDPtime [host [port]]\n");
		exit(1);
	}

	memset(&indexServer, 0, sizeof(indexServer));
        indexServer.sin_family = AF_INET;                                          
        indexServer.sin_port = htons(port);
                                                                                        
    /* Map host name to IP address, allowing for dotted decimal */
        if ( phe = gethostbyname(host) ){
                memcpy(&indexServer.sin_addr, phe->h_addr, phe->h_length);
        }
        else if ( (indexServer.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE )
		fprintf(stderr, "Can't get host entry \n");
                                                                                
    /* Allocate a socket */
        s = socket(AF_INET, SOCK_DGRAM, 0);
        if (s < 0)
		fprintf(stderr, "Can't create socket \n");
	                                                                         
    /* Connect the socket */
        if (connect(s, (struct sockaddr *)&indexServer, sizeof(indexServer)) < 0)
		fprintf(stderr, "Can't connect to %s %s \n", host, "Time");
		
    /* Setup TCP socket */
    	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    		fprintf(stderr, "Can't create a socket\n");
    		exit(EXIT_FAILURE);
    	}
    	bzero((char*)&reg_addr, sizeof(struct sockaddr_in));
    	reg_addr.sin_family = AF_INET;
    	reg_addr.sin_port = htons(0);
    	reg_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    	if (bind(sd,(struct sockaddr*)&reg_addr, sizeof(reg_addr)) == -1){
    		fprintf(stderr, "Can't bind name to socket\n");
    		exit(EXIT_FAILURE);
    	}
    	
    	reg_len = sizeof(struct sockaddr_in);
    	getsockname(sd,(struct sockaddr*)&reg_addr,&reg_len);
    	
    /* Queue up to 10 connection requests */
    	if(listen(sd,10) < 0){
    		fprintf(stderr, "Listening failed\n");
    		exit(EXIT_FAILURE);
    	}
    
    /* Listen to multiple sockets */
       fd_set rfds, afds;
    
    /* Prompt Peer for Name */
    	peerName[0] = '\0';	
    	printf("Enter username:\n");
	
	while(1) {
		
		FD_ZERO(&afds);
       	FD_SET(0,&afds);	// Listening on stdin
       	FD_SET(sd, &afds);	// Listening on server TCP socket
		memcpy(&rfds, &afds, sizeof(rfds));
		
	    	if (ret_sel = select(FD_SETSIZE, &rfds, NULL, NULL, NULL) < 0){
	    		printf("Select() Error\n");
	    		exit(EXIT_FAILURE);
	    	}
	    	
		if(FD_ISSET(sd, &rfds)) {	// Check server TCP socket
			client_len = sizeof(client);
			new_sd = accept(sd,(struct sockaddr*)&client,&client_len);
			if (new_sd >= 0) {		// New Accepted TCP socket
				handle_client(new_sd);	// Handle download request
				close(new_sd);
				printf("Enter Command:\n");
			}
		}
		if(FD_ISSET(fileno(stdin), &rfds)) {
			handle_user(s, reg_addr, peerName, regPtr);	// Handle user interaction
			printf("Enter Command:\n");
		}
	}
	
	close(sd);
	exit(EXIT_SUCCESS);
}

/* Peer to Index Server interactions */
void handle_user(int s, struct sockaddr_in reg_addr, char *peerName, int *registered)
{
	struct pdu spdu;
	struct standardPDU rpdu;
	char	cmd;
	char	contentName[NAMESIZE];
	char	helpMsg[] = "R - Registration, T - De-Registration, D - Download, L - List Local Content, O - List of On-Line Registered Content, Q - Quit";
	int	cont_server, downloadStatus;
	char 	downloadName[NAMESIZE];
	
	if(peerName[0] == '\0'){
		scanf("%s", peerName);
		peerName[NAMESIZE] = '\0';
		printf("Enter Command:\n");
	}
	
	scanf(" %c", &cmd);
	switch(cmd){
		case 'D':
			printf("Enter name of content to download:\n");
			scanf("%s", downloadName);
			cont_server = getContentServer(s, peerName, downloadName, reg_addr);	// Ask index server to search for content and receive address of content server
			if (cont_server < 0){
				printf("No such content available\n\n");
				break;
			}
			downloadStatus = download_request(cont_server, downloadName);	// Send download request to content server and receive file
			if (downloadStatus == -1)
				break;
		case 'R':
			if (cmd == 'R'){
				printf("Enter content name:\n");
				scanf("%s", contentName);
				setupPDU(&spdu, 'R', peerName, contentName);
			}
			else {
				setupPDU(&spdu, 'R', peerName, downloadName);
			}
			spdu.addr = reg_addr;
			write(s, &spdu, sizeof(spdu));	// Send registration PDU to index server
			
			recv(s, &rpdu, PACKETSIZE, 0);	// Receive acknowledgement or error PDU from index server
			if (rpdu.type == 'E') {
				printf("%s\n", rpdu.data);
			}
			else if (rpdu.type == 'A') {
				printf("Registration Name: %s\n", spdu.contentName);
				printf("Registration Port: '%d'\n\n", spdu.addr.sin_port);
				++(*registered);
			}
			break;
		case 'Q':
		case 'T':
			if (*registered == 0 && cmd == 'T')	// If user tries to register content without any registered content
				printf("You have no registered content\n");
			else if (*registered >= 1){
				printf("Content De-Registering...\n");
				setupPDU(&spdu, 'T', peerName, contentName);
				write(s, &spdu, sizeof(spdu));	// Send de-registration PDU to index server
				recv(s, &rpdu, PACKETSIZE, 0);	// Receive Acknowledgement of de-registration
				if (rpdu.type == 'A') {
					--(*registered);
					printf("Content De-Registered!\n\n");
				}
			}
			if (cmd == 'Q'){	// If quitting, exit
				printf("Terminating...\n");
				exit(EXIT_SUCCESS);
			}
			break;
		case 'O':
			spdu.type = 'O';
			write(s, &spdu, sizeof(spdu));	// Request online content from index server
			recv(s, &rpdu, PACKETSIZE, 0);
			printf("Online Content:\n%s\n", rpdu.data);
			break;
		case 'L':
			if(getLocalFiles() == 0)
				printf("No local content available\n");
			break;
		case '?':
			printf("%s\n\n", helpMsg);
			break;
		default:
			break;
	}
}

/* Handle content client download request */
void handle_client(int sd)
{
	struct pdu rpdu;
	struct standardPDU spdu;
	char	fileName[NAMESIZE];
	char	fileNotFound[] = "FILE NOT FOUND\n";
	int 	n;
	FILE	*file;
	
	if ((n = recv(sd, &rpdu, PACKETSIZE, 0)) == -1){
		fprintf(stderr, "Content Server recv: %s (%d)\n", strerror(errno), errno);
		exit(EXIT_FAILURE);
	}
	if (rpdu.type == 'D'){
		memcpy(fileName, rpdu.contentName, NAMESIZE);
		char filePath[NAMESIZE+2];	// Add current directory to file name
		snprintf(filePath, sizeof(filePath), "%s%s", "./", fileName);
		
		file = fopen(filePath, "r");	
		if (file == NULL) {			// File does not exist
			spdu.type = 'E';
			memcpy(spdu.data, fileNotFound, sizeof(fileNotFound));
			write(sd, &spdu, sizeof(spdu));
		}
		else {
			printf("Sending file...\n");
			struct stat fileInfo;
			stat(fileName, &fileInfo);
			sendFile(sd, file, fileInfo.st_size);
			printf("Successfuly sent file\n\n");
			fclose(file);
		}
	}
}

/* Send file from content server to content client */
void sendFile(int sd, FILE *p, int fileByteSize)
{
	struct	standardPDU packet;
	char	fileData[DATASIZE] = {0};
	int	n, bytesSent, totalBytesSent = 0;

	while((n = fread(fileData, sizeof(char), DATASIZE, p)) > 0) {	// NULL if EOF reached or error occurs
		if (totalBytesSent + DATASIZE >= fileByteSize)
			packet.type = 'F';
		else
			packet.type = 'C';
		
		memcpy(packet.data, fileData, DATASIZE);
		
		if((bytesSent = send(sd, &packet, sizeof(packet), 0)) == -1){
		  fprintf(stderr, "Error sending data\n");
		  exit(1);
		}
		totalBytesSent += n;
		bzero(fileData, DATASIZE);	//Erase data
	}
}

/* Get content server info from index server and connect to socket */
int getContentServer(int s, char peerName[], char downloadName[], struct sockaddr_in addr)
{
	struct pdu spdu, rpdu;
	struct standardPDU dpdu;
	int tcp_sock;
	
	setupPDU(&spdu, 'S', peerName, downloadName);
	spdu.addr = addr;
	write(s, &spdu, sizeof(spdu));
	recv(s, &rpdu, PACKETSIZE, 0);	// Receive content server address/port from index server
	
	if(rpdu.type == 'E'){	// If error, content doesn't exist
		return -1;
	}
	else if (rpdu.type == 'S')
		setupPDU(&spdu, 'D', peerName, downloadName);
		
	printf("Address Received. Establishing Connection With Content Server...\n");
	printf("Content Server Port: '%d'\n", rpdu.addr.sin_port);
	
	/* Create a socket */
        if ((tcp_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        	fprintf(stderr, "Can't create a socket\n");
        	exit(EXIT_FAILURE);
        }
	                                                                         
    	/* Connect to the content server */
        if (connect(tcp_sock, (struct sockaddr *)&rpdu.addr, sizeof(rpdu.addr)) == -1){
        	fprintf(stderr, "Can't Connect: %s (%d)\n", strerror(errno), errno);
        	exit(EXIT_FAILURE);
        }
        
        printf("Content Server Connection Established!\n");
        return tcp_sock;
}

/* Send download request to content server and wait to receive file */
int download_request(int sd, char downloadName[]){
	struct pdu spdu;
	int bytes, ret = -1;
	
	spdu.type = 'D';
	strcpy(spdu.contentName, downloadName);
	spdu.contentName[NAMESIZE] = '\0';
	bytes = write(sd, &spdu, sizeof(spdu));
	ret = receiveFile(sd, downloadName);
	
	return ret;
}

/* Receive file data from content server */
int receiveFile(int sd, char fileName[])
{
	int	n;
	FILE	*file;
	struct standardPDU packet;
	
	file = fopen(fileName, "w");	// Create file
	if (file == NULL) {
		fprintf(stderr, "Can't create file \n");
		return -1;
	}
	
	while (1) {
		n = recv(sd, &packet, PACKETSIZE, 0);
		packet.data[DATASIZE] = '\0';
		if (packet.type == 'E'){
			printf("ERROR: %s\n", packet.data);
			remove(fileName);
			return -1;
		}
		fprintf(file, "%s", packet.data);	// Write to file
		if (packet.type  == 'F'){		// When final pdu received, break loop
			break;
		}
	
	}
	fclose(file);
	return 0;
}

int getLocalFiles(void)
{
	DIR *currDir;
	struct dirent *entry;
	currDir = opendir("."); // Open current directory
	int numberOfContent = 0;
	
	if(currDir){
		while ((entry = readdir(currDir)) != NULL)
		{
			if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
				continue; // Disclude these names
			if (entry->d_type == DT_REG){
				const char *extension = strrchr(entry->d_name,'.');
				if (extension != NULL && strcmp(extension, ".c") != 0){
					numberOfContent++;
					printf("%s\n", entry->d_name);
				}
			}
		}
		closedir(currDir);
	}
	return numberOfContent;
}

/* Generic PDU structure mapping */
int setupPDU(struct pdu *spdu, char type, char peerName[], char contentName[])
{
	spdu->type = type;
	strcpy(spdu->peerName, peerName);
	spdu->peerName[NAMESIZE] = '\0';
	strcpy(spdu->contentName, contentName);
	spdu->contentName[NAMESIZE] = '\0';
	return 0;
}
