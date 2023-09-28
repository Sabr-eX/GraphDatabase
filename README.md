# OS Assignment 1

Hello guys, this file has nothing to do with how the application actually functions. Problem Statement is also attached below. I will probably use this file for documentation. So first thing is we need to establish communication between two files, we will use message queue for that. We use `ftok` to get the key of a particular file and use that as key for the message queue itself.

[ftok](https://man7.org/linux/man-pages/man3/ftok.3.html) - convert a pathname and a project identifier to a System V IPC key.

Then we make the queue system itself. Our message structure is defined as the following:

```c
struct data
{
    char message[MESSAGE_LENGTH];
    char operation;
};

struct msg_buffer
{
    long msg_type;
    struct data data;
};
```

Here we use data struct which keeps track of which operation is being performed. 1 stands for ping, 2 stands for file search, 3 stands for within file search and 4 starts of cleanup. Here the important part is r. r stands for reply. When the server is replying to a client it uses r to ensure it doesn't get mixed up by anything else. We also faced major issues while trying to use wait() since forgetting wait leads to race conditions and it calling itself for an infinite number of times.

## PROBLEM STATEMENT:

In this Assignment we are going to use the different concepts learnt so far like Linux commands, process creation, inter-process communication, wait() and exec(). The problem statement of the Assignment consists of the following parts.

### Write a POSIX-compliant C program client.c.

[DONE] a) On execution, each instance of this program creates a separate client process,
i.e., if the executable file corresponding to client.c is client.out, then each time
client.out is executed on a separate terminal, a separate client process is
created.

[DONE] b) When a client process is run, it will ask the user to enter a positive integer as its
client-id. The prompt message will look like this.
`Enter Client-ID:`
If you are running four instances of client.out, then for the first instance, the user
should enter client-id as 1, for the second instance, the user should enter client-id
as 2 and so on. Out of the total clients running, who will get what id, can be
decided randomly.

[DONE] c) Then, each client will display a menu to the user (no GUI or beautification is
required):

```
1. Enter 1 to contact the Ping Server
2. Enter 2 to contact the File Search Server
3. Enter 3 to contact the File Word Count Server
4. Enter 4 if this Client wishes to exit
```

For menu option 1, the client will not ask the user to enter any input. For each of
menu options 2 and 3, the client will ask for a filename from the user. Assume the
files to be ASCII (text only) files.

[DONE] d) Upon receiving a command from the user (using the above menu), the client will
communicate the command and the necessary argument(s) (if required) to the
main server (described later). This communication with the main server should
take place only using a message queue.

[DONE] e) There can be any number of client processes communicating with the main
server. All communications between all the clients and the main server (or the
main server’s children) (both ways, i.e., from client to main server and from
server’s child to client) need to be done through the single message queue
mentioned in the above point. You are not allowed to create more than one
message queue for this purpose. [Hint: use the mtype]

[DONE] f) The client will wait for a reply from the server (actually it will be the server’s child,
as described later). On receiving the reply, the client will display the output to the
user and redisplay the menu options, until the user chooses option 4. Upon
choosing option 4, the client will gracefully terminate. Graceful termination implies
performing any sort of cleanup activities that are required to be done and then
terminate.

### Write a POSIX-compliant C program server.c (let’s call this the main server).

[DONE] a) The main server is responsible for creating the message queue to be used for
communication with the clients.

[DONE] b) The main server will listen to the message queue for new requests from the
clients.

[DONE] c) Upon receipt of a request from a client, the main server will spawn a child server
and offload the execution of the specific task (as specified by the client) to the
child server. Thus, for every client request, a separate child server will be created
and the main server will keep listening for more client requests.

[DONE] d) All communications between the main server and its children will need to be done
only through pipes. No other IPC mechanism should be used for this purpose.
You may create as many pipes as required for communication between the main
server and its children.

[DONE] e) For option 1 in the client menu, the client will send a “hi” to the server via the
single message queue and a child of the server (spawned by the server) will
reply back to the client with a “hello”. The “hello” should be sent back to the client
using the single message queue described above. After sending the reply, the
specific server child will perform the relevant cleanup activities and terminate.

[DONE] f) For options 2 and 3 in the client menu, the child should use the exec() family of
functions and the relevant Linux commands to accomplish the tasks. For the file
search task, the child server (not the main server) will inform the client if the file
exists or not and the client will display the search result to the user. For the file
word count task, the child server will communicate to the client the word count for
the specified file and the client needs to display the result to the user. However,
the results generated by the child server instances need to be communicated to
the client via the single message queue described above, in a programmer
defined format. `You will need to figure out how to do this given that the exec()
functions replace the executable code in the current process and then
accordingly figure out how to communicate the result back to the client.`

[DONE] g) The main server should loop forever listening for new requests from any of the
clients. Upon a request, it spawns a child to service the request and listen for
more requests.

### Write a POSIX compliant C program cleanup.c.

[DONE] 1. This cleanup process can keep running along with the clients and the main
server. This process will keep displaying a menu as:
Do you want the server to terminate? Press Y for Yes and N for No.

[DONE] 2. If N is given as input, the process keeps running as usual and will not
communicate with any other process. If Y is given as input, the process will
inform the main server via the single message queue that the main server needs
to terminate. Remember when the main server terminates, the child servers
should also terminate.

3. Assume that the main server will never force one or more
   child servers to exit without servicing the relevant client requests. After passing
   on the terminate information to the main server, the cleanup process will
   terminate. When the main server receives the terminate information from the
   cleanup process, the main server deletes the message queue, performs other
   cleanup tasks (if required) and terminates.

4. Use wait()s appropriately. Perform all relevant error handling properly without which
   marks will be deducted.

[DONE] 5. Note the files that are used in options 2 and 3 are simple ASCII (text only) files.

[DONE] 6. You can assume that all the C files as well as the files for options 2 and 3 are present in
the same directory.

[DONE] 7. You should not use more than one message queue in the entire assignment. The
communications which are specified to be done using the single message queue, should
be done using the message queue only and no other IPC mechanism should be used in
those cases.
