//
//  peer_channel.hpp
//  sign_server
//
//  Created by chifl on 2020/6/25.
//  Copyright © 2020 chifl. All rights reserved.
//

#ifndef peer_channel_hpp
#define peer_channel_hpp

#include <time.h>
#include <queue>
#include <string>
#include <vector>

class DataSocket;

class ChannelMember{
public:
    explicit ChannelMember(DataSocket* socket);
    ~ChannelMember();
    
    bool connected() const { return connected_; }
    int id() const { return id_; }
    void set_disconnected() { connected_ = false; }
    bool is_wait_request(DataSocket* ds) const;
    const std::string& name() const { return name_; }
    
    bool TimeOut();
    
    std::string GetPeerIdHeader() const;
    bool NotifyOfOtherMember(const ChannelMember& other);
    
    std::string GetEntry() const;
    void ForwardRequestToPeer(DataSocket* ds, ChannelMember* peer);
    
    void OnClosing(DataSocket* ds);
    
    void QueueResponse(const std::string& status,
                       const std::string& content_type,
                       const std::string& extra_headers,
                       const std::string& data);
    
    void SetWaitingSocket(DataSocket* ds);
protected:
    struct QueuedResponse {
        std::string status, content_type, extra_headers, data;
    };
    
    DataSocket* waiting_socket_;
    int id_;
    bool connected_;
    time_t timestamp_;
    std::string name_;
    std::queue<QueuedResponse> queue_;
    static int s_member_id_;
};

class PeerChannel {
public:
    typedef std::vector<ChannelMember*> Members;
    
    PeerChannel() {}
    ~PeerChannel() { DeleteAll(); }
    
    const Members& members() const { return members_; }
    static bool IsPeerConnection(const DataSocket* ds);
    
    ChannelMember* Lookup(DataSocket* ds) const;
    
    ChannelMember* IsTargetedRequest(const DataSocket* ds) const;
    
    bool AddMember(DataSocket* ds);
    
    void CloseAll();
    
    void OnClosing(DataSocket* ds);
    
    void CheckForTimeout();
    
protected:
    void DeleteAll();
    void BroadcastChangedState(const ChannelMember& member, Members* delivery_failures);
    void HandleDeliveryFailures(Members* failures);
    
    std::string BuildResponseForNewMember(const ChannelMember& member,
                                          std::string* content_type);
protected:
    Members members_;
    
};
#endif /* peer_channel_hpp */
