#include <iostream>
#include <string>
#include <sstream>
#include <filesystem>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <fstream>
#include <fcntl.h>
#include <termios.h>
#include <set>
#include <algorithm>
using namespace std;

enum ValidCommands {
  cd,
  echo,
  type,
  exit0,
  pwd,
  invalid
};

ValidCommands isValid(string command){
  command = command.substr(0, command.find(" "));

  if (command == "cd") return ValidCommands::cd;
  if (command == "echo") return ValidCommands::echo;
  if (command == "type") return ValidCommands::type;
  if (command == "pwd") return ValidCommands::pwd;
  if (command == "exit") return ValidCommands::exit0;
  return invalid;
}

string getPath(string command){
  char* pathEnv = getenv("PATH");
  if(!pathEnv) return "";
  string pathenv = pathEnv;
  stringstream ss(pathenv);

  string path;
  while(!ss.eof()){
    getline(ss, path, ':');
    string abs_path = path + "/" + command;
    if(filesystem::exists(abs_path)){
      return abs_path;
    }
  }
  return "";
}

vector<string> parseArgs(const string& input) {
    vector<string> tokens;
    string token;
    bool inSingleQuotes = false;
    bool inDoubleQuotes = false;
    bool escapeNext = false;

    for (size_t i = 0; i < input.size(); ++i) {
        char ch = input[i];

        // Handle escaped characters
        if (escapeNext) {
            token.push_back(ch);
            escapeNext = false;
            continue;
        }

        // Handle backslash escaping
        if (ch == '\\') {
            if (inDoubleQuotes && (i + 1 < input.size())) {
                char nextCh = input[i + 1];
                // Special handling of \, ", $ inside double quotes
                if (nextCh == '\\' || nextCh == '\"' || nextCh == '$') {
                    token.push_back(nextCh);
                    i++;
                    continue;
                }
                // For all other characters inside double quotes, keep both backslash and character
                token.push_back('\\');
                token.push_back(input[i+1]);
                i++;
            } else if (!inSingleQuotes) {
                // Outside of quotes or in single quotes, escape next char
                escapeNext = true;
            } else {
                // In single quotes, backslash is literal
                token.push_back(ch);
            }
            continue;
        }

        // Quote handling
        if (ch == '\'' && !inDoubleQuotes) {
            inSingleQuotes = !inSingleQuotes;
            continue;
        }

        if (ch == '\"' && !inSingleQuotes) {
            inDoubleQuotes = !inDoubleQuotes;
            continue;
        }

        // Space handling (token separator outside quotes)
        if (ch == ' ' && !inSingleQuotes && !inDoubleQuotes) {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
            continue;
        }

        // Add character to token
        token.push_back(ch);
    }

    if (!token.empty()) {
        tokens.push_back(token);
    }

    return tokens;
}

// Update the RedirectInfo struct to include append flags
struct RedirectInfo {
    string command;
    string stdoutFile;
    string stderrFile;
    bool stdoutAppend;
    bool stderrAppend;
};

