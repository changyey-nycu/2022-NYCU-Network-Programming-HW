#include "socks_server.h"
#include <fstream>

using boost::asio::ip::tcp;
boost::asio::io_context *session::io_context;
// ------------------ class session ------------------ //

session::session(tcp::socket socket) : socket_(std::move(socket))
{
    memset(data_, 0, max_length);
    userid = "";
    d_ip = "";
    command = "";
}

void session::setContext(boost::asio::io_context *_io_context)
{
    io_context = _io_context;
}

void session::start()
{
    readSOCKS4();
}

void session::readSOCKS4()
{
    auto self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(data_, 1024),
                            [this, self](boost::system::error_code ec, std::size_t length)
                            {
                                if (!ec)
                                {
                                    // std::cout << "data_ len :" << strlen(data_) << std::endl;
                                    parser(length);
                                    if (clientFirewall() == 0 || firewall() == 0 || d_ip == "0.0.0.0" || data_[0] != 0x04)
                                    {
                                        connectfail();
                                    }
                                    else
                                    {
                                        if (command == "BIND")
                                        {
                                            bindconnect();
                                        }
                                        else
                                        {
                                            SOCKS4connect();
                                        }
                                    }
                                }
                            });
}

void session::parser(int len)
{
    tcp::endpoint remote = socket_.remote_endpoint();
    s_ip = remote.address().to_string();
    s_port = remote.port();

    // command
    char CD = data_[1];
    if (CD == 0x01)
        command = "CONNECT";
    else if (CD == 0x02)
    {
        command = "BIND";
    }

    // port
    int t = int(data_[2]);
    if (t < 0)
        t = t + 256;
    d_port = t * 256;
    t = int(data_[3]);
    if (t < 0)
        t = t + 256;
    d_port = d_port + t;

    // ip
    d_ip = "";
    for (int i = 4; i < 8; i++)
    {
        if (data_[i] == 0)
        {
            // printf("is 0 ??");
            d_ip.append(std::to_string(0) + ".");
        }
        else
        {
            t = int(data_[i]);
            if (t < 0)
                t = t + 256;

            d_ip.append(std::to_string(t) + ".");
        }
        // std::cout << "t=" << t << "ip=" << d_ip << std::endl;
    }
    d_ip.pop_back();

    // copy for reply
    int i = 2;
    while (i < 8 && i < len)
    {
        Reply[i] = data_[i];
        i++;
    }
    // USERID, domain name
    userid = "";
    domainName = "";
    bool temp = false;
    for (int i = 8; i < len; i++)
    {
        if (data_[i] == 0)
        {
            temp = true;
            continue;
        }
        if (!temp)
        {
            userid.push_back(data_[i]);
            i++;
        }
        else
        {
            domainName.push_back(data_[i]);
            i++;
        }
    }
    // std::cout << "domainName:" << domainName << std::endl;
    // find ip
    if (data_[4] == 0 && data_[5] == 0 && data_[6] == 0 && d_ip != "0.0.0.0" && domainName != "")
    {
        tcp::resolver resolver(*io_context);
        tcp::resolver::query query(domainName.c_str(), std::to_string(d_port));
        tcp::resolver::iterator iter = resolver.resolve(query);
        tcp::endpoint ep = *iter;
        d_ip = ep.address().to_string();
        // std::cout << "domainName:" << domainName << " d_ip:" << d_ip;
    }
}

bool session::clientFirewall()
{
    bool pass = 0;
    std::fstream file;
    file.open("client_socks.conf");

    // file not exist (test)
    if (!file.is_open())
    {
        // std::cerr << "no conf file\n";
        return 1;
    }

    string commit, type, fip;
    string sip[4];
    std::istringstream ipStream(s_ip);
    for (size_t i = 0; i < 4; i++)
    {
        getline(ipStream, sip[i], '.');
    }

    string temp;
    while (file >> commit >> type >> fip)
    {
        if ((command == "CONNECT" && type == "c") || (command == "BIND" && type == "b"))
        {
            bool block = 0;
            std::istringstream fipStream(fip);
            for (size_t i = 0; i < 4; i++)
            {
                getline(fipStream, temp, '.');
                if (temp == "*" || temp == sip[i])
                    continue;
                else
                {
                    block = 1;
                    break;
                }
            }
            if (block == 0)
            {
                pass = 1;
                break;
            }
        }
    }

    file.close();
    return pass;
}

