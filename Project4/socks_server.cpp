#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <fstream>
#include <stdio.h>

using namespace std;
#define MAX_CLIENTS     5
#define TRANS_SIZE	    10000000
#define MAX_BUF_SIZE    512

class Request{
    public:
        unsigned char	vn;
        unsigned char	cd;
        unsigned short	dest_port;
        struct in_addr	dest_ip;

        void showMsg(struct sockaddr_in *src, unsigned char replyCD){
            string msg;
            msg = "<S_IP>: " + string(inet_ntoa(src->sin_addr)) + "\n" +
                  "<S_PORT>: " + to_string(src->sin_port) + "\n" +
                  "<D_IP>: " + string(inet_ntoa (dest_ip)) + "\n" +
                  "<D_PORT>: " + to_string(dest_port) + "\n";
            msg += (cd == 1)? "<Command>: CONNECT\n" : "<Command>: BIND\n";
            msg += (replyCD == 90)? "<Reply>: Accept\n\n" : "<Reply>: Reject\n\n";
            write(STDERR_FILENO, msg.c_str(), msg.length());
        }
};

class Reply{
    public:
        unsigned char	vn;
        unsigned char	cd;
        unsigned short	dest_port;
        struct in_addr	dest_ip;

        void replyBack(){
            dest_port = htons(dest_port);
            write(STDOUT_FILENO, this, sizeof(Reply));
	        dest_port = ntohs(dest_port);
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

Request		req = {0};
Reply		rep = {0};

int passiveTCP(int port){
	int			sockFd;
	struct sockaddr_in	serverAddress;

	// open TCP socket 
	if ((sockFd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        cout << "[Server] Cannot open socket\n";
        return -1;
	}

    // Reuse address
    int ture = 1;
    if(setsockopt(sockFd, SOL_SOCKET, SO_REUSEADDR, &ture, sizeof(ture)) < 0) {
        cout << "[Server] Setsockopt failed\n";
        return -1;
    }

    // Set up server socket address
    bzero((char *) &serverAddress, sizeof(serverAddress));
    serverAddress.sin_family            = AF_INET;
    serverAddress.sin_addr.s_addr       = htonl(INADDR_ANY);
    serverAddress.sin_port              = htons(port);

    if(bind(sockFd, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0) {
        cout << "[Server] Cannot bind address\n";
        return -1;
    }

    // Listen
    if(listen(sockFd, MAX_CLIENTS) < 0) {
        cout << "[Server] Failed to listen\n";
        return -1;
    }

	return sockFd;
}

int checkIP(string firewallIP, string destIP){
    vector<string> fwIPList, destIPList;
    
    // split [140] [113] [*] [*]
    split(firewallIP, fwIPList, ".");
    split(destIP, destIPList, ".");
    
    for(int i=0; i<4; i++){
        if(fwIPList.at(i) == destIPList.at(i) || fwIPList.at(i) == "*")
            continue;
        else    
            return 0; //reject
    }
    return 1; //pass
}

int firewall(){
    string inputStr;
    ifstream in("socks.conf");

    while(getline(in, inputStr)){
        vector<string> content;
        split(inputStr, content, " ");
        // split [permit] [c] [140.113.*.*]
        if((content.at(1) == "c" && req.cd == 1) || (content.at(1) == "b" && req.cd == 2)){
            if(checkIP(content.at(2), string(inet_ntoa(req.dest_ip)))){
                in.close();
                return 1;
            }      
        }
    }
    in.close();
    return 0;
}

int socks(struct sockaddr_in src){
	// Get SOCKS4 request 
	unsigned char buffer[MAX_BUF_SIZE];
	read(STDIN_FILENO, buffer, MAX_BUF_SIZE);
    
    req.vn = buffer[0];
    req.cd = buffer[1];
    req.dest_port = buffer[2] << 8 | buffer[3];
    // Meet Big-Endian network order
    req.dest_ip.s_addr = buffer[7] << 24 | buffer[6] << 16 | buffer[5] << 8 | buffer[4];

	if(req.vn != 4 || (req.cd != 1 && req.cd != 2)){
		return -1;
	}

	// Check firewall rules 
	if (!firewall()) {
        // Request rejected or failed
		rep.cd = 91;	
		rep.dest_port = 0;
		rep.dest_ip.s_addr = 0;
		rep.replyBack();
		req.showMsg(&src, req.cd);
		return 0;
	}
	rep.cd = 90;	// Request granted

    int	dest;	
	switch (req.cd) {
        case 1:	
            // CONNECT mode 
            rep.dest_port = req.dest_port;
            rep.dest_ip = req.dest_ip;
            struct sockaddr_in	des;
            
            // open TCP socket 
            if ((dest = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
                cout << "[Server] Cannot open socket\n";
                rep.cd = 91;
            }

            // Set up client socket address
            bzero((char *) &des, sizeof(des));
            des.sin_family =    AF_INET;
            des.sin_port =      htons (req.dest_port);
            des.sin_addr =      req.dest_ip;

            // Start to connect
            if (connect(dest,(const struct sockaddr *) &des, sizeof (des)) < 0) {
                fputs ("error: failed to connect\n", stderr);
                rep.cd = 91;
            }
            break;

        case 2:		
            // BIND mode
            int	sockFd;
            struct sockaddr_in clientAddress;
            unsigned int clientLen = sizeof(clientAddress);

            // dynamically bind to an unused port
            if ((sockFd = passiveTCP (0)) < 0) {
                fputs ("error: passiveTCP failed\n", stderr);
                rep.cd = 91;
            }

            // getting the port 
            getsockname(sockFd, (struct sockaddr *) &clientAddress, &clientLen);
            rep.dest_port = ntohs(clientAddress.sin_port);
            rep.replyBack();

            if((dest = accept (sockFd, (struct sockaddr *) &clientAddress, &clientLen)) < 0) {
                rep.cd = 91;
            }
            rep.dest_ip.s_addr = 0;
            break;
	}

    // Send SOCK4 reply and show messages on console
	rep.replyBack();
	req.showMsg(&src, rep.cd);
    
    // Start to trasfer
	char	buf[TRANS_SIZE];
    fd_set readFds,activeFds;
    int maxFd, end;

    FD_ZERO(&activeFds);
    FD_SET(dest, &activeFds);
    FD_SET(STDIN_FILENO, &activeFds);
    maxFd = getdtablesize();

	while (__FDS_BITS(&activeFds)) {
        memcpy (&readFds, &activeFds, sizeof(readFds));

        if(select(maxFd+1, &readFds, NULL, NULL, NULL) < 0){
            if(errno == 4)
                continue;
        }
		// Get from SRC and forward to DEST
		if(FD_ISSET(STDIN_FILENO, &readFds)) {
			end = read(STDIN_FILENO, buf, TRANS_SIZE);
			if (end == 0) {
				FD_CLR(STDIN_FILENO, &activeFds);
				close(STDIN_FILENO);
				close(STDOUT_FILENO);
                close(dest);
                FD_CLR(dest, &activeFds);
			} else {
				write(dest, buf, end);
			}
		}
		// Get from DEST and forward to SRC
		if(FD_ISSET (dest, &readFds)) {
			end = read (dest, buf, TRANS_SIZE);
			if (end == 0) {
				FD_CLR(dest, &activeFds);
				close(dest);
                close(STDIN_FILENO);
				close(STDOUT_FILENO);
                FD_CLR(STDIN_FILENO, &activeFds);
			} else {
				write(STDOUT_FILENO, buf, end);
			}
		}
	}

	return 0;
}

void sig_handler (int sig)
{
	if (sig == SIGCHLD)
		while (waitpid (-1, NULL, WNOHANG) > 0);
	signal (sig, sig_handler);
}

int main(int argc, char *argv[]) {
    int sockFd, childFd;
    int serverPort = atoi(argv[1]);
    unsigned int clientLen;
    struct sockaddr_in clientAddress;
    pid_t childPid;

    signal (SIGCHLD, sig_handler);

    // build TCP connection
	if ((sockFd = passiveTCP(serverPort)) < 0) {
		fputs ("error: passiveTCP failed\n", stderr);
		return -1;
	}

    // Accept
    while(1) {
        // cout << "[Server] Waiting for connections on " << serverPort << "...\n";
        clientLen = sizeof(clientAddress);
        if((childFd = accept(sockFd, (struct sockaddr *) &clientAddress, &clientLen)) < 0) {
            cout << errno<<"\n";
            cout << "[Server] Failed to accept\n";
            continue;
        }
        if((childPid=fork())==0) {
            // children process
            dup2(childFd,STDOUT_FILENO);
            dup2(childFd,STDIN_FILENO);
            close(sockFd);
            close(childFd);
            socks(clientAddress);
            _exit(0);
        }else{
            //parent process
            close(childFd);
        }
    }
    return 0;
}          