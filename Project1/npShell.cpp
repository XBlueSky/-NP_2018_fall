#include <string>
#include <cstring>
#include <vector>
#include <map>
#include <csignal>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <iostream> 
using namespace std; 

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

bool builtInFunction(string line){
    map<string, int> builtInMap;
    map<string, int>::iterator iter;
    builtInMap["setenv"] = 0;
    builtInMap["printenv"] = 1;
    builtInMap["exit"] = 2;
    vector<string> programName;

    split(line, programName, " ");
    iter = builtInMap.find(programName.at(0));
    switch(iter->second){
        case 0:
            setenv(programName.at(1).c_str(), programName.at(2).c_str(), 1);
            break;
        case 1:
            cout << getenv(programName.at(1).c_str()) << endl;
            break;
        case 2:
            _exit(1);
        default:
            return false;
    }
    return true;
}

bool isNumber(const std::string& s){
    vector <string> number;
    split(s, number, " ");
    std::string::const_iterator it = number.front().begin();
    while (it != number.front().end() && std::isdigit(*it)) ++it;
    return !number.front().empty() && it == number.front().end();
}

int commandParse(string line, vector<string>& programList){
    int count; //count the number of delimiters "|"
    count = split(line, programList, "|");
    if(builtInFunction(line)){ 
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

int createProcess(vector<string>& programList, int countDown, vector<GlobalPipe>& globalPipe, int isStderrPipe){
    signal(SIGCHLD, signalHandler);
    
    pid_t childProcessId;
    vector<int> processTable;
    vector<LocalPipe> localPipe;
    // pipe init
    LocalPipe temp;
    while(pipe(temp.pipeFd()) < 0){
        usleep(1000);
    }
    // pipe(temp.pipeFd());
    localPipe.push_back(temp);
    
    // create process by each command
    for(int commandNum=0; commandNum<programList.size(); commandNum++){ 
        
        LocalPipe temp;
        while(pipe(temp.pipeFd()) < 0){
            usleep(1000);
        }
        // pipe(temp.pipeFd());
        localPipe.push_back(temp);
        // fork process to exec command
        while((childProcessId = fork()) < 0){
            usleep(1000);
        }
       
        if(childProcessId == 0){ 
            // child process
            vector<string> initName,programName;
            bool isRedirect = false;

            if(split(programList.at(commandNum), initName, ">") > 0){
                isRedirect = true;
            }
            for(int i=0; i<initName.size(); i++)
                split(initName.at(i), programName, " ");
            
            // define file descriptor input
            int fd_in = (commandNum == 0) ? (STDIN_FILENO) : (localPipe.front().pipeFd()[0]);;
            if(commandNum == 0)
                for(int i=0; i<globalPipe.size(); i++){
                    if(globalPipe.at(i).countDownEqual(0)){
                        fd_in = globalPipe.at(i).pipeFd()[0];
                    }             
                }
            
            // define file descriptor output
            int fd_out = (commandNum != programList.size() - 1) ? (localPipe.back().pipeFd()[1]) : (STDOUT_FILENO);
            if(commandNum == programList.size() - 1)
                for(int i=0; i<globalPipe.size(); i++){
                    if(globalPipe.at(i).countDownEqual(countDown)){
                        fd_out = globalPipe.at(i).pipeFd()[1];
                    }             
                }
            
            // let fd_in => stdin and fd_out => stdout
            if(fd_in != STDIN_FILENO) { dup2(fd_in, STDIN_FILENO); }
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
                exit(EXIT_FAILURE);
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

int main(int argc, char *argv[], char *envp[]) { 
    setenv("PATH", "bin:.", 1); 
    string line;
    vector<string> programList;
    vector<GlobalPipe> globalPipe;
    int isStderrPipe;

    cout <<"% ";
    while(getline(cin, line)){
        if(!line.empty()){
            if(commandParse(line, programList) != -1){
                int countDown = -1;

                // all countdown in global pipe should be minused one every round
                for(int i=0; i<globalPipe.size(); i++)
                    globalPipe.at(i).countDownMinusOne();
                
                // check "!"r or not
                if((isStderrPipe = split(programList.back(),programList,"!")) == 0){
                    programList.pop_back();
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
                createProcess(programList, countDown, globalPipe, isStderrPipe);
            }    
        }
        cout << "% ";
    }
    return 0; 
}