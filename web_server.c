#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>



#define BUFSIZE 8096
#define BADREQUEST 400
#define FORBIDDEN 403
#define NOTFOUND  404
#define SERVERERROR 500

#define MAX_CLEINT 99

struct {
	char *ext;
	char *filetype;
} extensions [] = {
	{"gif", "image/gif" },  
	{"jpg", "image/jpg" }, 
	{"jpeg","image/jpeg"},
	{"png", "image/png" },  
	{"ico", "image/ico" },  
	{"zip", "application/zip" },  
	{"gz",  "application/gz"  },  
	{"tar", "application/tar" },
	{"pdf", "application/pdf"},  
	{"js",  "application/javascript" },	
	{"htm", "text/html" },  
	{"html","text/html" }, 
	{"txt", "text/plain" },
	{"css", "text/css" },
	{"mp3", "audio/mp3" },
	{"xml","application/xml" }, 
	{"json","application/json" }, 
	{0,0} };

void send_error(int type, int socket_fd)
{
	switch (type) {
	case BADREQUEST:
		(void)write(socket_fd, "HTTP/1.1 400 Bad Request\nContent-Length: 152\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>400 Bad Request</title>\n</head><body>\n<h1>Bad Request</h1>\nBad Request. Please try again with correct request\n</body></html>\n",233);
		break;
	case FORBIDDEN: 
		(void)write(socket_fd, "HTTP/1.1 403 Forbidden\nContent-Length: 155\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forbidden</title>\n</head><body>\n<h1>Forbidden</h1>\nThe requested URL, file type or operation is not allowed.\n</body></html>\n",234);
		break;
	case NOTFOUND: 
		(void)write(socket_fd, "HTTP/1.1 404 Not Found\nContent-Length: 136\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>404 Not Found</title>\n</head><body>\n<h1>Not Found</h1>\nThe requested URL was not found on this server.\n</body></html>\n",224);
		break;
	case SERVERERROR:
		(void)write(socket_fd, "HTTP/1.1 500 Internal Server Error\nContent-Length: 181\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>500 Internal Server Error</title>\n</head><body>\n<h1>Internal Server Error</h1>\nInternal Server Error Occurred. Try After some time...\n</body></html>\n",267);
		break;
	}	
	
	//exit(3);
}


void log_msg(char *s1)
{
	printf("%s \n", s1);
}

void server_error(char *s1)
{
	printf("%s \n", s1);
	exit(5);
}


void handle_client_request(int fd)
{
	int j, file_fd, buflen;
	long i, ret, len;
	char * fstr;
	static char buffer[BUFSIZE+1];

	ret = read(fd,buffer,BUFSIZE);
	
	if(ret == 0 || ret == -1) {
		log_msg("unable to read client request");
		send_error(BADREQUEST,fd);
		return;
	}
	
	printf("Return %ld ", ret);
	if(ret > 0 && ret < BUFSIZE)
		buffer[ret]=0;
	else buffer[0]=0;
	
	for(i=0;i<ret;i++)	// remove CF and LF characters
		if(buffer[i] == '\r' || buffer[i] == '\n')
			buffer[i]='*';
		
	log_msg("Received Request:");
	log_msg(buffer);
	
	if( strncmp(buffer,"GET ",4) && strncmp(buffer,"get ",4) ) {
		log_msg("Only GET operation supported");
		send_error(BADREQUEST,fd);
		return;
	}

	
	// ignoring unwanted stuff from the request	
	for(i=4;i<BUFSIZE;i++) {
		if(buffer[i] == ' ') {
			buffer[i] = 0;
			break;
		}
	}
	
	// validating directory usage in the request
	for(j=0;j<i-1;j++) 
		if(buffer[j] == '.' && buffer[j+1] == '.') {
			log_msg("Parent directory (..) path names not supported");
			send_error(FORBIDDEN,fd);
			return;
		}
	
	// if no file name is provided in the GET request, by default assume index.html
	if( !strncmp(&buffer[0],"GET /\0",6) || !strncmp(&buffer[0],"get /\0",6) )
		(void)strcpy(buffer,"GET /index.html");


	// validating file type supprted or not
	buflen=strlen(buffer);
	fstr = (char *)0;
	
	for(i=0;extensions[i].ext != 0;i++) {
		len = strlen(extensions[i].ext);
		if( !strncmp(&buffer[buflen-len], extensions[i].ext, len)) {
			fstr =extensions[i].filetype;
			break;
		}
	}
	
	if(fstr == 0) {
		log_msg("file extension type not supported");
		send_error(NOTFOUND,fd);
		return;
	}

	// Checking is file is present and has read permission or not
	if(( file_fd = open(&buffer[5],O_RDONLY)) == -1) {
		log_msg("failed to open file");
		send_error(NOTFOUND,fd);
		return;
	}
	
	log_msg("SENDING Response:");
	
	len = (long)lseek(file_fd, (off_t)0, SEEK_END);
	      (void)lseek(file_fd, (off_t)0, SEEK_SET);
          (void)sprintf(buffer,"HTTP/1.1 200 OK\nContent-Length: %ld\nConnection: close\nContent-Type: %s\n\n", len, fstr);
	
	log_msg(buffer);
	
	(void)write(fd,buffer,strlen(buffer));

	// Sending file in 8KB block
	while (	(ret = read(file_fd, buffer, BUFSIZE)) > 0 ) {
		(void)write(fd,buffer,ret);
	}
	
	sleep(1);
	close(fd);
}

int main(int argc, char **argv)
{
	int port, serverfd, clientfd;
	socklen_t length;
	static struct sockaddr_in client_addr;
	static struct sockaddr_in server_addr;
	
	//*******************************************************************//
	//************      VALIDATE SERVER STARTING PARAMETERS      ********//
	//*******************************************************************//
	char *root;
	if( argc != 2 && argc != 3) {
		server_error("Error in command. Check commands properly and retry");
	}
	
	if(argc == 2){
		printf("Current Directory is being treated as root directory since you did not specify root directory");
		root = getenv("PWD");
	}
	
	else {
		root = argv[2];
		if( !strncmp(root,"/"   ,2 ) || !strncmp(root,"/etc", 5 ) ||
			!strncmp(root,"/bin",5 ) || !strncmp(root,"/lib", 5 ) ||
			!strncmp(root,"/tmp",5 ) || !strncmp(root,"/usr", 5 ) ||
			!strncmp(root,"/dev",5 ) || !strncmp(root,"/sbin",6) ){
			server_error("Error: Bad Root directory\n");
		}
		if(chdir(root) == -1){
			server_error("Error: Can not start the server with %s as root directory\n");
		}
	}
	//*******************************************************************//
	
	/* setup the network socket */
	if((serverfd = socket(AF_INET, SOCK_STREAM,0)) <0)
		server_error("Socket Opening Failed");
	
	log_msg("My HTTP Server starting");

	port = atoi(argv[1]);
	if(port < 1 || port >65535)
		server_error("Port input wrong");
	
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(port);
	
	if(bind(serverfd, (struct sockaddr *)&server_addr,sizeof(server_addr)) <0){
		server_error("May be Port error. Bind failed. Can you try a different non-reserved port");
	}
	if( listen(serverfd,MAX_CLEINT) <0){
		server_error("listen failed");
	}
	
	

	while(1){
		length = sizeof(client_addr);
		if((clientfd = accept(serverfd, (struct sockaddr *)&client_addr, &length)) < 0){
			server_error("system call, accept error");
		}
		handle_client_request(clientfd);
	}
	
	close(serverfd);
	return 0;
}
