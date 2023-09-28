/**
 * @file server.c
 * @author Divyateja Pasupuleti (pro)
 * @brief
 * @version 0.1
 * @date 2023-09-20
 *
 * @copyright Copyright (c) 2023
 * POSIX-compliant C program server.c
 *
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MESSAGE_LENGTH 100

struct data {
    char message[MESSAGE_LENGTH];
    char operation;
};

struct msg_buffer {
    long msg_type;
    struct data data;
};

/**
 * @brief Ping Server
 *
 */
void ping(int msg_queue_id, int client_id, struct msg_buffer msg) {
    printf("[Child Process: Ping] Message received from Client %ld-Operation %c -> %s\n", msg.msg_type, msg.data.operation, msg.data.message);
    sleep(5);
    printf("[Child Process: Ping] Sending message back to the client...\n");

    // We send back hello
    msg.data.message[0] = 'H';
    msg.data.message[1] = 'e';
    msg.data.message[2] = 'l';
    msg.data.message[3] = 'l';
    msg.data.message[4] = 'o';
    msg.data.message[5] = '\0';

    // We set message type as the client id
    msg.msg_type = client_id;
    msg.data.operation = 'r';

    if (msgsnd(msg_queue_id, &msg, sizeof(msg.data), 0) == -1) {
        perror("[Child Process: Ping] Message could not be sent, please try again");
        exit(-3);
    } else {
        printf("[Child Process: Ping] Message sent back to client %d successfully\n", client_id);
    }
}

/**
 * @brief File Search Server. Uses 'find' function to get output.
 *
 */
void file_search(const char *filename, int msg_queue_id, int client_id, struct msg_buffer msg) {
    int link[2];
    pid_t pid;
    char output[4096];  // Read the filename from user input

    if (pipe(link) == -1) {
        fprintf(stderr, "%s\n", "[Child Process: File Search] Error in pipe creation");
        exit(EXIT_FAILURE);
    }

    if ((pid = fork()) == -1) {
        fprintf(stderr, "%s\n", "[Child Process: File Search] Error in fork creation");
        exit(EXIT_FAILURE);
    }

    // Child process
    if (pid == 0) {
        dup2(link[1], STDOUT_FILENO);
        close(link[0]);
        close(link[1]);
        fprintf(stderr, "[Child Process: File Search] Entered filename: %s\n", filename);
        execlp("find", "find", ".", "-name", filename, NULL);
        fprintf(stderr, "%s\n", "Error in execl");
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        close(link[1]);
        int nbytes = read(link[0], output, sizeof(output));
        fprintf(stderr, "Output of find: (%.*s)\n", nbytes, output);

        if (nbytes < 0) {
            perror("[Child Process: File Search] Error in reading from pipe");
        }

        if (nbytes < 1) {
            strcpy(msg.data.message, "File not found\n");
        } else {
            strcpy(msg.data.message, "File found\n");
        }

        msg.msg_type = client_id;
        msg.data.operation = 'r';

        if (msgsnd(msg_queue_id, &msg, sizeof(msg.data), 0) == -1) {
            perror("[Child Process: File Search] Message could not be sent, please try again");
            exit(-3);
        } else {
            fprintf(stderr, "[Child Process: File Word] Message '%s' sent back to client %d successfully\n", msg.data.message, client_id);
        }

        wait(NULL);
    }
}

/**
 * @brief File Word Count Server
 *
 */
