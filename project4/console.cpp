#include "console.h"
using boost::asio::ip::tcp;
boost::asio::io_context io_context;
// ------------------ class CGI ------------------ //
CGI::CGI()
{
}

void CGI::init()
{
    parser();
    initHTMLheader();
    initHTMLbody();
    writeHTML(htmlcontent + htmlHead + htmlBody + htmlTail);
}

void CGI::parser()
{
    string host[5];
    int port[5];
    string file[5];
    char del = '&';
    string buf;
    string QUERY_STRING = getenv("QUERY_STRING");
    std::istringstream dataStream(QUERY_STRING);
    int test = 1;
    for (size_t i = 0; i < 5; i++)
    {
        string hostline, portline, fileline;

        getline(dataStream, buf, del);
        if (buf.size() <= 4)
        {
            test = 0;
        }

        hostline = buf;
        getline(dataStream, buf, del);
        portline = buf;
        getline(dataStream, buf, del);
        fileline = buf;
        if (test == 1)
        {
            host[i] = hostline.substr(3);
            port[i] = stoi(portline.substr(3));
            file[i] = fileline.substr(3);

            client.push_back(npClient(host[i], port[i], file[i]));
        }
    }
    // sh, sp
    getline(dataStream, buf, del);
    s_host = buf.substr(3);
    getline(dataStream, buf, del);
    s_port = buf.substr(3);
}

void CGI::initConnect()
{
    for (unsigned int i = 0; i < client.size(); i++)
    {
        conSocks(i);
    }
}

void CGI::initHTMLheader()
{
    htmlcontent = "Content-type: text/html\r\n\r\n";
    htmlHead = "<!DOCTYPE html>\n\
                        <html lang=\"en\">\n\
                        <head>\n\
                            <meta charset=\"UTF-8\" />\n\
                                <title>NP Project 3 Sample Console</title>\n\
                                    <link\
                                        rel=\"stylesheet\"\
                                        href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\"\
                                        integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\"\
                                        crossorigin=\"anonymous\"\
                                    />\n\
                                    <link\
                                        href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\
                                        rel=\"stylesheet\"\
                                    />\n\
                                    <link\
                                        rel=\"icon\"\
                                        type=\"image/png\"\
                                        href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\"\
                                    />\n\
                            <style>\n\
                                * {\
                                    font-family: 'Source Code Pro', monospace;\
                                    font-size: 1rem !important;\
                                }\n\
                                body {\
                                    background-color: #212529;\
                                }\n\
                                pre {\
                                    color: #cccccc;\
                                }\n\
                                b {\
                                    color: #01b468;\
                                }\n\
                            </style>\n\
                        </head>\n";
    htmlTail = "</html>";
}

void CGI::initHTMLbody()
{
    string bodyHead = "<body>\n\
    			        <table class=\"table table-dark table-bordered\">";
    string tableHead = "<thead><tr>\n";
    string tableBody = "<tbody><tr>\n";

    for (unsigned int i = 0; i < client.size(); i++)
    {
        tableHead = tableHead + "<th scope=\"col\">" + client[i].host + ":" + std::to_string(client[i].port) + "</th>\n";
        tableBody = tableBody + "<td><pre id=\"s" + std::to_string(i) + "\" class=\"mb-0\"></pre></td>\n";
    }
    tableHead = tableHead + "</tr></thead>\n";
    tableBody = tableBody + "</tr></tbody>\n";
    string BodyTail = "</table></body>\n";
    htmlBody = bodyHead + tableHead + tableBody + BodyTail;
}

void CGI::writeHTML(string text)
{
    std::cout << text;
    std::cout.flush();
}

void CGI::start()
{
    initConnect();
    // for (unsigned int i = 0; i < client.size(); i++)
    // {
    //     do_read(i);
    // }
    io_context.run();
}

void CGI::do_read(int i)
{
    client[i].socket.async_read_some(boost::asio::buffer(client[i].data_, 14336),
                                     [this, i](boost::system::error_code ec, std::size_t length)
                                     {
                                         if (!ec)
                                         {
                                             string buf(client[i].data_);
                                             buf = toHtmlStr(buf);
                                             addContent(i, buf);
                                             memset(client[i].data_, 0, 14336);
                                             if (buf.find('%') != string::npos) // read command and send to server and html after buf sending
                                             {
                                                 sendCommand(i);
                                             }
                                             else
                                                 do_read(i);
                                         }
                                         else
                                             do_read(i);
                                     });
}

string CGI::toHtmlStr(string str)
{
    string buffer;
    for (size_t i = 0; i < str.size(); i++)
    {
        switch (str[i])
        {
        case '&':
            buffer.append("&amp;");
            break;
        case '\"':
            buffer.append("&quot;");
            break;
        case '\'':
            buffer.append("&apos;");
            break;
        case '<':
            buffer.append("&lt;");
            break;
        case '>':
            buffer.append("&gt;");
            break;
        case '\r':
            break;
        case '\n':
            buffer.append("&NewLine;");
            break;
        default:
            buffer.append(&str[i], 1);
            break;
        }
    }

    return buffer;
}

