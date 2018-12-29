#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <errno.h>
#include <sstream>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <csignal>
#include <fcntl.h>
#include <arpa/inet.h> // for network address
using namespace std;

#define MAX_CLIENTS    5
#define MAX_USERS	30
#define MAX_MSG_SIZE	1024
#define MAX_NAME_SIZE	20

const char welcome[] =	"****************************************\n"
			        "** Welcome to the information server. **\n"
			        "****************************************\n\n";
const char prompt[] = "% ";

class GlobalPipe{
    public:
        GlobalPipe(int countDown){
            _countDown = countDown;
        }
        int countDown(){return _countDown;}
        int* pipeFd(){return _pipeFd;}
        void countDownMinusOne(){
            _countDown--;
        }
        bool countDownEqual(int count){
            return (_countDown == count);
        }
        void pipeFdClose(){
            close(_pipeFd[0]);
            close(_pipeFd[1]);
        }
    private:
        int _countDown;
        int _pipeFd[2];
};

class LocalPipe{
    public:
        int* pipeFd(){return _pipeFd;}
        void pipeFdClose(){
            close(_pipeFd[0]);
            close(_pipeFd[1]);
        }
    private:
        int _pipeFd[2];
};

class UserPipe{
    public:
        UserPipe(int pipeFd, int id){
            _pipeFd = pipeFd;
            _id = id;
        }
        int _pipeFd;
        int _id;
        void pipeFdClose(){
            close(_pipeFd);
        }
};

