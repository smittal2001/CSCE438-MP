#include <ctime>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>
#include <thread>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <memory>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>
#include <time.h>       /* time_t, struct tm, difftime, time, mktime */
#include <vector>


#include "snsCoordinator.grpc.pb.h"
#include "sync.grpc.pb.h"

using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ClientContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ClientReaderWriter;

using grpc::ServerWriter;
using grpc::Status;
using snsCoordinator::Request;
using snsCoordinator::Reply;
using snsCoordinator::SNSCoordinator;
using snsCoordinator::SYNCHRONIZER;
using snsCoordinator::SERVER;
using snsCoordinator::HeartBeat;



using snsSync::SyncRequest;
using snsSync::SyncReply;
using snsSync::SNSSync;

using namespace std;
string id;


unordered_map<int, string> routing_table;

vector<pair<time_t, int> > files;
// unordered_set<string> output;

string port1;
string port2;
string type = "master";
string port = "3010";
string coord_hostname = "localhost";
string coord_port = "9000";
bool connected = false;

unique_ptr<SNSSync::Stub> stub1;
unique_ptr<SNSSync::Stub> stub2;
unique_ptr<SNSCoordinator::Stub> stub;


struct Client {
  std::string username;
  bool connected = true;
  int following_file_size = 0;
  std::vector<string> client_followers;
  std::vector<string> client_following;
//   ServerReaderWriter<Message, Message>* stream = 0;
  bool operator==(const Client& c1) const{
    return (username == c1.username);
  }
};

//Vector that stores every client that has been created
vector<Client> client_db;

int find_user(std::string username){
  int index = 0;
  for(Client c : client_db){
    //cout << "user " << c.username << endl;
    if(c.username == username)
      return index;
    index++;
  }
  cout << "no user" << endl;
  return -1;
}



class SyncImpl final : public SNSSync::Service { 
    Status check_connection(ServerContext* context, const SyncRequest* request, SyncReply* reply) override {
        cout << request->id() << endl;
        return Status::OK;
    }
    
    
    Status new_follow(ServerContext* context, const SyncRequest* request, SyncReply* reply) override { 
        string username = request->username();
        string username2 = request->msg();
        cout << "follow request from: " << request->id() << " with user " << username << " and " << username2;
        if(find_user(username2) >= 0) {
            string output = "SYNC::follow: " + username + " " + username2;
            
            
            string filename = "logfiles/master" + id + "_output.txt";
            ofstream logfile(filename, std::ios::app|std::ios::out|std::ios::in);
            
            filename = "logfiles/slave" + id + "_output.txt";
            ofstream slavefile(filename, std::ios::app|std::ios::out|std::ios::in);
            
            slavefile << output << endl;
            logfile << output << endl;
            files[0].second += 1;
            
            Client *client = &client_db[find_user(username2)];
            client->client_followers.push_back(username);
        }
        return Status::OK;
    }
    
    
    Status new_post(ServerContext* context, const SyncRequest* request, SyncReply* reply) override { 
        string username = request->username();
        string req_id = request->id();
        cout << "post request from: " << request->id() << " with user " << username << endl;
        for(auto client : client_db) {
            for( string following : client.client_following) {
                if(following == username) {
                    string filename = "master" + id + "_userfiles/" + client.username + "_timeline.txt";
                    cout << filename << endl;
                    ofstream userfile(filename, std::ios::app|std::ios::out|std::ios::in);

                    filename = "slave" + id + "_userfiles/" + client.username + "_timeline.txt";
                    ofstream slavefile(filename, std::ios::app|std::ios::out|std::ios::in);

                    if(userfile.is_open() && slavefile.is_open()) {
                        cout << "sending to user files" << endl;
                        userfile << request->msg() << endl;
                        slavefile << request->msg() << endl;
                        files[1].second += 1;
                    }
                }
            }
        }
        
        return Status::OK;
    }
    
    Status new_user(ServerContext* context, const SyncRequest* request, SyncReply* reply) override { 
        string username = request->username();
        cout << " user request from: " << request->id() << " with user " << username;
        string output = "SYNC::newuser: " + username;
        
        string filename = "logfiles/master" + id + "_output.txt";
        ofstream logfile(filename, std::ios::app|std::ios::out|std::ios::in);
        
        filename = "logfiles/slave" + id + "_output.txt";
        ofstream slavefile(filename, std::ios::app|std::ios::out|std::ios::in);

        
        slavefile << output << endl;
        logfile << output << endl;
        
        files[0].second += 1;
        return Status::OK;
    }
    
};


void RunServer(string port_no) {
  cout << "runnning server" << endl;
  string server_address = "0.0.0.0:"+port_no;
  SyncImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  unique_ptr<Server> server(builder.BuildAndStart());
  cout << "Server listening on " << server_address << std::endl;

  server->Wait();
}


