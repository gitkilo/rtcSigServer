//
//  main.cpp
//  sign_server
//
//  Created by chifl on 2020/6/25.
//  Copyright © 2020 chifl. All rights reserved.
//

#include <iostream>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <time.h>
#include <string>
#include <vector>
#include "data_socket.hpp"
#include "peer_channel.hpp"

//ABSL_FLAG(
//    std::string,
//    force_fieldtrials,
//    "",
//    "Field trials control experimental features. This flag specifies the field "
//    "trials in effect. E.g. running with "
//    "--force_fieldtrials=WebRTC-FooFeature/Enabled/ "
//    "will assign the group Enabled to field trial WebRTC-FooFeature. Multiple "
//    "trials are separated by \"/\"");
//ABSL_FLAG(int, port, 8888, "default: 8888");

static const size_t kMaxConnections = (FD_SETSIZE - 2);

void HandleBrowserRequest(DataSocket* ds, bool* quit)
{
    assert(ds && ds->valid());
    assert(quit);
    
    const std::string& path = ds->request_path();
    *quit = (path.compare("/quit") == 0);
    
    if (*quit)
    {
        ds->Send("200 OK", true, "text/html", "", "<html><body>Quitting...</body></html>");
    }
    else if (ds->method() == DataSocket::OPTIONS)
    {
        ds->Send("200 OK", true, "", "", "");
    }
    else
    {
        printf("Received an invalid request: %s\n", ds->request_path().c_str());
        ds->Send("500 Sorry", true, "text/html", "",
                 "<html><body>Sorry, not yet implemented</body></html>");
    }
}

int main(int argc, const char * argv[]) {
    std::cout << "Hello, World!\n";
    int port = 8888;
    if (argc > 1 && argv[1])
    {
        port = atoi(argv[1]);
    }
    
    // Abort if the user specifies a port that is outside the allowed
    // range [1, 65535].
    if ((port < 1) || (port > 65535)) {
      printf("Error: %i is not a valid port.\n", port);
      return -1;
    }

    ListeningSocket listener;
    if (!listener.Create()) {
      printf("Failed to create server socket\n");
      return -1;
    } else if (!listener.Listen(port)) {
      printf("Failed to listen on server socket\n");
      return -1;
    }

    printf("Server listening on port %i\n", port);

    PeerChannel clients;
    typedef std::vector<DataSocket*> SocketArray;
    SocketArray sockets;
    bool quit = false;
    while (!quit) {
      fd_set socket_set;
      FD_ZERO(&socket_set);
      if (listener.valid())
        FD_SET(listener.socket(), &socket_set);

      for (SocketArray::iterator i = sockets.begin(); i != sockets.end(); ++i)
        FD_SET((*i)->socket(), &socket_set);

      struct timeval timeout = {10, 0};
      if (select(FD_SETSIZE, &socket_set, NULL, NULL, &timeout) == SOCKET_ERROR) {
        printf("select failed\n");
        break;
      }

      for (SocketArray::iterator i = sockets.begin(); i != sockets.end(); ++i) {
        DataSocket* s = *i;
        bool socket_done = true;
        if (FD_ISSET(s->socket(), &socket_set)) {
          if (s->OnDataAvailable(&socket_done) && s->request_received()) {
            ChannelMember* member = clients.Lookup(s);
            if (member || PeerChannel::IsPeerConnection(s)) {
              if (!member) {
                if (s->PathEquals("/sign_in")) {
                  clients.AddMember(s);
                } else {
                  printf("No member found for: %s\n", s->request_path().c_str());
                  s->Send("500 Error", true, "text/plain", "",
                          "Peer most likely gone.");
                }
              } else if (member->is_wait_request(s)) {
                // no need to do anything.
                socket_done = false;
              } else {
                ChannelMember* target = clients.IsTargetedRequest(s);
                if (target) {
                  member->ForwardRequestToPeer(s, target);
                } else if (s->PathEquals("/sign_out")) {
                  s->Send("200 OK", true, "text/plain", "", "");
                } else {
                  printf("Couldn't find target for request: %s\n",
                         s->request_path().c_str());
                  s->Send("500 Error", true, "text/plain", "",
                          "Peer most likely gone.");
                }
              }
            } else {
              HandleBrowserRequest(s, &quit);
              if (quit) {
                printf("Quitting...\n");
                FD_CLR(listener.socket(), &socket_set);
                listener.Close();
                clients.CloseAll();
              }
            }
          }
        } else {
          socket_done = false;
        }

        if (socket_done) {
          printf("Disconnecting socket\n");
          clients.OnClosing(s);
          assert(s->valid());  // Close must not have been called yet.
          FD_CLR(s->socket(), &socket_set);
          delete (*i);
          i = sockets.erase(i);
          if (i == sockets.end())
            break;
        }
      }

      clients.CheckForTimeout();

      if (FD_ISSET(listener.socket(), &socket_set)) {
        DataSocket* s = listener.Accept();
        if (sockets.size() >= kMaxConnections) {
          delete s;  // sorry, that's all we can take.
          printf("Connection limit reached\n");
        } else {
          sockets.push_back(s);
          printf("New connection...\n");
        }
      }
    }

    for (SocketArray::iterator i = sockets.begin(); i != sockets.end(); ++i)
      delete (*i);
    sockets.clear();

    return 0;
}
