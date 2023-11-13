/**
 * @file secondary_server.c
 * @author kumarasamy chelliah and anagha g
 * @brief
 * @version 0.1
 * @date 2023-11-10
 *
 * @copyright Copyright (c) 2023
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
#define MAX_VERTICES 100

/**
 * This structure, struct data, is used to store message data. It includes sequence numbers, operation codes, a graph name, and arrays for storing BFS sequence and its length.
 */
struct data
{
    long seq_num;
    long operation;
    char graph_name[MESSAGE_LENGTH];
};

/**
 * The struct msg_buffer is used for message passing through message queues. It contains a message type and the struct data structure to carry the message data.
 */
struct msg_buffer
{
    long msg_type;
    struct data data;
};

/**
 * Used to pass data to threads for BFS and dfs processing.
 * It includes a message queue ID and a message buffer.
 * Starting vertex is the vertex from which the BFS or DFS starts.
 * Storage pointer is used to store the SHM pointer address.
 * Number of nodes is the number of nodes in the graph.
 * Adjacency matrix is the adjacency matrix of the graph
 * Visited is an array to keep track of visited nodes.
 */
struct data_to_thread
{
    int msg_queue_id;
    struct msg_buffer msg;
    int starting_vertex;
    int *storage_pointer;
    int number_of_nodes;
    int **adjacency_matrix;
    int *visited;
};

void *dfsThread(void *arg)
{
    struct data_to_thread *dtt = (struct data_to_thread *)arg;

    int current_vertex = dtt->starting_vertex;
    printf("[Secondary Server] DFS Thread: Current vertex: %d\n", current_vertex);
    int *s = dtt->storage_pointer;

    int flag = 0;
    pthread_t dfs_thread_id[dtt->number_of_nodes];

    for (int i = 0; i < dtt->number_of_nodes; i++)
    {
        if ((dtt->adjacency_matrix[dtt->starting_vertex][i] == 1) && (dtt->visited[i] == 0))
        {
            flag = 1;
            dtt->visited[i] = 1;

            dtt->starting_vertex = i;
            dtt->storage_pointer = s;

            pthread_create(&dfs_thread_id[i], NULL, dfsThread, (void *)dtt);
        }
        else if ((i == dtt->number_of_nodes - 1) && (flag == 0))
        {
            *s = current_vertex;
            s++;
            *s = (int)' ';
            s++;
        }
    }

    for (int i = 0; i < dtt->number_of_nodes; i++)
        pthread_join(dfs_thread_id[i], NULL);

    pthread_exit(NULL);
}

/**
 * @brief Will be called by the main thread of the secondary server to perform DFS
 * It will find the starting vertex from the shared memory and then perform DFS
 *
 *
 * @param arg
 * @return void*
 */
void *dfs(void *arg)
{
    struct data_to_thread *dtt = (struct data_to_thread *)arg;

    // Connect to shared memory
    key_t shm_key;
    int shm_id;
    int *shmptr, *s;

    // Generate key for the shared memory
    // Here, we are using the seq_num as the key because
    // we want to ensure that each request has a unique shared memory
    while ((shm_key = ftok(".", dtt->msg.data.seq_num)) == -1)
    {
        perror("[Secondary Server] Error while generating key for shared memory");
        exit(EXIT_FAILURE);
    }
    printf("[Secondary Server] Generated shared memory key %d\n", shm_key);
    // Connect to the shared memory using the key
    if ((shm_id = shmget(shm_key, sizeof(dtt->number_of_nodes), 0666)) < 0)
    {
        perror("[Secondary Server] Error occurred while connecting to shm\n");
        exit(EXIT_FAILURE);
    }
    // Attach to the shared memory
    if ((shmptr = (int *)shmat(shm_id, NULL, 0)) == (void *)-1)
    {
        perror("[Secondary Server] Error in shmat \n");
        exit(EXIT_FAILURE);
    }

    dtt->starting_vertex = *shmptr;
    *shmptr = (int)'*';
    s = shmptr++;

    // Opening the Graph file to read the read the adjacency matrix
    FILE *fptr = fopen(dtt->msg.data.graph_name, "r");
    if (fptr == NULL)
    {
        printf("[Seconday Server] Error opening file");
        exit(EXIT_FAILURE);
    }
    fscanf(fptr, "%d", &(dtt->number_of_nodes));

    printf("[Secondary Server] DFS Request: Number of nodes: %d\n", dtt->number_of_nodes);
    printf("[Secondary Server] DFS Request: Starting vertex: %d\n", dtt->starting_vertex);

    dtt->adjacency_matrix = (int **)malloc(dtt->number_of_nodes * sizeof(int *));
    for (int i = 0; i < dtt->number_of_nodes; i++)
    {
        dtt->adjacency_matrix[i] = (int *)malloc(dtt->number_of_nodes * sizeof(int));
    }

    for (int i = 0; i < dtt->number_of_nodes; i++)
    {
        for (int j = 0; j < dtt->number_of_nodes; j++)
        {
            fscanf(fptr, "%d", &(dtt->adjacency_matrix[i][j]));
        }
    }

    printf("[Secondary Server] DFS Request: Adjacency Matrix Read Successfully\n");

    pthread_t dfs_thread_id[dtt->number_of_nodes];
    int flag = 0;
    for (int i = 0; i < dtt->number_of_nodes; i++)
    {
        if ((dtt->adjacency_matrix[dtt->starting_vertex][i] == 1) && (dtt->visited[i] == 0))
        {
            flag = 1;
            dtt->visited[i] = 1;

            dtt->starting_vertex = i;
            dtt->storage_pointer = s;

            pthread_create(&dfs_thread_id[i], NULL, dfsThread, (void *)dtt);
        }
        else if ((i == dtt->number_of_nodes - 1) && (flag == 0))
        {
            *s = dtt->starting_vertex;
            s++;
            *s = (int)' ';
            s++;
        }
    }

    for (int i = 0; i < dtt->number_of_nodes; i++)
        pthread_join(dfs_thread_id[i], NULL);

    // Exit the DFS thread
    pthread_exit(NULL);
}

