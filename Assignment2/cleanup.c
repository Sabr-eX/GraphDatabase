/**
 * @file cleanup.c
 * @author Divyateja Pasupuleti
 * @brief
 * @version 0.1
 * @date 2023-11-02
 *
 * @copyright Copyright (c) 2023
 * POSIX-compliant C program cleanup.c
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
#define LOAD_BALANCER_RECEIVES_ON_CHANNEL 1
#define PRIMARY_SERVER_CHANNEL 2
#define SECONDARY_SERVER_CHANNEL 3

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
 * @brief The function to contact the Main Server and instruct it to terminate gracefully
 * In this function, the cleanup process will send a message to the Main Server
 * and terminate itself. We will set operation to 5 to indicate that we wish to terminate
 * and set the msg_type to 1
 */
void clean(int msg_queue_id, struct msg_buffer msg_buf)
{
    // Asking for the user's approval to terminate
    while (1)
    {
        printf("Do you want the server to terminate? Press Y for 'Yes' and N for 'No': ");
        char x;
        scanf("%s", &x);
        if (x == 'Y' || x == 'y')
        {
            msg_buf.msg_type = LOAD_BALANCER_RECEIVES_ON_CHANNEL;
            msg_buf.data.operation = 5;

            // Sending a message to the message queue if the user wishes to exit
            if (msgsnd(msg_queue_id, &msg_buf, sizeof(msg_buf.data), 0) == -1)
            {
                printf("[Cleanup] Message could not be sent, please try again\n");
            }
            else
            {
                printf("[Cleanup] Message sent to Main Server\n");
                exit(EXIT_SUCCESS);
            }
        }
        else
        {
            continue;
        }
    }
}

int main()
{

    // Initialize the cleanup process
    printf("Initializing Cleanup process...\n");

    key_t key;
    int msg_queue_id;
    struct msg_buffer msg_buf;

    // Generate key for the message queue
    while ((key = ftok(".", 'B')) == -1)
    {
        printf("Error while generating key of the file");
        exit(EXIT_FAILURE);
    }

    // Connect to the messsage queue
    while ((msg_queue_id = msgget(key, 0644)) == -1)
    {
        printf("Error while connecting with Message Queue");
        exit(EXIT_FAILURE);
    }

    printf("Successfully connected to the Message Queue Key:%d ID:%d\n", key, msg_queue_id);

    clean(msg_queue_id, msg_buf);

    return 0;
}