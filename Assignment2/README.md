# OPERATING SYSTEMS (CS F372) 2023-2024 ASSIGNMENT 2 - 45 Marks

# Base Data Structure

```c
#define MESSAGE_LENGTH 100
#define LOAD_BALANCER_CHANNEL 1
#define PRIMARY_SERVER_CHANNEL 2
#define SECONDARY_SERVER_CHANNEL_1 3
#define SECONDARY_SERVER_CHANNEL_2 4

struct data
{
    long client_id;
    long seq_num;
    long operation;
    char graph_name[MESSAGE_LENGTH];
};

struct msg_buffer
{
    long msg_type;
    struct data data;
};
```

# Base Tasks

-   client.c -> sends requests to the load balancer through the load balancer channel
-   load_balancer.c - receives requests through load balancer channel and sends it to primary or secondary server channels
-   primary_server.c -> receives requests on primary server channel and creates a thread and does the job -> write only jobs
-   secondary_server.c -> same as primary but read only jobs
-   cleanup.c

# Task 1: Add a new graph to the Database

1. Create client so that the user can choose the sequence, operation, graph file name
2. Check that these requests are going to the load balancer and are redirected to the primary server on the loadbalancer and primaryserver channels respectively
3. Write basic code to check that the primary server receives everything
4. Ensure that the input is taken from the user for number of nodes and the adjacency matrix itself.
5. Store this in shared memory in a way that it doesn't clash with other shared memories
6. Delete this stored memory after the request is completed
7. The primary server's thread should be able to read this data and you should also keep track of threads so as to ensure that we wait for them before the main thread terminates
8. And send this data to the new text file
9. After this send a message back to the client that `File successfully added`
10. Ensure that concurrency is managed properly which otherwise can cause read-write dependencies
11. The parent thread should wait for the children threads to terminate

# Task 2: Modifying existing graph
1. Client can request add/delete nodes/edges, so we need to performa write operation for this.
2. Primary server
   1. Takes this request
   2. Creates a new thread
   3. Reads the contents of shared memory
   4. Opens the corresponding file
   5. Updates the file
   6. Closes the file
   7. Thread sends `File successfully modified`
3. Client display the message on console

# Cleanup
1. The cleanup process keeps displaying Y or N menu. If Y is given as input, the process informs load balancer via single message queue that the load balancer needs to terminate. After this the cleanup process will terminate.
2. The load balancer informs all the three servers to terminate via the single message queue, sleeps for 5 seconds, waits for all threads to terminate, deletes the message queue and terminates
3. The servers perform the relevant cleanup activities and terminate.
Note that the cleanup process will not force the load balancer to terminate while there are pending client requests. Moreover, the load balancer will not force the servers to terminate in the midst of servicing any client request or while there are pending client requests.

