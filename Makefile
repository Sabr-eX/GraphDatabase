CC = gcc
FLAGS = -Wall

%: %.c
	$(CC) $(FLAGS) $< -o $@.out
	./$@.out

clean:
	rm -rf *.out