int main()
{
    // Initialize the server
    printf("[Secondary Server] Initializing Secondary Server...\n");

    // Create the message queue
    key_t key;
    int msg_queue_id;
    struct msg_buffer msg;

    // Link it with a key which lets you use the same key to communicate from both sides
    if ((key = ftok(".", 'B')) == -1)
    {
        perror("[Secondary Server] Error while generating key of the file");
        exit(EXIT_FAILURE);
    }

    // Create the message queue
    if ((msg_queue_id = msgget(key, 0644)) == -1)
    {
        perror("[Secondary Server] Error while connecting with Message Queue");
        exit(EXIT_FAILURE);
    }
    printf("[Secondary Server] Successfully connected to the Message Queue with Key:%d ID:%d\n", key, msg_queue_id);

    // Store the thread_ids thread
    pthread_t thread_ids[200];

    int channel;
    printf("[Secondary Server] Enter the channel number: ");
    scanf("%d", &channel);
    if (channel == 1)
    {
        channel = SECONDARY_SERVER_CHANNEL_1;
    }
    else
    {
        channel = SECONDARY_SERVER_CHANNEL_2;
    }

    printf("[Secondary Server] Using Channel: %d\n", channel);

    // Listen to the message queue for new requests from the clients
    while (1)
    {
        struct data_to_thread *dtt = (struct data_to_thread *)malloc(sizeof(struct data_to_thread)); // Declare dtt here

        if (msgrcv(msg_queue_id, &msg, sizeof(msg.data), channel, 0) == -1)
        {
            perror("[Secondary Server] Error while receiving message from the client");
            exit(EXIT_FAILURE);
        }
        else
        {
            printf("[Secondary Server] Received a message from Client: Op: %ld File Name: %s\n", msg.data.operation, msg.data.graph_name);

            if (msg.data.operation == 3)
            {
                // Operation code for DFS request
                // Create a data_to_thread structure
                dtt->msg_queue_id = msg_queue_id;
                dtt->msg = msg;

                // Determine the channel based on seq_num
                int channel;
                if (msg.data.seq_num % 2 == 1)
                {
                    channel = SECONDARY_SERVER_CHANNEL_1;
                }
                else
                {
                    channel = SECONDARY_SERVER_CHANNEL_2;
                }

                // Set the channel in the message structure
                dtt->msg.msg_type = channel;
                // Create a new thread to handle BFS
                if (pthread_create(&thread_ids[msg.data.seq_num], NULL, dfs, (void *)&dtt) != 0)
                {
                    perror("[Secondary Server] Error in DFS thread creation");
                    exit(EXIT_FAILURE);
                }
            }
            else if (msg.data.operation == 4)
            {
            }
            else if (msg.data.operation == 5)
            {
                // Operation code for cleanup
                for (int i = 0; i < 200; i++)
                {
                    pthread_join(thread_ids[i], NULL);
                }
            }
        }
    }

    return 0;
}
