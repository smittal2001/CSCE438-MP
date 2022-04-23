#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>
#include <thread>
#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_set>
#include <memory>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>
#include <regex>

#include "sns.grpc.pb.h"
#include "snsCoordinator.grpc.pb.h"

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
using snsCoordinator::SERVER;
using snsCoordinator::MASTER;
using snsCoordinator::SLAVE;
using snsCoordinator::HeartBeat;

using csce438::Message;
using csce438::ListReply;
using csce438::ServiceRequest;
using csce438::ServiceReply;
using csce438::SNSService;
using csce438::TimelineRequest;


using namespace std;
unique_ptr<SNSCoordinator::Stub> stub;
int outputFileSize = 0;
unordered_set<string> output_lines;
unordered_set<string> post_lines;

struct Client {
  std::string username;
  bool connected = true;
  int following_file_size = 0;
  std::vector<Client*> client_followers;
  std::vector<Client*> client_following;
  ServerReaderWriter<Message, Message>* stream = 0;
  bool operator==(const Client& c1) const{
    return (username == c1.username);
  }
};

string output;
//Vector that stores every client that has been created
vector<Client> client_db;
unordered_set<string> usernames;

string type = "";
int id;
unique_ptr<SNSService::Stub> slave_stub_;

//Helper function used to find a Client object given its username
int find_user(std::string username){
  int index = 0;
  for(Client c : client_db){
    cout << "user " << c.username << endl;
    if(c.username == username)
      return index;
    index++;
  }
  cout << "no user" << endl;
  return -1;
}

class SNSServiceImpl final : public SNSService::Service {
  Status Slave_Timeline(ServerContext* context, const TimelineRequest* request, ServiceReply* reply) override {
    string filename = "slave" + to_string(id) + "_userfiles/" +request->username()+"_timeline.txt";
    ofstream user_file(filename,std::ios::app|std::ios::out|std::ios::in);
     
    user_file << request->msg();
    user_file.close();
     
    string filename2 = "logfiles/slave" + to_string(id) + "_posts.txt";
    ofstream file(filename2,std::ios::app|std::ios::out|std::ios::in);
    file << request->msg();
     return Status::OK;
  }
  
  
  Status List(ServerContext* context, const ServiceRequest* request, ListReply* list_reply) override {
    
    if(find_user(request->username()) >= 0) {
      Client user = client_db[find_user(request->username())];
      int index = 0;
      for(Client c : client_db){
        list_reply->add_all_users(c.username);
      }
      std::vector<Client*>::const_iterator it;
      for(it = user.client_followers.begin(); it!=user.client_followers.end(); it++){
        list_reply->add_followers((*it)->username);
      }
    } 
    else {
      cout << "ERROR: user does not exist" << endl;
    }
    
    return Status::OK;
  }
  
  Status Follow(ServerContext* context, const ServiceRequest* request, ServiceReply* reply) override {
    string username1 = request->username();
    string username2 = request->arguments(0);
    
    if(find_user(username2) < 0 || find_user(username1) < 0) {
      reply->set_msg("Join Failed -- Invalid Username");
      return Status::OK;
    }
    
    if (type == "master") {
        cout << "slave follow request" << endl;
        ServiceReply reply2;
        ClientContext context2;
        ServiceRequest request2;
        request2.set_username(request->username());
        request2.add_arguments(request->arguments(0));
        Status status = slave_stub_->Follow(&context2, request2, &reply2);
    }
    
    
    
    ofstream fout;
    fout.open(output,  std::ios::in | std::ios_base::app | std::ios_base::ate);
    fout<< "follow: " << username1 << " "<< username2 << endl;
    output_lines.insert("follow: " + username1 + " " + username2);
    fout.close();
    outputFileSize++;
    
    
    int join_index = find_user(username2);
    
    if(join_index < 0 || username1 == username2)
      reply->set_msg("Join Failed -- Invalid Username");
    else{
      Client *user1 = &client_db[find_user(username1)];
      Client *user2 = &client_db[join_index];
      if(find(user1->client_following.begin(), user1->client_following.end(), user2) != user1->client_following.end()){
	    reply->set_msg("Join Failed -- Already Following User");
        return Status::OK;
      }
      user1->client_following.push_back(user2);
      user2->client_followers.push_back(user1);
      reply->set_msg("Follow Successful");
    }
    return Status::OK; 
  }
  
