#include "cgi_server.h"
using boost::asio::ip::tcp;

boost::asio::io_context io_context;

// ------------------ class CGI ------------------ //

CGI::CGI(string query, tcp::socket _socket) : socket(std::move(_socket))
{
    QUERY_STRING = query;
}

void CGI::init()
{
    parser();
    initConnect();
    initHTMLheader();
    initHTMLbody();
    writeHTML(htmlcontent + htmlHead + htmlBody + htmlTail);
    start();
}

void CGI::parser()
{
    string host[5];
    int port[5];
    string file[5];
    char del = '&';
    string buf;
    // string QUERY_STRING = getenv("QUERY_STRING");
    std::istringstream dataStream(QUERY_STRING);
    for (size_t i = 0; i < 5; i++)
    {
        string hostline, portline, fileline;

        getline(dataStream, buf, del);
        if (buf.size() <= 4)
        {
            break;
        }

        hostline = buf;
        getline(dataStream, buf, del);
        portline = buf;
        getline(dataStream, buf, del);
        fileline = buf;

        host[i] = hostline.substr(3);
        port[i] = stoi(portline.substr(3));
        file[i] = fileline.substr(3);

        client.push_back(npClient(host[i], port[i], file[i]));
    }
}

void CGI::initConnect()
{
    for (std::vector<npClient>::iterator i = client.begin(); i != client.end(); i++)
    {
        i->connectNP();
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
    const char *buf = text.c_str();
    boost::asio::async_write(socket, boost::asio::buffer(buf, strlen(buf)),
                             [](boost::system::error_code ec, std::size_t /*length*/) {});
}

void CGI::start()
{
    for (unsigned int i = 0; i < client.size(); i++)
        do_read(i);
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

// ------------------ class npClient ------------------ //

npClient::npClient(string _host, int _port, string _file)
    : host(_host), port(_port), file(_file), socket(io_context)
{
    readFile();
}

void npClient::connectNP()
{
    tcp::resolver resolver(io_context);
    tcp::resolver::query query(host, std::to_string(port));
    tcp::resolver::iterator iter = resolver.resolve(query);
    tcp::endpoint ep = *iter;
    socket.async_connect(ep, [this](boost::system::error_code ec) {});
}

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

// ------------------ class panel ------------------ //

panel::panel(tcp::socket _socket) : socket(std::move(_socket))
{
}

void panel::start()
{
    string httpPage = genHTTP();
    writePanel(httpPage);
}

string panel::genHTTP()
{
    string htmlcontent = "Content-type: text/html\r\n\r\n";
    string htmlHead = "<!DOCTYPE html>\n\
                        <html lang=\"en\">\n\
                        <head>\n\
                                <title>NP Project 3 Panel</title>\n\
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
                                * {font-family: 'Source Code Pro', monospace;}\n\
                            </style>\n\
                        </head>\n";
    string htmlbody = "<body class=\"bg-secondary pt-5\">\
    <form action=\"GET\" method=\"console.cgi\">\
      <table class=\"table mx-auto bg-light\" style=\"width: inherit\">\
        <thead class=\"thead-dark\">\
          <tr>\
            <th scope=\"col\">#</th>\
            <th scope=\"col\">Host</th>\
            <th scope=\"col\">Port</th>\
            <th scope=\"col\">Input File</th>\
          </tr>\
        </thead>";
    string tblbody = "<tbody>";
    string hostmenu = "<option></option><option value=\"nplinux1.cs.nctu.edu.tw\">nplinux1</option><option value=\"nplinux2.cs.nctu.edu.tw\">nplinux2</option><option value=\"nplinux3.cs.nctu.edu.tw\">nplinux3</option>\
                  <option value=\"nplinux4.cs.nctu.edu.tw\">nplinux4</option><option value=\"nplinux5.cs.nctu.edu.tw\">nplinux5</option><option value=\"nplinux6.cs.nctu.edu.tw\">nplinux6</option>\
                  <option value=\"nplinux7.cs.nctu.edu.tw\">nplinux7</option><option value=\"nplinux8.cs.nctu.edu.tw\">nplinux8</option><option value=\"nplinux9.cs.nctu.edu.tw\">nplinux9</option>\
                  <option value=\"nplinux10.cs.nctu.edu.tw\">nplinux10</option><option value=\"nplinux11.cs.nctu.edu.tw\">nplinux11</option><option value=\"nplinux12.cs.nctu.edu.tw\">nplinux12</option>";
    string testmenu = "<option></option><option value=\"t1.txt\">t1.txt</option><option value=\"t2.txt\">t2.txt</option><option value=\"t3.txt\">t3.txt</option><option value=\"t4.txt\">t4.txt</option><option value=\"t5.txt\">t5.txt</option>";
    for (int i = 0; i < 5; i++)
    {
        string temp = "<tr>\
            <th scope=\"row\" class=\"align-middle\">Session " +
                      std::to_string(i + 1) + "</th>\
            <td>\
              <div class=\"input-group\">\
                <select name=\"h" +
                      std::to_string(i) + "\" class=\"custom-select\">" + hostmenu +
                      "</select>\
                <div class=\"input-group-append\">\
                  <span class=\"input-group-text\">.cs.nctu.edu.tw</span>\
                </div>\
              </div>\
            </td>\
            <td>\
              <input name=\"p" +
                      std::to_string(i) + "\" type=\"text\" class=\"form-control\" size=\"5\" />\
            </td>\
            <td>\
              <select name=\"f" +
                      std::to_string(i) + "\" class=\"custom-select\">" +
                      testmenu +
                      "</select>\
            </td>\
          </tr>";
        tblbody = tblbody + temp;
    }

    string htmlTail = "<tr><td colspan=\"3\"></td>\
            <td>\
              <button type=\"submit\" class=\"btn btn-info btn-block\">Run</button>\
            </td>\
          </tr>\
        </tbody>\
      </table>\
    </form>\
  </body>\
</html>";
    return htmlcontent + htmlHead + htmlbody + tblbody + htmlTail;
}

void panel::writePanel(string str)
{
    boost::asio::async_write(socket, boost::asio::buffer(str.c_str(), str.length()),
                             [](boost::system::error_code ec, std::size_t /*length*/) {});
    return;
}

// ------------------ class session ------------------ //

session::session(tcp::socket socket) : socket_(std::move(socket))
{
}

void session::start()
{
    do_read();
}

void session::do_read()
{
    auto self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(data_, max_length),
                            [this, self](boost::system::error_code ec, std::size_t length)
                            {
                                if (!ec)
                                {
                                    parser();
                                    startHttp();
                                }
                            });
}

void session::startHttp()
{
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer("HTTP/1.1 200 OK\n", strlen("HTTP/1.1 200 OK\n")),
                             [this, self](boost::system::error_code ec, std::size_t /*length*/)
                             {
                                 if (!ec)
                                 {
                                     callCgi();
                                 }
                             });
}

