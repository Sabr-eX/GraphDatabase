/**
 * @file server.c
 * @author Divyateja Pasupuleti (pro)
 * @brief
 * @version 0.1
 * @date 2023-09-20
 *
 * @copyright Copyright (c) 2023
 * POSIX-compliant C program server.c
 *
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <limits.h>

#define MESSAGE_LENGTH 100
#define SERVER_RECEIVES_ON_CHANNEL 1
#define WRITE_END_OF_PIPE 1
#define READ_END_OF_PIPE 0

struct data
{
    char message[MESSAGE_LENGTH];
    char operation;
    long client_id;
};

struct msg_buffer
{
    long msg_type;
    struct data data;
};

/**
Here we use data struct which keeps track of which operation is being performed. 1 stands for ping,
2 stands for file search, 3 stands for within file search and 4 starts of cleanup. Here the important
part is r. r stands for reply. When the server is replying to a client it uses r to ensure it doesn't
get mixed up by anything else. We also faced major issues while trying to use wait() since forgetting
wait leads to race conditions and it calling itself for an infinite number of times.
*/

/**
 * @brief Ping Server: Sends back hello to the client
 *
 */
void ping(int msg_queue_id, int client_id, struct msg_buffer msg)
{
    printf("[Child Process: Ping] Message received from Client %d-Operation %c -> %s\n", client_id, msg.data.operation, msg.data.message);
    printf("[Child Process: Ping] Sending message back to the client...\n");

    // We send back hello
    msg.data.message[0] = 'H';
    msg.data.message[1] = 'e';
    msg.data.message[2] = 'l';
    msg.data.message[3] = 'l';
    msg.data.message[4] = 'o';
    msg.data.message[5] = '\0';

    // We set message type as the client id so that only client receives the code
    msg.msg_type = client_id;
    msg.data.client_id = client_id;
    msg.data.operation = 'r';

    if (msgsnd(msg_queue_id, &msg, sizeof(msg.data), 0) == -1)
    {
        perror("[Child Process: Ping] Message could not be sent, please try again");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("[Child Process: Ping] Message sent back to client %d successfully\n", client_id);
        exit(EXIT_SUCCESS);
    }
}

/**
 * @brief File Search Server: Uses 'find' function to get output.
 *
 */
void file_search(const char *filename, int msg_queue_id, int client_id, struct msg_buffer msg)
{
    int link[2];
    pid_t pid;

    // Read the filename from user input
    char output[4096];

    // We create pipe first
    if (pipe(link) == -1)
    {
        perror("[Child Process: File Search] Error in pipe creation");
        exit(EXIT_FAILURE);
    }
    // We fork a new process
    if ((pid = fork()) == -1)
    {
        perror("[Child Process: File Search] Error in fork creation");
        exit(EXIT_FAILURE);
    }

    if (pid == 0)
    {
        // Child process
        dup2(link[WRITE_END_OF_PIPE], STDOUT_FILENO);
        close(link[READ_END_OF_PIPE]);
        close(link[WRITE_END_OF_PIPE]);

        // We print the filename to std error because dup2 has connected stdout to write end of pipe
        fprintf(stderr, "[Child Process: File Search] Entered filename: %s\n", filename);

        // use the find command to check file names
        execlp("find", "find", ".", "-name", filename, NULL);

        // If the execlp fails then we print the error
        fprintf(stderr, "%s\n", "Error in execlp");

        exit(EXIT_FAILURE);
    }
    else
    {
        // Parent process

        // First wait for the child process to terminate
        waitpid(pid, NULL, 0);

        close(link[WRITE_END_OF_PIPE]);

        // we find the number of bytes first
        int numberOfBytes = read(link[READ_END_OF_PIPE], output, sizeof(output));

        // We use .* to restrict the size of input
        fprintf(stderr, "[Child Process: File Search] Output of find: (%.*s)\n", numberOfBytes, output);

        if (numberOfBytes < 0)
        {
            perror("[Child Process: File Search] Error in reading from pipe");
        }

        if (numberOfBytes < 1)
        {
            strcpy(msg.data.message, "File not found\n");
        }
        else
        {
            strcpy(msg.data.message, "File found\n");
        }

        msg.msg_type = client_id;
        msg.data.client_id = client_id;
        msg.data.operation = 'r';

        if (msgsnd(msg_queue_id, &msg, sizeof(msg.data), 0) == -1)
        {
            perror("[Child Process: File Search] Message could not be sent, please try again");
            exit(EXIT_FAILURE);
        }
        else
        {
            fprintf(stderr, "[Child Process: File Word] Message '%s' sent back to client %d successfully\n", msg.data.message, client_id);
        }
        close(link[READ_END_OF_PIPE]);
        exit(EXIT_SUCCESS);
    }
}