  Status Login(ServerContext* context, const ServiceRequest* request, ServiceReply* reply) override {
    Client c;
    string username = request->username();
    cout << "logging in is user: " << username << endl;
    int user_index = find_user(username);
    
    if(type == "master" ) {
        cout << "slave login request" << endl;
        ServiceReply reply2;
        ClientContext context2;
        ServiceRequest request2 = *request;
        Status status = slave_stub_->Login(&context2, request2, &reply2);
    }
    
    if(user_index < 0){
        cout << "new user " << output << endl;
        
        ofstream fout;
        fout.open(output, std::ios::app|std::ios::out|std::ios::in);
        
        fout<< "newuser: " << username << endl;
        output_lines.insert("newuser: " + username);
        fout.close();
        
        c.username = username;
        client_db.push_back(c);
        usernames.insert(username);
        reply->set_msg("Login Successful!");
    }
    else{ 
      Client *user = &client_db[user_index];
      if(user->connected)
        reply->set_msg("Invalid Username");
      else{
        std::string msg = "Welcome Back " + user->username;
	      reply->set_msg(msg);
        user->connected = true;
      }
    }
    return Status::OK;
  }
  Status Timeline(ServerContext* context, ServerReaderWriter<Message, Message>* stream) override {
    string username;
    Message message;
    stream->Read(&message);
    username = message.username();
    
    thread writer([stream, post_lines, type] () {
      Message message;
      Client *c;
      string username;
      TimelineRequest req;
      ServiceReply rep;
    
      while(stream->Read(&message)) {
        username = message.username();
        int user_index = find_user(username);
        c = &client_db[user_index];
        
        // write it to the server posts and the users timeline
        string filename = type + to_string(id) + "_userfiles/" +username+"_timeline.txt";
        ofstream user_file(filename,std::ios::app|std::ios::out|std::ios::in);
        ofstream file("logfiles/" + type+ to_string(id) + "_posts.txt", std::ios::app|std::ios::out|std::ios::in );
        
        //create the message
        google::protobuf::Timestamp temptime = message.timestamp();
        time_t time = temptime.seconds();
        string t_str = ctime(&time);
        t_str[t_str.size()-1] = '\0';
        
        string fileinput = message.username()+ " (" + t_str + ") >> " + message.msg();
        cout << fileinput << endl;
        
        //Check if you need to set the stream first
          if(message.msg() != "Set Stream") {
            //if you are the master send the request to the slave to write in the slave files
            if(type == "master" ) {
              ClientContext context2;
              req.set_username(username);
              req.set_msg(fileinput);
              slave_stub_->Slave_Timeline(&context2, req, &rep);
            }
            
            // record the message in the posts and timeline files
            user_file << fileinput;
            file << fileinput;
            
            //remove the new lines and store it in post_lines set
            regex newlines_re("\n+");
            string result = regex_replace(fileinput, newlines_re, "");
            post_lines.insert(result);
            
            cout << "inserted:  " << fileinput << endl; 
          }
        
        
        //If message = "Set Stream", print the first 20 chats from the people you follow
        else{
          if(c->stream==0) c->stream = stream;
          //open the user timeline textfile
          string line;
          vector<string> newest_twenty;
          ifstream in(type + to_string(id) + "_userfiles/" +username+"_timeline.txt");
          int count = 0;
          //Read the last up-to-20 lines (newest 20 messages) from userfollowing.txt
          while(getline(in, line)){
              if(c->following_file_size > 20){
                  if(count < c->following_file_size-20) {
                      count++;
                      continue;
                  }
              }
            newest_twenty.push_back(line);
            post_lines.insert(line);
          }
          Message new_msg; 
   	    
   	    //Send the newest messages to the client to be displayed
  	    for(int i = 0; i<newest_twenty.size(); i++){
      	    
      	    new_msg.set_msg(newest_twenty[i]);
      	    new_msg.set_username("display");
            stream->Write(new_msg);
            cout << "whats this " << newest_twenty[i] << endl;
          }    
          continue;
        }
        //Send the message to each follower's stream
        
        vector<Client*>::const_iterator it;
        
        for(it = c->client_followers.begin(); it!=c->client_followers.end(); it++){
          Client *temp_client = *it;
        	if(temp_client->stream!=0 && temp_client->connected) temp_client->stream->Write(message);
          //For each of the current user's followers, put the message in their following.txt file
          string temp_username = temp_client->username;
          
          //only send to followers that are in this server
          if(usernames.find(temp_username) != usernames.end() ) {
            //get the followers timeline text file name
            string temp_file = type + to_string(id) + "_userfiles/" +temp_username+"_timeline.txt";
            cout << temp_file << endl;
            cout << message.msg() << endl;
            //if you are the master send the request to the slave
            if(type == "master" && message.msg() != "Set Stream" ) {
                ClientContext context3;
                req.set_username(temp_username);
                req.set_msg(fileinput);
                slave_stub_->Slave_Timeline(&context3, req, &rep);
            }
            
            if(message.msg() != "Set Stream") {
              //open the follower tiemeline
              ofstream following_file(temp_file,std::ios::app|std::ios::out|std::ios::in);
              
              google::protobuf::Timestamp temptime = message.timestamp();
              time_t time = temptime.seconds();
              string t_str = ctime(&time);
              t_str[t_str.size()-1] = '\0';
              
              string inp = message.username()+ " (" + t_str + ") >> " + message.msg();
              //write the message in the followers timeline;
              cout << inp << endl;
      	      following_file << inp;
              temp_client->following_file_size++;
               
            }
          }
        }
      }
      //If the client disconnected from Chat Mode, set connected to false
      cout << "user: " << username << endl;
      c->connected = false;
    });
    
    
    
    thread reader([post_lines, stream, type, id, username]() {
      cout << "reading user: " << username << endl;
      while(1) {
        ifstream new_check(type + to_string(id) + "_userfiles/" +username+"_timeline.txt");
        string line;
        while(getline(new_check, line)){
          regex newlines_re("\n+");
          string result = regex_replace(line, newlines_re, "");
          line = result;
          if(post_lines.find(line) == post_lines.end()) {
            cout << "sending new line " << line << endl;
            post_lines.insert(line);
            Message new_msg2; 
      	    new_msg2.set_msg(line);
      	    new_msg2.set_username("display");
            stream->Write(new_msg2);
          }
        }
      } 
    });
    
    reader.join();
    writer.join();
    
    return Status::OK;
  }

};


