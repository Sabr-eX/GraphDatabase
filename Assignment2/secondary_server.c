/**
 * @file secondary_server.c
 * @author Kumarasamy Chelliah and anagha g
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

/*
 * Implementation of Queue
 */
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
    if (queue == NULL)
    {
        fprintf(stderr, "Memory allocation failed. Exiting program.\n");
        exit(EXIT_FAILURE);
    }
    queue->front = -1;
    queue->rear = -1;
    return queue;
}

int isEmpty(struct Queue *q)
{
    return q->front == -1;
}

int isFull(struct Queue *q)
{
    return q->rear == MAX_QUEUE_SIZE - 1;
}

void enqueue(struct Queue *q, int value)
{
    if (isFull(q))
    {
        printf("Queue is full. Cannot enqueue.\n");
        return;
    }

    if (isEmpty(q))
    {
        q->front = 0;
    }

    q->rear++;
    q->items[q->rear] = value;
    printf("Enqueued: %d\n", value);
}

int dequeue(struct Queue *q)
{
    int value;

    if (isEmpty(q))
    {
        printf("Queue is empty. Cannot dequeue.\n");
        return -1;
    }

    value = q->items[q->front];

    if (q->front == q->rear)
    {
        q->front = q->rear = -1;
    }
    else
    {
        q->front++;
    }

    printf("Dequeued: %d\n", value);
    return value;
}

void display(struct Queue *q)
{
    if (isEmpty(q))
    {
        printf("Queue is empty.\n");
        return;
    }

    printf("Queue elements: ");
    for (int i = q->front; i <= q->rear; i++)
    {
        printf("%d ", q->items[i]);
    }
    printf("\n");
}

int queueSize(struct Queue *q)
{
    if (isEmpty(q))
    {
        return 0;
    }
    else
    {
        return q->rear - q->front + 1;
    }
}

/**
 * Used to pass data to threads for BFS and dfs processing.
 * It includes a message queue ID and a message buffer.
 * Index is the index at which the next vertex number should be entered into graph_name[]
 * Number of nodes is the number of nodes in the graph.
 * Adjacency matrix is the adjacency matrix of the graph
 * Visited is an array to keep track of visited nodes.
 * Mutexlock to keep track of when we are editing the output i.e. graph_name[]
 * QueueLock to keep track of when BFS threads are editing the queue
 * Current Vertex to keep track of current vertex
 * BFS Queue is the queue used in BFS
 */
struct data_to_thread
{
    int *msg_queue_id;
    struct msg_buffer *msg;
    int *index;
    int *number_of_nodes;
    int **adjacency_matrix;
    int *visited;
    pthread_mutex_t *mutexLock;
    pthread_mutex_t *queueLock;
    int current_vertex;
    struct Queue *bfs_queue;
};

/**
 * @brief Called by the thread on creation. Every child spawns the thread and calls this function for DFA task.
 * 
 * @param arg 
 * @return void* 
 */