void connectSyncs () {
    Request request;
    request.set_server_type(SYNCHRONIZER);
    request.set_requester(SERVER);
    
  
    
    
    request.set_port_number(port);
    request.set_id(stoi(id));
    
    Reply reply;
    ClientContext context;
    Status status = stub->Syncs(&context, request, &reply);

    port1 = reply.msg();
    port2 = reply.msg2();
    
    cout << "ports: " << port1 << " " << port2 << endl;
    
    string login_info = coord_hostname + ":" + port1;
    cout << login_info << endl;
    stub1 = unique_ptr<SNSSync::Stub>(SNSSync::NewStub(
               grpc::CreateChannel(
                    login_info, grpc::InsecureChannelCredentials())));
                    
    login_info = coord_hostname + ":" + port2;
    stub2 = unique_ptr<SNSSync::Stub>(SNSSync::NewStub(
               grpc::CreateChannel(
                    login_info, grpc::InsecureChannelCredentials())));
    cout << login_info << endl;
    
    SyncRequest request1;
    request1.set_id(id);
    
    SyncReply reply1;
    ClientContext context1;
    
    SyncRequest request2;
    request2.set_id(id);
    
    SyncReply reply2;
    ClientContext context2;
    
    Status stat1 = stub1->check_connection(&context1, request1, &reply1);
    Status stat2 = stub2->check_connection(&context2, request2, &reply2);
    
    if(stat1.ok() && stat2.ok()) {
        connected = true;
        cout << "syncs connnected" << endl;
    } else {
        cout << "error: " << stat1.ok() << " " << stat2.ok() << endl;
    }
    
}

int connectToCoord() {
    cout << "connecting to coord" << endl;
    string login_info = coord_hostname + ":" + coord_port;
    stub = unique_ptr<SNSCoordinator::Stub>(SNSCoordinator::NewStub(
               grpc::CreateChannel(
                    login_info, grpc::InsecureChannelCredentials())));
                    
    Request request;
    request.set_server_type(SYNCHRONIZER);
    request.set_requester(SERVER);
    
  
    
    
    request.set_port_number(port);
    request.set_id(stoi(id));
    
    Reply reply;
    ClientContext context;

    Status status = stub->Login(&context, request, &reply); 
  
    
    if(!status.ok()) {
        return -1;
    }
    return 1;
}


