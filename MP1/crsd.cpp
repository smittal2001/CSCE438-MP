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
#include <set>


using namespace std;


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


    
    listen(serverSd, SOMAXCONN);


    fd_set current_sockets, ready_sockets;
    
	FD_ZERO(&current_sockets);
    FD_SET(serverSd, &current_sockets);

   

   
    sockaddr_in newSockAddr;
    socklen_t newSockAddrSize = sizeof(newSockAddr);
   

    unordered_map<string, vector<int> > chatrooms;
    unordered_map<int, string > clients;
    vector<string> listChatrooms;
    set<int> chatmodeClients;
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
                    FD_SET(client,&current_sockets);
   
                } else {
                    bool chatmode = false;
                    if(chatmodeClients.find(i) != chatmodeClients.end()) {
                        cout << i << " is in chatmode " <<endl;
                        chatmode = true;
                    }
                    memset(&msg, 0, sizeof(msg));//clear the buffer
                    int data = recv(i, (char*)&msg, sizeof(msg), 0);
                    cout << "Client " << i << " " << msg << endl;
                    string chatMsg = string(msg,0,data);
                    
                    if(data<= 0){
                        close(i);
                        FD_CLR(i,&current_sockets);
                    } else {
                        string status = "";
                        string list ="";
                        string data="";
                        string num_member = "";
                        cout << ">Client " << i << ": " << msg << endl;
                        touppercase(msg, strlen(msg) - 1);
                        if(strncmp(msg, "CREATE", 6) == 0 && !chatmode) {
                            char name[150];
                            memcpy(name, &msg[7], strlen(msg)+1);
                            string s = "";
                            for (int ind = 0; ind < strlen(msg)-7; ind++) {
                                s = s + name[ind];
                            }
                            
                            if (chatrooms.find(s) == chatrooms.end()) {
                                vector<int> room;
                                chatrooms[s] = room;
                                listChatrooms.push_back(s);
                                status = "0";
                                num_member = "0";
                            } else {
                                status = "1";
                            }
                            
                        }
                        
                        else if(strncmp(msg, "JOIN", 4) == 0 && !chatmode) {
                           
                            string s(msg);
                           
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
                                chatmodeClients.insert(i);
                            }
                            
                        }
                        
                        else if(strncmp(msg, "DELETE", 6) == 0 && !chatmode) {
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
                                string bye = "Warning: the chatting room is going to be closed..";
                               
                                
                                for(int mem : chatrooms.at(s)) {
                                    chatmodeClients.erase(mem);
                                    if(mem != i) {
                                        send(mem, bye.c_str(), bye.size()+1, 0);
                                    }
                                }
                                
                                for (int ind =0; ind<listChatrooms.size(); ind++) {
                                    if (listChatrooms.at(ind) == s) {
                                        listChatrooms.erase(listChatrooms.begin()+ind);
                                        break;
                                    }
                                }
                                chatrooms.erase(s);
                                
                            }

                        } else if(strncmp(msg, "LIST", 4) == 0 && !chatmode) {
                            if(chatrooms.size() <1 ){
                                status = "L";
                                list = "";
                            } else {
                                status = "L";
                                for(int i=0; i<listChatrooms.size();i++) {
                                    list+= listChatrooms.at(i)+",";
                                }
                                
                            }
                        } else {
                            //cout<<msg<<endl;
                            chatmode = true;
                            string chatroom = clients.at(i);
                            for(int mem : chatrooms.at(chatroom)) {
                                if(mem != i) {
                                    cout << i << " " << chatMsg <<endl;
                                    send(mem, chatMsg.c_str(), chatMsg.size()+1, 0);
                                }
                            }
                        }
                        
                        
                        if(!chatmode) {
                            if(status=="L") {
                                data = status + " " + list;
                            } else {
                                data = status + " " + num_member;
                            }
                            
                            data = to_string(port)  + data;
                            memset(&msg, 0, sizeof(msg)); 
                            strcpy(msg, data.c_str());
                            send(i, (char*)&msg, strlen(msg), 0);
                        }
                        
                    }
                    
                }
               
            }

        }

    }
    FD_CLR(serverSd, &current_sockets);
    close(serverSd);

    for(int i=0; i<FD_SETSIZE; i++)
	{
        if(FD_ISSET(i, &current_sockets)) {
    // 		string bye = "Goodbye...";
    //         send(i, bye.c_str(), bye.size() + 1, 0);

    		FD_CLR(i, &current_sockets);
    		close(i);
        }
		
	}
    
   
    return 0;   
}
