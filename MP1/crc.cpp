#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <thread> 
#include <future>
#include <atomic>

#include "interface.h"

using namespace std;
atomic<bool> stop(false);

/*
 * TODO: IMPLEMENT BELOW THREE FUNCTIONS
 */
int connect_to(const char *host, const int port);
struct Reply process_command(const int sockfd, char* command);
void process_chatmode(const char* host, const int port);

int main(int argc, char** argv) 
{
	if (argc != 3) {
		fprintf(stderr,
				"usage: enter host address and port number\n");
		exit(1);
	}

    display_title();
    int sockfd = connect_to(argv[1], atoi(argv[2]));

	while (1) {
	
    
		char command[MAX_DATA];
        get_command(command, MAX_DATA);
        

		struct Reply reply = process_command(sockfd, command);
		display_reply(command, reply);
		if(reply.status == SUCCESS) {
			touppercase(command, strlen(command) - 1);
			if (strncmp(command, "JOIN", 4) == 0) {
				printf("Now you are in the chatmode\n");
				process_chatmode(argv[1],sockfd);
				display_title();
			}
		}
		
	
    }
	close(sockfd);

    return 0;
}

/*
 * Connect to the server using given host and port information
 *
 * @parameter host    host address given by command line argument
 * @parameter port    port given by command line argument
 * 
 * @return socket fildescriptor
 */
int connect_to(const char *host, const int port)
{
	
	int sockfd;
	struct sockaddr_in server;
	struct hostent* host2 = gethostbyname(host); 
	bzero((char*)&server, sizeof(server));
	server.sin_family = AF_INET;
  	server.sin_addr.s_addr = inet_addr(inet_ntoa(*(struct in_addr*)*host2->h_addr_list));
  	server.sin_port = htons(port);
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	int status = connect(sockfd,
                         (struct sockaddr *) &server, sizeof(server));
	
	if(status < 0)
    {
        printf("Error connecting to socket!"); exit(0);
    } 
	return sockfd;
}

/* 
 * Send an input command to the server and return the result
 *
 * @parameter sockfd   socket file descriptor to commnunicate
 *                     with the server
 * @parameter command  command will be sent to the server
 *
 * @return    Reply    
 */
struct Reply process_command(const int sockfd, char* command)
{
	
	char msg[1500]; 

	memset(&msg, 0, sizeof(msg));
	strcpy(msg, command);
	
	send(sockfd, (char*)&msg, strlen(msg), 0);

	memset(&msg, 0, sizeof(msg));
	recv(sockfd, (char*)&msg, sizeof(msg), 0);
	
	string rep = "";
    for (int ind = 0; ind < strlen(msg); ind++) {
       rep += msg[ind];
    }
	
	string port = rep.substr(0,4);
	rep=rep.substr(4);
	// REMOVE below code and write your own Reply.
	struct Reply reply;
	
	reply.port = atoi(port.c_str());
	if(rep.substr(0,1) == "0") {
		reply.status = SUCCESS;
		reply.num_member = atoi(rep.substr(2).c_str());
	} 
	else if(rep.substr(0,1) == "1" ) {
		reply.status = FAILURE_ALREADY_EXISTS;
	}
	else if(rep.substr(0,1) == "2" ) {
		reply.status = FAILURE_NOT_EXISTS;
	}	
	else if(rep.substr(0,1) == "3" ) {
		reply.status = FAILURE_INVALID;
	}
	else if(rep.substr(0,1) == "4" ) {
		reply.status = FAILURE_UNKNOWN;
	} 
	else if(rep.substr(0,1) == "L" ) {
		reply.status = SUCCESS;
		strcpy(reply.list_room, rep.substr(2).c_str());
	}
	
	
	
	return reply;
}

/* 
 * Constant process of sending a message that will be stopped
 * when the chatroom gets deleted
 *
 * @parameter sockfd   socket file descriptor to commnunicate
 *                     with the server
 */

void *sendMessage(int sock) {
	string input;
	while(!stop) {
		char msg[MAX_DATA];
		memset(&msg, 0, sizeof(msg));
    	get_message(msg, MAX_DATA);
    	//cout<<"sending " << msg << endl;
		int sendResult = send(sock, (char*)&msg, strlen(msg), 0);
		cout << stop << endl;
		
		if (sendResult == -1) {
	        cout << "Could not send to server." << endl;
	    }
    }
}

 /* 
 * Constant process of recieving a message that will be stopped
 * when the chatroom gets deleted
 *
 * @parameter sockfd   socket file descriptor to commnunicate
 *                     with the server
 */
void *receiveMessage(int sock, promise<int> * promObj) {
	
	while(1){
		char msg[MAX_DATA];
		memset(&msg, 0, sizeof(msg));
		int recieved = recv(sock, (char*)&msg, sizeof(msg), 0);
		if(recieved > 0) {
			cout << "> " << msg<< endl;
			if(strncmp(msg,"Warning:",8) ==0) {
				promObj->set_value(35);
				break;
			}
		}
	}
	
}
/* 
 * Get into the chat mode
 * 
 * @parameter host     host address
 * @parameter port     port
 */
void process_chatmode(const char* host, const int port)
{
	int sockfd = port;

	promise<int> promiseObj;
	future<int> futureObj = promiseObj.get_future();
	thread t1(sendMessage, sockfd);
    thread t2(receiveMessage, sockfd, &promiseObj);
    
    
    if(futureObj.get() > 0) {
    	stop=true;
    }
	t1.join();
    t2.join();
}


