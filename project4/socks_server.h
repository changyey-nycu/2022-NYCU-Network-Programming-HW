#ifndef _SOCK_SERVER_
#define _SOCK_SERVER_
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <memory>
#include <utility>
#include <sys/wait.h>
#include <boost/asio.hpp>
using boost::asio::ip::tcp;
using std::string;

class session : public std::enable_shared_from_this<session>
{
public:
    session(tcp::socket socket);
    void start();
    static boost::asio::io_context *io_context;
    static void setContext(boost::asio::io_context *_io_context);

private:
    void readSOCKS4();
    void parser(int len);
    bool firewall();
    bool clientFirewall();
    void bindconnect();
    void SOCKS4connect();
    void connectSuccess();
    void connectfail();
    void sendReply(int CD);
    void printMessage();
    void do_read1();
    void do_read2();
    void do_write1(std::size_t length);
    void do_write2(std::size_t length);

    tcp::socket socket_;
    tcp::socket *bindSock;
    tcp::socket *connectSock;
    tcp::acceptor *acceptor_;
    enum
    {
        max_length = 10240
    };
    char data_[1024];
    char data1[max_length];
    char data2[max_length];

    unsigned char Reply[8];

    string userid;
    string s_ip;
    int s_port;
    string d_ip;
    int d_port;
    string command;
    string reply;
    string domainName;
};

class server
{
public:
    server(boost::asio::io_context &io_context, short port);

private:
    void do_accept();

    tcp::acceptor acceptor_;
};

#endif