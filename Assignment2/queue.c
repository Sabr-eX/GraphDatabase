#include <stdio.h>
#include <stdlib.h>

#define MAX_SIZE 100

struct Queue {
    int items[MAX_SIZE];
    int front;
    int rear;
};

struct Queue *createQueue() {
    struct Queue *queue = (struct Queue *)malloc(sizeof(struct Queue));
    if (queue == NULL) {
        fprintf(stderr, "Memory allocation failed. Exiting program.\n");
        exit(EXIT_FAILURE);
    }
    queue->front = -1;
    queue->rear = -1;
    return queue;
}

int isEmpty(struct Queue *q) {
    return q->front == -1;
}

int isFull(struct Queue *q) {
    return q->rear == MAX_QUEUE_SIZE - 1;
}

void enqueue(struct Queue *q, int value) {
    if (isFull(q)) {
        printf("Queue is full. Cannot enqueue.\n");
        return;
    }

    if (isEmpty(q)) {
        q->front = 0;
    }

    q->rear++;
    q->items[q->rear] = value;
    printf("Enqueued: %d\n", value);
}

int dequeue(struct Queue *q) {
    int value;

    if (isEmpty(q)) {
        printf("Queue is empty. Cannot dequeue.\n");
        return -1;
    }

    value = q->items[q->front];

    if (q->front == q->rear) {
        q->front = q->rear = -1;
    } else {
        q->front++;
    }

    printf("Dequeued: %d\n", value);
    return value;
}

void display(struct Queue *q) {
    if (isEmpty(q)) {
        printf("Queue is empty.\n");
        return;
    }

    printf("Queue elements: ");
    for (int i = q->front; i <= q->rear; i++) {
        printf("%d ", q->items[i]);
    }
    printf("\n");
}

int queueSize(struct Queue *q) {
    if (isEmpty(q)) {
        return 0;
    } else {
        return q->rear - q->front + 1;
    }
}

int main() {
    struct Queue *myQueue = createQueue();

    enqueue(myQueue, 10);
    enqueue(myQueue, 20);
    enqueue(myQueue, 30);

    display(myQueue);

    printf("Queue size: %d\n", queueSize(myQueue));

    dequeue(myQueue);
    display(myQueue);

    printf("Queue size: %d\n", queueSize(myQueue));

    free(myQueue);

    return 0;
}

