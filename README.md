# Custom Unix Shell in C++

## Overview
This project is a **custom Unix shell** written in C++ that includes advanced tab-completion features, built-in command execution, and support for external executables. It provides an interactive command-line interface with robust parsing and dynamic tab-based autocompletion.

## Features
✅ **Command Execution** - Supports built-in commands (`echo`, `pwd`, `cd`, `type`, `exit`) and external executables.  
✅ **Advanced Tab Completion** - Handles both built-in commands and executables from the system `$PATH`.  
✅ **Longest Common Prefix (LCP) Completion** - Automatically fills in the longest matching command prefix.  
✅ **Multi-Level Completion** - Successive `<TAB>` presses refine completion when multiple commands share a common prefix.  
✅ **Candidate Listing** - Displays possible matches if a command has multiple valid completions.  
✅ **Raw Terminal Mode** - Uses `termios` to read input dynamically like a real shell.  

## Installation & Compilation
### Prerequisites
- **C++17 or later**
- **GNU Make (optional)**

### Build Instructions
Compile the shell using `g++`:

```bash
g++ -std=c++17 -o my_shell shell.cpp
