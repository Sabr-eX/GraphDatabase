/**
 * @file cleanup.c
 * @author Kumarasamy Chelliah
 * @brief
 * @version 0.1
 * @date 2023-09-20
 *
 * @copyright Copyright (c) 2023
 * POSIX-compliant C program cleanup.c
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

void clean(int msg_queue_id, struct msg_buffer msg_buf)
{
    while (1)
    {
        printf("Do you want the server to terminate? Press Y for 'Yes' and N for 'No': ");
        char x;
        scanf("%s", &x);
        if (x == 'Y')
        {
            msg_buf.msg_type = __INT_MAX__;
            msg_buf.data.operation = '4';
            msg_buf.data.message[0] = '\0';

            if (msgsnd(msg_queue_id, &msg_buf, sizeof(msg_buf.data), 0) == -1)
            {
                printf("[Cleanup] Message could not be sent, please try again\n");
            }
            else
            {
                printf("[Cleanup] Message sent to Main Server\n");
                exit(EXIT_FAILURE);
            }
        }
        else if (x == 'N')
        {
            continue;
        }
    }
}

int main()
{

    // Initialize the client
    printf("Initializing Client...\n");

    key_t key;
    int msg_queue_id;
    struct msg_buffer msg_buf;

    // Generate key for the message queue
    while ((key = ftok("README.md", 'B')) == -1)
    {
        printf("Error while generating key of the file");
        exit(EXIT_FAILURE);
    }

    // Connect to the messsage queue
    while ((msg_queue_id = msgget(key, 0644 | IPC_CREAT)) == -1)
    {
        printf("Error while connecting with Message Queue");
        exit(EXIT_FAILURE);
    }

    printf("Successfully connected to the Message Queue %d %d\n", key, msg_queue_id);

    clean(msg_queue_id, msg_buf);

    return 0;
}