void RunServer(string port_no) {
  cout << "runnning server" << endl;
  string server_address = "0.0.0.0:"+port_no;
  SNSServiceImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  unique_ptr<Server> server(builder.BuildAndStart());
  cout << "Server listening on " << server_address << std::endl;

  server->Wait();
}

 int connectToCoord(string hostname, string coord_port, string port, int id, string type) {
    cout << "connecting to coord" << endl;
    string login_info = hostname + ":" + coord_port;
    stub = unique_ptr<SNSCoordinator::Stub>(SNSCoordinator::NewStub(
               grpc::CreateChannel(
                    login_info, grpc::InsecureChannelCredentials())));

    Request request;
    request.set_requester(SERVER);
    
    if(type== "master" ) {
        request.set_server_type(MASTER);
    } else {
        request.set_server_type(SLAVE);
    }
    
    
    request.set_port_number(port);
    request.set_id(id);
    
    Reply reply;
    ClientContext context;
    Status status = stub->Login(&context, request, &reply);
    cout << reply.msg() << endl;

    if(type == "master") {
        string slave_port = reply.msg();
        cout << "slave port: " << slave_port << endl;
        string login_info2 = hostname+":"+slave_port;
        slave_stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(
              grpc::CreateChannel(
                    login_info2, grpc::InsecureChannelCredentials())));
    }
    
    if(!status.ok()) {
        return -1;
    }
    return 1;
}