bool session::firewall()
{
    bool pass = 0;
    std::fstream file;
    file.open("socks.conf");

    // file not exist (test)
    if (!file.is_open())
    {
        // std::cerr << "no conf file\n";
        return 1;
    }

    string commit, type, fip;
    string dip[4];
    std::istringstream ipStream(d_ip);
    for (size_t i = 0; i < 4; i++)
    {
        getline(ipStream, dip[i], '.');
    }

    string temp;
    while (file >> commit >> type >> fip)
    {
        if ((command == "CONNECT" && type == "c") || (command == "BIND" && type == "b"))
        {
            bool block = 0;
            std::istringstream fipStream(fip);
            for (size_t i = 0; i < 4; i++)
            {
                getline(fipStream, temp, '.');
                if (temp == "*" || temp == dip[i])
                    continue;
                else
                {
                    block = 1;
                    break;
                }
            }
            if (block == 0)
            {
                pass = 1;
                break;
            }
        }
    }

    file.close();
    return pass;
}

void session::bindconnect()
{
    reply = "Accept";
    printMessage();
    tcp::endpoint endpoint(boost::asio::ip::address::from_string("0.0.0.0"), 0);
    acceptor_ = new tcp::acceptor(*session::io_context);
    acceptor_->open(tcp::v4());
    acceptor_->set_option(tcp::acceptor::reuse_address(true));
    acceptor_->bind(endpoint);
    acceptor_->listen(boost::asio::socket_base::max_connections);

    Reply[0] = 0;
    Reply[1] = 0x5A;
    int temp;
    temp = acceptor_->local_endpoint().port() / 256;
    Reply[2] = temp;
    temp = acceptor_->local_endpoint().port() % 256;
    Reply[3] = temp;

    for (int i = 4; i < 8; i++)
        Reply[i] = 0;
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(Reply, 8),
                             [this, self](boost::system::error_code ec, std::size_t /*length*/)
                             {
                                 if (!ec)
                                 {
                                     connectSock = new tcp::socket(*session::io_context);
                                     acceptor_->accept(*connectSock);
                                     boost::asio::write(socket_, boost::asio::buffer(Reply, 8));
                                     do_read1();
                                     do_read2();
                                 }
                             });
}

void session::SOCKS4connect()
{
    connectSock = new tcp::socket(*session::io_context);
    auto self(shared_from_this());
    tcp::endpoint ep(boost::asio::ip::address::from_string(d_ip), d_port);
    connectSock->async_connect(ep, [this, self](boost::system::error_code ec)
                               {
                                if (!ec)
                                {
                                    // std::cout << "do connect success" << std::endl;
                                    connectSuccess();
                                }
                                else
                                {
                                    connectfail();
                                } });
}

void session::connectSuccess()
{
    reply = "Accept";
    printMessage();
    sendReply(90);
}

void session::connectfail()
{
    reply = "Reject";
    printMessage();
    sendReply(91);
}

void session::sendReply(int CD)
{
    Reply[0] = 0;
    if (CD == 91)
        Reply[1] = 0x5B;
    else
    {
        Reply[1] = 0x5A;
        if (strlen(data_) >= 8 && command == "CONNECT")
        {
            for (int i = 2; i < 8; i++)
                Reply[i] = data_[i];
        }
        else if (command == "BIND")
        {
            string connectIp = connectSock->remote_endpoint().address().to_string();
            int connectport = connectSock->remote_endpoint().port();
            int temp = connectport / 256;
            Reply[2] = temp;
            temp = connectport % 256;
            Reply[3] = temp;

            std::istringstream ipStream(connectIp);
            string t;
            for (int i = 4; i < 8; i++)
            {

                for (size_t i = 0; i < 4; i++)
                {
                    getline(ipStream, t, '.');
                    temp = stoi(t);
                    Reply[i] = temp;
                }
            }
        }
    }

    auto self(shared_from_this());
    if (CD == 91) // reject
        boost::asio::async_write(socket_, boost::asio::buffer(Reply, 8),
                                 [this, self](boost::system::error_code ec, std::size_t /*length*/)
                                 {
                                     if (!ec)
                                     {
                                         socket_.close();
                                         exit(0);
                                     }
                                 });
    else
        boost::asio::async_write(socket_, boost::asio::buffer(Reply, 8),
                                 [this, self](boost::system::error_code ec, std::size_t /*length*/)
                                 {
                                     if (!ec)
                                     {
                                         //  std::cout << "do reading" << std::endl;
                                         do_read1();
                                         do_read2();
                                     }
                                 });
    // _io_context.run();
}

