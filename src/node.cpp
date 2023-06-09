#include "node.h"

#include "proto_messages.h"
#include "util/proto_messages.h"

#include <fstream>

Node::Node() {
  fingerTable = vector<pair<contactInfo_t, digest_t> >(
      my_config::finger_table_length);  // Construct an empty finger table
                                        // with a length (specified in
                                        // config.h)
}

/*
    Initialize the node server and listen on the port to accept connections from
   other nodes. Accept nodeRequest and spawn to handle it.
*/
void Node::run_server() {
  int server_fd = initializeServer(my_config::listening_port_num);
  cout << "Start listening on port" << my_config::listening_port_num << endl;
  while (1) {
    string clientIP;
    try {
      int client_fd = serverAcceptConnection(server_fd, clientIP);
      cout << "Accepted a connection from " << clientIP << endl;
      proto_in* in = new proto_in(client_fd);
      NodeRequest request;
      recvMesgFrom<NodeRequest>(request, in);
      cout << "Received a request " << request.DebugString() << endl;
      delete in;
      close(client_fd);

      // TODO: Spawn a new thread to handle the request
      int64_t type = request.type();
      switch (type) {
        case 2:
          // AddFileRequest fm128
        {
            const AddFileRequest & add_file_req = request.addfile();
            thread t = thread(&Node::add_file_req_handle, this, add_file_req);
            t.detach();
            break;
        }
        case 3:  // LookupFileRequest ky99
        {
          const LookupFileRequest& lfr = request.lookup();
          thread t = thread(&Node::lookup_req_handle, this, lfr);
          t.detach();
          break;
        }
        case 4:
          // DeleteFileRequest fm128
        {
            const DeleteFileRequest & del_file_req = request.deletefile();
            thread t = thread(&Node::del_file_req_handle, this, del_file_req);
            t.detach();
            break;
        }
        case 5:  // DownloadRequest ky99
        {
          const DownloadRequest& d_req = request.download();
          thread t = thread(&Node::download_req_handle, this, d_req);
          t.detach();
          break;
        }
        case 6:  // JoinRequest jz399
        {
          const JoinRequest& join_req = request.join();
          thread t = thread(&Node::join_req_handle, this, join_req);
          t.detach();
          break;
        }
        case 7:  // RouteUpdateRequest jz399
        {
          const RouteUpdateRequest& update_route_req = request.updateroute();
          thread t = thread(&Node::handle_update_route_req, this, update_route_req);
          t.detach();
          break;
        }
        case 8:  // RouteDeleteRequest jz399
          break;
        case 9:  // HelpJoinRequest jz399
        {
          const HelpJoinRequset& help_join_req = request.helpjoin();
          thread t = thread(&Node::help_join_req_handle, this, help_join_req);
          t.detach();
          break;
        }
        default:
          cout << "Invalid Request Type: " << type << endl;
      }
    } catch (const std::exception& e) {
      std::cerr << e.what() << endl;
      continue;
    }
  }
  close(server_fd);
}

/*
    User terminal interface
*/
void Node::run_user_terminal_interface() {
  cout << "Welcome to the file sharing system! What can we help with you "
          "today?"
       << endl;
  while (1) {
    // Print the options
    cout << "Please choose a number from the following options:" << endl;

    cout << "1 Upload a file" << endl;
    cout << "2 Delete a file" << endl;
    cout << "3 Search if a file exists" << endl;
    cout << "4 Download a file" << endl;
    cout << "5 Node exit" << endl;

    // Receive command from user
    string option_str;
    cin >> option_str;
    int option;
    try {
      option = stoi(option_str);
    } catch (const exception& e) {
      cout << "Invalid input, please select a number from the provided ";
      continue;
    }

    // Process command
    switch (option) {
      case 1:
        // TODO: fm128 这里调用add file函数
      {
          cout << "Please enter the name of the file you want to upload\n\n";
          string filename;
          cin >> filename;
          add_file(filename, my_config::listening_port_num);
          break;
      }
      case 2:
        // TODO: fm128 这里调用delete file函数
      {
          cout << "Please enter the name of the file you want to delete\n\n";
          string filename;
          cin >> filename;
          delete_file(filename, my_config::listening_port_num);
          break;
      }
      case 3:
        // TODO: ky99 这里调用lookup file函数
        {
          cout << "Please enter the name of the file you are looking for \n\n";
          string file_name;
          cin >> file_name;
          digest_t file_hash = get_hash(file_name);
          bool does_exist;
          contactInfo_t successor, owner;
          lookup(file_hash, my_config::user_interface_port_num, &does_exist,
                 &successor, &owner);
          if (does_exist) {
            cout << "\nYes! We have this file!\n\n";
          } else {
            cout << "\nSorry! We do not have this file.\n\n";
          }
          break;
        }
      case 4:
        // TODO: ky99 这里调用lookup file, 然后downlod file
        {
          cout << "Please enter the name of the file you are looking for \n\n";
          string file_name;
          cin >> file_name;
          download(file_name);
          break;
        }
      case 5:
        // TODO: jz399 这里调用节点退出函数
        exit(EXIT_SUCCESS);
      default:
        cout << "Invalid input, please select a number from the provided "
                "options!"
             << endl;
        continue;  // Start over, print the options again and let user choose
    }

    cout << "\nYour request has been completed! What else can we do?" << endl;
  }
}