int main(int argc, char** argv) {
    

    int opt = 0;
    
    while ((opt = getopt(argc, argv, "a:b:c:d:e:")) != -1){
        switch(opt) {
            case 'a':
                coord_hostname = optarg;
                break;
            case 'b':
                coord_port = optarg;
                break;
            case 'c':
                port = optarg;
                break;
            case 'd':
                id = optarg;
                break;
            
            default:
                cerr << "Invalid Command Line Argument\n";
        }
    }
    
    connectToCoord();
    
    ifstream logfile;
    stringstream ss;
    
    string output = "logfiles/"+routing_table[stoi(id)]+id+"_output.txt";
    string posts = "logfiles/"+routing_table[stoi(id)]+id+"posts.txt";
    struct stat buf;
    struct stat buf2;
    
    stat(output.c_str(), &buf);
    stat(posts.c_str(), &buf2);
    
    files.push_back(make_pair(buf.st_mtime, 0));
    files.push_back(make_pair(buf.st_mtime, 0));
    cout << " test1 " << endl;
    logfile.open(output);
    
     if(logfile.is_open()) {
        string line;
        string temp;
        while(getline(logfile, line)) {
          ss.str(line);
          while(getline(ss,temp,' ')) {
            if(temp=="newuser:") {
                Client c;

                getline(ss,temp);
                string user_id = temp;
                c.username = user_id;
                client_db.push_back(c);
            }
            else if(temp == "follow:") {
                getline(ss,temp, ' ');
                string user1 = temp;
          
                getline(ss,temp, ' ');
                string user2 = temp;
                
                if(find_user(user1) >=0 ) {
                    Client *client1 = &client_db[find_user(user1)];
                    client1->client_following.push_back(user2);
                }
                
                if(find_user(user2) >= 0 ) {
                    Client *client2 = &client_db[find_user(user2)];
                    client2->client_followers.push_back(user1);
                }
                
                
            }
          }
          ss.clear();
        }
    }
    
    logfile.close();
    cout << " test2 " << endl;
    ClientContext context;
    
    shared_ptr<grpc::ClientReaderWriter<HeartBeat,HeartBeat>> stream(stub->ServerCommunicate(&context));
    HeartBeat heartbeat;
    heartbeat.set_server_id(stoi(id));
    heartbeat.set_server_type(SYNCHRONIZER);
    
    thread writer([stream,heartbeat]() {
       time_t begin = time(0);
       while(1){
           stream->Write(heartbeat);
           this_thread::sleep_for(chrono::seconds(10)); 
        }
    });
    
    thread server_reader([stream, type] () {
        HeartBeat m;
        while(stream->Read(&m)) {
            if(m.server_id() == 1) {
                cout << "connecting syncs " << endl;
                connectSyncs();
            } else if (m.server_id() == -1) {
                type = "slave";
                cout << "switched to slave" << endl;
            }
        }
    });
    
    thread reader([files, type, id, stub1, stub2, connected]() {
        unordered_set<string> posts;

       while(1)
       {
            cout << "is connected: " << connected << endl;
            if(connected) {
                ifstream logfile;
                string filename = "logfiles/" + type + id + "_output.txt";
                //cout << "filename: " << filename << endl;
                logfile.open(filename);
                stringstream ss;
                struct stat buf;
                stat(filename.c_str(), &buf);
                
                if(difftime(buf.st_mtime,files[0].first) > 0 && logfile.is_open()) {
                    //cout << "Reading: filename: " << filename << endl;
                    string line;
                    int i = 0;
                    while(i < files[0].second) {
                        // cout << "pointer: " << files[0].second << endl;
                        getline(logfile, line);
                        // cout << "old lines " << line << endl;
                        i++;
                    }
                    
                    
                    string temp;
                    while(getline(logfile, line)) {
                        cout << " new line: " << line << endl;
                        files[0].second += 1;
                        ss.str(line);
                        while(getline(ss,temp,' ')) {
                            cout << temp << endl;
                            if(temp=="newuser:") {
                                getline(ss,temp);
                                string user_id = temp;
                                
                                SyncRequest request;
                                request.set_id(id);
                                request.set_username(user_id);
                                
                                SyncReply reply;
                                ClientContext context;
                                stub1->new_user(&context, request, &reply);
                                cout << "sent new user request to sync 1" << endl;
                                SyncReply reply2;
                                ClientContext context2;
                                SyncRequest request2;
                                request2.set_id(id);
                                request2.set_username(user_id);
                                stub2->new_user(&context2, request2, &reply2);
                                cout << "sent new user request to sync 1" << endl;
                                
                                Client c;
                                c.username = user_id;
                                client_db.push_back(c);
                                
                            }
                            else if(temp == "follow:") {
                                getline(ss,temp, ' ');
                                string user1 = temp;
                          
                                getline(ss,temp, ' ');
                                string user2 = temp;
                                
                                
                                SyncRequest request;
                                request.set_id(id);
                                request.set_username(user1);
                                request.set_msg(user2);
                                
                                SyncReply reply;
                                ClientContext context;
                                stub1->new_follow(&context, request, &reply);
                                
                                SyncReply reply2;
                                ClientContext context2;
                                SyncRequest request2;
                                request2.set_id(id);
                                request2.set_username(user1);
                                request2.set_msg(user2);
                                
                                stub2->new_follow(&context2, request2, &reply2);
                                
                                if(find_user(user1) >=0 ) {
                                    Client *client1 = &client_db[find_user(user1)];
                                    client1->client_following.push_back(user2);
                                }
                                
                                if(find_user(user2) >=0 ) {
                                    Client *client2 = &client_db[find_user(user2)];
                                    client2->client_followers.push_back(user1);
                                }
                            }
                        }
                        ss.clear();
                    }
                }
                
                ifstream postfile;
                string filename2 = "logfiles/" + type + id + "_posts.txt";
                
                postfile.open(filename2);
                struct stat buf2;
                
                cout << "Reading: filename: " << filename << endl;
                string line;
                
                string temp;
                while(getline(postfile, line)) {
                    if(posts.find(line) == posts.end()) {
                        posts.insert(line);
                        ss.str(line);
                        getline(ss,temp,' ');
                        ss.clear();
                        
                        // send to the other syncs
                        SyncRequest request;
                        request.set_id(id);
                        request.set_username(temp);
                        request.set_msg(line);
                        cout << temp << " " << id << " " << line << endl;
                        
                        SyncReply reply;
                        ClientContext context;
                        stub1->new_post(&context, request, &reply);
                       
                        SyncReply reply2;
                        ClientContext context2;
                        SyncRequest request2;
                        request2.set_id(id);
                        request2.set_username(temp);
                        request2.set_msg(line);
                        
                        stub2->new_post(&context2, request2, &reply2);
                    }
                }
            }
            this_thread::sleep_for(chrono::seconds(5)); 
        }
    });
    
    
    RunServer(port);
    cout << " test3 " << endl;
    
    reader.join();
    server_reader.join();
    writer.join();
}