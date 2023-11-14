CC = gcc
FLAGS = -Wall -g -pthread

help: # Show help for each of the Makefile recipes.
	@grep -E '^[a-zA-Z0-9 -]+:.*#'  Makefile | sort | while read -r l; do printf "\033[1;32m$$(echo $$l | cut -f 1 -d':')\033[00m:$$(echo $$l | cut -f 2- -d'#')\n"; done

%: %.c # Usage 'make client' (given client.c is present)
	mkdir -p executables
	$(CC) $(FLAGS) $< -o executables/$@.out
	./executables/$@.out

build: # Usage 'make build t=client' (given client.c is present)
	mkdir -p executables
	$(CC) $(FLAGS) $(t).c -o executables/$(t).out
	./executables/$(t).out

trace: # Usage 'make trace t=client' (given client.c is present)
	mkdir -p executables
	mkdir -p logs
	$(CC) $(FLAGS) $(t).c -o executables/$(t).out
	strace -o logs/$(t).log ./executables/$(t).out

clean: # Usage 'make clean'
	@if [ -d executables ]; then \
        rm -rf executables; \
        echo "Removed directory: executables"; \
    else \
        echo "Directory executables does not exist."; \
    fi

	@if [ -d logs ]; then \
        rm -rf logs; \
        echo "Removed directory: logs"; \
    else \
        echo "Directory logs does not exist."; \
    fi