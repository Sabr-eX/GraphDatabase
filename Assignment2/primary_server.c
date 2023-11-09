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
#include <sys/shm.h>
#include <pthread.h>

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

struct data_to_thread
{
    int msg_queue_id;
    struct msg_buffer msg;
};

/**
 * @brief This function is executed by the thread which is responsible for writing to the new graph file
 *
 * @param arg
 * @return void*
 */
void *writeToNewGraphFile(void *arg)
{
    struct data_to_thread *dtt = (struct data_to_thread *)arg;
    // On the server side for storing data, we just start with an integer
    // NOTE: Here we can use this and get away with it because we are not storing data here but only reading
    // Refer: https://man7.org/linux/man-pages/man3/shmget.3p.html
    int number_of_nodes;

    // Connect to shared memory
    key_t shm_key;
    int shm_id;
    // Generate key for the shared memory
    // Here, we are using the seq_name as the key because
    // we want to ensure that each request has a unique shared memory
    while ((shm_key = ftok(".", dtt->msg.data.seq_num)) == -1)
    {
        perror("[Primary Server] Error while generating key for shared memory");
        exit(EXIT_FAILURE);
    }
    printf("[Primary Server] Generated shared memory key %d\n", shm_key);
    // Connect to the shared memory using the key
    if ((shm_id = shmget(shm_key, sizeof(number_of_nodes), 0666)) == -1)
    {
        perror("[Primary Server] Error occurred while connecting to shm\n");
        exit(EXIT_FAILURE);
    }
    // Attach to the shared memory
    int *shmptr = (int *)shmat(shm_id, NULL, 0);
    if (shmptr == (void *)-1)
    {
        perror("[Primary Server] Error in shmat \n");
        exit(EXIT_FAILURE);
    }

    int shmptr_index = 0;
    number_of_nodes = shmptr[shmptr_index++];
    int adjacency_matrix[number_of_nodes][number_of_nodes];
    for (int i = 0; i < number_of_nodes; i++)
    {
        for (int j = 0; j < number_of_nodes; j++)
        {
            adjacency_matrix[i][j] = shmptr[shmptr_index++];
        }
    }

    // It's time to open the file and write the data to it
    FILE *fp;
    // Choose an appropriate size for your filename
    char filename[256];
    // Make sure the filename is null-terminated, and copy it to the 'filename' array
    snprintf(filename, sizeof(filename), "%s", dtt->msg.data.graph_name);
    fp = fopen(filename, "w");
    if (fp == NULL)
    {
        perror("[Primary Server] Error while opening the file");
        exit(EXIT_FAILURE);
    }
    else
    {
        // Write the data to the file
        fprintf(fp, "%d\n", number_of_nodes);
        for (int i = 0; i < number_of_nodes; i++)
        {
            for (int j = 0; j < number_of_nodes; j++)
            {
                fprintf(fp, "%d ", adjacency_matrix[i][j]);
            }
            fprintf(fp, "\n");
        }
        fclose(fp);
    }
    printf("[Primary Server] Successfully written to the file %s\n", filename);

    // Send reply to the client
    dtt->msg.msg_type = dtt->msg.data.seq_num;
    dtt->msg.data.operation = 0;
    if (msgsnd(dtt->msg_queue_id, &(dtt->msg), sizeof(dtt->msg.data), 0) == -1)
    {
        perror("[Primary Server] Message could not be sent, please try again");
        exit(EXIT_FAILURE);
    }

    // Detach from the shared memory
    if (shmdt(shmptr) == -1)
    {
        perror("[Primary Server] Could not detach from shared memory\n");
        exit(EXIT_FAILURE);
    }
    printf("[Primary Server] Successfully Completed Operation 1\n");
    pthread_exit(NULL);
}

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

    // Store the thread_ids
    pthread_t thread_ids[MAX_THREADS];

    // Listen to the message queue for new requests from the clients
    while (1)
    {
        if (msgrcv(msg_queue_id, &msg, sizeof(msg.data), PRIMARY_SERVER_CHANNEL, 0) == -1)
        {
            perror("[Primary Server] Error while receiving message from the client");
            exit(EXIT_FAILURE);
        }
        else
        {
            printf("[Primary Server] Received a message from Client: Op: %ld File Name: %s\n", msg.data.operation, msg.data.graph_name);

            if (msg.data.operation == 1)
            {
                // Write to a new file
                struct data_to_thread dtt;
                dtt.msg_queue_id = msg_queue_id;
                dtt.msg = msg;
                pthread_create(&thread_ids[msg.data.seq_num], NULL, writeToNewGraphFile, (void *)&dtt);
            }
            else if (msg.data.operation == 2)
            {
                // Modify an existing file
            }
            else if (msg.data.operation == 5)
            {
                // Cleanup
                for (int i = 0; i < 200; i++)
                {
                    pthread_join(thread_ids[i], NULL);
                }
            }
        }
    }

    return 0;
}
