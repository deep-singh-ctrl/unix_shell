#include <iostream>
#include <string>
#include <filesystem>
#include <vector>
#include <sstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

enum parser_state {
    NORMAL,
    SINGLE_QUOTE_MODE,
    DOUBLE_QUOTE_MODE,
};

void runExecutableFilePath(std::vector<std::string> &userInput){
    const char* env_p = std::getenv("PATH");
    //WARNING : No error handling in case PATH does not exist
    std::string env_val(env_p);
    std::stringstream ss(env_val);
    std::string token;
    std::vector<char* > argv;
    for(std::string &s : userInput){
      argv.push_back(s.data());
    }
    // execv expects a NULL at the end of the argument list so we append one to argv.
    argv.push_back(NULL);
    char** argvPointer = argv.data();
    // Use this argvPointer inside exec when you fork for a new process 
    // For LINUX the delimiter for PATH directories is a colon
    char delimiter = ':';
    std::vector<std::string> results;

    while (std::getline(ss, token, delimiter)) {
        results.push_back(token);
    }

    bool foundExecutable = false;
    namespace fs = std::filesystem;
    for (std::string& s : results) {
        fs::path filePath = s + "/" + userInput[0];
        if (std::filesystem::exists(filePath)) {
            
            if ((fs::status(filePath).permissions() & fs::perms::owner_exec) != fs::perms::none ||
                (fs::status(filePath).permissions() & fs::perms::others_exec) != fs::perms::none ||
                (fs::status(filePath).permissions() & fs::perms::group_exec) != fs::perms::none) {
                 foundExecutable = true;
                 pid_t child = fork();
                 // assuming that the child can ALWAYS be created.
                 if(child == 0){
                  execv(filePath.string().data(), argvPointer); 
                 }
                 else if(child > 0){
                  waitpid(child, NULL, 0);
                 }
                 break;
            }
        }
    }
    if(!foundExecutable){
        std::cout << userInput[0] << ": command not found" << std::endl;
    }
}

void parseUserInput(std::vector<std::string> &userInput, 
    const std::string &command){
    std::string token = "";
    int current_state = NORMAL;
    for(int i = 0; i < command.size(); i++){
        char ch = command[i];
        switch (current_state) {
        case NORMAL:
            if(ch == '\''){
                current_state = SINGLE_QUOTE_MODE;
            }
            else if(ch == '\"'){
                current_state = DOUBLE_QUOTE_MODE;
            }
            else if(ch == ' '){
                if(token.size() > 0){
                    userInput.push_back(token);
                    token = "";
                }
            }
            else if(ch == '\\'){
                // here assuming that there IS a character after the escape else the
                // shell just waits for an extra character.
                token += command[i+1];
                i++;
            }
            else{
                token += ch;
            }
            break;
        case SINGLE_QUOTE_MODE:
            if(ch == '\''){
                current_state = NORMAL;
            }
            else{
                token += ch;
            }
            break;
        case DOUBLE_QUOTE_MODE:
            if(ch == '\"'){
                current_state = NORMAL;
            }
            else if(ch == '\\'){
                if(i+1 < command.size() && command[i+1] == '\"' || 
                command[i+1] == '\\' || command[i+1] == '$' || 
                command[i+1] == '`'){
                    token += command[i+1];
                }
                else{
                    token += command[i];
                    token += command[i+1];
                }
                i++;
            }
            else{
                token += ch;
            }
            break;
        }
    }
    if(token.size() > 0) userInput.push_back(token);
}


int main() {
    // Flush after every std::cout / std::cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    // TODO: Uncomment the code below to pass the first stage
    while (true) {
        std::cout << "$ ";
        std::string command;
        std::getline(std::cin, command);
        std::vector<std::string> userInput;
        // this helps us take care of single quotes, double quotes etc inside
        // the command.
        parseUserInput(userInput, command);
        if (userInput[0] == "cd"){
            std::filesystem::path home_directory = std::getenv("HOME");
            if(userInput[1].substr(0,1) == "~"){
                userInput[1] = home_directory.string() + "/" + userInput[1].substr(1);
            }
            std::filesystem::path new_directory = userInput[1];
            if(std::filesystem::exists(new_directory)){
                std::filesystem::current_path(new_directory);
            }
            else{
                std::cout << "cd: " << new_directory.string() <<": No such file or directory" << std::endl; 
            }
        }
        else if(command == "pwd"){
            //I am not dealing with errors here, if error is thrown that's an afterthought
            std::filesystem::path cwd = std::filesystem::current_path();
            std::cout << cwd.string() << std::endl;
        }
        else if (command == "exit") {
            break;
        } else if (command.substr(0, 4) == "echo") {
            for(int i = 1; i < userInput.size() - 1; i++){
                std::cout << userInput[i] << " ";
            }
            std::cout << userInput.back() << std::endl;
        } else if (command.substr(0, 4) == "type") {
            std::string argument = command.substr(5);

            if (argument == "echo" || argument == "type" || argument == "exit" || argument == "pwd") {
                std::cout << argument << " is a shell builtin" << std::endl;
            } else {
                const char* env_p = std::getenv("PATH");

                if (env_p == nullptr) {
                    std::cout << "Environment variable not found." << std::endl;
                    return 1;
                }

                std::string env_val(env_p);
                std::stringstream ss(env_val);
                std::string token;

                // For LINUX the delimiter for PATH directories is a colon
                char delimiter = ':';
                std::vector<std::string> results;

                while (std::getline(ss, token, delimiter)) {
                    results.push_back(token);
                }

                bool valid = false;
                namespace fs = std::filesystem;
                for (std::string& s : results) {
                    fs::path filePath = s + "/" + command.substr(5);
                    if (std::filesystem::exists(filePath)) {
                        
                        if ((fs::status(filePath).permissions() & fs::perms::owner_exec) != fs::perms::none ||
                            (fs::status(filePath).permissions() & fs::perms::others_exec) != fs::perms::none ||
                            (fs::status(filePath).permissions() & fs::perms::group_exec) != fs::perms::none) {
                            std::cout << command.substr(5) << " is " << filePath.string() << std::endl;
                            valid = true;
                            break;
                        }
                    }
                }

                if (!valid) {
                    std::cout << command.substr(5) << ": not found" << std::endl;
                }
            }
            
        } else {
            // Checking whether it's a executable
            runExecutableFilePath(userInput);
        }
    }
    return 0;
}