RedirectInfo parseRedirection(const string& input) {
    string command = input;
    string stdoutFile = "";
    string stderrFile = "";
    bool stdoutAppend = false;
    bool stderrAppend = false;
    
    // Create a copy of the input to work with
    string remaining = input;
    
    // Look for stdout append redirection (>> or 1>>)
    size_t stdoutAppendPos = remaining.find(" >> ");
    if (stdoutAppendPos == string::npos) {
        stdoutAppendPos = remaining.find(" 1>> ");
    }
    
    // Look for stdout truncate redirection (> or 1>) if no append found
    size_t stdoutTruncPos = string::npos;
    if (stdoutAppendPos == string::npos) {
        stdoutTruncPos = remaining.find(" > ");
        if (stdoutTruncPos == string::npos) {
            stdoutTruncPos = remaining.find(" 1> ");
        }
    }
    
    // Process stdout redirection
    size_t stdoutRedirectPos = stdoutAppendPos;
    if (stdoutRedirectPos == string::npos) {
        stdoutRedirectPos = stdoutTruncPos;
    } else {
        stdoutAppend = true;
    }
    
    if (stdoutRedirectPos != string::npos) {
        // Extract the command part
        command = remaining.substr(0, stdoutRedirectPos);
        
        // Find start of output file name
        size_t redirectOpEnd = remaining.find(">", stdoutRedirectPos);
        if (stdoutAppend) redirectOpEnd++; // Skip second '>'
        redirectOpEnd++;
        
        size_t fileStart = remaining.find_first_not_of(" ", redirectOpEnd);
        
        // Find end of output file name
        size_t fileEnd = remaining.find(" ", fileStart);
        if (fileEnd == string::npos) {
            fileEnd = remaining.length();
        }
        
        stdoutFile = remaining.substr(fileStart, fileEnd - fileStart);
        
        // Update remaining string for potential stderr redirection
        if (fileEnd < remaining.length()) {
            remaining = remaining.substr(0, stdoutRedirectPos) + " " + remaining.substr(fileEnd);
        } else {
            remaining = remaining.substr(0, stdoutRedirectPos);
        }
    }
    
    // Look for stderr append redirection (2>>)
    size_t stderrAppendPos = remaining.find(" 2>> ");
    
    // Look for stderr truncate redirection (2>) if no append found
    size_t stderrTruncPos = string::npos;
    if (stderrAppendPos == string::npos) {
        stderrTruncPos = remaining.find(" 2> ");
    }
    
    // Process stderr redirection
    size_t stderrRedirectPos = stderrAppendPos;
    if (stderrRedirectPos == string::npos) {
        stderrRedirectPos = stderrTruncPos;
    } else {
        stderrAppend = true;
    }
    
    if (stderrRedirectPos != string::npos) {
        // If we didn't extract command yet from stdout redirection
        if (stdoutRedirectPos == string::npos) {
            command = remaining.substr(0, stderrRedirectPos);
        }
        
        // Find start of error file name
        size_t redirectOpEnd = remaining.find(">", stderrRedirectPos);
        if (stderrAppend) redirectOpEnd++; // Skip second '>'
        redirectOpEnd++;
        
        size_t fileStart = remaining.find_first_not_of(" ", redirectOpEnd);
        
        // Find end of file name
        size_t fileEnd = remaining.find(" ", fileStart);
        if (fileEnd == string::npos) {
            fileEnd = remaining.length();
        }
        
        stderrFile = remaining.substr(fileStart, fileEnd - fileStart);
    }
    
    return {command, stdoutFile, stderrFile, stdoutAppend, stderrAppend};
}

vector<string> COMMANDS = {
    "cd",
    "echo",
    "exit",
    "pwd",
    "type"
};

void enableRawMode(termios& orig_termios) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disableRawMode(const termios& orig_termios) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

string findCompletion(const string& partial) {
    if (partial.empty()) {
        return "";
    }
    
    // First check builtin commands
    for (const auto& cmd : COMMANDS) {
        if (cmd.substr(0, partial.length()) == partial) {
            return cmd;
        }
    }
    
    // If no builtin match, check executables in PATH
    char* pathEnv = getenv("PATH");
    if (!pathEnv) return "";
    
    string pathenv = pathEnv;
    stringstream ss(pathenv);
    string path;
    
    // Store found executables that match the partial
    vector<string> matches;
    
    while (getline(ss, path, ':')) {
        // Skip empty path components
        if (path.empty()) continue;
        
        // Ensure path ends with '/'
        if (path.back() != '/') {
            path += '/';
        }
        
        // Try to read directory contents
        try {
            for (const auto& entry : filesystem::directory_iterator(path)) {
                if (filesystem::is_regular_file(entry.path()) && 
                    (filesystem::status(entry.path()).permissions() & 
                    filesystem::perms::owner_exec) != filesystem::perms::none) {
                    
                    string filename = entry.path().filename().string();
                    
                    // Check if this executable matches our partial
                    if (filename.substr(0, partial.length()) == partial) {
                        matches.push_back(filename);
                    }
                }
            }
        } catch (const filesystem::filesystem_error&) {
            // Skip directories we can't access
            continue;
        }
    }
    
    // If we found exactly one match, return it
    if (matches.size() == 1) {
        return matches[0];
    }
    // If we found multiple matches, return the common prefix
    else if (matches.size() > 1) {
        // Find common prefix of all matches
        string prefix = matches[0];
        for (size_t i = 1; i < matches.size(); i++) {
            size_t j = 0;
            while (j < prefix.length() && j < matches[i].length() && 
                   prefix[j] == matches[i][j]) {
                j++;
            }
            prefix = prefix.substr(0, j);
        }
        return prefix;
    }
    
    return "";
}

