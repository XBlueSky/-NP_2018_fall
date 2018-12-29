#include <iostream>
#include <string>
#include <vector>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <signal.h>
#include <istream>
#include <ostream>
#include <fstream>
#include <boost/bind.hpp>
#include<stdio.h>
#include <algorithm>

using namespace std;
using namespace boost::asio;
using namespace boost::asio::ip;

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

string html_escape(const string& str) {
    string escaped;
    for (auto&& ch : str) escaped += ("&#" + to_string(int(ch)) + ";");
    return escaped;
}

class ShellSession : public enable_shared_from_this<ShellSession> {
    private:
        tcp::resolver _resolver{global_io_service};
        tcp::socket _socket{global_io_service};
        // std::shared_ptr<boost::asio::ip::tcp::socket> s_socket;
        ip::tcp::socket s_socket{global_io_service};
        boost::asio::streambuf _response;
        ifstream _in;
        string _id;

    public:
        void start(ip::tcp::socket *socket, string id, string hostname, string port, string filename){   
            // s_socket = socket;
            s_socket.assign(tcp::v4(), socket->native_handle());
            _id = id;
            _in.open(filename, ifstream::in);
            tcp::resolver::query query{hostname, port};
            _resolver.async_resolve(
                query,
                boost::bind(
                    &ShellSession::do_connect,
                    this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::iterator)
                );
        }