int main(int argc, char** argv) {
    string port = "3010";
    string coord_hostname = "localhost";
    string coord_port = "9000";

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
                id = stoi(optarg);
                break;
            case 'e':
                type = optarg;
                break;
            default:
                cerr << "Invalid Command Line Argument\n";
        }
    }
  
    connectToCoord(coord_hostname, coord_port, port, id, type);
    ifstream logfile;
    stringstream ss;

    output = "logfiles/"+type+to_string(id)+"_output.txt";
    cout << output << endl;
    logfile.open(output);
    
     if(logfile.is_open()) {
        string line;
        string temp;
        while(getline(logfile, line)) {
          output_lines.insert(line);
          ss.str(line);
          while(getline(ss,temp,' ')) {
            if(temp=="newuser:") {
                Client c;

                getline(ss,temp);
                string user_id = temp;
                c.username = user_id;
            
                c.connected = false;
                client_db.push_back(c);
                cout << "new user " << c.username << endl;
                usernames.insert(c.username);
            }
            else if(temp == "follow:") {
                getline(ss,temp, ' ');
                string user1 = temp;
          
                getline(ss,temp, ' ');
                string user2 = temp;
                
                cout << user1 << " " << user2 << endl;
                
                Client *client1 = &client_db[find_user(user1)];
                Client *client2 = &client_db[find_user(user2)];
                client1->client_following.push_back(client2);
                client2->client_followers.push_back(client1);
            }
            if(temp=="SYNC::newuser:") {
              Client c;

              getline(ss,temp);
              string user_id = temp;
              c.username = user_id;
          
              c.connected = false;
              client_db.push_back(c);
              cout << "new user from sync " << c.username << endl;
            }
            else if(temp == "SYNC::follow:") {
                getline(ss,temp, ' ');
                string user1 = temp;
          
                getline(ss,temp, ' ');
                string user2 = temp;
                
                cout <<" new follow from sync" << user1 << " " << user2 << endl;
                
                Client *client1 = &client_db[find_user(user1)];
                Client *client2 = &client_db[find_user(user2)];
                client1->client_following.push_back(client2);
                client2->client_followers.push_back(client1);
            }
          }
          ss.clear();
        }
    }
    logfile.close();
    

    cout << "port " << port << endl;
    string login_info = coord_hostname+":"+coord_port;
    // stub = unique_ptr<SNSCoordinator::Stub>(SNSCoordinator::NewStub(
    //           grpc::CreateChannel(
    //                 login_info, grpc::InsecureChannelCredentials())));
    
    ClientContext context;
    
    shared_ptr<grpc::ClientReaderWriter<HeartBeat,HeartBeat>> stream(
        stub->ServerCommunicate(&context));
        HeartBeat heartbeat;
        heartbeat.set_server_id(id);
        if (type == "master"){
            heartbeat.set_server_type(MASTER);
        }
        else{
           heartbeat.set_server_type(SLAVE); 
        }
        
    thread writer([stream,heartbeat]() {
       time_t begin = time(0);
       while(1){
           stream->Write(heartbeat);
           this_thread::sleep_for(chrono::seconds(10)); 
        }
    });
    
    thread check_new([output_lines, output, client_db]() {
       //time_t begin = time(0);
       while(1){
          ifstream userRead;
          string temp;
          userRead.open(output);
          if(userRead.is_open()) {
            string line;
            stringstream ss;
            string temp;   
            
            while(getline(userRead, line)){
              if(output_lines.find(line) == output_lines.end()) {
                output_lines.insert(line);
                ss.str(line);
                while(getline(ss,temp,' ')) {
                  if(temp=="SYNC::newuser:") {
                    Client c;
    
                    getline(ss,temp);
                    string user_id = temp;
                    c.username = user_id;
                    
                    c.connected = false;
                    client_db.push_back(c);
                    cout << "SYNC new user " << c.username << endl;
                  }
                  else if(temp == "SYNC::follow:") {
                      getline(ss,temp, ' ');
                      string user1 = temp;
                
                      getline(ss,temp, ' ');
                      string user2 = temp;
                      
                      cout <<"SYNC new follow " << user1 << " " << user2 << endl;
                      
                      Client *client1 = &client_db[find_user(user1)];
                      Client *client2 = &client_db[find_user(user2)];
                      client1->client_following.push_back(client2);
                      client2->client_followers.push_back(client1);
                  }
                }
                ss.clear();
              }
            }
        }
    }});
    
    RunServer(port);

    
    writer.join();
    check_new.join();
    return 0;
}


