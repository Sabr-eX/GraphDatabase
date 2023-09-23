#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define die(e)                      \
    do {                            \
        fprintf(stderr, "%s\n", e); \
        exit(EXIT_FAILURE);         \
    } while (0);

void file_search() {
    int link[2];
    pid_t pid;
    char filename[100];  // Assuming a maximum filename length of 100 characters
    char output[4096];

    printf("Enter the filename: ");
    scanf("%99s", filename);  // Read the filename from user input

    if (pipe(link) == -1)
        die("pipe");

    if ((pid = fork()) == -1)
        die("fork");

    if (pid == 0) {
        dup2(link[1], STDOUT_FILENO);
        close(link[0]);
        close(link[1]);
        execlp("find", "find", ".", "-name", filename, NULL);
        die("execl");

    } else {
        close(link[1]);
        int nbytes = read(link[0], output, sizeof(output));
        printf("Output: (%.*s)\n", nbytes, output);
        wait(NULL);
    }
    return 0;
}

int main() {
    file_search();
}