    private:
        void do_connect(
            const boost::system::error_code& err,
            tcp::resolver::iterator endpoint_iterator)
        {
            if (!err){
                tcp::endpoint endpoint = *endpoint_iterator;
                _socket.async_connect(
                    endpoint,
                    boost::bind(
                        &ShellSession::do_read, 
                        this,
                        boost::asio::placeholders::error)
                    );
            }
            else{
                std::cout << "Error: " << err.message() << "\n";
            }       
        }
        void do_read(const boost::system::error_code& err){
            if (!err){
                async_read_until(
                    _socket,
                    _response, 
                    "%",
                    boost::bind(
                        &ShellSession::do_send_cmd, 
                        this,
                        boost::asio::placeholders::error)
                    );
            }
            else{
                std::cout << "Error: " << err.message() << "\n";
            }   
        }
        void do_send_cmd(const boost::system::error_code& err){
            if (!err){
                string inputStr, getStr, replyStr;
                getline(_in, inputStr);
                std::istream is(&_response);
                while(!is.eof()){
                    getline(is, getStr);
                    if(getStr.find("%", 0) != std::string::npos){
                        replyStr = "<script>document.getElementById(\"s"+_id+"\").innerHTML += \""+html_escape(getStr)+"\";</script>\n";
                        reply(replyStr);
                        // std::cout << "<script>document.getElementById(\"s"+_id+"\").innerHTML += \""+html_escape(getStr)+"\";</script>"<< endl;
                    }
                    else{
                        getStr += "\n";
                        replyStr = "<script>document.getElementById(\"s"+_id+"\").innerHTML += \""+html_escape(getStr)+"\";</script>\n";
                        reply(replyStr);
                        // std::cout << "<script>document.getElementById(\"s"+_id+"\").innerHTML += \""+html_escape(getStr)+"\";</script>"<< endl;
                    }
                    std::cout.flush();
                }
                inputStr += "\n";
                replyStr = "<script>document.getElementById(\"s"+_id+"\").innerHTML += \""+html_escape(inputStr)+"</b>\";</script>\n";
                reply(replyStr);
                // std::cout << "<script>document.getElementById(\"s"+_id+"\").innerHTML += \""+html_escape(inputStr)+"</b>\";</script>"<< endl;
                
                async_write(
                    _socket, 
                    buffer(inputStr),
                    boost::bind(
                        &ShellSession::do_read,
                        this,
                        boost::asio::placeholders::error)
                    );
            }
            else{
                std::cout << "Error: " << err.message() << "\n";
            }   
        }
        void reply(string sendStr){
            auto self(shared_from_this());
            char replyStr[10000];
            sendStr.copy(replyStr, sendStr.size());
            std::array<char,10000> data;
            std::copy(std::begin(replyStr), std::end(replyStr), std::begin(data));
            // replStr[sendStr.size()] = '\0';
            async_write(s_socket, 
                buffer(data, sendStr.size()),
                [this, self](boost::system::error_code ec, std::size_t){
                    if (!ec){
                        // Initiate graceful connection closure.
                        // boost::system::error_code ignored_ec;
                        // _socket.shutdown(ip::tcp::socket::shutdown_both, ignored_ec);
                    }
            });
        }
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

string panel(string protocol){
    string panel = protocol + " 200 OK\n";
    panel +=  "Content-type: text/html\r\n\r\n";
    panel +=    "<!DOCTYPE html>\n";
    panel +=    "<html lang=\"en\">\n";
    panel +=    "<head>\n";
    panel +=        "<title>NP Project 3 Panel</title>\n";
    panel +=       "<link\n";
    panel +=        "rel=\"stylesheet\"\n";
    panel +=        "href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.1.3/css/bootstrap.min.css\"\n";
    panel +=       "integrity=\"sha384-MCw98/SFnGE8fJT3GXwEOngsV7Zt27NXFoaoApmYm81iuXoPkFOJwJ8ERdknLPMO\"\n";
    panel +=       "crossorigin=\"anonymous\"\n";
    panel +=      "/>\n";
    panel +=      "<link\n";
    panel +=      "href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\n";
    panel +=     "rel=\"stylesheet\"\n";
    panel +=     "/>\n";
    panel +=     "<link\n";
    panel +=     "rel=\"icon\"\n";
    panel += "type=\"image/png\"\n";
    panel +=     "href=\"https://cdn4.iconfinder.com/data/icons/iconsimple-setting-time/512/dashboard-512.png\"\n";
    panel +=      "/>\n";
    panel +=      "<style>\n";
    panel +=      "* {\n";
    panel +=          "font-family: \'Source Code Pro\', monospace;\n";
    panel +=      "}\n";
    panel +=      "</style>\n";
    panel +=    "</head>\n";
    panel +=    "<body class=\"bg-secondary pt-5\">\n";
    panel +=    "<form action=\"console.cgi\" method=\"GET\">\n";
    panel +=    "<table class=\"table mx-auto bg-light\" style=\"width: inherit\">\n";
    panel +=     "<thead class=\"thead-dark\">\n";
    panel +=      "<tr>\n";
    panel +=          "<th scope=\"col\">#</th>\n";
    panel +=          "<th scope=\"col\">Host</th>\n";
    panel +=         "<th scope=\"col\">Port</th>\n";
    panel +=         "<th scope=\"col\">Input File</th>\n";
    panel +=     "</tr>\n";
    panel +=     "</thead>\n";
    panel +=      "<tbody>\n";

for(int i=0;i<5;i++){
    panel +=   "<tr>\n";
    panel =  panel + "<th scope=\"row\" class=\"align-middle\">Session " + std::to_string(i+1) + "</th>\n";
    panel +=         "<td>\n";
    panel +=       "<div class=\"input-group\">\n";
    panel = panel + "<select name=\"h"+ std::to_string(i) + "\" class=\"custom-select\">\n";
    panel +=        "<option></option><option value=\"nplinux1.cs.nctu.edu.tw\">nplinux1</option><option value=\"nplinux2.cs.nctu.edu.tw\">nplinux2</option><option value=\"nplinux3.cs.nctu.edu.tw\">nplinux3</option><option value=\"nplinux4.cs.nctu.edu.tw\">nplinux4</option><option value=\"nplinux5.cs.nctu.edu.tw\">nplinux5</option>\n";
    panel +=      "</select>\n";
    panel +=        "<div class=\"input-group-append\">\n";
    panel +=          "<span class=\"input-group-text\">.cs.nctu.edu.tw</span>\n";
    panel +=        "</div>\n";
    panel +=      "</div>\n";
    panel +=     "</td>\n";
    panel +=    "<td>\n";
    panel = panel + "<input name=\"p"+ std::to_string(i) + "\" type=\"text\" class=\"form-control\" size=\"5\" />\n";
    panel +=    "</td>\n";
    panel +=    "<td>\n";
    panel = panel  + "<select name=\"f"+ std::to_string(i) + "\" class=\"custom-select\">\n";
    panel +=        "<option></option>\n";
    panel +=        "<option value=\"t1.txt\">t1.txt</option><option value=\"t2.txt\">t2.txt</option><option value=\"t3.txt\">t3.txt</option><option value=\"t4.txt\">t4.txt</option><option value=\"t5.txt\">t5.txt</option><option value=\"t6.txt\">t6.txt</option><option value=\"t7.txt\">t7.txt</option><option value=\"t8.txt\">t8.txt</option><option value=\"t9.txt\">t9.txt</option><option value=\"t10.txt\">t10.txt</option>\n";
    panel +=      "</select>\n";
    panel +=      "</td>\n";
    panel +=     "</tr>\n";
}
    
    panel +=      "<tr>\n";
    panel +=         "<td colspan=\"3\"></td>\n";
    panel +=         "<td>\n";
    panel +=          "<button type=\"submit\" class=\"btn btn-info btn-block\">Run</button>\n";
    panel +=          "</td>\n";
    panel +=       "</tr>\n";
    panel +=        "</tbody>\n";
    panel +=    "</table>\n";
    panel +=     "</form>\n";
    panel +=   "</body>\n";
    panel +=   "</html>\n";
    return panel;
}

string printLayout(string protocol){
    string layout = protocol + " 200 OK\n";
    layout +=  "Content-type: text/html\r\n\r\n";
    layout +=      "<!DOCTYPE html>\n";
    layout +=       "<html lang=\"en\">\n";
    layout +=       "<head>\n";
    layout +=      "<meta charset=\"UTF-8\" />\n";
    layout +=      "<title>NP Project 3 Console</title>\n";
    layout +=      "<link\n";
    layout +=      "rel=\"stylesheet\"\n";
    layout +=     "href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.1.3/css/bootstrap.min.css\"\n";
    layout +=     "integrity=\"sha384-MCw98/SFnGE8fJT3GXwEOngsV7Zt27NXFoaoApmYm81iuXoPkFOJwJ8ERdknLPMO\"\n";
    layout +=     "crossorigin=\"anonymous\"\n";
    layout +=     "/>\n";
    layout +=    "<link\n";
    layout +=     "href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\n";
    layout +=    "rel=\"stylesheet\"\n";
    layout +=    "/>\n";
    layout +=    "<link\n";
    layout +=    "rel=\"icon\"\n";
    layout +=    "type=\"image/png\"\n";
    layout +=    "href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\"\n";
    layout +=    "/>\n";
    layout +=    "<style>\n";
    layout +=    "* {\n";
    layout +=    "font-family: 'Source Code Pro', monospace;\n";
    layout +=    "font-size: 1rem !important;\n";
    layout +=    "}\n";
    layout +=    "body {\n";
    layout +=    "background-color: #212529;\n";
    layout +=    "}\n";
    layout +=    "pre {\n";
    layout +=    "color: #cccccc;\n";
    layout +=    "}\n";
    layout +=    "b {\n";
    layout +=    "color: #ffffff;\n";
    layout +=    "}\n";
    layout +=    "</style>\n";
    layout +=    "</head>\n";
    layout +=    "<body>\n";
    layout +=   "<table class=\"table table-dark table-bordered\">\n";
    layout +=   "<thead>\n";
    layout +=    "<tr>\n";
    layout +=    "<th scope=\"col\">shell one</th>\n";
    layout +=   "<th scope=\"col\">shell two</th>\n";
    layout +=    "<th scope=\"col\">shell three</th>\n";
    layout +=    "<th scope=\"col\">shell four</th>\n";
    layout +=    "<th scope=\"col\">shell five</th>\n";
    layout +=   "</tr>\n";
    layout +=    "</thead>\n";
    layout +=    "<tbody>\n";
    layout +=    "<tr>\n";
    layout +=    "<td><pre id=\"s0\" class=\"mb-0\"></pre></td>\n";
    layout +=    "<td><pre id=\"s1\" class=\"mb-0\"></pre></td>\n";
    layout +=    "<td><pre id=\"s2\" class=\"mb-0\"></pre></td>\n";
    layout +=    "<td><pre id=\"s3\" class=\"mb-0\"></pre></td>\n";
    layout +=    "<td><pre id=\"s4\" class=\"mb-0\"></pre></td>\n";
    layout +=    "</tr>\n";
    layout +=    "</tbody>\n";
    layout +=   "</table>\n";
    layout +=   "</body>\n";
    layout +=   "</html>\n";
    return layout;
}


class HttpSession : public enable_shared_from_this<HttpSession> {
    private:
        ip::tcp::socket _socket;
        boost::asio::streambuf _response;
        enum { max_length = 1024 };
        array<char, max_length> _data;
        Header headerParse;
        // std::shared_ptr<boost::asio::ip::tcp::socket> _socket;