void word_count(const char *filename, int msg_queue_id, int client_id, struct msg_buffer msg) {
    int link[2];
    pid_t pid;
    char output[4096];

    if (pipe(link) == -1) {
        fprintf(stderr, "%s\n", "[Child Process: File Search] Error in pipe creation");
        exit(EXIT_FAILURE);
    }

    if ((pid = fork()) == -1) {
        fprintf(stderr, "%s\n", "[Child Process: File Search] Error in fork creation");
        exit(EXIT_FAILURE);
    }

    // Child process
    if (pid == 0) {
        dup2(link[1], STDOUT_FILENO);
        close(link[0]);
        close(link[1]);
        fprintf(stderr, "[Child Process: File Search] Entered filename: %s\n", filename);

        // Use the 'wc' command to count words in the file
        execlp("wc", "wc", "-w", filename, NULL);

        fprintf(stderr, "%s\n", "Error in execl");
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        close(link[1]);
        int nbytes = read(link[0], output, sizeof(output));
        // fprintf(stderr, "Output of wc: (%.*s)\n", nbytes, output);

        if (nbytes < 0) {
            perror("[Child Process: File Search] Error in reading from pipe");
        }

        int wordCount = 0;
        if (nbytes > 0) {
            sscanf(output, "%d", &wordCount);
        }

        // Message to be sent to client
        snprintf(msg.data.message, sizeof(msg.data.message), "Word count: %d\n", wordCount);
        msg.msg_type = client_id;
        msg.data.operation = 'r';

        if (msgsnd(msg_queue_id, &msg, sizeof(msg.data), 0) == -1) {
            perror("[Child Process: File Search] Message could not be sent, please try again");
            exit(-3);
        } else {
            fprintf(stderr, "[Child Process: File Search] Message '%s' sent back to client %d successfully\n", msg.data.message, client_id);
        }

        wait(NULL);
    }
}

/**
 * @brief Cleanup
 *
 */
void cleanup() {
    while (wait(NULL) > 0)
        ;
    exit(-1);
}

/**
 * @brief The main server is responsible for creating the message queue to be used for
 * communication with the clients. The main server will listen to the message queue
 * for new requests from the clients.
 *
 *
 * @return int
 */
int main() {
    // Iniitalize the server
    printf("[Server] Initializing Server...\n");

    // Create the message queue
    key_t key;
    int msg_queue_id;
    struct msg_buffer msg;

    // Link it with a key which lets you use the same key to communicate from both sides
    if ((key = ftok("README.md", 'B')) == -1) {
        perror("[Server] Error while generating key of the file");
        exit(-1);
    }

    // Create the message queue
    if ((msg_queue_id = msgget(key, 0644 | IPC_CREAT)) == -1) {
        perror("[Server] Error while connecting with Message Queue");
        exit(-1);
    }

    printf("[Server] Successfully connected to the Message Queue %d %d\n", key, msg_queue_id);

    // Listen to the message queue for new requests from the clients
    while (1) {
        if (msgrcv(msg_queue_id, &msg, sizeof(msg.data), 0, 0) == -1) {
            perror("[Server] Error while receiving message from the client");
            exit(-2);
        } else {
            // printf("Message received from Client %ld-Operation %c -> %s\n", msg.msg_type, msg.data.operation, msg.data.message);
            pid_t temporary_pid;
            temporary_pid = fork();

            if (temporary_pid < 0) {
                perror("[Server] Error while creating child process\n");
            } else if (temporary_pid == 0) {
                switch (msg.data.operation) {
                    case '1':
                        ping(msg_queue_id, msg.msg_type, msg);
                        break;
                    case '2':
                        file_search(msg.data.message, msg_queue_id, msg.msg_type, msg);
                        break;
                    case '3':
                        word_count(msg.data.message, msg_queue_id, msg.msg_type, msg);
                        break;
                    case '4':
                        cleanup();
                        break;
                    case 'r': {
                        msg.data.operation = 'r';
                        if (msgsnd(msg_queue_id, &msg, MESSAGE_LENGTH, 0) == -1) {
                            printf("[Server] Message added back to the queue\n");
                        }
                        break;
                    }
                    default:
                        perror("Incorrect operation");
                        break;
                }
                exit(EXIT_SUCCESS);
            }
        }
    }
    wait(NULL);

    // Destroy the message queue
    if (msgctl(msg_queue_id, IPC_RMID, NULL) == -1) {
        perror("[Server] Error while destroying the message queue");
        exit(-4);
    }

    return 0;
}