/**
 * @brief File Word Count Server: Uses 'wc' function to get output.
 * This function counts words in a file and sends the result to the client.
 *
 */
void word_count(const char *filename, int msg_queue_id, int client_id, struct msg_buffer msg)
{
    int link[2];       // array for file descriptors
    pid_t pid;         // process id
    char output[4096]; // buffer to store output of 'wc'

    if (pipe(link) == -1) // pipe creation
    {
        fprintf(stderr, "%s\n", "[Child Process: File Search] Error in pipe creation");
        exit(EXIT_FAILURE);
    }

    if ((pid = fork()) == -1) // fork a new process
    {
        fprintf(stderr, "%s\n", "[Child Process: File Search] Error in fork creation");
        exit(EXIT_FAILURE);
    }

    if (pid == 0)
    {
        // Child process
        dup2(link[WRITE_END_OF_PIPE], STDOUT_FILENO); // redirect stdout to write end of pipe
        close(link[READ_END_OF_PIPE]);                // close read end not needed
        close(link[WRITE_END_OF_PIPE]);               // close write end redirected
        fprintf(stderr, "[Child Process: File Search] Entered filename: %s\n", filename);

        // Use'wc' command to count words in the file
        execlp("wc", "wc", "-w", filename, NULL);

        fprintf(stderr, "%s\n", "Error in execl");
        exit(EXIT_FAILURE);
    }
    else
    {
        // Parent process
        // Wait for child process to terminate
        waitpid(pid, NULL, 0);
        close(link[WRITE_END_OF_PIPE]); // close write end not needed

        int numberOfBytes = read(link[READ_END_OF_PIPE], output, sizeof(output)); // read wc output from read end
        // fprintf(stderr, "Output of wc: (%.*s)\n", numberOfBytes, output);

        if (numberOfBytes < 0)
        {
            perror("[Child Process: File Search] Error in reading from pipe");
        }

        int wordCount = 0;
        if (numberOfBytes > 0)
        {
            sscanf(output, "%d", &wordCount);
        }

        // Message to be sent to client
        snprintf(msg.data.message, sizeof(msg.data.message), "Word count: %d\n", wordCount);

        // We use message type as client so that only client receives that message
        msg.msg_type = client_id;
        msg.data.client_id = client_id;
        msg.data.operation = 'r';

        if (msgsnd(msg_queue_id, &msg, sizeof(msg.data), 0) == -1) // send message
        {
            perror("[Child Process: File Search] Message could not be sent, please try again");
            exit(EXIT_FAILURE);
        }
        else
        {
            fprintf(stderr, "[Child Process: File Search] Message '%s' sent back to client %d successfully\n", msg.data.message, client_id);
        }
        close(link[READ_END_OF_PIPE]);
        exit(EXIT_SUCCESS);
    }
}

/**
 * @brief Cleanup
 *
 */