void *dfs_subthread(void *arg)
{
    struct data_to_thread *dtt = (struct data_to_thread *)arg;

    int currentVertex = dtt->current_vertex + 1;
    printf("[Secondary Server] DFS Sub Thread: Current vertex: %d\n", currentVertex);

    int flag = 0;
    pthread_t dfs_thread_id[*dtt->number_of_nodes];
    int threads[*dtt->number_of_nodes];
    int threadIndex = 0;

    for (int i = 0; i < *dtt->number_of_nodes; i++)
    {
        if ((dtt->adjacency_matrix[dtt->current_vertex][i] == 1) && (dtt->visited[i] == 0))
        {
            flag = 1;
            dtt->visited[i] = 1;

            struct data_to_thread *newdtt = malloc(sizeof(struct data_to_thread));
            *newdtt = *dtt;
            newdtt->current_vertex = i;

            pthread_create(&dfs_thread_id[i], NULL, dfs_subthread, (void *)newdtt);
            threads[threadIndex++] = i;
        }
        else if ((i == (*dtt->number_of_nodes - 1)) && (flag == 0))
        {
            int leaf = dtt->current_vertex + 1;
            printf("[Secondary Server] DFS Sub Thread: New Leaf: %d\n", leaf);
            printf("[Secondary Server] DFS Sub Thread: Storing %d at Index: %d\n", leaf, *dtt->index);

            pthread_mutex_lock(dtt->mutexLock);
            dtt->msg->data.graph_name[*dtt->index] = (char)(leaf);
            *dtt->index = *dtt->index + 1;
            dtt->msg->data.graph_name[*dtt->index] = '*';
            pthread_mutex_unlock(dtt->mutexLock);
        }
    }

    // Join all the subthreads
    for (int i = 0; i < threadIndex; i++)
    {
        pthread_join(dfs_thread_id[threads[i]], NULL);
    }

    // Exit the DFS thread
    printf("[Secondary Server] DFS Sub Thread: Exiting DFS Thread\n");
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
    while ((shm_key = ftok(".", dtt->msg->data.seq_num)) == -1)
    {
        perror("[Secondary Server] DFS Main Thread: Error while generating key for shared memory");
        exit(EXIT_FAILURE);
    }
    printf("[Secondary Server] Generated shared memory key %d\n", shm_key);

    // Connect to the shared memory using the key
    if ((shm_id = shmget(shm_key, sizeof(int), 0666)) < 0)
    {
        perror("[Secondary Server] DFS Main Thread: Error occurred while connecting to shm\n");
        exit(EXIT_FAILURE);
    }
    // Attach to the shared memory
    if ((shmptr = (int *)shmat(shm_id, NULL, 0)) == (void *)-1)
    {
        perror("[Secondary Server] DFS Main Thread: Error in shmat \n");
        exit(EXIT_FAILURE);
    }

    // Take input of vertex from shared memory
    dtt->current_vertex = *shmptr;

    // Choose an appropriate size for your filename
    char filename[250];
    // Make sure the filename is null-terminated, and copy it to the 'filename' array
    snprintf(filename, sizeof(filename), "%s", dtt->msg->data.graph_name);
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

    printf("[Secondary Server] Waiting for the semaphore to be available\n");
    sem_wait(read_sem);
    int current_readers = 0;
    sem_getvalue(&read_count, &current_readers);
    if (current_readers == 1)
        sem_wait(rw_sem);
    sem_post(read_sem);

    FILE *fptr = fopen(filename, "r");
    if (fptr == NULL)
    {
        printf("[Seconday Server] DFS Main Thread: Error opening file");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("[Secondary Server] Successfully opened the file %s\n", filename);
        fscanf(fptr, "%d", dtt->number_of_nodes);

        // Allocate space for adjacency matrix
        dtt->adjacency_matrix = (int **)malloc((*dtt->number_of_nodes) * sizeof(int *));
        for (int i = 0; i < (*dtt->number_of_nodes); i++)
        {
            dtt->adjacency_matrix[i] = (int *)malloc((*dtt->number_of_nodes) * sizeof(int));
        }
        for (int i = 0; i < (*dtt->number_of_nodes); i++)
        {
            for (int j = 0; j < (*dtt->number_of_nodes); j++)
            {
                fscanf(fptr, "%d", &(dtt->adjacency_matrix[i][j]));
            }
        }
        fclose(fptr);
    }

    printf("[Secondary Server] Releasing the semaphore\n");

    sem_wait(read_sem);
    sem_getvalue(&read_count, &current_readers);
    current_readers--;
    if (current_readers == 0)
        sem_post(rw_sem);
    sem_post(read_sem);

    // Allocate space for visited array
    dtt->visited = (int *)malloc((*dtt->number_of_nodes) * sizeof(int));
    for (int i = 0; i < *dtt->number_of_nodes; i++)
    {
        dtt->visited[i] = 0;
    }
    dtt->visited[dtt->current_vertex] = 1;
    int startingNode = dtt->current_vertex + 1;

    // Debug logs
    printf("[Secondary Server] DFS Main Thread: Adjacency Matrix Read Successfully\n");
    printf("[Secondary Server] DFS Main Thread: Number of nodes: %d\n", *dtt->number_of_nodes);
    printf("[Secondary Server] DFS Main Thread: Starting vertex: %d\n", startingNode);

    // All the subthreads
    pthread_t dfs_thread_id[*dtt->number_of_nodes];
    int threads[*dtt->number_of_nodes];
    int threadIndex = 0;
    int currentVertex = dtt->current_vertex;

    // flag variable to check if it's leaf or not
    int flag = 0;
    for (int i = 0; i < (*dtt->number_of_nodes); i++)
    {
        if ((dtt->adjacency_matrix[currentVertex][i] == 1) && (dtt->visited[i] == 0))
        {
            flag = 1;
            dtt->visited[i] = 1;

            struct data_to_thread *newdtt = malloc(sizeof(struct data_to_thread));
            *newdtt = *dtt;
            newdtt->current_vertex = i;

            pthread_create(&dfs_thread_id[i], NULL, dfs_subthread, (void *)newdtt);
            threads[threadIndex++] = i;
        }
        else if ((i == ((*dtt->number_of_nodes) - 1)) && (flag == 0))
        {
            int leaf = dtt->current_vertex + 1;
            printf("[Secondary Server] DFS Main Thread: New Leaf: %d\n", leaf);
            printf("[Secondary Server] DFS Main Thread: Storing %d at Index: %d\n", leaf, *dtt->index);

            pthread_mutex_lock(dtt->mutexLock);
            dtt->msg->data.graph_name[*dtt->index] = (char)(leaf);
            *dtt->index = *dtt->index + 1;
            dtt->msg->data.graph_name[*dtt->index] = '*';
            pthread_mutex_unlock(dtt->mutexLock);
        }
    }

    // Join all subthreads
    for (int i = 0; i < threadIndex; i++)
    {
        pthread_join(dfs_thread_id[threads[i]], NULL);
    }

    dtt->msg->data.graph_name[++(*dtt->index)] = '\0';

    // Send the list of Leaf Nodes to the client via message queue
    dtt->msg->msg_type = dtt->msg->data.seq_num;
    dtt->msg->data.operation = 0;

    printf("[Secondary Server] DFS Main Thread: Sending reply to the client %ld @ %d\n", dtt->msg->msg_type, *dtt->msg_queue_id);

    if (msgsnd(*dtt->msg_queue_id, dtt->msg, sizeof(struct data), 0) == -1)
    {
        perror("[Secondary Server] DFS Main Thread: Message could not be sent, please try again");
        exit(EXIT_FAILURE);
    }

    // Detach from the shared memory
    if (shmdt(shmptr) == -1)
    {
        perror("[Secondary Server] DFS Main Thread: Could not detach from shared memory\n");
        exit(EXIT_FAILURE);
    }
    printf("[Secondary Server] DFS Main Thread: Freeing dtt\n");
    free(dtt);
    // Destroy mutexLock
    if (pthread_mutex_destroy(dtt->mutexLock) != 0)
    {
        printf("[Secondary Server] BFS Main Thread: Error destroying mutexLock");
    }
    // Exit the DFS thread
    printf("[Secondary Server] DFS Main Thread: Exiting DFS Request\n");
    printf("[Secondary Server] Successfully Completed Operation 3\n");
    pthread_exit(NULL);
}

