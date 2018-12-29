#include <unistd.h>
#include <iostream>
#include <fstream>
#include <boost/asio.hpp>
#include <vector>
#include <istream>
#include <ostream>
#include <boost/bind.hpp>
#include <array>
#include <string>
using namespace std;
using namespace boost::asio;
using namespace boost::asio::ip;

io_service global_io_service;

string html_escape(const string& str) {
    string escaped;
    for (auto&& ch : str) escaped += ("&#" + to_string(int(ch)) + ";");
    return escaped;
}

class ShellSession : public enable_shared_from_this<ShellSession> {
    private:
        tcp::resolver _resolver{global_io_service};
        tcp::socket _socket{global_io_service};
        boost::asio::streambuf _response;
        boost::asio::deadline_timer timer{global_io_service};
        enum { max_length = 1024 };
        array<char, max_length> _data;
        ifstream _in;
        string _id;

    public:
        void start(string id, string hostname, string port, string filename){   
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
                // std::cout.flush();
                memset(_data.data(),0,max_length);
                _socket.async_read_some(
                    buffer(_data, max_length),
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
            // auto self(shared_from_this());
            if (!err){
                string getStr(_data.data());
                // string inputStr;
                // std::istream is(&_response);
                // while(!is.eof()){
                    // std::cout.flush();
                    // getline(is, getStr);
                    if(getStr.find("%", 0) != std::string::npos){
                        
                        std::cout << "<script>document.getElementById(\"s"+_id+"\").innerHTML += \""+html_escape(getStr)+"\";</script>"<< endl;
                        std::cout.flush();

                        timer.expires_from_now(boost::posix_time::seconds(1));
                        timer.async_wait([this](boost::system::error_code ec){
                            if(!ec){
                                string inputStr;
                                getline(_in, inputStr);
                                inputStr += "\n";
                                std::cout << "<script>document.getElementById(\"s"+_id+"\").innerHTML += \""+html_escape(inputStr)+"</b>\";</script>"<< endl;
                                std::cout.flush();
                                async_write(
                                    _socket, 
                                    buffer(inputStr),
                                    boost::bind(
                                        &ShellSession::do_read,
                                        this,
                                        boost::asio::placeholders::error)
                            );
                            }
                        });
                        
                    }
                    else{
                        // getStr += "\n";
                        std::cout << "<script>document.getElementById(\"s"+_id+"\").innerHTML += \""+html_escape(getStr)+"\";</script>"<< endl;
                        std::cout.flush();
                        do_read(err);
                    }
                // }
                
                    
            }
            else{
                std::cout << "Error: " << err.message() << "\n";
            }   
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

void printLayout(){
    /* [Required] HTTP Header */
    cout << "Content-type: text/html" << endl << endl;
    cout << "<!DOCTYPE html>" << endl;
    cout << "<html lang=\"en\">" << endl;
    cout << "<head>" << endl;
    cout << "<meta charset=\"UTF-8\" />" << endl;
    cout << "<title>NP Project 3 Console</title>" << endl;
    cout << "<link" << endl;
    cout << "rel=\"stylesheet\"" << endl;
    cout << "href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.1.3/css/bootstrap.min.css\"" << endl;
    cout << "integrity=\"sha384-MCw98/SFnGE8fJT3GXwEOngsV7Zt27NXFoaoApmYm81iuXoPkFOJwJ8ERdknLPMO\"" << endl;
    cout << "crossorigin=\"anonymous\"" << endl;
    cout << "/>" << endl;
    cout << "<link" << endl;
    cout << "href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"" << endl;
    cout << "rel=\"stylesheet\"" << endl;
    cout << "/>" << endl;
    cout << "<link" << endl;
    cout << "rel=\"icon\"" << endl;
    cout << "type=\"image/png\"" << endl;
    cout << "href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\"" << endl;
    cout << "/>" << endl;
    cout << "<style>" << endl;
    cout << "* {" << endl;
    cout << "font-family: 'Source Code Pro', monospace;" << endl;
    cout << "font-size: 1rem !important;" << endl;
    cout << "}" << endl;
    cout << "body {" << endl;
    cout << "background-color: #212529;" << endl;
    cout << "}" << endl;
    cout << "pre {" << endl;
    cout << "color: #cccccc;" << endl;
    cout << "}" << endl;
    cout << "b {" << endl;
    cout << "color: #ffffff;" << endl;
    cout << "}" << endl;
    cout << "</style>" << endl;
    cout << "</head>" << endl;
    cout << "<body>" << endl;
    cout << "<table class=\"table table-dark table-bordered\">" << endl;
    cout << "<thead>" << endl;
    cout << "<tr>" << endl;
    cout << "<th scope=\"col\">shell one</th>" << endl;
    cout << "<th scope=\"col\">shell two</th>" << endl;
    cout << "<th scope=\"col\">shell three</th>" << endl;
    cout << "<th scope=\"col\">shell four</th>" << endl;
    cout << "<th scope=\"col\">shell five</th>" << endl;
    cout << "</tr>" << endl;
    cout << "</thead>" << endl;
    cout << "<tbody>" << endl;
    cout << "<tr>" << endl;
    cout << "<td><pre id=\"s0\" class=\"mb-0\"></pre></td>" << endl;
    cout << "<td><pre id=\"s1\" class=\"mb-0\"></pre></td>" << endl;
    cout << "<td><pre id=\"s2\" class=\"mb-0\"></pre></td>" << endl;
    cout << "<td><pre id=\"s3\" class=\"mb-0\"></pre></td>" << endl;
    cout << "<td><pre id=\"s4\" class=\"mb-0\"></pre></td>" << endl;
    cout << "</tr>" << endl;
    cout << "</tbody>" << endl;
    cout << "</table>" << endl;
    cout << "</body>" << endl;
    cout << "</html>" << endl;
}
int main(int, char* const[], char* const envp[]) {
    
    string method, qString;
    vector<string> qPhase;
    ShellSession shellSession[5];

    printLayout();
    method = getenv("REQUEST_METHOD");
    qString = getenv("QUERY_STRING");
    split(qString, qPhase, "&");

    cout << method << endl;

    for(int i = 0; i < qPhase.size(); i++){
        if(i % 3 == 2){
            vector<string> sTemp;
            for(int j = 2; j >= 0; j--)
                split(qPhase.at(i-j), sTemp, "=");

            // there is no connection  
            if(sTemp.size() != 6)
                continue;

            shellSession[i/3].start(std::to_string(i/3), sTemp.at(1), sTemp.at(3), "test_case/" + sTemp.at(5));
        }
    }
    global_io_service.run();

    return 0;
}