void cleanup(int msg_queue_id)
{
    int wstatus; // to store the exit status of a process
    pid_t w;

    while (wait(NULL) > 0)
    {
    }

    do
    {
        w = wait(&wstatus);

        if (w == -1)
        {
            // This would mean there is no child
            perror("[Server - Cleanup] Waitpid");

            // Destroy the message queue
            while (msgctl(msg_queue_id, IPC_RMID, NULL) == -1)
            {
                perror("[Server - Cleanup] Error while destroying the message queue");
            }

            exit(EXIT_FAILURE);
        }

        if (WIFEXITED(wstatus))
        {
            printf("[Server - Cleanup] Exited using status=%d\n", WEXITSTATUS(wstatus));
        }
        else if (WIFSIGNALED(wstatus))
        {
            printf("[Server - Cleanup] Killed using signal %d\n", WTERMSIG(wstatus));
        }
        else if (WIFSTOPPED(wstatus))
        {
            printf("[Server - Cleanup] Stopped using signal %d\n", WSTOPSIG(wstatus));
        }
        else if (WIFCONTINUED(wstatus))
        {
            printf("[Server - Cleanup] Process still continuing\n");
        }
    } while (!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus));

    // Destroy the message queue
    while (msgctl(msg_queue_id, IPC_RMID, NULL) == -1)
    {
        perror("[Server] Error while destroying the message queue");
    }

    exit(EXIT_SUCCESS);
}

/**
 * @brief The main server is responsible for creating the message queue to be used for
 * communication with the clients. The main server will listen to the message queue
 * for new requests from the clients.
 *
 *
 * @return int
 */
int main()
{
    // Iniitalize the server
    printf("[Server] Initializing Server...\n");

    // Create the message queue
    key_t key;
    int msg_queue_id;
    struct msg_buffer msg;

    // Link it with a key which lets you use the same key to communicate from both sides
    if ((key = ftok(".", 'B')) == -1)
    {
        perror("[Server] Error while generating key of the file");
        exit(EXIT_FAILURE);
    }

    // Create the message queue
    if ((msg_queue_id = msgget(key, 0644 | IPC_CREAT)) == -1)
    {
        perror("[Server] Error while connecting with Message Queue");
        exit(EXIT_FAILURE);
    }

    printf("[Server] Successfully connected to the Message Queue %d %d\n", key, msg_queue_id);

    // Listen to the message queue for new requests from the clients
    while (1)
    {
        if (msgrcv(msg_queue_id, &msg, sizeof(msg.data), SERVER_RECEIVES_ON_CHANNEL, 0) == -1)
        {
            perror("[Server] Error while receiving message from the client");
            exit(EXIT_FAILURE);
        }
        else
        {
            // Debugging logs
            // printf("[Logs] Message received from Client %ld-Operation %c -> %s\n", msg.data.client_id, msg.data.operation, msg.data.message);

            pid_t temporary_pid;

            if (msg.data.operation == '4')
            {
                cleanup(msg_queue_id);
                exit(EXIT_SUCCESS);
            }
            else if (msg.data.operation == 'r')
            {
                msg.data.operation = 'r';
                if (msgsnd(msg_queue_id, &msg, MESSAGE_LENGTH, 0) == -1)
                {
                    printf("[Server] Message could not be added back to the queue\n");
                }
            }
            else
            { // Executing the respective fuctions if a message has been received
                printf("[Server] Creating new child process for the new request received\n");
                temporary_pid = fork();
                if (temporary_pid < 0)
                {
                    perror("[Server] Error while creating child process");
                }
                else if (temporary_pid == 0)
                {
                    switch (msg.data.operation)
                    {
                    case '1':
                        ping(msg_queue_id, msg.data.client_id, msg);
                        break;
                    case '2':
                        file_search(msg.data.message, msg_queue_id, msg.data.client_id, msg);
                        break;
                    case '3':
                        word_count(msg.data.message, msg_queue_id, msg.data.client_id, msg);
                        break;
                    default:
                        perror("Incorrect operation");
                        break;
                    }
                    exit(EXIT_SUCCESS);
                }
            }
        }
    }

    return 0;
}