void session::do_read1()
{
    memset(data1, 0, max_length);
    auto self(shared_from_this());

    socket_.async_read_some(boost::asio::buffer(data1, max_length),
                            [this, self](boost::system::error_code ec, std::size_t length)
                            {
                                if (ec == boost::asio::error::eof)
                                {
                                    socket_.close();
                                    connectSock->close();
                                    exit(0);
                                }

                                if (!ec)
                                {
                                    // std::cout << "read socket_:" << data1 << std::endl;
                                    do_write1(length);
                                }
                            });
}

void session::do_read2()
{
    memset(data2, 0, max_length);
    auto self(shared_from_this());
    connectSock->async_read_some(boost::asio::buffer(data2, max_length),
                                 [this, self](boost::system::error_code ec, std::size_t length)
                                 {
                                     if (ec == boost::asio::error::eof)
                                     {
                                         socket_.close();
                                         connectSock->close();
                                         exit(0);
                                     }
                                     if (!ec)
                                     {
                                         //  std::cout << "read connectSocket:" << data2 << std::endl;
                                         do_write2(length);
                                     }
                                 });
}

void session::do_write1(std::size_t length)
{
    auto self(shared_from_this());
    boost::asio::async_write(*connectSock, boost::asio::buffer(data1, length),
                             [this, self](boost::system::error_code ec, std::size_t /*length*/)
                             {
                                 if (!ec)
                                 {
                                     //  std::cout << "write1:" << data1 << std::endl;
                                     do_read1();
                                 }
                             });
}

void session::do_write2(std::size_t length)
{
    auto self(shared_from_this());

    boost::asio::async_write(socket_, boost::asio::buffer(data2, length),
                             [this, self](boost::system::error_code ec, std::size_t /*length*/)
                             {
                                 if (!ec)
                                 {
                                     //  std::cout << "write2:" << data2 << std::endl;
                                     do_read2();
                                 }
                             });
}

void session::printMessage()
{
    std::cout << "<S_IP>: " << s_ip << std::endl
              << "<S_PORT>: " << s_port << std::endl
              << "<D_IP>: " << d_ip << std::endl
              << "<D_PORT>: " << d_port << std::endl
              << "<Command>: " << command << std::endl
              << "<Reply>: " << reply << std::endl
              << std::endl;
}

// ------------------ class server ------------------ //

server::server(boost::asio::io_context &io_context, short port) : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
{
    session::setContext(&io_context);
    do_accept();
}

void server::do_accept()
{
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket)
        {
            if (ec)
                return;
            if (!ec)
            {
                (*session::io_context).notify_fork(boost::asio::io_context::fork_prepare);

                pid_t pid = fork();
                if (pid == 0)
                {
                    (*session::io_context).notify_fork(boost::asio::io_context::fork_child);
                    acceptor_.close();
                    std::make_shared<session>(std::move(socket))->start();
                }
                else
                {
                    (*session::io_context).notify_fork(boost::asio::io_context::fork_parent);
                    socket.close();
                    do_accept();
                }
            }
        });
}

// ------------------ int main ------------------ //

void killChild(int sig)
{
    waitpid(-1, NULL, WNOHANG);
}

int main(int argc, char *argv[])
{
    // string t = getenv("PATH");
    // t = t + ":.";
    // setenv("PATH", t.c_str(), 1);
    signal(SIGCHLD, killChild);
    boost::asio::io_context io_context;
    int port = 8888;
    if (argc == 2)
    {
        port = std::atoi(argv[1]);
    }

    server s(io_context, port);

    io_context.run();

    return 0;
}