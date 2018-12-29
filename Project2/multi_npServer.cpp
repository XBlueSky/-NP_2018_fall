#include <sys/wait.h>
#include <sys/socket.h>
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
#include <csignal>
#include <fcntl.h>
#include <arpa/inet.h> // for network address
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include "semaphore.h"
using namespace std;

#define MAX_CLIENTS    5
#define MAX_USERS	30
#define MAX_MSG_NUM 20
#define MAX_MSG_SIZE	1024
#define MAX_NAME_SIZE	20
#define SHMKEY ((key_t) 8798)
#define PERM 0666

const char welcome[] =	"****************************************\n"
			        "** Welcome to the information server. **\n"
			        "****************************************\n\n";
const char prompt[] = "% ";

static int	shmid = 0, uid = 0;

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
        void initSet(char *ip, int port, int id, int pid){
            strcpy(_name, "(no name)");
            strcpy(_ip, ip);
            _port = port;
            _id = id;
            _pid = pid;
            _connection = 1;
        }
        char _name[MAX_NAME_SIZE+1];
        char _ip[16];
        int _port;
        int _id;
        int _pid;
        char msg[MAX_USERS][MAX_MSG_SIZE + 1];
        int _connection;
        // char msg[MAX_USERS][MAX_MSG_NUM][MAX_MSG_SIZE + 1];
};

static User	*users = NULL;

void broadcast (string msg)
{
	for (int i = 0; i < MAX_USERS; i++) {
		/* broadcast to on-line clients */
        
        if(users[i]._id > 0){
            sem_wait(semid);
            strncpy (users[i].msg[uid], msg.c_str(), MAX_MSG_SIZE + 1);
            sem_signal(semid);
            kill (users[i]._pid, SIGUSR1);
            // for (int j = 0; j < MAX_MSG_NUM; ++j) {
			// 	if (users[i].msg[uid][j][0] == 0) {
			// 		strncpy (users[i].msg[uid][j], msg.c_str(), MAX_MSG_SIZE + 1);
			// 		kill (users[i]._pid, SIGUSR1);
			// 		break;
			// 	}
			// }
        }
	}
}

void userRemove(){
    char	msg[MAX_MSG_SIZE + 1];
    snprintf (msg, MAX_MSG_SIZE + 1, "*** User '%s' left. ***\n", users[uid]._name);
	broadcast (msg);
    string fileName;
    close (STDIN_FILENO);
	close (STDOUT_FILENO);
	close (STDERR_FILENO);
    users[uid]._id = 0;

    for(int i=0; i<MAX_USERS; i++){
        fileName = "user_pipe/" + std::to_string(i) + "_to_" + std::to_string(uid + 1);
        remove(fileName.c_str());
        fileName = "user_pipe/" + std::to_string(uid + 1) + "_to_" + std::to_string(i);
        remove(fileName.c_str());
    }
    memset (&users[uid], 0, sizeof (User));
}
void shmSignalHandler(int sig){
    if(sig == SIGUSR1){
        for(int i=0; i<MAX_USERS; i++){
            // for (int j = 0; j < MAX_MSG_NUM; ++j) {
			// 	if (users[uid].msg[i][j][0] != 0) {
			// 		write (STDOUT_FILENO, users[uid].msg[i][j], strlen (users[uid].msg[i][j]));
			// 		memset (users[uid].msg[i][j], 0, MAX_MSG_SIZE);
			// 	}
			// }
            if (users[uid].msg[i][0] != 0){
                sem_wait(semid);
                write (STDOUT_FILENO, users[uid].msg[i], strlen (users[uid].msg[i]));
                memset (users[uid].msg[i], 0, MAX_MSG_SIZE);
                sem_signal(semid);
            }
        }
    }
    else if (sig == SIGUSR2){
        userRemove();
		shmdt(users);
        sem_close(semid);
    }
    else if (sig == SIGINT || sig == SIGQUIT || sig == SIGTERM) {
		userRemove();
		shmdt(users);
        sem_close(semid);
	}
}
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

void who(){
	char msg[MAX_MSG_SIZE + 1];
	snprintf (msg, MAX_MSG_SIZE + 1, "<ID>\t<nickname>\t<IP/port>\t<indicate me>\n");
    write(STDOUT_FILENO, msg, strlen (msg));
    for(int i = 0; i < MAX_USERS; i++){
        if(users[i]._id>0){
            snprintf (msg, MAX_MSG_SIZE + 1, "%d\t%s\t%s/%d", i+1, users[i]._name, users[i]._ip, users[i]._port);
            if(i == uid)
                strcat (msg, "\t<-me\n");
            else
                strcat (msg, "\n");
            write (STDOUT_FILENO, msg, strlen(msg));
        }
    }
}

