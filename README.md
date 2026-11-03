# 🖥️ MiniShell — Unix Shell & Concurrent HTTP Server

A lightweight Unix-style command-line shell developed in **C**, featuring process management, command execution, pipelines, input/output redirection, and background jobs. The project also includes a **multithreaded HTTP status server** built using POSIX threads and TCP sockets.

## 🚀 Features

### 🐚 MiniShell
- **Command Execution:** Run external Linux commands using `fork()` and `exec()`.
- **Pipelines:** Connect multiple commands using the `|` operator.
- **I/O Redirection:** Redirect input and output using `<` and `>`.
- **Background Jobs:** Execute commands asynchronously using `&`.
- **Job Management:** Track background processes with the `jobs` command.
- **Command History:** View previously executed commands using `history`.
- **Directory Navigation:** Change directories using `cd`.
- **Signal Handling:** Handle `Ctrl+C` for foreground processes.
- **Built-in Commands:** `cd`, `history`, `jobs`, and `exit`.

### 🌐 Concurrent HTTP Status Server
- Built using C, POSIX threads (`pthread`), and TCP sockets.
- Listens for HTTP connections on port `8080`.
- Handles multiple client connections using threads.
- Returns service information and runtime statistics in JSON format.
- Tracks total requests and active clients.

**Example JSON Response:**

```json
{
  "service": "status_server",
  "requests": 5,
  "active_clients": 1
}
```

## 🛠️ Technologies Used

| Technology | Purpose |
|---|---|
| C | Core programming language |
| Linux / Unix | Execution environment |
| POSIX APIs | Process and signal management |
| POSIX Threads | Concurrent HTTP request handling |
| TCP Sockets | Client-server communication |
| HTTP / JSON | Server communication and responses |
| GNU Make | Build automation |
| Python unittest | Automated testing |

## 📦 Installation & Setup

### Step 1: Clone the Repository

```bash
git clone https://github.com/LavenGit/LavenProjects.git
```

Navigate to the directory containing the MiniShell project.

### Step 2: Compile the Project

Make sure GCC and Make are installed.

```bash
make
```

This compiles the source files using the project's Makefile.

### Step 3: Run MiniShell

```bash
./minishell
```

You can now execute Linux commands inside the shell.

## 💻 Example Commands

**Execute a command:**

```bash
ls -l
```

**Change directories:**

```bash
cd /tmp
```

**Use pipelines:**

```bash
echo hello | wc -c
```

**Redirect output:**

```bash
echo hello > output.txt
```

**Run a background process:**

```bash
sleep 10 &
```

**View background jobs:**

```bash
jobs
```

**View command history:**

```bash
history
```

**Exit MiniShell:**

```bash
exit
```

## 🌐 Running the HTTP Server

Open a separate terminal and start the server:

```bash
./status_server
```

The server listens on port `8080`.

In another terminal, send an HTTP request:

```bash
curl http://localhost:8080
```

The server returns a JSON response containing its current statistics.

## 🧪 Automated Testing

The project includes automated tests for shell functionality and server behavior.

Run:

```bash
make test
```

Tests cover:

- Command execution and pipelines
- Directory navigation
- Command history
- Invalid command handling
- HTTP server responses and concurrency, where included in the test suite

## 🧹 Clean Build Files

Remove generated executables and other build artifacts:

```bash
make clean
```

## 📚 Concepts Demonstrated

This project demonstrates practical knowledge of:

- Operating system fundamentals
- Process creation and execution
- Inter-process communication
- File descriptors and I/O redirection
- Signal handling and process groups
- Multithreading and concurrency
- TCP socket programming
- HTTP request handling
- Automated software testing

## 👨‍💻 Author

**Laven Joshi**
[GitHub Profile](https://github.com/LavenGit)