void CGI::sendCommand(int i)
{
    string str = client[i].command[0];

    const char *buf = str.c_str();
    boost::asio::async_write(client[i].socket, boost::asio::buffer(buf, strlen(buf)),
                             [this, str, i](boost::system::error_code ec, std::size_t /*length*/)
                             {
                                 string content = toHtmlStr(str);
                                 string session = "s" + std::to_string(i);
                                 string htmlstr = "<script>document.getElementById('" + session + "').innerHTML += '<b>" + content + "</b>';</script>";
                                 writeHTML(htmlstr);

                                 client[i].command.erase(client[i].command.begin());
                                 do_read(i);
                             });
}

void CGI::addContent(int i, string content)
{
    string session = "s" + std::to_string(i);
    string str = "<script>document.getElementById('" + session + "').innerHTML += '" + content + "';</script>";
    writeHTML(str);
}

void CGI::conSocks(unsigned int i)
{

    tcp::resolver resolver(io_context);
    tcp::resolver::query query(client[i].host, std::to_string(client[i].port));
    tcp::resolver::iterator iter = resolver.resolve(query);
    tcp::endpoint ep = *iter;
    string ip = ep.address().to_string();

    client[i].connects[0] = 4;
    client[i].connects[1] = 1;
    // port
    int t = client[i].port / 256;
    int t2 = client[i].port % 256;
    client[i].connects[2] = char(t);
    client[i].connects[3] = char(t2);

    // ip
    std::istringstream ips(ip);
    string temp;
    for (int j = 4; j < 8; j++)
    {
        getline(ips, temp, '.');
        int t3 = stoi(temp);
        client[i].connects[j] = char(t3);
    }

    do_connect(i);
}

void CGI::do_connect(unsigned int i)
{
    tcp::resolver resolver2(io_context);
    boost::asio::ip::tcp::resolver::query socksquery(s_host, s_port);
    boost::asio::ip::tcp::resolver::iterator socksiter = resolver2.resolve(socksquery);
    boost::asio::ip::tcp::endpoint endpoint = *socksiter;

    client[i].socket.connect(endpoint);
    sendSOCKS4(i);
}

void CGI::sendSOCKS4(unsigned int i)
{

    boost::asio::async_write(client[i].socket, boost::asio::buffer(client[i].connects, 8),
                             [this, i](boost::system::error_code ec, std::size_t /*length*/)
                             {
                                 if (!ec)
                                 {

                                     recvReply(i);
                                 }
                             });
}

void CGI::recvReply(unsigned int i)
{
    client[i].socket.async_read_some(boost::asio::buffer(client[i].connects, 8),
                                     [this, i](boost::system::error_code ec, std::size_t length)
                                     {
                                         if (!ec)
                                         {

                                             do_read(i);
                                         }
                                     });
}

// ------------------ class npClient ------------------ //

npClient::npClient(string _host, int _port, string _file)
    : host(_host), port(_port), file(_file), socket(io_context)
{
    readFile();
}

// void npClient::connectNP(string s_host, string s_port)
// {
//     tcp::resolver resolver(io_context);
//     tcp::resolver::query query(host, std::to_string(port));
//     tcp::resolver::iterator iter = resolver.resolve(query);
//     tcp::endpoint ep = *iter;
//     string ip = ep.address().to_string();

//     connects[0] = 4;
//     connects[1] = 1;
//     // port
//     int t = port / 256;
//     int t2 = port % 256;
//     connects[2] = char(t);
//     connects[3] = char(t2);

//     // ip
//     std::istringstream ips(ip);
//     string temp;
//     for (int i = 4; i < 8; i++)
//     {
//         getline(ips, temp, '.');
//         int t3 = stoi(temp);
//         connects[i] = char(t3);
//     }

//     boost::asio::ip::tcp::resolver::query socksquery(s_host, s_port);
//     boost::asio::ip::tcp::resolver::iterator socksiter = resolver.resolve(socksquery);
//     boost::asio::ip::tcp::endpoint endpoint = *iter;

//     socket.connect(endpoint);
//     // sendSOCKS4();
// }

// void npClient::sendSOCKS4()
// {
//     boost::asio::async_write(socket, boost::asio::buffer(connect, 8),
//                              [this](boost::system::error_code ec, std::size_t /*length*/)
//                              {
//                                  if (!ec)
//                                      recvReply();
//                              });
// }

// void npClient::recvReply()
// {
//     socket.async_read_some(boost::asio::buffer(data_, 8),
//                            [this](boost::system::error_code ec, std::size_t length)
//                            {
//                                if (ec || data_[1] == 91)
//                                    socket.close();
//                                if (!ec)
//                                {
//                                    memset(data_, 0, 14336);
//                                }
//                            });
// }

void npClient::readFile()
{
    string fileName = "./test_case/" + file;
    std::ifstream fd(fileName.c_str());
    string buf;
    while (getline(fd, buf))
    {
        command.push_back(buf + "\n");
        buf.clear();
    }
    fd.close();
}

// -------------------- int main -------------------- //
int main(int argc, char *argv[])
{
    CGI console;
    console.init();
    console.start();
    return 0;
}