    public:
        HttpSession(ip::tcp::socket socket) : _socket(move(socket)) {}
        // HttpSession(std::shared_ptr<boost::asio::ip::tcp::socket> socket) : _socket(socket) {}

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
            // // auto self(shared_from_this());
            string getStr(_data.data());
            vector<string> dataList, goalList, pathAndQString, hostAndPort;
            split(getStr, dataList, "\r\n");

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
            
            if(headerParse.path == "panel.cgi"){
                string panelStr = panel(headerParse.protocol);
                reply(panelStr);
             }
            else if(headerParse.path == "console.cgi"){
                string layoutStr = printLayout(headerParse.protocol);
                replyCGI(layoutStr);
                ShellSession shellSession[5];
                vector<string> qPhase;
                split(headerParse.qString, qPhase, "&");

                // boost::shared_ptr<tcp::socket> s_socket = &_socket;
                for(int i = 0; i < qPhase.size(); i++){
                    if(i % 3 == 2){
                        vector<string> sTemp;
                        for(int j = 2; j >= 0; j--)
                            split(qPhase.at(i-j), sTemp, "=");

                        if(sTemp.size() != 6)
                            continue;
                        // make_shared<ShellSession>(_socket)->start(std::to_string(i/3), sTemp.at(1), sTemp.at(3), "test_case/" + sTemp.at(5));
                        // make_shared<ShellSession>(move(_socket))->start(std::to_string(i/3), sTemp.at(1), sTemp.at(3), "test_case/" + sTemp.at(5));
                        shellSession[i/3].start(&_socket, std::to_string(i/3), sTemp.at(1), sTemp.at(3), "test_case/" + sTemp.at(5));
                    }
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
        void replyCGI(string sendStr){
            auto self(shared_from_this());
            char replyStr[10000];
            sendStr.copy(replyStr, sendStr.size());
            std::array<char,10000> data;
            std::copy(std::begin(replyStr), std::end(replyStr), std::begin(data));
            // replStr[sendStr.size()] = '\0';
            async_write(_socket, 
                buffer(data, sendStr.size()),
                [this, self](boost::system::error_code ec, std::size_t){
                    if (!ec){
                        // Initiate graceful connection closure.
                        // boost::system::error_code ignored_ec;
                        // _socket.shutdown(ip::tcp::socket::shutdown_both, ignored_ec);
                    }
            });
        }
        void reply(string sendStr){
            auto self(shared_from_this());
            char replyStr[10000];
            sendStr.copy(replyStr, sendStr.size());
            std::array<char,10000> data;
            std::copy(std::begin(replyStr), std::end(replyStr), std::begin(data));
            // replStr[sendStr.size()] = '\0';
            async_write(_socket, 
                buffer(data, sendStr.size()),
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
        // std::shared_ptr<boost::asio::ip::tcp::socket> _socket;
    public:
        HttpServer(unsigned short port)
          : _acceptor(global_io_service),
           _socket(global_io_service)
        {
            // _socket = std::make_shared<boost::asio::ip::tcp::socket>(global_io_service);
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
                    // make_shared<HttpSession>(move(_socket))->start();
                    make_shared<HttpSession>(move(_socket))->start();
                do_accept();
            });
        }
};

// void reaper (int sig){
// 	while (waitpid (-1, NULL, WNOHANG) > 0);
// 	signal (sig, reaper);
// }

int main(int argc, char* argv[]){
    if (argc != 2) {
        std::cerr << "Usage:" << argv[0] << " [port]" << endl;
        return 1;
    }

    try{
        // chdir(myDirectory);
        // signal (SIGCHLD, reaper);

        unsigned short port = atoi(argv[1]);
        HttpServer server(port);
        global_io_service.run();
    } 
    catch (exception& e) {
        cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}