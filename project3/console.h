#ifndef _CONSOLE_
#define _CONSOLE_
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
    CGI();
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

    
    char data_[1024];
};

#endif