int main() {
    cout << unitbuf;
    cerr << unitbuf;

    termios orig_termios;
    enableRawMode(orig_termios);
    
    // Variables to track tab completion state
    bool tabPressed = false;
    string lastTabInput;

    while (true) {
        cout << "$ ";
        string input;
        char c;
        
        // Reset tab state at the start of a new command
        tabPressed = false;
        lastTabInput = "";
        
        while (read(STDIN_FILENO, &c, 1) == 1) {
            if (c == '\t') {  // Tab key
                // Check if this is the second tab press on the same input
                if (tabPressed && input == lastTabInput) {
                    // Second tab press - show all matches
                    cout << endl;
                    
                    // Collect all matching executables
                    set<string> matches;
                    
                    // Check builtins
                    for (const auto& cmd : COMMANDS) {
                        if (cmd.substr(0, input.length()) == input) {
                            matches.insert(cmd);
                        }
                    }
                    
                    // Check PATH executables
                    char* pathEnv = getenv("PATH");
                    if (pathEnv) {
                        string pathenv = pathEnv;
                        stringstream ss(pathenv);
                        string path;
                        
                        while (getline(ss, path, ':')) {
                            if (path.empty()) continue;
                            
                            if (path.back() != '/') {
                                path += '/';
                            }
                            
                            try {
                                for (const auto& entry : filesystem::directory_iterator(path)) {
                                    if (filesystem::is_regular_file(entry.path()) && 
                                        (filesystem::status(entry.path()).permissions() & 
                                        filesystem::perms::owner_exec) != filesystem::perms::none) {
                                        
                                        string filename = entry.path().filename().string();
                                        
                                        if (filename.substr(0, input.length()) == input) {
                                            matches.insert(filename);
                                        }
                                    }
                                }
                            } catch (const filesystem::filesystem_error&) {
                                continue;
                            }
                        }
                    }
                    
                    // Print matches with two spaces between each
                    bool first = true;
                    for (const auto& match : matches) {
                        if (!first) {
                            cout << "  ";
                        }
                        cout << match;
                        first = false;
                    }
                    
                    // Print a new prompt
                    cout << endl << "$ " << input;
                } else {
                    // First tab press or new input - attempt completion
                    string completion = findCompletion(input);
                    if (!completion.empty() && completion != input) {
                        // Valid completion
                        cout << '\r';
                        
                        // Check if this is a complete match (exact match with an executable)
                        bool isExactMatch = false;
                        
                        // Check if this is a complete executable name
                        set<string> allMatches;
                        
                        // First check builtins
                        for (const auto& cmd : COMMANDS) {
                            if (cmd.substr(0, completion.length()) == completion) {
                                allMatches.insert(cmd);
                                if (cmd == completion) {
                                    isExactMatch = true;
                                }
                            }
                        }
                        
                        // Then check PATH executables
                        char* pathEnv = getenv("PATH");
                        if (pathEnv) {
                            string pathenv = pathEnv;
                            stringstream ss(pathenv);
                            string path;
                            
                            while (getline(ss, path, ':')) {
                                if (path.empty()) continue;
                                
                                if (path.back() != '/') path += '/';
                                
                                try {
                                    for (const auto& entry : filesystem::directory_iterator(path)) {
                                        if (filesystem::is_regular_file(entry.path()) && 
                                            (filesystem::status(entry.path()).permissions() & 
                                            filesystem::perms::owner_exec) != filesystem::perms::none) {
                                            
                                            string filename = entry.path().filename().string();
                                            
                                            // Check if this executable matches our completion
                                            if (filename.substr(0, completion.length()) == completion) {
                                                allMatches.insert(filename);
                                                if (filename == completion) {
                                                    isExactMatch = true;
                                                }
                                            }
                                        }
                                    }
                                } catch (const filesystem::filesystem_error&) {
                                    // Skip directories we can't access
                                    continue;
                                }
                            }
                        }
                        
                        // Only add space if it's an exact match and there are no other possible completions
                        if (isExactMatch && allMatches.size() == 1) {
                            cout << "$ " << completion << " ";
                            input = completion + " ";
                        } else {
                            cout << "$ " << completion;
                            input = completion;
                        }
                        
                        // Reset tab state as we've used a completion
                        tabPressed = false;
                        lastTabInput = "";
                    } else {
                        // No completion or incomplete - ring bell and mark as tabbed
                        cout << '\a' << flush;
                        tabPressed = true;
                        lastTabInput = input;
                    }
                }
                continue;
            }

            // Any other key press resets the tab state
            if (c != '\t') {
                tabPressed = false;
                lastTabInput = "";
            }

            // Rest of your key handling code
            if (c == '\n') {
                cout << '\n';
                break;
            }

            if (c == 127) {  // Backspace
                if (!input.empty()) {
                    input.pop_back();
                    cout << "\b \b";
                }
                continue;
            }

            if (isprint(c)) {  // Printable characters
                input += c;
                cout << c;
            }
        }

        // Handle empty input
        if (input.empty()) {
            continue;
        }

        // Trim trailing spaces
        while (!input.empty() && input.back() == ' ') {
            input.pop_back();
        }

        // Parse redirection before handling the command
        auto [commandPart, stdoutFile, stderrFile, stdoutAppend, stderrAppend] = parseRedirection(input);

        // Use commandPart instead of input for command parsing
        ValidCommands command = isValid(commandPart);

        // Set up stdout redirection if needed
        int originalStdout = -1;
        if (!stdoutFile.empty()) {
            originalStdout = dup(STDOUT_FILENO);
            // Use O_APPEND or O_TRUNC based on the redirection type
            int flags = O_WRONLY | O_CREAT | (stdoutAppend ? O_APPEND : O_TRUNC);
            int fd = open(stdoutFile.c_str(), flags, 0644);
            if (fd == -1) {
                perror("open");
            } else {
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
        }

        // Set up stderr redirection if needed
        int originalStderr = -1;
        if (!stderrFile.empty()) {
            originalStderr = dup(STDERR_FILENO);
            // Use O_APPEND or O_TRUNC based on the redirection type
            int flags = O_WRONLY | O_CREAT | (stderrAppend ? O_APPEND : O_TRUNC);
            int fd = open(stderrFile.c_str(), flags, 0644);
            if (fd == -1) {
                perror("open");
            } else {
                dup2(fd, STDERR_FILENO);
                close(fd);
            }
        }

        switch(command) {
            case cd: {
                string dir = commandPart.substr(3);
                if(dir == "~"){
                    char* home = getenv("HOME");
                    if(home){
                        dir = home;
                    }
                }
                if(filesystem::exists(dir)){
                    filesystem::current_path(dir);
                }
                else{
                    cout<<dir<<": No such file or directory"<<endl;
                }
                break;
            }
            // case ls: {
            //   cout << "ls: command not implemented" << endl;
            //   break;
            // }
            case echo: {
                size_t spacePos = commandPart.find(" ");
                string s = (spacePos == string::npos) ? "" : commandPart.substr(spacePos + 1);
                vector<string> args = parseArgs(s);
                for(auto arg: args){
                    cout<<arg<<" ";
                }
                cout<<endl;
                break;
            }
            case type: {
                commandPart.erase(0, commandPart.find(" ") + 1);
                if(isValid(commandPart) != invalid){
                    cout << commandPart << " is a shell builtin" << endl;
                }
                else{
                    string path = getPath(commandPart);
                    if(path.empty()){
                        cout << commandPart << ": not found" << endl;
                    }
                    else{
                        cout << commandPart << " is " << path << endl;
                    }
                }
                break;
            }
            case pwd: {
                char cwd[1024];
                if(getcwd(cwd, sizeof(cwd)) != NULL){
                    cout<<cwd<<endl;
                }
                else{
                    perror("getcwd");
                }
                break;
            }
            case exit0:
                disableRawMode(orig_termios);
                return 0;
            default: {
                vector<string> args = parseArgs(commandPart);

                string path = getPath(args[0]);
                if(path.empty()){
                    cout<<commandPart<<": command not found"<<endl;
                    break;
                }

                vector<char*> argv;
                for(auto& arg: args){
                    argv.push_back(&arg[0]);
                }
                argv.push_back(NULL);

                pid_t pid = fork();
                if(pid == 0) {
                    execv(path.c_str(), argv.data());
                    perror("execv");
                    exit(1);
                } else if(pid > 0) {
                    wait(nullptr);
                } else {
                    perror("fork");
                }
                break;
            }
        }

        // Restore stdout if we redirected it
        if (originalStdout != -1) {
            dup2(originalStdout, STDOUT_FILENO);
            close(originalStdout);
        }

        // Restore stderr if we redirected it
        if (originalStderr != -1) {
            dup2(originalStderr, STDERR_FILENO);
            close(originalStderr);
        }
    }
}