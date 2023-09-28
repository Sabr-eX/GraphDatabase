#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

void word_count() {
    pid_t pid;
    int pfds[2], s;
    char file_name[100];
    
    printf("Enter filename: ");
    scanf("%99s", file_name); 

    if (pipe(pfds) == -1) {
        perror("Error: Could not create pipe");
        exit(EXIT_FAILURE);
    }

    pid = fork();

    if (pid == -1) {
        perror("Error in forking()");
        exit(EXIT_FAILURE);
    } 
    else if (pid == 0) { // Child process
        close(pfds[1]); 
        dup2(pfds[0], STDIN_FILENO); 
        close(pfds[0]); 
        execlp("wc", "wc", "-w", NULL);
        perror("Error in execlp()");
        exit(EXIT_FAILURE);
    } else { // Parent process
        close(pfds[0]); 
        FILE *file = fopen(file_name, "r");

        if (file == NULL) {
            perror("Error opening file");
            exit(EXIT_FAILURE);
        }

        char buffer[5000];
        size_t bytes_read;

        while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
            write(pfds[1], buffer, bytes_read); 
        }

        close(pfds[1]); 
        fclose(file);

        wait(&s);
    }

    
}

int main()
{
	word_count();
	return 0;
}

