//
//  peer_channel.cpp
//  sign_server
//
//  Created by chifl on 2020/6/25.
//  Copyright © 2020 chifl. All rights reserved.
//

#include "peer_channel.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>

#include "utils.hpp"
#include "data_socket.hpp"

static const char kPeerIdHeader[] = "Pragma: ";
static const char* kRequestPaths[] = {
    "/wait",
    "/sign_out",
    "/message",
};
enum RequestPathIndex {
    kWait,
    kSignOut,
    kMessage,
};
const size_t kMaxNameLength = 512;
int ChannelMember::s_member_id_ = 0;

ChannelMember::ChannelMember(DataSocket* socket)
    :waiting_socket_(nullptr),
      id_(++s_member_id_),
    connected_(true),
    timestamp_(time(nullptr))
{
    assert(socket);
    assert(socket->method() == DataSocket::GET);
    assert(socket->PathEquals("/sign_in"));
    name_ = socket->request_arguments();
    if (name_.empty())
        name_ = "peer_" + int2str(id_);
    else if (name_.length() > kMaxNameLength)
        name_.resize(kMaxNameLength);
    std::replace(name_.begin(), name_.end(), ',', '_');
}

ChannelMember::~ChannelMember() {}

bool ChannelMember::is_wait_request(DataSocket *ds) const
{
    return ds && ds->PathEquals(kRequestPaths[kWait]);
}

bool ChannelMember::TimeOut()
{
    return waiting_socket_ == nullptr && (time(nullptr) - timestamp_) > 30;
}
std::string ChannelMember::GetPeerIdHeader() const
{
    std::string ret(kPeerIdHeader + int2str(id_) + "\r\n");
    return ret;
}

bool ChannelMember::NotifyOfOtherMember(const ChannelMember &other)
{
    assert(&other != this);
    QueueResponse("200 OK", "text/plain", GetPeerIdHeader(), other.GetEntry());
    return true;
}

std::string ChannelMember::GetEntry() const
{
    assert(name_.length() <= kMaxNameLength);
    
    char entry[kMaxNameLength + 15];
    snprintf(entry, sizeof(entry), "%s,%d,%d\n", name_.substr(0, kMaxNameLength).c_str(), id_, connected_);
    return entry;
}

void ChannelMember::ForwardRequestToPeer(DataSocket *ds, ChannelMember *peer)
{
    assert(peer);
    assert(ds);
    std::string extra_headers(GetPeerIdHeader());
    
    if (peer == this)
    {
        ds->Send("200 OK", true, ds->content_type(), extra_headers, ds->data());
    }
    else
    {
        printf("client %s sending to %s\n", name_.c_str(), peer->name().c_str());
        peer->QueueResponse("200 OK", ds->content_type(), extra_headers, ds->data());
        ds->Send("200 OK", true, "text/plain", "", "");
    }
}

void ChannelMember::OnClosing(DataSocket *ds)
{
    if (ds == waiting_socket_)
    {
        waiting_socket_ = nullptr;
        timestamp_ = time(nullptr);
    }
}

void ChannelMember::QueueResponse(const std::string &status, const std::string &content_type, const std::string &extra_headers, const std::string &data)
{
    if (waiting_socket_)
    {
        assert(queue_.empty());
        assert(waiting_socket_->method() == DataSocket::GET);
        bool ok = waiting_socket_->Send(status, true, content_type, extra_headers, data);
        if (!ok)
        {
            printf("Failed to deliver data to waiting socket\n");
        }
        waiting_socket_ = nullptr;
        timestamp_ = time(nullptr);
    }
    else
    {
        QueuedResponse qr;
        qr.status = status;
        qr.content_type = content_type;
        qr.extra_headers = extra_headers;
        qr.data = data;
        queue_.push(qr);
    }
}

void ChannelMember::SetWaitingSocket(DataSocket *ds)
{
    assert(ds->method() == DataSocket::GET);
    if (ds && !queue_.empty())
    {
        assert(waiting_socket_ == nullptr);
        const QueuedResponse& response = queue_.front();
        ds->Send(response.status, true, response.content_type, response.extra_headers, response.data);
        queue_.pop();
    }
    else
    {
        waiting_socket_ = ds;
    }
}

/***********peerChannel*********/
//static
bool PeerChannel::IsPeerConnection(const DataSocket *ds)
{
    assert(ds);
    return (ds->method() == DataSocket::POST && ds->content_length() > 0) ||
    (ds->method() == DataSocket::GET && ds->PathEquals("/sign_in"));
}

