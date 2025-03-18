# Custom Shell

A simple custom shell implementation in C++ with features such as command execution, tab completion, and redirection parsing.

## Features

- **Command Execution**: Run built-in and external commands seamlessly.
- **Tab Completion**: Autocomplete commands and file paths using the Tab key.
- **Input Redirection (`<`)**: Read input from a file.
- **Output Redirection (`>` and `>>`)**: Redirect command output to a file.
- **Pipe (`|`) Support**: Chain commands together.
- **History Feature**: Keep track of previous commands.
- **Background Execution (`&`)**: Run processes in the background.

## Installation

1. Clone the repository:
   ```sh
   git clone https://github.com/yourusername/custom-shell.git
   cd custom-shell
   ```
2. Compile the shell:
   ```sh
   g++ -o shell main.cpp shell.cpp -lreadline
   ```
3. Run the shell:
   ```sh
   ./shell
   ```

## Usage

- Type commands like in a regular shell.
- Use **Tab** for autocompletion.
- Use **Ctrl + C** to terminate a running command.
- Use **Ctrl + D** to exit the shell.

### Examples

- **Running a command:**
  ```sh
  ls -l
  ```
- **Redirecting output:**
  ```sh
  ls > output.txt
  ```
- **Chaining commands with a pipe:**
  ```sh
  ls | grep .cpp
  ```
- **Running a command in the background:**
  ```sh
  ./long_running_task &
  ```

## Dependencies

- `g++` (C++ Compiler)
- `readline` (For tab completion and history management)

## Contributing

1. Fork the repository
2. Create a new branch (`git checkout -b feature-branch`)
3. Commit your changes (`git commit -m 'Add new feature'`)
4. Push to the branch (`git push origin feature-branch`)
5. Create a Pull Request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- Inspired by Unix shell implementations
- Uses the Readline library for command-line editing and history

