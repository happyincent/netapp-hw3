# netapp hw3

## Build

`$ make clean all`

## Usage:
    
Server:
```
./server
Server is listening on [::]:1234
```
Client:
```
./client
[B023040025 @ hw3]$ help
usage 1: connect [IP] [PORT] [username]
usage 2: chat [username] [message]
usage 3: chat {[username1] [username2] ...} [message]
usage 4: bye
```

## Program Introduction

* Server
  - Use "select" to handle which socket descriptor has been changed.
  - Create a "linke-list" to save each client's information.
  - Use this list to handle messages exchange between clients.
  
* IPC Server
  - Create a "shared memory" (size = MAX_USERS defined at ipc_user.h) to save each client's information.
  - Use semaphore to lock/unlock when read/write shared memory.
  - After successfully login, Fork a new process to handle messages exchange between clients.
  - For each child process, create a thread to monitor the shared memory.

* Client
  - Connect to Server. (IP: localhost or 127.0.0.1 or ::1, PORT: 1234 )
  - Get User's command and do corresponding actions.
  - Create a thread to receive messages send from the server(othrer clients).