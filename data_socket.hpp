//
//  data_socket.hpp
//  sign_server
//
//  Created by chifl on 2020/6/25.
//  Copyright © 2020 chifl. All rights reserved.
//

#ifndef data_socket_hpp
#define data_socket_hpp

#include <stdio.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>

#define closesocket close
typedef int NativeSocket;
#ifndef SOCKET_ERROR
#define SOCKET_ERROR (-1)
#endif

#ifndef INVALID_SOCKET
#define INVALID_SOCKET static_cast<NativeSocket>(-1)
#endif
#include <string>

class SocketBase {
public:
    SocketBase():socket_(INVALID_SOCKET){}
    explicit SocketBase(NativeSocket socket):socket_(socket){}// 防止类构造函数的隐式自动转换
    ~SocketBase(){Close();}
    
    NativeSocket socket() const{return socket_;}
    bool valid() const {return socket_ != INVALID_SOCKET;}
    
    bool Create();
    void Close();
protected:
    NativeSocket socket_;
};

class DataSocket : public SocketBase {
public:
    enum RequestMethod {
        INVALID,
        GET,
        POST,
        OPTIONS,
    };
    explicit DataSocket(NativeSocket socket)
    :SocketBase(socket), method_(INVALID), content_length_(0){}
    ~DataSocket(){}
    
    static const char kCrossOriginAllowHeaders[];
    
    bool headers_received() const { return method_ != INVALID; }
    RequestMethod method() const { return method_; }
    
    const std::string& request_path() const { return request_path_; }
    std::string request_arguments() const;
    
    const std::string& data() const { return data_; }
    const std::string& content_type() const { return content_type_; }
    size_t content_length() const { return content_length_; }
    
    bool request_received() const {
        return headers_received() && (method_ != POST || data_received());
    }
    
    bool data_received() const {
        return method_ != POST || data_.length() >= content_length_;
    }
    
    bool PathEquals(const char* path) const;
    
    // Called when we have received some data from clients.
    // Returns false if an error occurred.
    bool OnDataAvailable(bool* close_socket);
    
    // 发送raw数据
    bool Send(const std::string& data) const;
    
    bool Send(const std::string& status,
              bool connection_close,
              const std::string& content_type,
              const std::string& extra_headers,
              const std::string& data) const;
    
    void Clear();
protected:
    bool ParseHeaders();
    
    bool ParseMethodAndPath(const char* begin, size_t len);
    
    bool ParseContentLengthAndType(const char* headers, size_t length);
protected:
    RequestMethod method_;
    size_t content_length_;
    std::string content_type_;
    std::string request_path_;
    std::string request_headers_;
    std::string data_;
};

// the server socket. Accepts connections and generates DataSocket instances for each new connection.
class ListeningSocket : public SocketBase {
public:
    ListeningSocket(){}
    
    bool Listen(unsigned short port);
    DataSocket* Accept() const;
};
#endif /* data_socket_hpp */