void name(string newName){
    char msg[MAX_MSG_SIZE + 1];
    // check the same name
    for(int i = 0; i < MAX_USERS; i++){
        if(users[i]._id > 0 && i != uid && strcmp(users[i]._name, newName.c_str()) == 0){
            snprintf (msg, MAX_MSG_SIZE + 1, "*** User '%s' already exists. ***\n", newName.c_str());
			write (STDOUT_FILENO, msg, strlen(msg));
			return;
        }
    }
    strncpy (users[uid]._name, newName.c_str(), MAX_NAME_SIZE+1);
	snprintf (msg, MAX_MSG_SIZE + 1, "*** User from %s/%d is named '%s'. ***\n", users[uid]._ip, users[uid]._port, users[uid]._name);
    broadcast(std::string(msg));
}

void tell(vector<string>& programName){
    int recvId = atoi(programName.at(1).c_str());
    string msg;
    if(users[recvId - 1]._id > 0){
        msg = "*** " + std::string(users[uid]._name) + " told you ***: ";
        for(int i = 2; i < programName.size(); i++){
            msg = msg + " " + programName.at(i);
        }
        msg += "\n";
        // for (int i = 0; i < MAX_MSG_NUM; ++i) {
		// 	if (users[recvId - 1].msg[uid][i][0] == 0) {
		// 		strncpy (users[recvId - 1].msg[uid][i], msg.c_str(), MAX_MSG_SIZE + 1);
		// 		kill (users[recvId - 1]._pid, SIGUSR1);
		// 		break;
		// 	}
		// }
        sem_wait(semid);
        strncpy (users[recvId - 1].msg[uid], msg.c_str(), MAX_MSG_SIZE + 1);
        sem_signal(semid);
		kill (users[recvId - 1]._pid, SIGUSR1);
    }
    else {
        msg = "*** Error: user #" +  std::to_string(recvId) + " does not exist yet. ***\n";
		write (STDERR_FILENO, msg.c_str(), msg.length());
	}
}

void yell(vector<string>& programName){
    string msg;
    msg = "*** " + std::string(users[uid]._name) + " yelled ***: ";
    for(int i = 1; i < programName.size(); i++){
        msg = msg + " " + programName.at(i);
    }
    msg += "\n";
    broadcast(msg);
}