/**
 * @brief Called by the thread on creation. Every child spawns the thread and calls this function for BFS task.
 * 
 * @param arg 
 * @return void* 
 */
void *bfs_subthread(void *arg)
{
    struct data_to_thread *dtt = (struct data_to_thread *)arg;

    //  Lock
    pthread_mutex_lock(dtt->mutexLock);
    int node = dtt->current_vertex + 1;
    dtt->msg->data.graph_name[*dtt->index] = (char)node;
    *dtt->index = *dtt->index + 1;
    dtt->msg->data.graph_name[*dtt->index] = '*';

    // Unlock
    pthread_mutex_unlock(dtt->mutexLock);
    dtt->visited[dtt->current_vertex] = 1;

    // Loop
    for (int i = 0; i < *dtt->number_of_nodes; i++)
    {
        if ((dtt->adjacency_matrix[dtt->current_vertex][i] == 1) && (dtt->visited[i] == 0))
        {
            pthread_mutex_lock(dtt->queueLock);
            enqueue((dtt->bfs_queue), i);
            pthread_mutex_unlock(dtt->queueLock);
        }
    }

    // Exit
    printf("[Secondary Server] BFS Sub Thread: Exiting...");
    pthread_exit(NULL);
}

/**
 * @brief Called by the main thread of secondary server for BFS task. Uses the starting vertex from the shared memory and performs dfs.
 * 
 * @param arg 
 * @return void* 
 */
