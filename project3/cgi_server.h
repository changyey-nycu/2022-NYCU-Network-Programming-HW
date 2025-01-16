#ifndef _CGI_SERVER_
#define _CGI_SERVER_
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;
using std::string;
using std::vector;

class npClient : public std::enable_shared_from_this<npClient>
{
public:
    npClient(string _host, int _port, string _file);
    void readFile();
    void connectNP();

    string host;
    int port;
    string file;
    tcp::socket socket;
    vector<string> command;
    char data_[14336];
};

class CGI : public std::enable_shared_from_this<CGI>
{
public:
    CGI(string query, tcp::socket _socket);
    void init();
    void start();
    string toHtmlStr(string str);

private:
    void parser();
    void initConnect();

    void initHTMLheader();
    void initHTMLbody();

    void writeHTML(string text);

    void do_read(int i);
    void addContent(int i, string content);
    void sendCommand(int i);

    vector<npClient> client;
    string htmlcontent;
    string htmlHead;
    string htmlBody;
    string htmlTail;

    string QUERY_STRING;
    tcp::socket socket;
    char data_[1024];
};

class panel : public std::enable_shared_from_this<panel>
{
public:
    panel(tcp::socket _socket);
    void start();
    string genHTTP();
    void writePanel(string str);

private:
    tcp::socket socket;
};

class session : public std::enable_shared_from_this<session>
{
public:
    session(tcp::socket socket);
    void start();

private:
    void do_read();
    void startHttp();
    void parser();
    void callCgi();

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