#include <iostream>
#include <string>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <fstream>
#include "interface.h"
#include <sys/select.h> 
#include <unordered_map>
#include <vector>


using namespace std;

//Server side
int main(int argc, char **argv)
{
    //for the server, we only need to specify a port number
    if(argc != 2)
    {
        fprintf(stderr,
				"usage: enter port number\n");
		exit(1);
    }
    //grab the port number
    int port = atoi(argv[1]);
    //buffer to send and receive messages with
    char msg[1500];
     
    //setup a socket and connection tools
    sockaddr_in servAddr;
    bzero((char*)&servAddr, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(port);
 
    //open stream oriented socket with internet address
    //also keep track of the socket descriptor
    int serverSd = socket(AF_INET, SOCK_STREAM, 0);
    if(serverSd < 0)
    {
        cerr << "Error establishing the server socket" << endl;
        exit(0);
    }
    //bind the socket to its local address
    int bindStatus = bind(serverSd, (struct sockaddr*) &servAddr, 
        sizeof(servAddr));
    if(bindStatus < 0)
    {
        cerr << "Error binding socket to local address" << endl;
        exit(0);
    }


    cout << "Waiting for a client to connect..." << endl;
    //listen for up to 5 requests at a time
    listen(serverSd, SOMAXCONN);


    fd_set current_sockets, ready_sockets;
    
	FD_ZERO(&current_sockets);
    FD_SET(serverSd, &current_sockets);

   

    //receive a request from client using accept
    //we need a new address to connect with the client
    sockaddr_in newSockAddr;
    socklen_t newSockAddrSize = sizeof(newSockAddr);
    //accept, create a new socket descriptor to 
    //handle the new connection with client
    // int newSd = accept(serverSd, (sockaddr *)&newSockAddr, &newSockAddrSize);


    
    //lets keep track of the session time
    struct timeval start1, end1;
    gettimeofday(&start1, NULL);
    //also keep track of the amount of data sent as well
    unordered_map<string, vector<int> > chatrooms;
    unordered_map<int, string > clients;


    while(1)
    {
        ready_sockets = current_sockets;

		// See who's talking to us
		int socketCount = select(FD_SETSIZE, &ready_sockets, nullptr, nullptr, nullptr);
		if(socketCount < 0) { 
		    perror("Select error");
		    exit(1);
		}
        for(int i =0; i< FD_SETSIZE; i++){
            if(FD_ISSET(i, &ready_sockets)) {
                if(i == serverSd) {
                    int client = accept(serverSd, (sockaddr *)&newSockAddr, &newSockAddrSize);
                    if(client < 0)
                    {
                        cerr << "Error accepting request from client!" << endl;
                        exit(1);
                    }
                    cout << "Connected with client " << client<< endl;
                    FD_SET(client,&current_sockets);
   
                } else {
                    memset(&msg, 0, sizeof(msg));//clear the buffer
                    int data = recv(i, (char*)&msg, sizeof(msg), 0);
                    string chatMsg = string(msg,0,data);
                    
                    if(data<= 0){
                        cout<<"---------"<<endl;
                        close(i);
                        FD_CLR(i,&current_sockets);
                    } else {
                        string status = "";
                        string list ="";
                        string data="";
                        bool chatmode = false;
                        string num_member = "";
                        cout << ">Client " << i << ": " << msg << endl;
                        touppercase(msg, strlen(msg) - 1);
                        if(strncmp(msg, "CREATE", 6) == 0) {
                            char name[150];
                            memcpy(name, &msg[7], strlen(msg)+1);
                            string s = "";
                            for (int ind = 0; ind < strlen(msg)-7; ind++) {
                                s = s + name[ind];
                            }
                            
                           
                            
                            
                            if (chatrooms.find(s) == chatrooms.end()) {
                                vector<int> room;
                                chatrooms[s] = room;
                                
                                status = "0";
                                num_member = "0";
                            } else {
                                status = "1";
                            }
                            
                            cout<<"Chatroom " << s<< " has " << chatrooms.at(s).size() << " people " << endl;


                        }
                        
                        else if(strncmp(msg, "JOIN", 4) == 0) {
                            //char name[150];
                            //memcpy(name, &msg[5], strlen(msg)+1);
                           // cout<<name;
                            string s(msg);
                            // for (int ind = 0; ind < strlen(msg); ind++) {
                            //     s = s + msg[ind];
                            // }
                            s = s.substr(5);
                            if (chatrooms.find(s) == chatrooms.end()) {
                                status = "2";
                            }
                            else {
                                status = "0";
                                if(clients.find(i) == clients.end()) {
                                    clients[i] = s;
                                }
                                chatrooms.at(s).push_back(i);
                                num_member = to_string(chatrooms.at(s).size());
                            }
                            //cout<<"Chatroom " << s<< " has " << chatrooms.at(s).size() << " people " << endl;

                        }
                        
                        else if(strncmp(msg, "DELETE", 6) == 0) {
                            char name[150];
                            memcpy(name, &msg[7], strlen(msg)+1);
                            string s = "";
                            for (int ind = 0; ind < strlen(msg)-7; ind++) {
                                s = s + name[ind];
                            }
                            
                            if (chatrooms.find(s) == chatrooms.end()) {
                                status = "2";
                            } else {
                                status = "0";
                                chatrooms.erase(s);
                            }
                            cout<<"Chatrooms count: " << chatrooms.size() << endl;

                        } else if(strncmp(msg, "LIST", 4) == 0) {
                            if(chatrooms.size() <1 ){
                                status = "L";
                                list = " ";
                            } else {
                                status = "L";
                                for(auto it : chatrooms) {
                                    list = it.first + ","+list;
                                }
                                
                            }
                        } else {
                            cout<<msg<<endl;
                            chatmode = true;
                            string chatroom = clients.at(i);
                            for(int mem : chatrooms.at(chatroom)) {
                                if(mem != i) {
                                    send(mem, chatMsg.c_str(), chatMsg.size()+1, 0);
                                }
                            }
                        }
                        
                        // string data;
                        // getline(cin, data);
                        if(!chatmode) {
                            if(status=="L") {
                                data = status + " " + list;
                            } else {
                                data = status + " " + num_member;
                            }
                            
                            data = to_string(port)  + data;
                            memset(&msg, 0, sizeof(msg)); //clear the buffer
                            strcpy(msg, data.c_str());
                            send(i, (char*)&msg, strlen(msg), 0);
                        }
                        
                    }
                    
                }
               
            }

        }

        //receive a message from the client (listen)
        // cout << "Awaiting client response..." << endl;
        // memset(&msg, 0, sizeof(msg));//clear the buffer
        // recv(newSd, (char*)&msg, sizeof(msg), 0);
        // touppercase(msg, strlen(msg) - 1);

        // if(!strcmp(msg, "EXIT"))
        // {
        //     cout << "Client has quit the session" << endl;
        //     break;
        // }
        // cout << "Client: " << msg << endl;
        // cout << ">";
        // string data;
        // getline(cin, data);
        // memset(&msg, 0, sizeof(msg)); //clear the buffer
        // strcpy(msg, data.c_str());
        // if(data == "exit")
        // {
        //     //send to the client that server has closed the connection
        //     send(newSd, (char*)&msg, strlen(msg), 0);
        //     break;
        // }
        // //send the message to client
        // send(newSd, (char*)&msg, strlen(msg), 0);
    }
    //we need to close the socket descriptors after we're all done
    FD_CLR(serverSd, &current_sockets);
    close(serverSd);

    for(int i=0; i<FD_SETSIZE; i++)
	{
		// Get the socket number
        if(FD_ISSET(i, &current_sockets)) {
    		// Send the goodbye message
    		string bye = "Goodbye...";
            send(i, bye.c_str(), bye.size() + 1, 0);

    		// Remove it from the master file list and close the socket
    		FD_CLR(i, &current_sockets);
    		close(i);
        }
		
	}
    gettimeofday(&end1, NULL);
    // close(newSd);
   
    return 0;   
}