bool builtInFunction(string line, bool *connection){
    vector<string> programName;
    split(line, programName, " ");

    if("setenv" == programName.at(0)){
        setenv(programName.at(1).c_str(), programName.at(2).c_str(), 1);
        //users[sockFd - 4].env.push_back(line);
        return true;
    } 
    else if("printenv" == programName.at(0)){
        cout << getenv(programName.at(1).c_str()) << endl;
        return true;
    }
    else if("exit" == programName.at(0)){
        users[uid]._connection = 0;
        return true;
    }
    else if("who" == programName.at(0)){
        who();
        return true;
    }
    else if("name" == programName.at(0)){
        name(programName.at(1));
        return true;
    }
    else if("tell" == programName.at(0)){
        tell(programName);
        return true;
    }
    else if("yell" == programName.at(0)){
        yell(programName);
        return true;
    }
    else if("kick" == programName.at(0)){
        int id = std::stoi(programName.at(1));
        users[id - 1]._connection = 0;
        string msg = std::to_string(uid + 1) + " kick " + programName.at(1);
        broadcast(msg);
        kill (users[id - 1]._pid, SIGUSR2);
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

void signalHandler(int signum){
    int status;
    while(waitpid(-1, &status, WNOHANG) > 0){
        //do nothing
    }
}

int initInUserPipe (int *sendId){
	string msg;
    string fileName = "user_pipe/" + std::to_string(*sendId) + "_to_" + std::to_string(uid + 1);
    int fd_in = -1;
	if (users[*sendId - 1]._id == 0) {
        msg = "*** Error: user #"+ std::to_string(*sendId) +" does not exist yet. ***\n";
		write (STDERR_FILENO, msg.c_str(), msg.length());
		return -1;
	} 
    else if ((fd_in = open(fileName.c_str(), O_RDONLY)) < 0) {
        msg = "*** Error: the pipe #"+ std::to_string(*sendId) +"->#"+ std::to_string(uid + 1) +" does not exist yet. ***\n";
		write (STDERR_FILENO, msg.c_str(), msg.length());
		return -1;
	} 
    return fd_in;
}

int initOutUserPipe(int *recvId){
    string msg;
    struct stat buf;
    string fileName = "user_pipe/" + std::to_string(uid + 1) + "_to_" + std::to_string(*recvId);
    if(users[*recvId -1]._id == 0){
        msg = "*** Error: user #"+ std::to_string(*recvId) +" does not exist yet. ***\n";
        write (STDERR_FILENO, msg.c_str(), msg.length());
        return -1;
    }
    else if(stat(fileName.c_str(), &buf) != -1){
        msg =  "*** Error: the pipe #"+ std::to_string(uid + 1) +"->#"+ std::to_string(*recvId) +" already exists. ***\n";
        write (STDERR_FILENO, msg.c_str(), msg.length());
        return -1;
    }
    
    return open(fileName.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
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

int createProcess(string line, vector<GlobalPipe>& globalPipe){
    signal(SIGCHLD, signalHandler);
    
    pid_t childProcessId;
    vector<int> processTable;
    vector<LocalPipe> localPipe;
    vector<string> programList;
    int userPipe[2];
    int isStderrPipe;

    int	stdfd[3];
    save_fds(stdfd);

    // pipe init
    LocalPipe temp;
    while(pipe(temp.pipeFd()) < 0){
        usleep(1000);
    }
    localPipe.push_back(temp);
    split(line, programList, "|");
    

    int countDown = -1;
    // all countdown in global pipe should be minused one every round
    for(int i=0; i<globalPipe.size(); i++)
        globalPipe.at(i).countDownMinusOne();

    // check StderrPipe "!" or not
    if((isStderrPipe = programList.back().find("!",0)) >= 0){
        split(programList.back(),programList,"!");
    }
    
    // check the number pipe or not
    if(isNumber(programList.back())){
        bool createNPipe = true;
        
        // check the exist pipe for the same countDown
        for(int i=0; i<globalPipe.size(); i++){
            if(globalPipe.at(i).countDownEqual(atoi(programList.back().c_str()))){
                createNPipe = false;
                break;
            }
        }
        
        // append into global pipe
        if(createNPipe){
            GlobalPipe temp(atoi(programList.back().c_str()));
            pipe(temp.pipeFd());
            globalPipe.push_back(temp);
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
            if((userPipe[0] = initInUserPipe(&sendId)) < 0){
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
            if((userPipe[1] = initOutUserPipe(&recvId)) < 0){
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
            for(int i=0; i<globalPipe.size(); i++){
                if(globalPipe.at(i).countDownEqual(0)){
                    fd_in = globalPipe.at(i).pipeFd()[0];
                }             
            }

        if(fd_in != STDIN_FILENO) { dup2(fd_in, STDIN_FILENO); }

        // in user pipe
        if(isInUserPipe){
            if(sendId){
                dup2(userPipe[0],STDIN_FILENO);
                close(userPipe[0]);
                snprintf (msg, MAX_MSG_SIZE + 1, "*** %s (#%d) just received from %s (#%d) by '%s' ***\n", users[uid]._name, users[uid]._id, users[sendId - 1]._name, sendId, line.c_str());   
                broadcast (std::string(msg));
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
                for(int i=0; i<globalPipe.size(); i++){
                    if(globalPipe.at(i).countDownEqual(countDown)){
                        fd_out = globalPipe.at(i).pipeFd()[1];
                    }             
                }
            
            // let fd_in => stdin and fd_out => stdout
            
            if(fd_out != STDOUT_FILENO) { dup2(fd_out, STDOUT_FILENO); }
            // if use "!", let fd_out => stderr
            if(isStderrPipe > 0){ dup2(fd_out, STDERR_FILENO); }
            
            // except for stdin and stdout, all of other file descriptor (pipe) would be closed
            for(int i=0; i<globalPipe.size(); i++){
                globalPipe.at(i).pipeFdClose();
            }
            for(int i=0; i<localPipe.size(); i++){
                localPipe.at(i).pipeFdClose();
            }

            // out user pipe
            if(isOutUserPipe){
                if(recvId){
                    dup2(userPipe[1],STDOUT_FILENO);
                    close(userPipe[1]);
                    snprintf (msg, MAX_MSG_SIZE + 1, "*** %s (#%d) just piped '%s' to %s (#%d) ***\n", users[uid]._name, users[uid]._id, line.c_str(), users[recvId - 1]._name, recvId);
                    broadcast (std::string(msg));
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
            for(int i=0; i<globalPipe.size();){
                if(globalPipe.at(i).countDownEqual(0)){
                    globalPipe.at(i).pipeFdClose();
                    globalPipe.erase(globalPipe.begin()+i);
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
            if(isInUserPipe){
                string fileName = "user_pipe/" + std::to_string(sendId) + "_to_" + std::to_string(uid + 1);
                remove(fileName.c_str());
            }
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
    restore_fds(stdfd);
    return 0;
}

int npShell(struct sockaddr_in *clientAddress){ 
    string line;
    vector<string> programList;
    bool connection = true;
    vector<GlobalPipe> globalPipe;

    // get shm
    if((shmid = shmget(SHMKEY, MAX_USERS * sizeof (User), PERM)) < 0) {
        fputs ("server error: shmget failed\n", stderr);
        exit (1);
    }
    if((users = (User *) shmat (shmid, NULL, 0)) == (User *) -1) {
        fputs ("server error: shmat failed\n", stderr);
        exit (1);
    }

    // init env
    clearenv ();
    setenv ("PATH", "bin:.", 1);

    // init signal
    signal(SIGUSR1, shmSignalHandler);
    signal(SIGUSR2, shmSignalHandler);
    signal(SIGINT, shmSignalHandler);
	signal(SIGQUIT, shmSignalHandler);
	signal(SIGTERM, shmSignalHandler);

    // add user
    char msg[MAX_MSG_SIZE + 1];
    for (int i=0; i<MAX_USERS; i++){
        if(users[i]._id == 0){
            uid = i; // this process is for this uid.
            users[uid].initSet("CGILAB", 511, uid+1, getpid());
            //users[uid].initSet(inet_ntoa(clientAddress->sin_addr), clientAddress->sin_port, uid+1, getpid());
            snprintf (msg, MAX_MSG_SIZE + 1, "*** User '%s' entered from %s/%d. ***\n", users[uid]._name, users[uid]._ip, users[uid]._port);
			broadcast(std::string(msg));
            break;
        }
    }

    //cout <<"% ";
    while(users[uid]._connection){
        write(STDOUT_FILENO, prompt, strlen(prompt));
        if(getline(cin, line)){
            if(line[line.size() - 1] == '\r')
                line.erase(line.size() - 1);
            if(!line.empty()){
                if(!builtInFunction(line, &connection)){ 
                    createProcess(line, globalPipe);
                }    
            }
            //cout << "% ";
        }
    }

    // clear
    if (users[uid]._pid == getpid ())
		userRemove();

	/* detach the shared memory segment */
	shmdt (users);

    return 0; 
}

int main(int argc, char *argv[]) {
    int sockFd, childFd, fd;
    int serverPort = atoi(argv[1]);
    unsigned int clientLen;
    struct sockaddr_in clientAddress;
    struct sockaddr_in serverAddress;
    vector<struct sockaddr_in> serverList;
    pid_t childPid;

    if((semid = sem_create(SEMKEY,1)) < 0){
        printf("Semaphore Error\n");
    }
    if((semid = sem_open(SEMKEY)) < 0){
        perror("SEM Open error");
    }

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

    if((shmid = shmget(SHMKEY, MAX_USERS * sizeof (User), PERM | IPC_CREAT)) < 0) {
		fputs ("server error: shmget failed\n", stderr);
		exit (1);
	}
    if((users = (User *) shmat (shmid, NULL, 0)) == (User *) -1) {
		fputs ("server error: shmat failed\n", stderr);
		exit (1);
	}
    shmdt (users);

    // Accept
    while(1) {
        cout << "[Server] Waiting for connections on " << serverPort << "...\n";
        clientLen = sizeof(clientAddress);
        if((childFd = accept(sockFd, (struct sockaddr *) &clientAddress, &clientLen)) < 0) {
            cout << errno<<"\n";
            cout << "[Server] Failed to accept\n";
            continue;
        }
        if((childPid=fork())==0) {
            // children process
            cout << "[Server] Connected\n";
            close(sockFd);
            dup2(childFd,STDOUT_FILENO);
            dup2(childFd,STDIN_FILENO);
            dup2(childFd,STDERR_FILENO);
            close(childFd);
            write(STDOUT_FILENO, welcome, strlen(welcome));
            npShell(&clientAddress);
            _exit(0);
            // Fork child socket
        }else{
            //parent process
            close(childFd);

        }
    }
    sem_close(semid);
    return 0;
}          