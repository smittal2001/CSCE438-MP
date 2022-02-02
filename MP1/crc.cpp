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

#include "interface.h"

using namespace std;


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
				cout<<"2"<<endl;
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
	// ------------------------------------------------------------
	// GUIDE :
	// In this function, you are suppose to connect to the server.
	// After connection is established, you are ready to send or
	// receive the message to/from the server.
	// 
	// Finally, you should return the socket fildescriptor
	// so that other functions such as "process_command" can use it
	// ------------------------------------------------------------
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
	// ------------------------------------------------------------
	// GUIDE 1:
	// In this function, you are supposed to parse a given command
	// and create your own message in order to communicate with
	// the server. Surely, you can use the input command without
	// any changes if your server understand it. The given command
    // will be one of the followings:
	//
	// CREATE <name>
	// DELETE <name>
	// JOIN <name>
    // LIST
	//
	// -  "<name>" is a chatroom name that you want to create, delete,
	// or join.
	// 
	// - CREATE/DELETE/JOIN and "<name>" are separated by one space.
	// ------------------------------------------------------------


	// ------------------------------------------------------------
	// GUIDE 2:
	// After you create the message, you need to send it to the
	// server and receive a result from the server.
	// ------------------------------------------------------------


	// ------------------------------------------------------------
	// GUIDE 3:
	// Then, you should create a variable of Reply structure
	// provided by the interface and initialize it according to
	// the result.
	//
	// For example, if a given command is "JOIN room1"
	// and the server successfully created the chatroom,
	// the server will reply a message including information about
	// success/failure, the number of members and port number.
	// By using this information, you should set the Reply variable.
	// the variable will be set as following:
	//
	// Reply reply;
	// reply.status = SUCCESS;
	// reply.num_member = number;
	// reply.port = port;
	// 
	// "number" and "port" variables are just an integer variable
	// and can be initialized using the message fomr the server.
	//
	// For another example, if a given command is "CREATE room1"
	// and the server failed to create the chatroom becuase it
	// already exists, the Reply varible will be set as following:
	//
	// Reply reply;
	// reply.status = FAILURE_ALREADY_EXISTS;
    // 
    // For the "LIST" command,
    // You are suppose to copy the list of chatroom to the list_room
    // variable. Each room name should be seperated by comma ','.
    // For example, if given command is "LIST", the Reply variable
    // will be set as following.
    //
    // Reply reply;
    // reply.status = SUCCESS;
    // strcpy(reply.list_room, list);
    // 
    // "list" is a string that contains a list of chat rooms such 
    // as "r1,r2,r3,"
	// ------------------------------------------------------------

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
		// string p =  rep.substr(2);
		// for (int i = 0; i < p.length(); i++) {
  //      	reply.list_room[i] = p[i];
	 //       cout << p[i] <<" " << reply.list_room<< endl;
	 //   }
		strcpy(reply.list_room, rep.substr(2).c_str());
	}
	
	
	
	return reply;
}


void *sendMessage(int sock, future<void> futureObj) {
	string input;
	while(promObj.get() != 35) {
		char msg[MAX_DATA];
		memset(&msg, 0, sizeof(msg));
    	get_message(msg, MAX_DATA);
    	//cout<<"sending " << msg << endl;
		int sendResult = send(sock, (char*)&msg, strlen(msg), 0);
		
		if (sendResult == -1) {
	        cout << "Could not send to server." << endl;
	    }
  //  	if(strlen(msg) > 0) {
		// 	cout << "sending..." << endl;
		// 	//int sendResult = send(sock, (char*)&msg, strlen(msg), 0);
		// 	int sendResult = send(sock, input.c_str(), input.size() + 1, 0);

		// 	if (sendResult == -1) {
		//         cout << "Could not send to server." << endl;
		//     }
		// }

    }
}
    

void *receiveMessage(int sock, promise<int> * promObj) {
	
	while(promObj.get()!=35){
		char msg[MAX_DATA];
		memset(&msg, 0, sizeof(msg));
		int recieved = recv(sock, (char*)&msg, sizeof(msg), 0);
		if(recieved > 0) {
			cout << "> " << msg<< endl;
			//string bye = "Warning: the chatting room is going to be closed..";
			if(strncmp(msg,"Warning:",8) ==0) {
				promObj->set_value(35);
			}
			//display_message(msg);
			//cout<<"test"<<endl;
			//string str = string(msg,0,recieved);
			//cout<<str << endl;
		
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
	// ------------------------------------------------------------
	// GUIDE 1:
	// In order to join the chatroom, you are supposed to connect
	// to the server using host and port.
	// You may re-use the function "connect_to".
	// ------------------------------------------------------------

	// ------------------------------------------------------------
	// GUIDE 2:
	// Once the client have been connected to the server, we need
	// to get a message from the user and send it to server.
	// At the same time, the client should wait for a message from
	// the server.
	// ------------------------------------------------------------
	
    // ------------------------------------------------------------
    // IMPORTANT NOTICE:
    // 1. To get a message from a user, you should use a function
    // "void get_message(char*, int);" in the interface.h file
    // 
    // 2. To print the messages from other members, you should use
    // the function "void display_message(char*)" in the interface.h
    //
    // 3. Once a user entered to one of chatrooms, there is no way
    //    to command mode where the user  enter other commands
    //    such as CREATE,DELETE,LIST.
    //    Don't have to worry about this situation, and you can 
    //    terminate the client program by pressing CTRL-C (SIGINT)
	// ------------------------------------------------------------
	int sockfd = port;
	promise<int> promiseObj;
	promise<int> promiseObj2;
	future<int> futureObj = promiseObj.get_future();
	future<int> futureObj2 = promiseObj2.get_future();
	
	thread t1(sendMessage, sockfd, &promiseObj2);
    thread t2(receiveMessage, sockfd, &promiseObj);
    if(futureObj.get() > 0) {
    	futureObj2->set_value(35);
    }
    // cout<<futureObj.get()<<endl;
	t1.join();
    t2.join();
	cout<<"1"<<endl;
}