void session::parser()
{
    string httpData(data_);
    string t;
    std::istringstream dataStream(httpData);
    dataStream >> REQUEST_METHOD >> REQUEST_URI >> SERVER_PROTOCOL >> t >> HTTP_HOST;

    dataStream = std::istringstream(REQUEST_URI);
    getline(dataStream, REQUEST_URI, '?');
    getline(dataStream, QUERY_STRING);

    tcp::endpoint loacl = socket_.local_endpoint();
    tcp::endpoint remote = socket_.remote_endpoint();
    SERVER_ADDR = loacl.address().to_string();
    SERVER_PORT = std::to_string(loacl.port());

    REMOTE_ADDR = remote.address().to_string();
    REMOTE_PORT = std::to_string(remote.port());

    execfile = REQUEST_URI.substr(1);
}

void session::callCgi()
{
    if (REQUEST_URI == "/panel.cgi")
    {
        std::make_shared<panel>(std::move(socket_))->start();
    }
    else
    {
        std::make_shared<CGI>(QUERY_STRING, std::move(socket_))->init();
    }
}

// ------------------ class server ------------------ //

server::server(boost::asio::io_context &io_context, short port) : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
{
    boost::asio::socket_base::reuse_address option(true);
    acceptor_.set_option(option);
    do_accept();
}

void server::do_accept()
{
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket)
        {
            if (!ec)
            {
                std::make_shared<session>(std::move(socket))->start();
            }

            do_accept();
        });
}

// ------------------ int main ------------------ //

int main(int argc, char *argv[])
{
    string t = getenv("PATH");
    t = t + ":.";
    setenv("PATH", t.c_str(), 1);

    int port = 8888;
    if (argc == 2)
    {
        port = std::atoi(argv[1]);
    }

    server s(io_context, port);

    io_context.run();

    return 0;
}