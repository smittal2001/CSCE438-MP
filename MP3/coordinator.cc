#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>

#include "snsCoordinator.grpc.pb.h"


using namespace std;

using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using snsCoordinator::HeartBeat;
using snsCoordinator::Request;
using snsCoordinator::Reply;
using snsCoordinator::SNSCoordinator;
using snsCoordinator::MASTER;
using snsCoordinator::SLAVE;
using snsCoordinator::SYNCHRONIZER;



enum server_status {
  INACTIVE = 0,
  ACTIVE = 1
};

bool done[3];

unordered_map<int, pair<string,server_status> > masters;
unordered_map<int, pair<string, server_status> > slaves;
unordered_map<int, string > syncs;

int status[4];
vector<int> heartbeats;

class CoordinatorImpl final : public SNSCoordinator::Service { 
  string getServer(int client_id) {
    int serverId = -1;
    
    serverId = (client_id % 3) + 1;
    
    if( masters[serverId].second == ACTIVE) {
      cout << serverId << endl;
      return masters[serverId].first;
    }
    
    return slaves[serverId].first;
  }

  Status Login(ServerContext* context, const Request* request, Reply* reply) override {
    int requester = request->requester();
    cout << "loggin in..." << requester <<endl;
    
    // server
    if(requester == 1) {
      int serverType = request->server_type();
      int id = request->id();
      
      string port = request->port_number(); 
      server_status s = ACTIVE;
      // it is the master
      if(serverType == 0) {
        if ( masters.find(id) == masters.end()) {
          masters[id] = make_pair(port, s);
          status[id] = 2;
          reply->set_msg(slaves[id].first);
        }
      } else if(serverType == 1) {
         if ( slaves.find(id) == slaves.end()) {
          slaves[id] = make_pair(port, s);
          //slaves_status[id] = true;
          reply->set_msg("Slave Connected");
        }
      }
      else if(serverType == 3) {
        syncs[id] = port;
        done[id-1] = false;
      }
    } 
    else {
      //client
      int id = request->id();
      string port = "port";
      port = getServer(id);
      cout << id << " " << port << endl;
      //port number
      reply->set_msg(port);
    }
  
    return Status::OK;
  }

  Status ServerCommunicate (ServerContext* context, ServerReaderWriter<HeartBeat, HeartBeat>* stream) override {
    HeartBeat hb;
    
   
    
    int server_id;
    string serverType;
    
    while(stream->Read(&hb)) { 
      server_id = hb.server_id();
      
      if(hb.server_type() == MASTER) {
        serverType = "master";
        if(masters[server_id].second == INACTIVE) {
          masters[server_id].second = ACTIVE;
        }
      } else if(hb.server_type() == SLAVE) {
        serverType = "slave";
        if(slaves[server_id].second == INACTIVE) {
          slaves[server_id].second = ACTIVE;
        }
      }
      else if(hb.server_type() == SYNCHRONIZER) {
        string status = "";
        if(syncs.size() == 3 && !done[server_id-1]) {
          done[server_id-1] = true;
          HeartBeat sync;
          sync.set_server_id(1);
          stream->Write(sync);
        }
        if(masters[server_id].second == INACTIVE) {
          HeartBeat slave;
          slave.set_server_id(-1);
          stream->Write(slave);
        }
       
      }

      cout << server_id << endl;
    }
    
    if(serverType == "master") {
      masters[server_id].second = INACTIVE;
      
    } else {
      slaves[server_id].second = INACTIVE;
    }
    string status = "";

    for(auto entry : masters) {
      cout << entry.first << " " << entry.second.second << endl;
      if(entry.second.second == INACTIVE) {
        status += "master ";
      } else {
        status += "slave ";
      }
    }
    
    string filename = "status_" + server_id;
    ofstream user_file(filename,std::ios::app|std::ios::out|std::ios::in);
    user_file << status;

    return Status::OK;
  }
  
  Status Syncs(ServerContext* context, const Request* request, Reply* reply) override {
    int id = request->id();
    string ports[2];
    
    int i = 0;
    for ( auto entry : syncs) {
      if(entry.first == id) continue;
      ports[i] = entry.second;
      i++;
    }
    reply->set_msg(ports[0]);
    reply->set_msg2(ports[1]);
    return Status::OK;
  }
};



void RunServer(string port_no) {
  string server_address = "0.0.0.0:"+port_no;
  CoordinatorImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  unique_ptr<Server> server(builder.BuildAndStart());
  cout << "Coordinator Server listening on " << server_address << std::endl;

  server->Wait();
}



int main(int argc, char** argv) {
  int opt = 0;
  string port;
  while ((opt = getopt(argc, argv, "p:")) != -1){
    switch(opt) {
      case 'p':
        port = optarg;
        break;
      default:
      std::cerr << "Invalid Command Line Argument\n";
    }
  }
  RunServer(port);
}