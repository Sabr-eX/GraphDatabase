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
#define MAX_VERTICES 100

// global variables
int adjacencyMatrix[MAX_VERTICES][MAX_VERTICES];
int visited[MAX_VERTICES];
int number_of_nodes;
int starting_vertex;

// Incorporate multithreading while traversing each evel

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
 * Used to pass data to threads for BFS and dfs processing. It includes a message queue ID and a message buffer.
 */
struct data_to_thread
{
    int msg_queue_id;
    struct msg_buffer msg;
    int starting_vertex;
    int *storage_pointer;
};

/**
 * Has structure of a particular node during BFS Traversal
 */
struct BFSNode
{
    int level;
    int vertex;
};

/**
 * Stores result of the BFS
 */
struct BFSResult
{
    int vertices[MAX_VERTICES];
    int num_vertices;
};

/**
 * Standard queue DS
 */
struct Queue
{
    int node_size[MAX_VERTICES];
    int front, rear;
};

/**
 * We use this structure to pass data to individual threads. It contains a struct BFSNode for the node being processed, a struct BFSResult to store results, and a struct data_to_thread to pass message data.
 */

struct thread_data
{
    struct BFSNode node;
    struct BFSResult result;
    struct data_to_thread dtt;
};

// Array of thread ids for BFS
pthread_t thread_ids[MAX_THREADS];

// Create and Initialize queue
struct Queue *createQueue()
{
    struct Queue *queue = (struct Queue *)malloc(sizeof(struct Queue));
    queue->front = -1;
    queue->rear = -1;
    return queue;
}

int isEmpty(struct Queue *queue)
{
    return (queue->front == -1);
}

void enqueue(struct Queue *queue, int value)
{
    if (queue->rear == MAX_VERTICES - 1)
    {
        printf("Queue is full\n");
        return;
    }
    if (queue->front == -1)
    {
        queue->front = 0;
    }
    queue->node_size[queue->rear++] = value;
}

int dequeue(struct Queue *queue)
{
    if (isEmpty(queue))
    {
        printf("Queue is empty\n");
        exit(EXIT_FAILURE);
    }
    int value = queue->node_size[queue->front];
    if (queue->front == queue->rear)
    {
        queue->front = -1;
        queue->rear = -1;
    }
    else
    {
        queue->front++;
    }
    return value;
}

int readGraphFromFile(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // Read the number of nodes
    if (fscanf(file, "%d", &number_of_nodes) != 1)
    {
        perror("Error reading the number of nodes");
        exit(EXIT_FAILURE);
    }

    // Read the adjacency matrix
    for (int i = 0; i < number_of_nodes; i++)
    {
        for (int j = 0; j < number_of_nodes; j++)
        {
            if (fscanf(file, "%d", &adjacencyMatrix[i][j]) != 1)
            {
                perror("Error reading the adjacency matrix");
                exit(EXIT_FAILURE);
            }
        }
    }

    fclose(file);
    return number_of_nodes;
}

/**
 * @brief This function performs BFS for the input graph. The starting vertex is shared by the client using shared memory.
 *
 * @param arg
 * @return void*
 */

// bfs_thread function
void *bfs_thread(void *arg)
{
    struct thread_data *td = (struct thread_data *)arg;

    int level = td->node.level;
    int vertex = td->node.vertex;

    // BFS for the specific level and vertex
    struct Queue *queue = createQueue();
    enqueue(queue, vertex);
    td->result.vertices[td->result.num_vertices++] = vertex;

    while (!isEmpty(queue))
    {
        int current_vertex = dequeue(queue);

        // Process the current_vertex and add its neighbors to the queue
        for (int i = 0; i < number_of_nodes; i++)
        {
            if (adjacencyMatrix[current_vertex][i] && !visited[i])
            {
                visited[i] = 1;
                struct BFSNode next_node;
                next_node.level = level + 1;
                next_node.vertex = i;

                // Enqueue the next node for processing
                enqueue(queue, next_node.vertex);
                td->result.vertices[td->result.num_vertices++] = next_node.vertex;
            }
        }
    }

    pthread_exit(NULL);
}

