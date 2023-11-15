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
#define MAX_QUEUE_SIZE 100

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
    int current_vertex;
    int index;
    int number_of_nodes;
    int **adjacency_matrix;
    int *visited;
    pthread_mutex_t mutexLock; 
    pthread_barrier_t bfs_barrier;
};

void *dfs_subthread(void *arg)
{
    struct data_to_thread *dtt = (struct data_to_thread *)arg;

    int currentVertex = dtt->current_vertex + 1;
    printf("[Secondary Server] DFS Thread: Current vertex: %d\n", currentVertex);

    int flag = 0;
    pthread_t dfs_thread_id[dtt->number_of_nodes];

    for (int i = 0; i < dtt->number_of_nodes; i++)
    {
        if ((dtt->adjacency_matrix[dtt->current_vertex][i] == 1) && (dtt->visited[i] == 0))
        {
            flag = 1;
            dtt->visited[i] = 1;

            dtt->current_vertex = i;

            pthread_create(&dfs_thread_id[i], NULL, dfs_subthread, (void *)dtt);
            pthread_join(dfs_thread_id[i], NULL);

            dtt->current_vertex = currentVertex - 1;
        }
        else if ((i == (dtt->number_of_nodes - 1)) && (flag == 0))
        {
            int leaf = dtt->current_vertex + 1;
            printf("[Secondary Server] DFS Sub Thread: New Leaf: %d\n", leaf);
            printf("[Secondary Server] DFS Sub Thread: Storing %c at Index: %d\n", (char)(leaf + 48), dtt->index);

            dtt->msg.data.graph_name[dtt->index] = (char)(leaf);
            dtt->index = dtt->index + 1;
            dtt->msg.data.graph_name[dtt->index] = '*';
        }
    }

    // Exit the DFS thread
    printf("[Secondary Server] DFS Thread: Exiting DFS Thread\n");
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
void *dfs_mainthread(void *arg)
{
    struct data_to_thread *dtt = (struct data_to_thread *)arg;

    // Connect to shared memory
    key_t shm_key;
    int shm_id;
    int *shmptr;

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

    dtt->current_vertex = *shmptr;

    // Opening the Graph file to read the read the adjacency matrix
    FILE *fptr = fopen(dtt->msg.data.graph_name, "r");
    if (fptr == NULL)
    {
        printf("[Seconday Server] Error opening file");
        exit(EXIT_FAILURE);
    }
    fscanf(fptr, "%d", &(dtt->number_of_nodes));

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
    fclose(fptr);

    dtt->visited = (int *)malloc(dtt->number_of_nodes * sizeof(int));
    dtt->visited[dtt->current_vertex] = 1;
    int startingNode = dtt->current_vertex + 1;

    printf("[Secondary Server] DFS Request: Adjacency Matrix Read Successfully\n");
    printf("[Secondary Server] DFS Request: Number of nodes: %d\n", dtt->number_of_nodes);
    printf("[Secondary Server] DFS Request: Starting vertex: %d\n", startingNode);

    pthread_t dfs_thread_id[dtt->number_of_nodes];

    int flag = 0;
    for (int i = 0; i < dtt->number_of_nodes; i++)
    {
        if ((dtt->adjacency_matrix[dtt->current_vertex][i] == 1) && (dtt->visited[i] == 0))
        {
            flag = 1;
            dtt->visited[i] = 1;

            dtt->current_vertex = i;

            pthread_create(&dfs_thread_id[i], NULL, dfs_subthread, (void *)dtt);
            pthread_join(dfs_thread_id[i], NULL);

            dtt->current_vertex = startingNode - 1;
        }
        else if ((i == (dtt->number_of_nodes - 1)) && (flag == 0))
        {
            int leaf = dtt->current_vertex + 1;
            printf("[Secondary Server] DFS Main Thread: New Leaf: %d\n", leaf);
            printf("[Secondary Server] DFS Main Thread: Storing %c at Index: %d\n", (char)(leaf + 48), dtt->index);

            dtt->msg.data.graph_name[dtt->index] = (char)(leaf);
            dtt->index = dtt->index + 1;
            dtt->msg.data.graph_name[dtt->index] = '*';
        }
    }

    dtt->msg.data.graph_name[++(dtt->index)] = '\0';

    // Send the list of Leaf Nodes to the client via message queue
    dtt->msg.msg_type = dtt->msg.data.seq_num;
    dtt->msg.data.operation = 0;

    printf("[Primary Server] Sending reply to the client %ld @ %d\n", dtt->msg.msg_type, dtt->msg_queue_id);

    if (msgsnd(dtt->msg_queue_id, &(dtt->msg), sizeof(dtt->msg.data), 0) == -1)
    {
        perror("[Secondary Server] Message could not be sent, please try again");
        exit(EXIT_FAILURE);
    }

    // Detach from the shared memory
    if (shmdt(shmptr) == -1)
    {
        perror("[Secondary Server] Could not detach from shared memory\n");
        exit(EXIT_FAILURE);
    }

    // Exit the DFS thread
    printf("[Secondary Server] DFS Request: Exiting DFS Request\n");
    printf("[Secondary Server] Successfully Completed Operation 3\n");
    pthread_exit(NULL);
}

void *bfs_subthread(void *arg)
{
    struct data_to_thread *dtt = (struct data_to_thread *)arg;
    int currentVertex = dtt->current_vertex + 1;
    printf("[Secondary Server] BFS Sub Thread: Current vertex: %d\n", currentVertex);
    //Barrier intialization
    if (pthread_barrier_init(&dtt->bfs_barrier, NULL, dtt->number_of_nodes) != 0)
    {
        perror("[Secondary Server] Error in initializing BFS barrier");
        exit(EXIT_FAILURE);
    }
    pthread_t subthread_id[dtt->number_of_nodes];
   
    pthread_mutex_lock(&dtt->mutexLock); 
    dtt->msg.data.graph_name[dtt->index] = (char)(currentVertex);
    dtt->index = dtt->index + 1;
    dtt->msg.data.graph_name[dtt->index] = '*';
    pthread_mutex_unlock(&dtt->mutexLock); 

    for (int i = 0; i < dtt->number_of_nodes; i++)
    {
        if (dtt->adjacency_matrix[dtt->current_vertex][i] == 1 && dtt->visited[i] == 0)
        {
            // Process the current node
            int node = i + 1;
            printf("[Secondary Server] BFS Thread: Processing node %d\n", node);
            dtt->visited[i] = 1;
            // dtt->bfs_result[dtt->bfs_result_index++] = node;
            pthread_create(&subthread_id[i], NULL, bfs_subthread, (void *)dtt);
            
        }
        
    }
    
    //Synchronize at barrier
    int status = pthread_barrier_wait(&dtt->bfs_barrier);
    if (status != 0 && status != PTHREAD_BARRIER_SERIAL_THREAD)
    {
        perror("[Secondary Server] Error in barrier synchronization");
        exit(EXIT_FAILURE);
    }

    // Process nodes at the next level with subthreads
    for (int i = 0; i < dtt->number_of_nodes; i++)
    {
        pthread_join(subthread_id[i], NULL);
    }
    
    pthread_barrier_destroy(&dtt->bfs_barrier);
    // Exit the BFS thread
    printf("[Secondary Server] BFS Thread: Exiting BFS Thread\n");
    pthread_exit(NULL);
}

void *bfs_mainthread(void *arg)
{
    struct data_to_thread *dtt = (struct data_to_thread *)arg;
    // Connect to shared memory to retrieve the starting vertex
    key_t shm_key;
    int shm_id;
    while ((shm_key = ftok(".", dtt->msg.data.seq_num)) == -1)
    {
        perror("[Secondary Server] Error while generating key for shared memory");
        exit(EXIT_FAILURE);
    }
    printf("[Secondary Server] Generated shared memory key %d\n", shm_key);
    // Connect to the shared memory using the key
    if ((shm_id = shmget(shm_key, sizeof(dtt->number_of_nodes), 0666)) == -1)
    {
        perror("[Secondary Server] Error occurred while connecting to shm\n");
        exit(EXIT_FAILURE);
    }
    // Attach to the shared memory
    int *shmptr = (int *)shmat(shm_id, NULL, 0);
    if (shmptr == (void *)-1)
    {
        perror("[Secondary Server] Error in shmat \n");
        exit(EXIT_FAILURE);
    }
   
    dtt->current_vertex = *shmptr;

    FILE *fp;
    char filename[250];
    snprintf(filename, sizeof(filename), "%s", dtt->msg.data.graph_name);
    fp = fopen(filename, "r");
    if (fp == NULL)
    {
        perror("[Secondary Server] Error while opening the file");
        exit(EXIT_FAILURE);
    }
    fscanf(fp, "%d", &(dtt->number_of_nodes));

    dtt->adjacency_matrix = (int **)malloc(dtt->number_of_nodes * sizeof(int *));
    for (int i = 0; i < dtt->number_of_nodes; i++)
    {
        dtt->adjacency_matrix[i] = (int *)malloc(dtt->number_of_nodes * sizeof(int));
    }
    for (int i = 0; i < dtt->number_of_nodes; i++)
    {
        for (int j = 0; j < dtt->number_of_nodes; j++)
        {
            fscanf(fp, "%d ", &(dtt->adjacency_matrix[i][j]));
        }
    }
    fclose(fp);

    

    // A few checks
    dtt->visited = (int *)malloc(dtt->number_of_nodes * sizeof(int));
    dtt->visited[dtt->current_vertex] = 1;
    int startingNode = dtt->current_vertex + 1;

    printf("[Secondary Server] BFS Request: Adjacency Matrix Read Successfully\n");
    printf("[Secondary Server] BFS Request: Number of nodes: %d\n", dtt->number_of_nodes);
    printf("[Secondary Server] BFS Request: Starting vertex: %d\n", startingNode);

    printf("[Secondary Server] BFS Main Thread: Starting BFS for graph %s\n", dtt->msg.data.graph_name);
    
    //Barrier intialization
    if (pthread_barrier_init(&dtt->bfs_barrier, NULL, dtt->number_of_nodes) != 0)
    {
        perror("[Secondary Server] Error in initializing BFS barrier");
        exit(EXIT_FAILURE);
    }

    pthread_t subthread_id[dtt->number_of_nodes];
    
    pthread_mutex_lock(&dtt->mutexLock); 
    dtt->msg.data.graph_name[dtt->index] = (char)(startingNode);
    dtt->index = dtt->index + 1;
    dtt->msg.data.graph_name[dtt->index] = '*';
    pthread_mutex_unlock(&dtt->mutexLock); 
    for (int i = 0; i < dtt->number_of_nodes; i++)
    {
        if (dtt->adjacency_matrix[dtt->current_vertex][i] == 1 && dtt->visited[i] == 0)
        {
            // Process the current node
            int node = i + 1;
            printf("[Secondary Server] BFS Thread: Processing node %d\n", node);
            dtt->visited[i] = 1;

            // dtt->bfs_result[dtt->bfs_result_index++] = node;
            pthread_create(&subthread_id[i], NULL, bfs_subthread, (void *)dtt);
            
        }
        
    }

    dtt->msg.data.graph_name[++(dtt->index)] = '\0';
    //Synchrnize at barrier
    int status = pthread_barrier_wait(&dtt->bfs_barrier);
    if (status != 0 && status != PTHREAD_BARRIER_SERIAL_THREAD)
    {
        perror("[Secondary Server] Error in barrier synchronization");
        exit(EXIT_FAILURE);
    }

    // Process nodes at the next level with subthreads
    for (int i = 0; i < dtt->number_of_nodes; i++)
    {
        pthread_join(subthread_id[i], NULL);
    }

    dtt->msg.msg_type = dtt->msg.data.seq_num;
    dtt->msg.data.operation = 0;
    // memcpy(dtt->msg.data.graph_name, dtt->bfs_result, dtt->bfs_result_index * sizeof(int));
    printf("[Secondary Server] Sending reply to the client %ld @ %d\n", dtt->msg.msg_type, dtt->msg_queue_id);

    if (msgsnd(dtt->msg_queue_id, &(dtt->msg), sizeof(dtt->msg.data), 0) == -1)
    {
        perror("[Secondary Server] Message could not be sent, please try again");
        exit(EXIT_FAILURE);
    }

    // Detach from the shared memory
    if (shmdt(shmptr) == -1)
    {
        perror("[Secondary Server] Could not detach from shared memory\n");
        exit(EXIT_FAILURE);
    }
    
    pthread_barrier_destroy(&dtt->bfs_barrier);

    // Exit the BFS thread
    printf("[Secondary Server] BFS Request: Exiting BFS Request\n");
    printf("[Secondary Server] Successfully Completed Operation 4\n");
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
                dtt->index = 0;
                // Create a new thread to handle BFS
                if (pthread_create(&thread_ids[msg.data.seq_num], NULL, dfs_mainthread, (void *)dtt) != 0)
                {
                    perror("[Secondary Server] Error in DFS thread creation");
                    exit(EXIT_FAILURE);
                }
            }
            else if (msg.data.operation == 4)
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
                dtt->index = 0;
                // Create a new thread to handle BFS
                if (pthread_create(&thread_ids[msg.data.seq_num], NULL, bfs_mainthread, (void *)dtt) != 0)
                {
                    perror("[Secondary Server] Error in DFS thread creation");
                    exit(EXIT_FAILURE);
                }
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
