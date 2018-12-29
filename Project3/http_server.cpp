#include <iostream>
#include <string>
#include <vector>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include<stdio.h>

using namespace std;
using namespace boost::asio;

io_service global_io_service;
char myDirectory[64] = "/u/gcs/106/0656120/NP/Project3";

class Header{
    public:
        string method;
        string path;
        string qString;
        string protocol;
        string host;
        string sAddr;
        string sPort;
        string rAddr;
        string rPort;
};

int split(string line, vector<string>& array, string delimiter){
    int count = 0; //count the number of delimiters
    std::string::size_type begin, end;
    begin = 0;
    end = line.find(delimiter);

    while(std::string::npos != end){ 
        if((end-begin) > 0){
            // avoid store NULL into array
            array.push_back(line.substr(begin, end-begin));
            count++;
        }    
        begin = end + delimiter.size();
        end = line.find(delimiter, begin);        
    }
    if(begin != line.length())
        array.push_back(line.substr(begin));
    return count;
}

class HttpSession : public enable_shared_from_this<HttpSession> {
    private:
        ip::tcp::socket _socket;
        boost::asio::streambuf _response;
        enum { max_length = 1024 };
        array<char, max_length> _data;
        Header headerParse;
        pid_t childpid;

    public:
        HttpSession(ip::tcp::socket socket) : _socket(move(socket)) {}

        void start(){ 
            do_read();
        }

    private:
        void do_read() {
            auto self(shared_from_this());
            _socket.async_read_some(
                buffer(_data, max_length),
                [this, self](boost::system::error_code ec, std::size_t length) {
                    if (!ec) do_parser();
            });
        }
        void do_parser(){
            auto self(shared_from_this());
            string getStr(_data.data());
            vector<string> dataList, goalList, pathAndQString, hostAndPort;
            split(getStr, dataList, "\n");

            // Method Path QueryString Protocol
            split(dataList.at(0), goalList, " ");
            headerParse.method = goalList.at(0);
            split(goalList.at(1), pathAndQString, "?");
            split(pathAndQString.at(0), pathAndQString, "/");
            headerParse.path = pathAndQString.back();
            headerParse.qString = (pathAndQString.size() == 2) ? "" : pathAndQString.at(1);
            headerParse.protocol = goalList.at(2);

            headerParse.sAddr = _socket.local_endpoint().address().to_string();
            headerParse.rAddr = _socket.remote_endpoint().address().to_string();
            headerParse.rPort = std::to_string(_socket.remote_endpoint().port());

            // Hostname Port
            split(dataList.at(1), hostAndPort, ":");
            boost::algorithm::trim(hostAndPort.at(1));
            headerParse.host = hostAndPort.at(1);
            headerParse.sPort = hostAndPort.at(2);

            if(access (headerParse.path.c_str(), F_OK|X_OK) != -1){		/* executables */
                if((childpid = fork ()) < 0){
                    fputs ("error: fork failed\n", stderr);
                    exit(1);
                }
                else if(childpid == 0){
                    setenv ("REDIRECT_STATUS", "200", 1);
                    setenv ("DOCUMENT_ROOT", myDirectory, 1);
                    setenv ("REQUEST_METHOD", headerParse.method.c_str(), 1);
                    setenv ("REQUEST_URI", headerParse.path.c_str(), 1);
                    setenv ("QUERY_STRING", headerParse.qString.c_str(), 1);
                    setenv ("SERVER_PROTOCOL", headerParse.protocol.c_str(), 1);
                    setenv ("HTTP_HOST", headerParse.host.c_str(), 1);
                    setenv ("SERVER_ADDR", headerParse.sAddr.c_str(), 1);
                    setenv ("SERVER_PORT", headerParse.sPort.c_str(), 1);
                    setenv ("REMOTE_ADDR", headerParse.rAddr.c_str(), 1);
                    setenv ("REMOTE_PORT", headerParse.rPort.c_str(), 1);
                    dup2 (_socket.native_handle(), STDIN_FILENO);
			        dup2 (_socket.native_handle(), STDOUT_FILENO);
                    reply(headerParse.protocol + " 200 OK\n");
                    execl(headerParse.path.c_str(), headerParse.path.c_str(), NULL);
                    exit(0);
                }
            }
            else{
                string bad =  
                    "HTTP/1.0 404 Not Found\n"
                    "Server: GG\n"
                    "Content-Type: text/html\n\n"
                    "<html>\n"
                    "<head><title>404 Not Found</title></head>\n"
                    "<body><h1>404 Not Found</h1></body>\n"
                    "</html>\n";
	            reply(bad);
            }

        }
        void reply(string sendStr){
            auto self(shared_from_this());
            async_write(_socket, 
                buffer(sendStr),
                [this, self](boost::system::error_code ec, std::size_t){
                    if (!ec){
                        // Initiate graceful connection closure.
                        boost::system::error_code ignored_ec;
                        _socket.shutdown(ip::tcp::socket::shutdown_both, ignored_ec);
                    }
            });
        }
};

class HttpServer {
    private:
        ip::tcp::acceptor _acceptor;
        ip::tcp::socket _socket;

    public:
        HttpServer(unsigned short port)
          : _acceptor(global_io_service),
            _socket(global_io_service)
        {
            ip::tcp::endpoint endpoint(ip::tcp::v4(), port);
            _acceptor.open(endpoint.protocol());
            _acceptor.set_option(ip::tcp::acceptor::reuse_address(true));
            _acceptor.bind(endpoint);
            _acceptor.listen();

            do_accept();
        }

    private:
        void do_accept() {
            _acceptor.async_accept(_socket, 
                [this](boost::system::error_code ec) {
                    make_shared<HttpSession>(move(_socket))->start();
                do_accept();
            });
        }
};

void reaper (int sig){
	while (waitpid (-1, NULL, WNOHANG) > 0);
	signal (sig, reaper);
}

int main(int argc, char* argv[]){
    if (argc != 2) {
        std::cerr << "Usage:" << argv[0] << " [port]" << endl;
        return 1;
    }

    try{
        chdir(myDirectory);
        signal (SIGCHLD, reaper);

        unsigned short port = atoi(argv[1]);
        HttpServer server(port);
        global_io_service.run();
    } 
    catch (exception& e) {
        cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}