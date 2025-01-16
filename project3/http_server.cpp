#include "http_server.h"
using boost::asio::ip::tcp;

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
                                    // PrintEnv();
                                    if (REQUEST_URI == "/panel.cgi")
                                        startHttp();
                                    else
                                    {
                                        boost::asio::async_write(socket_, boost::asio::buffer("HTTP/1.1 403 Forbidden\n", strlen("HTTP/1.1 403 Forbidden\n")),
                                                                 [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                                                                    socket_.close();
                                                                 });
                                    }
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
    setEnv();
}

void session::setEnv()
{
    setenv("REQUEST_METHOD", REQUEST_METHOD.c_str(), 1);   // GET
    setenv("REQUEST_URI", REQUEST_URI.c_str(), 1);         // /.cgi
    setenv("QUERY_STRING", QUERY_STRING.c_str(), 1);       // h0=...
    setenv("SERVER_PROTOCOL", SERVER_PROTOCOL.c_str(), 1); // HTTP/1.1
    setenv("HTTP_HOST", HTTP_HOST.c_str(), 1);             // localhost:port
    setenv("SERVER_ADDR", SERVER_ADDR.c_str(), 1);
    setenv("SERVER_PORT", SERVER_PORT.c_str(), 1);
    setenv("REMOTE_ADDR", REMOTE_ADDR.c_str(), 1);
    setenv("REMOTE_PORT", REMOTE_PORT.c_str(), 1);
}

void session::callCgi()
{
    io_service_.notify_fork(boost::asio::io_service::fork_prepare);
    fflush(stdout);

    pid_t pid = fork();
    if (pid == 0)
    {
        io_service_.notify_fork(boost::asio::io_service::fork_child);
        int sock = socket_.native_handle();

        dup2(sock, STDERR_FILENO);
        dup2(sock, STDIN_FILENO);
        dup2(sock, STDOUT_FILENO);

        socket_.close();
        if (execlp(execfile.c_str(), execfile.c_str(), NULL) < 0)
        {
            std::cerr << "exec error" << std::endl;
            exit(1);
        }
    }
    else
    {
        io_service_.notify_fork(boost::asio::io_service::fork_parent);
        socket_.close();
    }
}

void session::PrintEnv()
{
    std::cout << "REQUEST_METHOD: " << REQUEST_METHOD << std::endl;
    std::cout << "REQUEST_URI: " << REQUEST_URI << std::endl;
    std::cout << "QUERY_STRING: " << QUERY_STRING << std::endl;
    std::cout << "SERVER_PROTOCOL: " << SERVER_PROTOCOL << std::endl;
    std::cout << "HTTP_HOST: " << HTTP_HOST << std::endl;
    std::cout << "SERVER_ADDR: " << SERVER_ADDR << std::endl;
    std::cout << "SERVER_PORT: " << SERVER_PORT << std::endl;
    std::cout << "REMOTE_ADDR: " << REMOTE_ADDR << std::endl;
    std::cout << "REMOTE_PORT: " << REMOTE_PORT << std::endl;
    std::cout << "execfile: " << execfile << std::endl;
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

void killChild(int sig)
{
    waitpid(-1, NULL, WNOHANG);
}

int main(int argc, char *argv[])
{
    string t = getenv("PATH");
    t = t + ":.";
    setenv("PATH", t.c_str(), 1);
    signal(SIGCHLD, killChild);

    int port = 8888;
    if (argc == 2)
    {
        port = std::atoi(argv[1]);
    }

    boost::asio::io_context io_context;

    server s(io_context, port);

    io_context.run();

    return 0;
}