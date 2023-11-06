/**
 * @file primary_server.c
 * @author Divyateja Pasupuleti (pro)
 * @brief
 * @version 0.1
 * @date 2023-11-02
 *
 * @copyright Copyright (c) 2023
 * POSIX-compliant C program primary_server.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>

#define MESSAGE_LENGTH 100
#define LOAD_BALANCER_CHANNEL 1
#define PRIMARY_SERVER_CHANNEL 2
#define SECONDARY_SERVER_CHANNEL_1 3
#define SECONDARY_SERVER_CHANNEL_2 4

struct data
{
    long seq_num;
    long operation;
    char graph_name[MESSAGE_LENGTH];
};

struct msg_buffer
{
    long msg_type;
    struct data data;
};

/**
 * @brief The Primary Server is responsible all the write operations
 * and this has nothing to do with creating the message queue
 *
 *
 * @return int
 */
int main()
{
    // Iniitalize the server
    printf("[Primary Server] Initializing Primary Server...\n");

    // Create the message queue
    key_t key;
    int msg_queue_id;
    struct msg_buffer msg;

    // Link it with a key which lets you use the same key to communicate from both sides
    if ((key = ftok(".", 'B')) == -1)
    {
        perror("[Primary Server] Error while generating key of the file");
        exit(EXIT_FAILURE);
    }

    // Create the message queue
    if ((msg_queue_id = msgget(key, 0644)) == -1)
    {
        perror("[Primary Server] Error while connecting with Message Queue");
        exit(EXIT_FAILURE);
    }

    printf("[Primary Server] Successfully connected to the Message Queue with Key:%d ID:%d\n", key, msg_queue_id);

    // Listen to the message queue for new requests from the clients
    while (1)
    {
        if (msgrcv(msg_queue_id, &msg, sizeof(msg.data), PRIMARY_SERVER_CHANNEL, 0) == -1)
        {
            perror("[Load Balancer] Error while receiving message from the client");
            exit(EXIT_FAILURE);
        }
        else
        {
            printf("[Primary Server] Received a message from Client: Op: %d File Name: %s\n", msg.data.operation, msg.data.graph_name);
        }
    }

    return 0;
}
