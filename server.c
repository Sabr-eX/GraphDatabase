/**
 * @file server.c
 * @author Divyateja Pasupuleti
 * @brief
 * @version 0.1
 * @date 2023-09-20
 *
 * @copyright Copyright (c) 2023
 * POSIX-compliant C program server.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <sys/wait.h>

#define MESSAGE_LENGTH 100

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

/**
 * @brief Ping Server
 *
 */
void ping(int msg_queue_id, int client_id, struct msg_buffer msg)
{
    int file_descriptor[2];
    if (pipe(file_descriptor) == -1)
    {
        printf("Error: Could not create pipe");
        exit(-1);
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("Error while creating child process");
        exit(-1);
    }
    else if (pid == 0)
    {
        // Child Process
        printf("[Child Process] Message received from Client %ld-Operation %c -> %s\n", msg.msg_type, msg.data.operation, msg.data.message);
        printf("[Child Process] Sending message back to the client...\n");

        // We send back hello
        msg.data.message[0] = 'H';
        msg.data.message[1] = 'e';
        msg.data.message[2] = 'l';
        msg.data.message[3] = 'l';
        msg.data.message[4] = 'o';
        msg.data.message[5] = '\0';

        // We set message type as the client id
        msg.msg_type = client_id;
        msg.data.operation = 'r';

        if (msgsnd(msg_queue_id, &msg, sizeof(msg.data), 0) == -1)
        {
            perror("[Child Process] Message could not be sent, please try again");
            exit(-3);
        }
        else
        {
            printf("[Child Process] Message sent back to client %d successfully\n", client_id);
        }
    }
    else
    {
        // Parent Process
        wait(NULL);
    }
}

/**
 * @brief File Search Server
 *
 */
void file_search()
{
}

/**
 * @brief File Word Count Server
 *
 */
void file_word_count()
{
}

/**
 * @brief Cleanup
 *
 */
void cleanup()
{
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
    printf("Initializing Server...\n");

    // Create the message queue
    key_t key;
    int msg_queue_id;
    struct msg_buffer msg;

    // Link it with a key which lets you use the same key to communicate from both sides
    if ((key = ftok("README.md", 'B')) == -1)
    {
        perror("Error while generating key of the file");
        exit(-1);
    }

    // Create the message queue
    if ((msg_queue_id = msgget(key, 0644 | IPC_CREAT)) == -1)
    {
        perror("Error while connecting with Message Queue");
        exit(-1);
    }

    printf("Successfully connected to the Message Queue %d %d\n", key, msg_queue_id);

    // Listen to the message queue for new requests from the clients
    while (1)
    {
        if (msgrcv(msg_queue_id, &msg, sizeof(msg.data), 0, 0) == -1)
        {
            perror("Error while receiving message from the client");
            exit(-2);
        }
        else
        {

            // printf("Message received from Client %ld-Operation %c -> %s\n", msg.msg_type, msg.data.operation, msg.data.message);
            printf("\n");
            if (msg.data.operation == '1')
            {
                ping(msg_queue_id, msg.msg_type, msg);
            }
            else if (msg.data.operation == '2')
            {
                file_search();
            }
            else if (msg.data.operation == '3')
            {
                file_word_count();
            }
            else if (msg.data.operation == '4')
            {
                cleanup();
            }
            else if (msg.data.operation == 'r')
            {
                msg.data.operation = 'r';
                if (msgsnd(msg_queue_id, &msg, MESSAGE_LENGTH, 0) == -1)
                {
                    printf("[Server] Message added back to the queue\n");
                }
            }
        }
    }

    // Destroy the message queue
    if (msgctl(msg_queue_id, IPC_RMID, NULL) == -1)
    {
        perror("Error while destroying the message queue");
        exit(-4);
    }

    return 0;
}