void *bfs(void *arg)
{
    struct data_to_thread *dtt = (struct data_to_thread *)arg;

    // Connect to shared memory
    // Only starting vertex stored in shared memory
    key_t shm_key;
    int shm_id;

    // Generate key for the shared memory
    while ((shm_key = ftok(".", dtt->msg.data.seq_num)) == -1)
    {
        perror("[Secondary Server] Error while generating key for shared memory");
        exit(EXIT_FAILURE);
    }
    printf("[Secondary Server] Generated shared memory key %d\n", shm_key);

    // Create shared memory for starting vertex
    if ((shm_id = shmget(shm_key, sizeof(int), IPC_CREAT | 0666)) == -1)
    {
        perror("[Secondary Server] Error creating shared memory");
        exit(EXIT_FAILURE);
    }

    // Attach to the shared memory
    int *shmptr = (int *)shmat(shm_id, NULL, 0);
    if (shmptr == (void *)-1)
    {
        perror("[Secondary Server] Error in shmat \n");
        exit(EXIT_FAILURE);
    }

    // Read number of nodes and adj matrix from file
    readGraphFromFile(dtt->msg.data.graph_name);

    // Write the starting vertex to shared memory
    shmptr[0] = starting_vertex;

    // Detach from shared memory
    if (shmdt(shmptr) == -1)
    {
        perror("[Secondary Server] Could not detach from shared memory\n");
        exit(EXIT_FAILURE);
    }

    // Notify the client that shared memory is ready
    msgsnd(dtt->msg_queue_id, &dtt->msg, sizeof(dtt->msg.data), 0);

    // Exit the BFS thread
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

    int number_of_nodes;
    int starting_vertex;

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
    if ((shm_id = shmget(shm_key, sizeof(number_of_nodes), 0666)) < 0)
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

    starting_vertex = *shmptr;
    *shmptr = (int)'*';
    s = shmptr++;

    // Opening the Graph file to read the read the adjacency matrix
    FILE *fptr = fopen(dtt->msg.data.graph_name, "r");
    if (fptr == NULL)
    {
        printf("[Seconday Server] Error opening file");
        exit(EXIT_FAILURE);
    }
    fscanf(fptr, "%d", &number_of_nodes);

    int adjacencyMatrix[number_of_nodes][number_of_nodes];
    int visited[number_of_nodes];

    while (!feof(fptr))
    {
        if (ferror(fptr))
        {
            printf("[Secondary Server] Error reading file");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < number_of_nodes; i++)
        {
            for (int j = 0; j < number_of_nodes; j++)
            {
                fscanf(fptr, "%d", &adjacencyMatrix[i][j]);
            }
        }
    }

    dtt->starting_vertex = starting_vertex;
    printf("[Secondary Server] Starting vertex: %d\n", starting_vertex);
    visited[starting_vertex] = 1;

    dfsThread(&dtt);

    // Exit the DFS thread
    pthread_exit(NULL);
}

void dfsThread(void *arg)
{
    struct data_to_thread *dtt = (struct data_to_thread *)arg;
    int current_vertex = dtt->starting_vertex;
    printf("[Secondary Server] Current vertex: %d\n", current_vertex);
    int *s = dtt->storage_pointer;

    int flag = 0;
    pthread_t dfs_thread_id[number_of_nodes];
    for (int i = 0; i < number_of_nodes; i++)
    {
        if ((adjacencyMatrix[dtt->starting_vertex][i] == 1) && (visited[i] == 0))
        {
            flag = 1;
            visited[i] = 1;

            dtt->starting_vertex = i;
            dtt->storage_pointer = s;

            pthread_create(&dfs_thread_id[i], NULL, dfsThread, (void *)&dtt);
        }
        else if ((i == number_of_nodes - 1) && (flag == 0))
        {
            *s = current_vertex;
            s++;
            *s = (int)' ';
            s++;
        }
    }
    for (int i = 0; i < number_of_nodes; i++)
        pthread_join(dfs_thread_id[i], NULL);
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

    // Listen to the message queue for new requests from the clients
    while (1)
    {
        struct data_to_thread dtt; // Declare dtt here

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
                dtt.msg_queue_id = msg_queue_id;
                dtt.msg = msg;

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
                dtt.msg.msg_type = channel;
                // Create a new thread to handle BFS
                if (pthread_create(&thread_ids[msg.data.seq_num], NULL, dfs, (void *)&dtt) != 0)
                {
                    perror("[Secondary Server] Error in DFS thread creation");
                    exit(EXIT_FAILURE);
                }
            }
            else if (msg.data.operation == 4)
            {
                // Operation code for BFS request
                // Create a data_to_thread structure
                dtt.msg_queue_id = msg_queue_id;
                dtt.msg = msg;

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
                dtt.msg.msg_type = channel;
                // Create a new thread to handle BFS
                if (pthread_create(&thread_ids[msg.data.seq_num], NULL, bfs, (void *)&dtt) != 0)
                {
                    perror("[Secondary Server] Error in BFS thread creation");
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