ChannelMember* PeerChannel::Lookup(DataSocket *ds) const
{
    assert(ds);
    
    if (ds->method() != DataSocket::GET && ds->method() != DataSocket::POST)
        return nullptr;
    size_t i = 0;
    for (; i < ARRAYSIZE(kRequestPaths); ++i)
    {
        if (ds->PathEquals(kRequestPaths[i]))
            break;
    }
    if (i == ARRAYSIZE(kRequestPaths))
        return nullptr;
    
    std::string args(ds->request_arguments());
    static const char kPeerId[] = "peer_id=";
    size_t found = args.find(kPeerId);
    if (found == std::string::npos)
        return nullptr;
    
    int id = atoi(&args[found + ARRAYSIZE(kPeerId) - 1]);
    Members::const_iterator iter = members_.begin();
    for (; iter != members_.end(); ++iter)
    {
        if (id == (*iter)->id())
        {
            if (i == kWait)
                (*iter)->SetWaitingSocket(ds);
            if (i == kSignOut)
                (*iter)->set_disconnected();
            return *iter;
        }
    }
    return nullptr;
}

ChannelMember* PeerChannel::IsTargetedRequest(const DataSocket *ds) const
{
    assert(ds);
    const std::string& path = ds->request_path();
    size_t args = path.find('?');
    if (args == std::string::npos)
        return nullptr;
    size_t found;
    const char kTargetPeerIdParam[] = "to=";
    do {
        found = path.find(kTargetPeerIdParam, args);
        if (found == std::string::npos)
            return nullptr;
        if (found == (args + 1) || path[found - 1] == '&')
        {
            found += ARRAYSIZE(kTargetPeerIdParam) - 1;
            break;
        }
        args = found + ARRAYSIZE(kTargetPeerIdParam) - 1;
    }while (true);
    int id = atoi(&path[found]);
    Members::const_iterator i = members_.begin();
    for (; i != members_.end(); ++i)
    {
        if ((*i)->id() == id)
        {
            return (*i);
        }
    }
    return nullptr;
}

bool PeerChannel::AddMember(DataSocket* ds)
{
    assert(IsPeerConnection(ds));
    ChannelMember* new_guy = new ChannelMember(ds);
    Members failures;
    BroadcastChangedState(*new_guy, &failures);
    HandleDeliveryFailures(&failures);
    members_.push_back(new_guy);
    
    printf("New member added (total=%s); %s\n", size_t2str(members_.size()).c_str(),
           new_guy->name().c_str());
    
    std::string content_type;
    std::string response = BuildResponseForNewMember(*new_guy, &content_type);
    ds->Send("200 Added", true, content_type, new_guy->GetPeerIdHeader(), response);
    return true;
}

void PeerChannel::CloseAll()
{
    Members::const_iterator i = members_.begin();
    for (; i != members_.end(); ++i)
    {
        (*i)->QueueResponse("200 OK", "text/plain", "", "Server shutting down");
    }
    DeleteAll();
}

void PeerChannel::OnClosing(DataSocket *ds)
{
    for (Members::iterator i = members_.begin(); i != members_.end(); ++i)
    {
        ChannelMember* m = (*i);
        m->OnClosing(ds);
        if (!m->connected())
        {
            i = members_.erase(i);
            Members failures;
            BroadcastChangedState(*m, &failures);
            HandleDeliveryFailures(&failures);
            delete m;
            if (i == members_.end())
                break;
        }
    }
    printf("Total connected: %s\n", size_t2str(members_.size()).c_str());
}

void PeerChannel::CheckForTimeout()
{
    for (Members::iterator i = members_.begin(); i != members_.end(); ++i)
    {
        ChannelMember* m = (*i);
        if (m->TimeOut())
        {
            printf("Timeout: %s\n", m->name().c_str());
            m->set_disconnected();
            i = members_.erase(i);
            Members failures;
            BroadcastChangedState(*m, &failures);
            HandleDeliveryFailures(&failures);
            delete m;
            if (i == members_.end())
                break;
        }
    }
}

void PeerChannel::DeleteAll()
{
    for (Members::iterator i = members_.begin(); i != members_.end(); ++i)
        delete (*i);
    members_.clear();
}

void PeerChannel::BroadcastChangedState(const ChannelMember &member, Members *delivery_failures)
{
    assert(delivery_failures);
    if (!member.connected())
    {
        printf("Member disconnected: %s\n", member.name().c_str());
    }
    
    Members::iterator i = members_.begin();
    for (; i != members_.end(); ++i)
    {
        if (&member != (*i))
        {
            if (!(*i)->NotifyOfOtherMember(member))
            {
                (*i)->set_disconnected();
                delivery_failures->push_back(*i);
                i = members_.erase(i);
                if (i == members_.end())
                    break;
            }
        }
    }
}

void PeerChannel::HandleDeliveryFailures(Members* failures)
{
    assert(failures);
    
    while (!failures->empty()) {
        Members::iterator i = failures->begin();
        ChannelMember* member = *i;
        assert(!member->connected());
        failures->erase(i);
        BroadcastChangedState(*member, failures);
        delete member;
    }
}

std::string PeerChannel::BuildResponseForNewMember(const ChannelMember &member, std::string *content_type)
{
    assert(content_type);
    
    *content_type = "text/plain";
    std::string response(member.GetEntry());
    for (Members::iterator i = members_.begin(); i != members_.end(); ++i)
    {
        if (member.id() != (*i)->id())
        {
            assert((*i)->connected());
            response += (*i)->GetEntry();
        }
    }
    return response;
}
