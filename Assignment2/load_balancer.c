/**
 * @file load_balancer.c
 * @author Divyateja Pasupuleti (pro)
 * @brief
 * @version 0.1
 * @date 2023-11-02
 *
 * @copyright Copyright (c) 2023
 * POSIX-compliant C program load_balancer.c
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
            perror("[Load Balancer - Cleanup] Waitpid");

            // Destroy the message queue
            while (msgctl(msg_queue_id, IPC_RMID, NULL) == -1)
            {
                perror("[Load Balancer - Cleanup] Error while destroying the message queue");
            }

            exit(EXIT_FAILURE);
        }

        if (WIFEXITED(wstatus))
        {
            printf("[Load Balancer - Cleanup] Exited using status=%d\n", WEXITSTATUS(wstatus));
        }
        else if (WIFSIGNALED(wstatus))
        {
            printf("[Load Balancer - Cleanup] Killed using signal %d\n", WTERMSIG(wstatus));
        }
        else if (WIFSTOPPED(wstatus))
        {
            printf("[Load Balancer - Cleanup] Stopped using signal %d\n", WSTOPSIG(wstatus));
        }
        else if (WIFCONTINUED(wstatus))
        {
            printf("[Load Balancer - Cleanup] Process still continuing\n");
        }
    } while (!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus));

    // Destroy the message queue
    while (msgctl(msg_queue_id, IPC_RMID, NULL) == -1)
    {
        perror("[Load Balancer] Error while destroying the message queue");
    }

    exit(EXIT_SUCCESS);
}

/**
 * @brief The load balancer is responsible for creating the message queue to be used for
 * communication with the clients. The client will send a message to the load balancer and
 * depending on the message the load balancer will distribute the work to the servers
 *
 *
 * @return int
 */
int main()
{
    // Iniitalize the server
    printf("[Load Balancer] Initializing Load Balancer...\n");

    // Create the message queue
    key_t key;
    int msg_queue_id;
    struct msg_buffer msg;

    // Link it with a key which lets you use the same key to communicate from both sides
    if ((key = ftok(".", 'B')) == -1)
    {
        perror("[Load Balancer] Error while generating key of the file");
        exit(EXIT_FAILURE);
    }

    // Create the message queue
    if ((msg_queue_id = msgget(key, 0644 | IPC_CREAT)) == -1)
    {
        perror("[Load Balancer] Error while connecting with Message Queue");
        exit(EXIT_FAILURE);
    }

    printf("[Load Balancer] Successfully connected to the Message Queue with Key:%d ID:%d\n", key, msg_queue_id);

    // Listen to the message queue for new requests from the clients
    while (1)
    {
        if (msgrcv(msg_queue_id, &msg, sizeof(msg.data), LOAD_BALANCER_RECEIVES_ON_CHANNEL, 0) == -1)
        {
            perror("[Load Balancer] Error while receiving message from the client");
            exit(EXIT_FAILURE);
        }
        else
        {
            // Check if it's cleanup
            if (msg.data.operation == 5)
            {
                cleanup(msg_queue_id);
                exit(EXIT_SUCCESS);
            }
            // Check for sequence number is odd or even
            if (msg.data.seq_num % 2 == 0)
            {
                // Secondary Server 2
            }
            else
            {
                // Secondary Server 1
            }
        }
    }

    return 0;
}
