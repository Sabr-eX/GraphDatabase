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

#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>

#define MESSAGE_LENGTH 100
#define LOAD_BALANCER_CHANNEL 4000
#define PRIMARY_SERVER_CHANNEL 4001
#define SECONDARY_SERVER_CHANNEL_1 4002
#define SECONDARY_SERVER_CHANNEL_2 4003
#define MAX_THREADS 200

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
    printf("[Load Balancer] Initiating cleanup process...\n");

    // Inform servers about termination
    struct msg_buffer terminationMessage;
    terminationMessage.msg_type = PRIMARY_SERVER_CHANNEL;
    terminationMessage.data.operation = 5; // Operation code for termination

    // Send termination message to all servers
    if (msgsnd(msg_queue_id, &terminationMessage, sizeof(terminationMessage.data), 0) == -1)
    {
        perror("[Load Balancer] Error while sending cleanup message to Primary Server");
    }

    terminationMessage.msg_type = SECONDARY_SERVER_CHANNEL_1;
    if (msgsnd(msg_queue_id, &terminationMessage, sizeof(terminationMessage.data), 0) == -1)
    {
        perror("[Load Balancer] Error while sending cleanup message to Secondary Server 1");
    }

    terminationMessage.msg_type = SECONDARY_SERVER_CHANNEL_2;
    if (msgsnd(msg_queue_id, &terminationMessage, sizeof(terminationMessage.data), 0) == -1)
    {
        perror("[Load Balancer] Error while sending cleanup message to Secondary Server 2");
    }

    printf("[Load Balancer] Cleanup message sent to all servers\n");
    // Sleep for a while to allow servers to perform cleanup
    sleep(5);

    // Destroy the message queue
    if (msgctl(msg_queue_id, IPC_RMID, NULL) == -1)
    {
        perror("[Load Balancer] Error while destroying the message queue");
    }
    printf("[Load Balancer] Message queue destroyed\n");

    // Destroy all mutexes
    // Choose an appropriate size for your filename
    char filename[250];
    for (int i = 0; i <= 20; i++)
    {
        // Make sure the filename is null-terminated, and copy it to the 'filename' array
        snprintf(filename, sizeof(filename), "G%d.txt", i);

        // SEMAPHORE PART
        char sema_name_rw[256];
        snprintf(sema_name_rw, sizeof(sema_name_rw), "rw_%s", filename);
        char sema_name_read[256];
        snprintf(sema_name_read, sizeof(sema_name_read), "read_%s", filename);

        // If O_CREAT is specified, and a semaphore with the given name already exists,
        // then mode and value are ignored.
        sem_t *rw_sem = sem_open(sema_name_rw, O_CREAT, 0644, 1);
        sem_t *read_sem = sem_open(sema_name_read, O_CREAT, 0644, 1);
        sem_t *read_count = sem_open("Assignment_Read_Count", O_CREAT, 0644, 200);

        // Destroy the semaphores
        sem_close(rw_sem);
        sem_close(read_sem);
        sem_close(read_count);
    }
    printf("[Load Balancer] Semaphores destroyed\n");

    printf("[Load Balancer] Cleanup process completed. Exiting.\n");
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
        if (msgrcv(msg_queue_id, &msg, sizeof(msg.data), LOAD_BALANCER_CHANNEL, 0) == -1)
        {
            perror("[Load Balancer] Error while receiving message from the client");
            exit(EXIT_FAILURE);
        }
        else
        {
            // Print the message received
            printf("[Load Balancer] Message received from the client: %ld -> %s using Op %ld\n", msg.data.seq_num, msg.data.graph_name, msg.data.operation);
            // Check if it's cleanup
            if (msg.data.operation == 5)
            {
                cleanup(msg_queue_id);
            }
            else if (msg.data.operation == 1 || msg.data.operation == 2)
            {
                // Primary server
                msg.msg_type = PRIMARY_SERVER_CHANNEL;
                if (msgsnd(msg_queue_id, &msg, sizeof(msg.data), 0) == -1)
                {
                    perror("[Load Balancer] Error while sending message to Primary Server");
                }
                else
                {
                    printf("[Load Balancer] Received a message from Client and Sent it to Primary Server\n");
                }
            }
            else if (msg.data.operation == 3 || msg.data.operation == 4)
            {
                // Check for sequence number is odd or even
                if (msg.data.seq_num % 2 == 0)
                {
                    // Secondary Server 2
                    msg.msg_type = SECONDARY_SERVER_CHANNEL_2;
                    if (msgsnd(msg_queue_id, &msg, sizeof(msg.data), 0) == -1)
                    {
                        perror("[Load Balancer] Error while sending message to Secondary Server 2");
                    }
                    else
                        printf("[Load Balancer] Received a message from Client and Sent it to Secondary Server\n");
                }
                else
                {
                    // Secondary Server 1
                    msg.msg_type = SECONDARY_SERVER_CHANNEL_1;
                    if (msgsnd(msg_queue_id, &msg, sizeof(msg.data), 0) == -1)
                    {
                        perror("[Load Balancer] Error while sending message to Secondary Server 1");
                    }
                }
            }
            else
            {
                printf("[Load Balancer] Invalid Operation\n");
            }
        }
    }

    return 0;
}