void *bfs_mainthread(void *arg)
{
    struct data_to_thread *dtt = (struct data_to_thread *)arg;

    // Connect to shared memory
    key_t shm_key;
    int shm_id;
    int *shmptr;

    // Generate key for the shared memory
    // Here, we are using the seq_num as the key because
    // we want to ensure that each request has a unique shared memory
    while ((shm_key = ftok(".", dtt->msg->data.seq_num)) == -1)
    {
        perror("[Secondary Server] BFS Main Thread: Error while generating key for shared memory");
        exit(EXIT_FAILURE);
    }
    printf("[Secondary Server] BFS Main Thread: Generated shared memory key %d\n", shm_key);

    // Connect to the shared memory using the key
    if ((shm_id = shmget(shm_key, sizeof(int), 0666)) < 0)
    {
        perror("[Secondary Server] BFS Main Thread: Error occurred while connecting to shm\n");
        exit(EXIT_FAILURE);
    }
    // Attach to the shared memory
    if ((shmptr = (int *)shmat(shm_id, NULL, 0)) == (void *)-1)
    {
        perror("[Secondary Server] BFS Main Thread: Error in shmat \n");
        exit(EXIT_FAILURE);
    }
    dtt->current_vertex = *shmptr;

    // Choose an appropriate size for your filename
    char filename[250];
    // Make sure the filename is null-terminated, and copy it to the 'filename' array
    snprintf(filename, sizeof(filename), "%s", dtt->msg->data.graph_name);
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

    printf("[Secondary Server] Waiting for the semaphore to be available\n");
    sem_wait(read_sem);
    int current_readers = 0;
    sem_getvalue(&read_count, &current_readers);
    if (current_readers == 1)
        sem_wait(rw_sem);
    sem_post(read_sem);

    // Opening the Graph file to read the read the adjacency matrix
    FILE *fptr = fopen(dtt->msg->data.graph_name, "r");
    if (fptr == NULL)
    {
        printf("[Seconday Server] BFS Main Thread: Error opening file");
        exit(EXIT_FAILURE);
    }
    fscanf(fptr, "%d", dtt->number_of_nodes);

    // Allocate space for adjacency matrix
    dtt->adjacency_matrix = (int **)malloc((*dtt->number_of_nodes) * sizeof(int *));
    for (int i = 0; i < (*dtt->number_of_nodes); i++)
    {
        dtt->adjacency_matrix[i] = (int *)malloc((*dtt->number_of_nodes) * sizeof(int));
    }
    for (int i = 0; i < (*dtt->number_of_nodes); i++)
    {
        for (int j = 0; j < (*dtt->number_of_nodes); j++)
        {
            fscanf(fptr, "%d", &(dtt->adjacency_matrix[i][j]));
        }
    }
    fclose(fptr);

    printf("[Secondary Server] Releasing the semaphore\n");

    sem_wait(read_sem);
    sem_getvalue(&read_count, &current_readers);
    current_readers--;
    if (current_readers == 0)
        sem_post(rw_sem);
    sem_post(read_sem);

    dtt->visited = (int *)malloc(*dtt->number_of_nodes * sizeof(int));
    for (int i = 0; i < *dtt->number_of_nodes; i++)
    {
        dtt->visited[i] = 0;
    }

    int starting_vertex = dtt->current_vertex + 1;
    dtt->visited[dtt->current_vertex] = 1;
    // Debugging
    printf("[Secondary Server] BFS Main Thread: Adjacency Matrix Read Successfully\n");
    printf("[Secondary Server] BFS Main Thread: Number of nodes: %d\n", *dtt->number_of_nodes);
    printf("[Secondary Server] BFS Main Thread: Starting vertex: %d\n", starting_vertex);

    // int currentVertex= dtt->current_vertex;
    // push starting vertex into queue
    // dtt->bfs_queue= createQueue();
    enqueue((dtt->bfs_queue), dtt->current_vertex);
    // printf("Entry in queue is: %d", starting_vertex);

    while (!isEmpty((dtt->bfs_queue)))
    {
        int entry = 0;
        int queue_size = queueSize((dtt->bfs_queue));
        printf("Queue size: %d\n", queue_size);
        int array[queue_size];
        // Copy entries into array, empty queue

        while (!isEmpty(dtt->bfs_queue))
        {
            array[entry++] = dequeue((dtt->bfs_queue));
        }

        pthread_t subthread_ids[queue_size];
        int threads[queue_size];
        int threadIndex = 0;

        for (int i = 0; i < queue_size; i++)
        {
            struct data_to_thread *newdtt = malloc(sizeof(struct data_to_thread));
            *newdtt = *dtt;
            newdtt->current_vertex = array[i];
            pthread_create(&subthread_ids[i], NULL, bfs_subthread, (void *)newdtt);
            threads[threadIndex++] = i;
            // free(newdtt);
        }

        // Join all the subthreads
        for (int i = 0; i < threadIndex; i++)
        {
            pthread_join(subthread_ids[threads[i]], NULL);
        }
    }

    // dtt->msg->data.graph_name[++(*dtt->index)] = '\0';
    // Sending shit to client
    dtt->msg->msg_type = dtt->msg->data.seq_num;
    dtt->msg->data.operation = 0;

    printf("[Secondary Server] BFS Main Thread: Sending reply to the client %ld @ %d\n", dtt->msg->msg_type, *dtt->msg_queue_id);

    if (msgsnd(*dtt->msg_queue_id, dtt->msg, sizeof(struct data), 0) == -1)
    {
        perror("[Secondary Server] BFS Main Thread: Message could not be sent, please try again");
        exit(EXIT_FAILURE);
    }

    // Detach from the shared memory
    if (shmdt(shmptr) == -1)
    {
        perror("[Secondary Server] BFS Main Thread: Could not detach from shared memory\n");
        exit(EXIT_FAILURE);
    }
    printf("[Secondary Server] BFS Main Thread: Freeing dtt\n");
    free(dtt);

    // Destroy mutexLock
    if (pthread_mutex_destroy(dtt->mutexLock) != 0)
    {
        printf("[Secondary Server] BFS Main Thread: Error destroying mutexLock");
    }

    // Destroy queueLock
    if (pthread_mutex_destroy(dtt->queueLock) != 0)
    {
        printf("[Secondary Server] BFS Main Thread: Error destroying queueLock");
    }

    // Exit the BFS thread
    printf("[Secondary Server] BFS Main Thread: Exiting...\n");
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
    int thread_counter = 0;
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
                dtt->msg_queue_id = (int *)malloc(sizeof(int));
                dtt->msg = (struct msg_buffer *)malloc(sizeof(struct msg_buffer));
                dtt->index = (int *)malloc(sizeof(int));
                *dtt->index = 0;

                dtt->number_of_nodes = (int *)malloc(sizeof(int));
                dtt->mutexLock = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
                if (pthread_mutex_init(dtt->mutexLock, NULL) != 0)
                {
                    perror("[Secondary Server] Error initializing mutexLock");
                    exit(EXIT_FAILURE);
                }

                *dtt->msg_queue_id = msg_queue_id;
                dtt->msg = &msg;

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
                dtt->msg->msg_type = channel;

                // Create a new thread to handle BFS
                if (pthread_create(&thread_ids[msg.data.seq_num], NULL, dfs_mainthread, (void *)dtt) != 0)
                {
                    perror("[Secondary Server] Error in DFS thread creation");
                    exit(EXIT_FAILURE);
                }
                thread_counter++;
            }
            else if (msg.data.operation == 4)
            {
                // Operation code for BFS request
                // Create a data_to_thread structure
                dtt->msg_queue_id = (int *)malloc(sizeof(int));
                dtt->msg = (struct msg_buffer *)malloc(sizeof(struct msg_buffer));
                dtt->index = (int *)malloc(sizeof(int));
                *dtt->index = 0;
                dtt->number_of_nodes = (int *)malloc(sizeof(int));
                dtt->mutexLock = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
                if (pthread_mutex_init(dtt->mutexLock, NULL) != 0)
                {
                    perror("[Secondary Server] Error initializing mutexLock");
                    exit(EXIT_FAILURE);
                }
                dtt->queueLock = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
                if (pthread_mutex_init(dtt->queueLock, NULL) != 0)
                {
                    perror("[Secondary Server] Error initializing queueLock");
                    exit(EXIT_FAILURE);
                }
                dtt->bfs_queue = createQueue();

                *dtt->msg_queue_id = msg_queue_id;
                dtt->msg = &msg;

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
                dtt->msg->msg_type = channel;

                // Create a new thread to handle BFS
                if (pthread_create(&thread_ids[msg.data.seq_num], NULL, bfs_mainthread, (void *)dtt) != 0)
                {
                    perror("[Secondary Server] Error in BFS thread creation");
                    exit(EXIT_FAILURE);
                }
                thread_counter++;
            }
            else if (msg.data.operation == 5)
            {
                // Operation code for cleanup
                for (int i = 1; i <= thread_counter; i++)
                {
                    printf("Attempting to Clean: %d %lu\n", i, thread_ids[i]);
                    if (thread_ids[i] != 0)
                    {
                        if (pthread_join(thread_ids[i], NULL) != 0)
                        {
                            perror("[Secondary Server] Error joining thread");
                        }
                    }
                }

                printf("[Secondary Server] Terminating...\n");
                exit(EXIT_SUCCESS);
            }
        }
    }

    return 0;
}
