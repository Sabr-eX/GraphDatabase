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

- client.c -> sends requests to the load balancer through the load balancer channel
- load_balancer.c - receives requests through load balancer channel and sends it to primary or secondary server channels
- primary_server.c -> receives requests on primary server channel and creates a thread and does the job -> write only jobs
- secondary_server.c -> same as primary but read only jobs
- cleanup.c

# Task 1: Add a new graph to the Database

- [x] Create client so that the user can choose the sequence, operation, graph file name
- [x] Check that these requests are going to the load balancer and are redirected to the primary server on the loadbalancer and primaryserver channels respectively
- [x] Write basic code to check that the primary server receives everything
- [x] Store this in shared memory in a way that it doesn't clash with other shared memories
- [x] Delete this stored memory after the request is completed
- [ ] The primary server's thread should be able to read this data and you should also keep track of threads so as to ensure that we wait for them before the main thread terminates
- [x] And send this data to the new text file
- [x] After this send a message back to the client that `File successfully added`
- [ ] Ensure that concurrency is managed properly which otherwise can cause read-write dependencies
- [ ] The parent thread should wait for the children threads to terminate

# Task 2: Modifying existing graph

- [ ] Client can request add/delete nodes/edges, so we need to perform write operation for this.
- [ ] Primary server
  - [ ] Takes this request
  - [ ] Creates a new thread
  - [ ] Reads the contents of shared memory
  - [ ] Opens the corresponding file
  - [ ] Updates the file
  - [ ] Closes the file
  - [ ] Thread sends `File successfully modified`
- [ ] Client display the message on console

# Task 3: BFS of the input graph

1. Create client. The user chooses sequence, operation, (graph file name is the same).
2. Secondary Server Tasks
   - [] Receive request from load balancer (nodes, adj matrix via message queue)
   - [] Spawn new thread to handle request
   - [] Receive starting vertex via shared memory segment
   - [] Ensure requests are redirected to the appropriate server
   - [] Error handling to ensure input is in right format
   - [] For each level, perform BFS by creating a new thread for processing the nodes of a particular level. Process nodes concurrently
   - [] Ensure parents wait for child threads to terminate
   - [] Check other error handling
   - [] Return order of vertices traversed via message queue

# Task 4: DFS of the input graph

1. The user chooses Sequence_Number, Operation_Number and Graph_File_Name.
2. Secondary Server Tasks
   - [ ] Receive request from load balancer (nodes, adj matrix via message queue)
   - [ ] Spawn new thread to handle request
   - [ ] Receive starting vertex via shared memory segment
   - [ ] Error handling to ensure input is in right format
   - [ ] For each unvisited node adjacent to the current node, perform DFS by creating a new thread for processing the nodes of each usique path from this node. Process all paths concurrently
   - [ ] Ensure parents wait for child threads to terminate
   - [ ] Check other error handling
   - [ ] Return all the leaf nodes

# Cleanup

1. The cleanup process keeps displaying Y or N menu. If Y is given as input, the process informs load balancer via single message queue that the load balancer needs to terminate. After this the cleanup process will terminate.
2. The load balancer informs all the three servers to terminate via the single message queue, sleeps for 5 seconds, waits for all threads to terminate, deletes the message queue and terminates
3. The servers perform the relevant cleanup activities and terminate.
   Note that the cleanup process will not force the load balancer to terminate while there are pending client requests. Moreover, the load balancer will not force the servers to terminate in the midst of servicing any client request or while there are pending client requests.
