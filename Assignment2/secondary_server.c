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
    int bfs_result[MAX_VERTICES];
    int bfs_result_index;
};

// Code for queue

// Queue structure
struct Queue
{
    int items[MAX_QUEUE_SIZE];
    int front;
    int rear;
};

// Function to create an empty queue
struct Queue *createQueue()
{
    struct Queue *queue = (struct Queue *)malloc(sizeof(struct Queue));
    queue->front = -1;
    queue->rear = -1;
    return queue;
}

// Function to check if the queue is empty
int isEmpty(struct Queue *queue)
{
    return queue->front == -1;
}

// Function to check if the queue is full
int isFull(struct Queue *queue)
{
    return queue->rear == MAX_QUEUE_SIZE - 1;
}

// Function to enqueue an item to the queue
void enqueue(struct Queue *queue, int value)
{
    if (isFull(queue))
    {
        printf("Queue is full. Cannot enqueue %d.\n", value);
        return;
    }

    if (isEmpty(queue))
    {
        queue->front = 0;
    }

    queue->rear++;
    queue->items[queue->rear] = value;
    printf("Enqueued %d to the queue.\n", value);
}

// Function to dequeue an item from the queue
int dequeue(struct Queue *queue)
{
    int item;

    if (isEmpty(queue))
    {
        printf("Queue is empty. Cannot dequeue.\n");
        return -1;
    }

    item = queue->items[queue->front];
    queue->front++;

    if (queue->front > queue->rear)
    {
        // Reset the queue after all elements are dequeued
        queue->front = queue->rear = -1;
    }

    printf("Dequeued %d from the queue.\n", item);
    return item;
}

// Function to get the front item of the queue without removing it
int front(struct Queue *queue)
{
    if (isEmpty(queue))
    {
        printf("Queue is empty.\n");
        return -1;
    }

    return queue->items[queue->front];
}

// Function to print the elements of the queue
void display(struct Queue *queue)
{
    int i;
    if (isEmpty(queue))
    {
        printf("Queue is empty.\n");
        return;
    }

    printf("Queue elements: ");
    for (i = queue->front; i <= queue->rear; i++)
    {
        printf("%d ", queue->items[i]);
    }
    printf("\n");
}

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
    struct Queue *queue = createQueue();
    enqueue(queue, dtt->current_vertex);
    dtt->visited[dtt->current_vertex] = 1;
    while (!isEmpty(queue))
    {
        int currentVertex = dequeue(queue);
        dtt->bfs_result[dtt->bfs_result_index++] = currentVertex; // Record the order of traversal
        // printf("[Secondary Server] BFS Thread: Current vertex: %d\n", currentVertex);
        for (int i = 0; i < dtt->number_of_nodes; i++)
        {
            if (dtt->adjacency_matrix[currentVertex][i] == 1 && dtt->visited[i] == 0)
            {
                enqueue(queue, i);
                dtt->visited[i] = 1;
            }
        }
    }
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
        fclose(fp);
        // A few checks
        dtt->visited = (int *)malloc(dtt->number_of_nodes * sizeof(int));
        dtt->visited[dtt->current_vertex] = 1;
        int startingNode = dtt->current_vertex + 1;

        printf("[Secondary Server] BFS Request: Adjacency Matrix Read Successfully\n");
        printf("[Secondary Server] BFS Request: Number of nodes: %d\n", dtt->number_of_nodes);
        printf("[Secondary Server] BFS Request: Starting vertex: %d\n", startingNode);

        pthread_t bfs_thread_id[dtt->number_of_nodes];
        if (pthread_create(&bfs_thread_id[dtt->current_vertex], NULL, bfs_subthread, (void *)dtt) != 0)
        {
            perror("[Secondary Server] Error in BFS thread creation");
            exit(EXIT_FAILURE);
        }
        // Join all BFS threads
        for (int i = 0; i < dtt->number_of_nodes; i++)
        {
            pthread_join(bfs_thread_id[i], NULL);
        }

        dtt->msg.msg_type = dtt->msg.data.seq_num;
    	dtt->msg.data.operation = 0;

    	// Assuming bfs_result is an array to store the BFS result
   	for (int i = 0; i < dtt->bfs_result_index; i++)
    	{
        	dtt->msg.data.graph_name[i] = dtt->bfs_result[i];
    	}

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

    	// Exit the BFS thread
    	printf("[Secondary Server] BFS Request: Exiting BFS Request\n");
    	printf("[Secondary Server] Successfully Completed Operation 4\n");
    	pthread_exit(NULL);
}


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
