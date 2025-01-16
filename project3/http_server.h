#ifndef _HTTP_SERVER_
#define _HTTP_SERVER_
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <memory>
#include <string>
#include <sys/wait.h>
#include <utility>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;
using std::string;

class session : public std::enable_shared_from_this<session>
{
public:
    session(tcp::socket socket);
    void start();

private:
    void do_read();
    void startHttp();
    void parser();
    void setEnv();
    void callCgi();
    void PrintEnv();

    boost::asio::io_service io_service_;
    tcp::socket socket_;
    enum
    {
        max_length = 1024
    };
    char data_[max_length];

    string REQUEST_METHOD;
    string REQUEST_URI;
    string QUERY_STRING;
    string SERVER_PROTOCOL;
    string HTTP_HOST;
    string SERVER_ADDR;
    string SERVER_PORT;
    string REMOTE_ADDR;
    string REMOTE_PORT;

    string execfile;
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