class User{
    public:
        User(){
            _connection = false;
        }
        void initSet(char *ip, int port, int fd){
            strcpy(_name, "(no name)");
            strcpy(_ip, ip);
            _port = port;
            _fd = fd;
            _connection = true;
            up = (int *)calloc(MAX_USERS, sizeof (int));
            //userPipe = (int*)calloc(MAX_USERS, sizeof (int));
        }
        char _name[MAX_NAME_SIZE+1];
        char _ip[16];
        int _port;
        int _fd;
        int _id;
        bool _connection;
        vector<GlobalPipe> globalPipe;
        vector<string> env;
        int *up;
        void upClear(){
            int	i;
            if (up) {
                for (i = 0; i < MAX_USERS; ++i) {
                    if (up[i])
                        close (up[i]);
                }
                free (up);
                up = NULL;
            }
        }
        // vector<UserPipe> userPipe;
        // int findFdUserPipe(int id){
        //     for(int i=0; i<userPipe.size(); i++){
        //         if(id == userPipe.at(i)._id){
        //             return userPipe.at(i)._pipeFd;
        //         }
        //     }
        //     return -1;
        // }
        // int removeFdUserPipe(int id){
        //     for(int i=0; i<userPipe.size(); i++){
        //         if(id == userPipe.at(i)._id){
        //             userPipe.erase(userPipe.begin()+ i);
        //             return 1;
        //         }
        //     }
        //     return -1;
        // }
        void globalPipeClose(){
            for(int i = 0; i < globalPipe.size(); i++){
                globalPipe.at(i).pipeFdClose();
            }
            globalPipe.clear();
        }
        // void userPipeClose(){
        //     for(int i = 0; i < userPipe.size(); i++){
        //         userPipe.at(i).pipeFdClose();
        //     }
        //     userPipe.clear();
        // }
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

void broadcast (string msg, User *users)
{
	for (int i = 0; i < MAX_USERS; i++) {
		/* broadcast to on-line clients */
		if (users[i]._connection) {
			write (users[i]._fd, msg.c_str(), msg.length());	/* print the message out */
		}
	}
}

void who(int sockFd, User *users){
	char msg[MAX_MSG_SIZE + 1];
	snprintf (msg, MAX_MSG_SIZE + 1, "<ID>\t<nickname>\t<IP/port>\t<indicate me>\n");
    write(STDOUT_FILENO, msg, strlen (msg));
    for(int i = 0; i < MAX_USERS; i++){
        if(users[i]._connection){
            snprintf (msg, MAX_MSG_SIZE + 1, "%d\t%s\t%s/%d", i+1, users[i]._name, users[i]._ip, users[i]._port);
            if(users[i]._fd == sockFd)
                strcat (msg, "\t<- me\n");
            else
                strcat (msg, "\n");
            write (STDOUT_FILENO, msg, strlen(msg));
        }
    }
}

void name(int sockFd, User *users, string newName){
    char msg[MAX_MSG_SIZE + 1];
    // check the same name
    for(int i = 0; i < MAX_USERS; i++){
        if(users[i]._connection && i != sockFd-4 && strcmp(users[i]._name, newName.c_str()) == 0){
            snprintf (msg, MAX_MSG_SIZE + 1, "*** User '%s' already exists. ***\n", newName.c_str());
			write (STDOUT_FILENO, msg, strlen(msg));
			return;
        }
    }
    strncpy (users[sockFd - 4]._name, newName.c_str(), MAX_NAME_SIZE+1);
	snprintf (msg, MAX_MSG_SIZE + 1, "*** User from %s/%d is named '%s'. ***\n", users[sockFd - 4]._ip, users[sockFd - 4]._port, users[sockFd - 4]._name);
    broadcast(std::string(msg), users);
}

void tell(int sockFd, User *users, vector<string>& programName){
    int recvId = atoi(programName.at(1).c_str());
    string msg;
    if(users[recvId - 1]._connection){
        msg = "*** " + std::string(users[sockFd - 4]._name) + " told you ***: ";
        for(int i = 2; i < programName.size(); i++){
            msg = msg + " " + programName.at(i);
        }
        msg += "\n";
        write (users[recvId - 1]._fd, msg.c_str(), msg.length());
    }
    else {
        msg = "*** Error: user #" +  std::to_string(recvId) + " does not exist yet. ***\n";
		write (STDERR_FILENO, msg.c_str(), msg.length());
	}
}

void yell(int sockFd, User *users, vector<string>& programName){
    string msg;
    msg = "*** " + std::string(users[sockFd - 4]._name) + " yelled ***: ";
    for(int i = 1; i < programName.size(); i++){
        msg = msg + " " + programName.at(i);
    }
    msg += "\n";
    broadcast(msg, users);
}

bool builtInFunction(string line, int sockFd, User *users){
    vector<string> programName;
    split(line, programName, " ");

    if("setenv" == programName.at(0)){
        setenv(programName.at(1).c_str(), programName.at(2).c_str(), 1);
        users[sockFd - 4].env.push_back(line);
        return true;
    } 
    else if("printenv" == programName.at(0)){
        cout << getenv(programName.at(1).c_str()) << endl;
        return true;
    }
    else if("exit" == programName.at(0)){
        users[sockFd - 4]._connection = false;
        return true;
    }
    else if("who" == programName.at(0)){
        who(sockFd, users);
        return true;
    }
    else if("name" == programName.at(0)){
        name(sockFd, users, programName.at(1));
        return true;
    }
    else if("tell" == programName.at(0)){
        tell(sockFd, users, programName);
        return true;
    }
    else if("yell" == programName.at(0)){
        yell(sockFd, users, programName);
        return true;
    }
    else if("kick" == programName.at(0)){
        int id = std::stoi(programName.at(1));
        users[id - 1]._connection = false;
        string msg = std::to_string(sockFd - 3) + " kick " + programName.at(1);
        broadcast(msg, users);
        return true;
    }
    else{
        return false;
    }
}

bool isNumber(const std::string& s){
    vector <string> number;
    split(s, number, " ");
    std::string::const_iterator it = number.front().begin();
    while (it != number.front().end() && std::isdigit(*it)) ++it;
    return !number.front().empty() && it == number.front().end();
}

int commandParse(string line, vector<string>& programList, int sockFd, User *users){
    int count; //count the number of delimiters "|"
    count = split(line, programList, "|");
    if(builtInFunction(line, sockFd, users)){ 
        // built-in function call
        programList.pop_back();
        return -1;
    } 
    return count;
}

void signalHandler(int signum){
    int status;
    while(waitpid(-1, &status, WNOHANG) > 0){
        //do nothing
    }
}

int initInUserPipe (int sockFd, User *users, int userPipe[2], int *sendId){
	string msg;
	if (!users[*sendId - 1]._connection) {
        msg = "*** Error: user #"+ std::to_string(*sendId) +" does not exist yet. ***\n";
		write (STDERR_FILENO, msg.c_str(), msg.length());
		return -1;
	} 
    // else if (users[sockFd - 4].findFdUserPipe(*sendId - 1) == -1) {
    else if (users[sockFd - 4].up[*sendId - 1] == 0) {
        msg = "*** Error: the pipe #"+ std::to_string(*sendId) +"->#"+ std::to_string(sockFd - 3) +" does not exist yet. ***\n";
		write (STDERR_FILENO, msg.c_str(), msg.length());
		return -1;
	} 
    else {
        // userPipe[0] = users[sockFd - 4].findFdUserPipe(*sendId - 1);
        // users[sockFd - 4].removeFdUserPipe(*sendId - 1);
        userPipe[0] = users[sockFd - 4].up[*sendId - 1];
		users[sockFd - 4].up[*sendId - 1] = 0;
	}
	return 0;
}

int initOutUserPipe(int sockFd, User *users, int userPipe[2], int *recvId){
    int pipeFd[2];
    string msg;
    if(!users[*recvId -1]._connection){
        msg = "*** Error: user #"+ std::to_string(*recvId) +" does not exist yet. ***\n";
        write (STDERR_FILENO, msg.c_str(), msg.length());
        return -1;
    }
    // else if(users[*recvId - 1].findFdUserPipe(sockFd - 4) != -1){
    else if(users[*recvId - 1].up[sockFd - 4] != 0){
        msg =  "*** Error: the pipe #"+ std::to_string(sockFd - 3) +"->#"+ std::to_string(*recvId) +" already exists. ***\n";
        write (STDERR_FILENO, msg.c_str(), msg.length());
        return -1;
    }
    else{
        pipe(pipeFd);
        // UserPipe temp(pipeFd[0], sockFd - 4);
        // users[*recvId - 1].userPipe.push_back(temp);
        users[*recvId - 1].up[sockFd - 4] = pipeFd[0];
        userPipe[1] = pipeFd[1]; 
    }
    return 0;
}


int createProcess(string line, int sockFd, User *users){
    signal(SIGCHLD, signalHandler);
    
    pid_t childProcessId;
    vector<int> processTable;
    vector<LocalPipe> localPipe;
    vector<string> programList;
    int userPipe[2];
    int isStderrPipe;
    // pipe init
    LocalPipe temp;
    while(pipe(temp.pipeFd()) < 0){
        usleep(1000);
    }
    localPipe.push_back(temp);
    split(line, programList, "|");
    

    int countDown = -1;
    // all countdown in global pipe should be minused one every round
    for(int i=0; i<users[sockFd - 4].globalPipe.size(); i++)
        users[sockFd - 4].globalPipe.at(i).countDownMinusOne();

    // check StderrPipe "!" or not
    if((isStderrPipe = programList.back().find("!",0)) >= 0){
        split(programList.back(),programList,"!");
    }
    
    // check the number pipe or not
    if(isNumber(programList.back())){
        bool createNPipe = true;
        
        // check the exist pipe for the same countDown
        for(int i=0; i<users[sockFd - 4].globalPipe.size(); i++){
            if(users[sockFd - 4].globalPipe.at(i).countDownEqual(atoi(programList.back().c_str()))){
                createNPipe = false;
                break;
            }
        }
        
        // append into global pipe
        if(createNPipe){
            GlobalPipe temp(atoi(programList.back().c_str()));
            pipe(temp.pipeFd());
            users[sockFd - 4].globalPipe.push_back(temp);
        }
        countDown = atoi(programList.back().c_str());
        programList.pop_back();    
    }
    //write (STDERR_FILENO, std::to_string(programList.size()).c_str(), std::to_string(programList.size()).length());
    // create process by each command
    for(int commandNum=0; commandNum<programList.size(); commandNum++){ 
        
        LocalPipe temp;
        while(pipe(temp.pipeFd()) < 0){
            usleep(1000);
        }
        localPipe.push_back(temp);
        
        vector<string> initName,programName,finIdCommand;
        bool isRedirect = false;
        bool isOutUserPipe = false;
        bool isInUserPipe = false;
        int findFlag, sendId, recvId, outFlag, inFlag;
        char msg[MAX_MSG_SIZE + 1];

        // check user pipe output " >" or redirection " > " 
        if((findFlag = programList.at(commandNum).find(" > ",0)) >= 0){
            isRedirect = true;
        }else if((findFlag = programList.at(commandNum).find(" >",0)) >= 0){
            isOutUserPipe = true;
        }
        // check user pipe input " <"
        if((findFlag = programList.at(commandNum).find("<",0)) >= 0){
            isInUserPipe = true;
            split(programList.at(commandNum), initName, "<");
            initName.pop_back();
        }else{
            split(programList.at(commandNum), initName, ">");
            if(isOutUserPipe){
                initName.pop_back();
            }
        }

        for(int i=0; i<initName.size(); i++)
            split(initName.at(i), programName, " ");
       
        if(isInUserPipe){
            split(programList.at(commandNum), finIdCommand, " ");
            for(int i=0 ; i<finIdCommand.size(); i++){
                if((inFlag = finIdCommand.at(i).find("<",0)) >= 0){
                    split(finIdCommand.at(i), finIdCommand, "<");
                    break;
                }
            }
            sendId = atoi(finIdCommand.back().c_str());
            finIdCommand.clear();
            if(initInUserPipe(sockFd, users, userPipe, &sendId) < 0){
                break;
            }
        }

        if(isOutUserPipe){
            split(programList.at(commandNum), finIdCommand, " ");
            for(int i=0 ; i<finIdCommand.size(); i++){
                if((outFlag = finIdCommand.at(i).find(">",0)) >= 0){
                    split(finIdCommand.at(i), finIdCommand, ">");
                    break;
                }
            }
            recvId = atoi(finIdCommand.back().c_str());
            finIdCommand.clear();
            if(initOutUserPipe(sockFd, users, userPipe, &recvId) < 0){
                break;
            }
            for(int i=0 ; i<programName.size(); i++){
                if((outFlag = programName.at(i).find(">",0)) >= 0){
                    programName.pop_back();
                }
            }
        }

        // define file descriptor input
        int fd_in = (commandNum == 0) ? (STDIN_FILENO) : (localPipe.front().pipeFd()[0]);;
        if(commandNum == 0)
            for(int i=0; i<users[sockFd - 4].globalPipe.size(); i++){
                if(users[sockFd - 4].globalPipe.at(i).countDownEqual(0)){
                    fd_in = users[sockFd - 4].globalPipe.at(i).pipeFd()[0];
                }             
            }

        if(fd_in != STDIN_FILENO) { dup2(fd_in, STDIN_FILENO); }

        // in user pipe
        if(isInUserPipe){
            if(sendId){
                dup2(userPipe[0],STDIN_FILENO);
                close(userPipe[0]);
                snprintf (msg, MAX_MSG_SIZE + 1, "*** %s (#%d) just received from %s (#%d) by '%s' ***\n", users[sockFd - 4]._name, sockFd - 3, users[sendId - 1]._name, sendId, line.c_str());   
                broadcast (std::string(msg), users);
                //sendId = 0;
            } 
        }

        // fork process to exec command
        while((childProcessId = fork()) < 0){
            usleep(1000);
        }

        if(childProcessId == 0){ 
            // child process
            
            // define file descriptor output
            int fd_out = (commandNum != programList.size() - 1) ? (localPipe.back().pipeFd()[1]) : (STDOUT_FILENO);
            if(commandNum == programList.size() - 1)
                for(int i=0; i<users[sockFd - 4].globalPipe.size(); i++){
                    if(users[sockFd - 4].globalPipe.at(i).countDownEqual(countDown)){
                        fd_out = users[sockFd - 4].globalPipe.at(i).pipeFd()[1];
                    }             
                }
            
            // let fd_in => stdin and fd_out => stdout
            
            if(fd_out != STDOUT_FILENO) { dup2(fd_out, STDOUT_FILENO); }
            // if use "!", let fd_out => stderr
            if(isStderrPipe > 0){ dup2(fd_out, STDERR_FILENO); }
            
            // except for stdin and stdout, all of other file descriptor (pipe) would be closed
            for(int i=0; i<users[sockFd - 4].globalPipe.size(); i++){
                users[sockFd - 4].globalPipe.at(i).pipeFdClose();
            }
            for(int i=0; i<localPipe.size(); i++){
                localPipe.at(i).pipeFdClose();
            }

            // out user pipe
            if(isOutUserPipe){
                if(recvId){
                    dup2(userPipe[1],STDOUT_FILENO);
                    close(userPipe[1]);
                    snprintf (msg, MAX_MSG_SIZE + 1, "*** %s (#%d) just piped '%s' to %s (#%d) ***\n", users[sockFd - 4]._name, sockFd - 3, line.c_str(), users[recvId - 1]._name, recvId);
                    broadcast (std::string(msg), users);
                    //recvId = 0;
                } 
            }

            // redirection state    
            if(isRedirect){
                int fd_out = open(programName.back().c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd_out < 0){
                    fprintf(stderr, "Error: Unable to open the output file.\n");
                    exit(EXIT_FAILURE);
                }
                else{
                    dup2(fd_out, STDOUT_FILENO);
                    close(fd_out);
                }
                programName.pop_back();
            }
            
            // vector <string> convert to char**
            char** commandArray = new char*[programName.size()];
            for (int j=0; j<programName.size(); j++){
                commandArray[j] = new char[programName.at(j).size()+1];
                strcpy(commandArray[j], programName.at(j).c_str());
            }

            // add char* 0 or NULL into commandArray
            commandArray[programName.size()] = NULL;
            
            if(execvp(programName.at(0).c_str(),commandArray) == -1 ){
                fprintf(stderr, "Unknown command: [%s].\n",programName.at(0).c_str());
                _exit(EXIT_FAILURE);
            }     
            programName.clear();
            return EXIT_SUCCESS;
        } 
        else{ 
            // parent process
            processTable.push_back(childProcessId);
            for(int i=0; i<users[sockFd - 4].globalPipe.size();){
                if(users[sockFd - 4].globalPipe.at(i).countDownEqual(0)){
                    users[sockFd - 4].globalPipe.at(i).pipeFdClose();
                    users[sockFd - 4].globalPipe.erase(users[sockFd - 4].globalPipe.begin()+i);
                }
                else
                    i++;       
            }
            localPipe.front().pipeFdClose();
            localPipe.erase(localPipe.begin());
            // if(isInUserPipe)
            //     close(userPipe[0]);
            if(isOutUserPipe)
                close(userPipe[1]);
        }
    }
    
    for(int i=0; i<localPipe.size(); i++){
        localPipe.at(i).pipeFdClose();
    }
    localPipe.clear();
    
    if(countDown <= 0)
        for(int i=0; i<processTable.size(); i++){
            int status;
            waitpid(processTable.at(i), &status, 0);
        }
    processTable.clear();
    programList.clear();
    return 0;
}

void restoreEnv(int sockFd, User *users){
    //clearenv();
    vector<string> programName;
    
    for(int i = 0; i < users[sockFd - 4].env.size(); i++){
        split(users[sockFd - 4].env.at(i), programName, " ");
        setenv(programName.at(1).c_str(), programName.at(2).c_str(), 1);
        programName.clear();
    }
}

int npShell(int sockFd, User *users){ 
    string line;
    vector<string> programList;
    //vector<GlobalPipe> globalPipe;

    restoreEnv (sockFd, users);
    // cout <<"% ";
    if(getline(cin, line)){
        if(line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        if(!line.empty()){
            if(!builtInFunction(line, sockFd, users)){
                createProcess(line, sockFd, users);
            }    
        }
        // cout << "% ";
    }
    return 0; 
}

void save_fds (int *stdfd)
{
	stdfd[0] = dup (STDIN_FILENO);
	stdfd[1] = dup (STDOUT_FILENO);
	stdfd[2] = dup (STDERR_FILENO);
}

void restore_fds (int *stdfd)
{
	dup2 (stdfd[0], STDIN_FILENO);
	close (stdfd[0]);
	dup2 (stdfd[1], STDOUT_FILENO);
	close (stdfd[1]);
	dup2 (stdfd[2], STDERR_FILENO);
	close (stdfd[2]);
}

int main(int argc, char *argv[]) {
    int sockFd, childFd, fd;
    int serverPort = atoi(argv[1]);
    unsigned int clientLen;
    struct sockaddr_in clientAddress;
    struct sockaddr_in serverAddress;
    vector<struct sockaddr_in> serverList;
    pid_t childPid;
    fd_set readFds,activeFds;
    int maxFd;
    User users[MAX_USERS] = {
        User()
    };
    // Socket
    if((sockFd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        cout << "[Server] Cannot create socket\n";
        exit(EXIT_FAILURE);
    }

    // Reuse address
    int ture = 1;
    if(setsockopt(sockFd, SOL_SOCKET, SO_REUSEADDR|SO_REUSEPORT, &ture, sizeof(ture)) < 0) {
        cout << "[Server] Setsockopt failed\n";
        exit(EXIT_FAILURE);
    }

    // Bind
    bzero((char *) &serverAddress, sizeof(serverAddress));
    serverAddress.sin_family            = AF_INET;
    serverAddress.sin_addr.s_addr       = htonl(INADDR_ANY);
    serverAddress.sin_port              = htons(serverPort);

    if(bind(sockFd, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0) {
        cout << "[Server] Cannot bind address\n";
        exit(EXIT_FAILURE);
    }

    // Listen
    if(listen(sockFd, MAX_CLIENTS) < 0) {
        cout << "[Server] Failed to listen\n";
    }

    char msg[MAX_MSG_SIZE + 1];

    FD_ZERO(&activeFds);
    FD_SET(sockFd, &activeFds);
    maxFd = getdtablesize();
    // Accept
    while(1) {
        cout << "[Server] Waiting for connections on " << serverPort << "...\n";

        // copy the active fds into read fds
		memcpy (&readFds, &activeFds, sizeof(readFds));

        if(select(maxFd+1, &readFds, NULL, NULL, NULL) < 0){
            if(errno == 4)
                continue;
        }

        if(FD_ISSET(sockFd, &readFds)){
            // new connection
            clientLen = sizeof(clientAddress);
            if((childFd = accept(sockFd, (struct sockaddr *) &clientAddress, &clientLen)) < 0) {
                cout << "[Server] Failed to accept\n";
                return -1;
            }
            else {
                FD_SET(childFd, &activeFds);
                users[childFd - 4].initSet("CGILAB", 511, childFd);
                //users[childFd - 4].initSet(inet_ntoa(clientAddress.sin_addr), clientAddress.sin_port, childFd);
                write(childFd, welcome, strlen(welcome));
                snprintf(msg, MAX_MSG_SIZE + 1, "*** User '%s' entered from %s/%d. ***\n", users[childFd - 4]._name, users[childFd - 4]._ip, users[childFd - 4]._port);
                broadcast(std::string(msg), users);
                users[childFd - 4].up = (int *)calloc(MAX_USERS, sizeof (int));
                write(childFd, prompt, strlen(prompt));
                clearenv();
                users[childFd - 4].env.push_back("setenv PATH bin:.");
            }
        }
        /* execute the command input by clients */
        for (int fd = 4; fd < maxFd; fd++){

            if (fd != sockFd && FD_ISSET(fd, &readFds)) {
                cout << "[Server] Connected\n";
                int	stdfd[3];
                save_fds(stdfd);
                dup2(fd,STDOUT_FILENO);
                dup2(fd,STDIN_FILENO);
                dup2(fd,STDERR_FILENO);
                npShell(fd, users);
                restore_fds(stdfd);
                
                if(users[fd - 4]._connection){
                    write(fd, prompt, strlen(prompt));
                }
                else{
                    char msg[MAX_MSG_SIZE + 1];
                    FD_CLR(fd, &activeFds);
                    snprintf (msg, MAX_MSG_SIZE + 1, "*** User '%s' left. ***\n", users[fd - 4]._name);
                    broadcast (msg, users);
                    write (fd, msg, strlen (msg));	/* since the connection has been set to 0 */
                    close (fd);
                    users[fd -4].globalPipeClose();
                    //users[fd -4].userPipeClose();
                    users[fd -4].env.clear();
                    users[fd -4].upClear();
                    // memset (&users[fd - 1], 0, sizeof (User));
                    // users.erase(users.begin()+i);
                }
            }
        }
    }
    return 0;
}          