/*
    Set up the node, spawn the listen thread => join the circle => run user
   terminal interface
*/
void Node::main() {
 try{
  my_hash = get_hash(my_hostname);
 
  // Spawn a thread to init server and listen
  thread listenThread(&Node::run_server, this);

  cout<< "my_hash: " << my_hash << endl;
  cout<< "my_hostname: " << my_hostname << endl;

  // Join the chord circle
  if (entryNode.first == "0.0.0.0") {
    initial_chord();
  } else {
    join_chord();
  }

  run_user_terminal_interface();
 }
 catch (const std::exception& e) {
  std::cerr << e.what() << endl;
 }
}

/*
  Get the SHA-1 hash for a given key
  Each node will be assigned a unique ID (within 2^m (m is 48 in this case))
  by hashing key which will be hostname of that node by SHA-1 Algorithm
  => a maximum of 2^48 nodes can join the circle, the finger table length is
  log2(2^m)=48
*/
digest_t Node::get_hash(string key) {
  unsigned char obuf[41];
  char finalHash[41];
  string keyHash = "";
  long unsigned int i;
  long unsigned int m = my_config::finger_table_length;
  digest_t mod = pow(2, m);

  // convert string to an unsigned char array because SHA1 takes unsigned char
  // array as parameter
  unsigned char unsigned_key[key.length() + 1];
  for (i = 0; i < key.length(); i++) {
    unsigned_key[i] = key[i];
  }
  unsigned_key[i] = '\0';

  SHA1(unsigned_key, sizeof(unsigned_key), obuf);
  for (i = 0; i < m / 8; i++) {
    sprintf(finalHash, "%d", obuf[i]);
    keyHash += finalHash;
  }

  digest_t hash = stoll(keyHash) % mod;

  return hash;
}

bool file_exist(const string & filename) {
    string fn = "./shared_file/" + filename;
    ifstream f(fn);
    return f.good();
}

void Node::add_file(string filename, const string & port) {
    //check if the file is uploaded to ./shared_file/ successfully
    if (!file_exist(filename)) {
        cerr << "Failed to add file: please upload the file into shared_file folder first!\n";
        return;
        // exit(EXIT_FAILURE);
    }
    //find successor node
    digest_t hash_filename = get_hash(filename);
    cout << "File hash for file " << filename << " is " << hash_filename << endl;
    pair<bool, contactInfo_t> successor_pair;
    successor_pair = lookup_successor(hash_filename, my_config::user_interface_port_num);
    // if (!successor_pair.first) {
    //    cerr << "No node yet!\n";
    //    exit(EXIT_FAILURE);
    // }
    //add to localFiles
    localFiles[hash_filename] = filename;
    //generate and send add_file packet to the successor node
    NodeRequest request = generate_add_file_request(hash_filename, my_hostname, port);
    ProtoStreamOut proto_stream_out(successor_pair.second.first, successor_pair.second.second);
    proto_out * out = proto_stream_out.get_proto_out();
    sendMesgTo<NodeRequest> (request, out);
    proto_stream_out.close_proto_out();
}

void Node::add_file_req_handle(const AddFileRequest & request) {
    digest_t hash_filename = request.filenamehash();
    string src_hostname = request.sourcehostname();
    string src_port = request.sourceport();

    //check if the file is uploaded to ./shared_file/ successfully
    if (!file_exist(localFiles[hash_filename])) {
        cerr << "Failed to add file: please upload the file into shared_file folder first!\n";
        exit(EXIT_FAILURE);
    }
    //check if the file already exists in DHT
    if (DHT.count(hash_filename)) {
        cout << "File already exists, no need to add again!\n";
        return;
    }
    //add to DHT
    contactInfo_t info(src_hostname, src_port);
    DHT[hash_filename] = info;
}

int Node::delete_file(string filename, const string & port) {
    digest_t hash_filename = get_hash(filename);
    //find owner node and successor node
    contactInfo_t successor, owner;
    bool does_exist;
    lookup(hash_filename, my_config::user_interface_port_num, &does_exist, &successor, &owner);
    if (!does_exist) {
        cerr << "No successor node or no such file exists!\n";
        exit(EXIT_FAILURE);
    }
    //if the node is the owner
    if (owner.first == my_hostname) {
        //delete file in localFiles
        localFiles.erase(hash_filename);
        //generate and send delete_file packet to the successor node
        NodeRequest request = generate_delete_file_request(hash_filename, my_hostname, port);
        ProtoStreamOut proto_stream_out(successor.first, successor.second);
        proto_out * out = proto_stream_out.get_proto_out();
        sendMesgTo<NodeRequest> (request, out);
        proto_stream_out.close_proto_out();
    }
}

void Node::del_file_req_handle(const DeleteFileRequest & request) {
    digest_t hash_filename = request.filenamehash();

    //check if the file exists DHT
    if (!DHT.count(hash_filename)) {
        cerr << "Failed to delete file: It is not in the DHT!\n";
        exit(EXIT_FAILURE);
    }
    //delete the file entry in DHT
    DHT.erase(hash_filename);
}
