#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <filesystem>
#include <cstdlib>
#include <unistd.h>
#include <cctype>
#include <algorithm>
#include <termios.h>

using namespace std;
namespace fs = std::filesystem;

// Global variables to track tab-completion state.
static string last_tab_token = "";
static int tab_press_count = 0;

// Helper: Compute longest common prefix among strings.
string longestCommonPrefix(const vector<string>& strs) {
    if (strs.empty())
        return "";
    string prefix = strs[0];
    for (size_t i = 1; i < strs.size(); i++) {
        size_t j = 0;
        while (j < prefix.size() && j < strs[i].size() && prefix[j] == strs[i][j])
            j++;
        prefix = prefix.substr(0, j);
        if (prefix.empty())
            break;
    }
    return prefix;
}

void enableRawMode() {
    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disableRawMode() {
    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    raw.c_lflag |= (ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void handleTabPress(string &input) {
    // Split the input into the first token (command) and the remainder.
    size_t pos = input.find_first_of(" \t");
    string firstToken = (pos == string::npos ? input : input.substr(0, pos));
    string remainder = (pos == string::npos ? "" : input.substr(pos));

    // Update tab-press state: if the token is the same as last time, increment count.
    if (firstToken == last_tab_token) {
        tab_press_count++;
    } else {
        last_tab_token = firstToken;
        tab_press_count = 1;
    }

    // If no token is present, ring the bell.
    if (firstToken.empty()) {
        cout << "\a";
        return;
    }

    // Collect external executable candidates from PATH that start with firstToken.
    vector<string> candidates;
    const char* path_env = getenv("PATH");
    if (path_env != nullptr) {
        stringstream ss(path_env);
        string dir;
        while (getline(ss, dir, ':')) {
            try {
                for (const auto &entry : fs::directory_iterator(dir)) {
                    if (!entry.is_regular_file())
                        continue;
                    string filename = entry.path().filename().string();
                    if (filename.size() < firstToken.size())
                        continue;
                    if (filename.substr(0, firstToken.size()) == firstToken) {
                        // Check if the file is executable.
                        if (access(entry.path().c_str(), X_OK) == 0) {
                            if (find(candidates.begin(), candidates.end(), filename) == candidates.end())
                                candidates.push_back(filename);
                        }
                    }
                }
            } catch (const fs::filesystem_error &e) {
                continue; // Skip directories that cannot be opened.
            }
        }
    }

    if (candidates.empty()) {
        cout << "\a";
        return;
    }

    // Sort candidates for consistency.
    sort(candidates.begin(), candidates.end());

    // Compute the longest common prefix (LCP) among the candidate names.
    string lcp = longestCommonPrefix(candidates);

    if (lcp.size() > firstToken.size()) {
        // We can extend the current completion.
        string missing = lcp.substr(firstToken.size());
        // If there is exactly one candidate and no arguments, append a trailing space.
        if (candidates.size() == 1 && remainder.empty()) {
            missing += " ";
            lcp += " ";
        }
        input = lcp + remainder;
        cout << missing;
        // Reset tab-completion state.
        last_tab_token = "";
        tab_press_count = 0;
    } else {
        // No further progress can be made.
        if (tab_press_count == 1) {
            // First TAB press: ring the bell.
            cout << "\a";
        } else {
            // Second (or subsequent) TAB press: list all candidates.
            cout << "\n";
            for (size_t i = 0; i < candidates.size(); i++) {
                cout << candidates[i];
                if (i != candidates.size() - 1)
                    cout << "  ";
            }
            cout << "\n";
            cout << "$ " << input;
        }
        last_tab_token = "";
        tab_press_count = 0;
    }
}

void readInputWithTabSupport(string &input) {
    enableRawMode();
    char c;
    while (true) {
        c = getchar();
        if (c == '\n') {
            cout << "\n";
            break;
        } else if (c == '\t') {
            handleTabPress(input);
        } else if (c == 127) {  // Backspace key.
            if (!input.empty()) {
                input.pop_back();
                cout << "\b \b";
            }
        } else {
            input.push_back(c);
            cout << c;
        }
    }
    disableRawMode();
}

string shell_escape(const string &s) {
    string escaped = "'";
    for (char c : s) {
        if (c == '\'')
            escaped += "'\\''";
        else
            escaped.push_back(c);
    }
    escaped += "'";
    return escaped;
}

vector<string> tokenize(const string &input) {
    vector<string> tokens;
    string current;
    enum State { NORMAL, IN_SINGLE, IN_DOUBLE };
    State state = NORMAL;
    for (size_t i = 0; i < input.size(); i++) {
        char c = input[i];
        switch (state) {
            case NORMAL:
                if (isspace(static_cast<unsigned char>(c))) {
                    if (!current.empty()) {
                        tokens.push_back(current);
                        current.clear();
                    }
                } else if (c == '\\') {
                    if (i + 1 < input.size()) {
                        current.push_back(input[i + 1]);
                        i++;
                    } else {
                        current.push_back(c);
                    }
                } else if (c == '\'') {
                    state = IN_SINGLE;
                } else if (c == '"') {
                    state = IN_DOUBLE;
                } else {
                    current.push_back(c);
                }
                break;
            case IN_SINGLE:
                if (c == '\'')
                    state = NORMAL;
                else
                    current.push_back(c);
                break;
            case IN_DOUBLE:
                if (c == '"')
                    state = NORMAL;
                else if (c == '\\') {
                    if (i + 1 < input.size()) {
                        char next = input[i + 1];
                        if (next == '\\' || next == '$' || next == '"' || next == '\n') {
                            current.push_back(next);
                            i++;
                        } else {
                            current.push_back(c);
                        }
                    } else {
                        current.push_back(c);
                    }
                } else {
                    current.push_back(c);
                }
                break;
        }
    }
    if (!current.empty())
        tokens.push_back(current);
    return tokens;
}

string get_executable_path(const string &command) {
    const char* path_env = getenv("PATH");
    if (!path_env)
        return "";
    stringstream ss(path_env);
    string directory;
    while (getline(ss, directory, ':')) {
        string abs_path = directory + "/" + command;
        if (fs::exists(abs_path))
            return abs_path;
    }
    return "";
}

using CommandHandler = function<void(const vector<string>&)>;

void handleEcho(const vector<string>& tokens) {
    for (size_t i = 1; i < tokens.size(); i++) {
        if (i > 1)
            cout << " ";
        cout << tokens[i];
    }
    cout << "\n";
}

void handlePwd(const vector<string>& tokens) {
    cout << fs::current_path().string() << "\n";
}

void handleCd(const vector<string>& tokens) {
    if (tokens.size() < 2)
        return;
    string pathArg = tokens[1], target;
    if (!pathArg.empty() && pathArg[0] == '~') {
        const char* home = getenv("HOME");
        if (!home) {
            cout << "cd: " << pathArg << ": No such file or directory\n";
            return;
        }
        target = string(home) + pathArg.substr(1);
    } else {
        target = pathArg;
    }
    if (!target.empty() && fs::exists(target) && fs::is_directory(target)) {
        try {
            fs::current_path(target);
        } catch (const fs::filesystem_error &e) {
            cout << "cd: " << pathArg << ": No such file or directory\n";
        }
    } else {
        cout << "cd: " << pathArg << ": No such file or directory\n";
    }
}

void handleType(const vector<string>& tokens) {
    if (tokens.size() < 2) {
        cout << "type: command not found\n";
    } else {
        string arg = tokens[1];
        if (arg == "echo" || arg == "type" || arg == "exit" || arg == "pwd" || arg == "cd")
            cout << arg << " is a shell builtin\n";
        else {
            string path = get_executable_path(arg);
            if (!path.empty())
                cout << arg << " is " << path << "\n";
            else
                cout << arg << ": not found\n";
        }
    }
}

void handleExit(const vector<string>& tokens) {
    exit(0);
}

int main(){
    cout << unitbuf;
    unordered_map<string, CommandHandler> builtInCommands{
        {"echo", handleEcho},
        {"pwd", handlePwd},
        {"cd", handleCd},
        {"type", handleType},
        {"exit", handleExit}
    };
    
    while (true) {
        cout << "$ ";
        string input;
        readInputWithTabSupport(input);
        if (input == "exit 0")
            break;
        vector<string> tokens = tokenize(input);
        if (tokens.empty())
            continue;
        string cmd = tokens[0];
        
        if (builtInCommands.find(cmd) != builtInCommands.end()) {
            builtInCommands[cmd](tokens);
        } else {
            string execPath = get_executable_path(cmd);
            if (execPath.empty() && fs::exists(cmd))
                execPath = cmd;
            if (execPath.empty()){
                cout << cmd << ": command not found\n";
            } else {
                string full_command = shell_escape(cmd);
                for (size_t i = 1; i < tokens.size(); i++) {
                    full_command += " " + shell_escape(tokens[i]);
                }
                int ret = system(full_command.c_str());
                if (ret == -1)
                    cerr << "Failed to execute the program\n";
            }
        }
    }
    return 0;
}
