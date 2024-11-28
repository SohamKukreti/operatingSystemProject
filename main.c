#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdlib.h>

// OneShot Algorithm

sem_t knife;
sem_t spoon;

int processAcquired[2] = {0, 0};

void* oneShotFunction(void* arg) {
    int processId = *(int*)arg;
    if (processAcquired[processId]) {
        printf("Process %d: Request refused, already acquired resources.\n", processId + 1);
        return NULL;
    }
    printf("Process %d: Requesting knife and spoon.\n", processId + 1);
    sem_wait(&knife);
    printf("Process %d: Acquired knife.\n", processId + 1);
    sem_wait(&spoon);
    printf("Process %d: Acquired spoon.\n", processId + 1);
    processAcquired[processId] = 1;
    printf("Process %d: Using knife and spoon.\n", processId + 1);
    sleep(2);
    sem_post(&knife);
    printf("Process %d: Released knife.\n", processId + 1);
    sem_post(&spoon);
    printf("Process %d: Released spoon.\n", processId + 1);
    processAcquired[processId] = 0;
    return NULL;
}


// MultiShot Algorithm

#define MAX_RESOURCES 5

sem_t resources[MAX_RESOURCES];
int totalResources = 3;
int processHeldResources[2][MAX_RESOURCES] = {0};

void requestResources(int processId, int* requestedResources, int requestCount) {
    int isHolding = 0;
    for (int i = 0; i < totalResources; i++) {
        if (processHeldResources[processId][i]) {
            isHolding = 1;
            break;
        }
    }

    if (isHolding) {
        printf("Process %d: Currently holding resources. Releasing them first.\n", processId + 1);
        for (int i = 0; i < totalResources; i++) {
            if (processHeldResources[processId][i]) {
                sem_post(&resources[i]);
                processHeldResources[processId][i] = 0;
                printf("Process %d: Released resource %d.\n", processId + 1, i + 1);
            }
        }
    }

    printf("Process %d: Requesting resources: ", processId + 1);
    for (int i = 0; i < requestCount; i++) {
        printf("%d ", requestedResources[i] + 1);
    }
    printf("\n");

    for (int i = 0; i < requestCount; i++) {
        if (requestedResources[i] >= totalResources || requestedResources[i] < 0) {
            printf("Process %d: Requested resource %d does not exist. Request refused.\n",
                   processId + 1, requestedResources[i] + 1);
            return;
        }
    }

    for (int i = 0; i < requestCount; i++) {
        sem_wait(&resources[requestedResources[i]]);
        processHeldResources[processId][requestedResources[i]] = 1;
        printf("Process %d: Acquired resource %d.\n", processId + 1, requestedResources[i] + 1);
    }

    printf("Process %d: Granted exclusive access to requested resources.\n", processId + 1);
}

void* multiShotFunction(void* arg) {
    int processId = *(int*)arg;

    int initialRequest[] = {0, 1};
    requestResources(processId, initialRequest, 2);

    sleep(2);

    int newRequest[] = {1, 2};
    requestResources(processId, newRequest, 2);

    sleep(2);

    for (int i = 0; i < totalResources; i++) {
        if (processHeldResources[processId][i]) {
            sem_post(&resources[i]);
            processHeldResources[processId][i] = 0;
            printf("Process %d: Released resource %d.\n", processId + 1, i + 1);
        }
    }

    return NULL;
}


// Hierarchical Algorithm

#define NUM_PROCESSES 2

sem_t resources[MAX_RESOURCES];
int resourcePriorities[MAX_RESOURCES];
int processHeldPriority[NUM_PROCESSES] = {0};
int totalResourcesHierarchical = 2;

void requestResourceHierarchical(int processId, int resourceId) {
    if (resourceId < 0 || resourceId >= totalResourcesHierarchical) {
        printf("Process %d: Requested resource %d does not exist. Request refused.\n", processId + 1, resourceId + 1);
        return;
    }

    if (processHeldPriority[processId] >= resourcePriorities[resourceId]) {
        printf("Process %d: Tried requesting %d\n", processId + 1, resourceId + 1);
        printf("Process %d: Request refused. Currently holding resource(s) with equal or higher priority.\n", processId + 1);
        return;
    }

    printf("Process %d: Requesting resource %d (priority %d).\n", processId + 1, resourceId + 1, resourcePriorities[resourceId]);

    sem_wait(&resources[resourceId]);
    printf("Process %d: Acquired resource %d.\n", processId + 1, resourceId + 1);

    processHeldPriority[processId] = resourcePriorities[resourceId];
}

void releaseResourceHierarchical(int processId, int resourceId) {
    int value;
    sem_getvalue(&resources[resourceId], &value);
    if (value <= 0) {
        sem_post(&resources[resourceId]);
        printf("Process %d: Released resource %d.\n", processId + 1, resourceId + 1);
        if (processHeldPriority[processId] == resourcePriorities[resourceId]) {
            processHeldPriority[processId] = 0;
        }
    }
}

void* hierarchicalFunction(void* arg) {
    int processId = *(int*)arg;

    requestResourceHierarchical(processId, 1);
    sleep(1);
    requestResourceHierarchical(processId, 0);

    sleep(2);

    releaseResourceHierarchical(processId, 0);
    releaseResourceHierarchical(processId, 1);

    return NULL;
}


// Hierarchical with Queuing

#define MAX_QUEUE_SIZE 10

typedef struct {
    int items[MAX_QUEUE_SIZE];
    int front;
    int rear;
} Queue;

void initQueue(Queue* q) {
    q->front = -1;
    q->rear = -1;
}

int isEmpty(Queue* q) {
    return q->front == -1;
}

int isFull(Queue* q) {
    return q->rear == MAX_QUEUE_SIZE - 1;
}

void enqueue(Queue* q, int processId) {
    if (isFull(q)) {
        printf("Queue is full. Cannot enqueue process %d.\n", processId);
        return;
    }
    if (isEmpty(q)) {
        q->front = 0;
    }
    q->rear++;
    q->items[q->rear] = processId;
}

int dequeue(Queue* q) {
    if (isEmpty(q)) {
        printf("Queue is empty. Cannot dequeue.\n");
        return -1;
    }
    int item = q->items[q->front];
    if (q->front == q->rear) {
        q->front = q->rear = -1;
    } else {
        q->front++;
    }
    return item;
}

typedef struct {
    int id;
    int priority;
    sem_t semaphore;
    Queue waitQueue;
    pthread_mutex_t queueLock;
} Resource;

Resource resourcesQueing[MAX_RESOURCES];
int totalResourcesHierarchicalQueing = 2;
int processHeldPriorityQueing[NUM_PROCESSES] = {0};

void requestResourceHierarchicalQueing(int processId, int resourceId) {
    if (resourceId < 0 || resourceId >= totalResourcesHierarchicalQueing) {
        printf("Process %d: Requested resource %d does not exist. Request refused.\n", processId + 1, resourceId + 1);
        return;
    }

    Resource* resource = &resourcesQueing[resourceId];

    if (processHeldPriorityQueing[processId] >= resource->priority) {
        printf("Process %d: Tried Requesting resource %d\n", processId + 1, resourceId + 1);
        printf("Process %d: Request refused. Currently holding resource(s) with equal or higher priority.\n", processId + 1);
        return;
    }

    pthread_mutex_lock(&resource->queueLock);

    if (sem_trywait(&resource->semaphore) == 0) {
        printf("Process %d: Acquired resource %d (priority %d).\n", processId + 1, resourceId + 1, resource->priority);
        processHeldPriorityQueing[processId] = resource->priority;
        pthread_mutex_unlock(&resource->queueLock);
    } else {
        printf("Process %d: Resource %d is busy. Added to wait queue.\n", processId + 1, resourceId + 1);
        enqueue(&resource->waitQueue, processId);
        pthread_mutex_unlock(&resource->queueLock);

        sem_wait(&resource->semaphore);

        printf("Process %d: Acquired resource %d after waiting (priority %d).\n", processId + 1, resourceId + 1, resource->priority);
        processHeldPriorityQueing[processId] = resource->priority;
    }
}

void releaseResourceHierarchicalQueing(int processId, int resourceId) {
    if (resourceId < 0 || resourceId >= totalResourcesHierarchicalQueing) {
        printf("Process %d: Attempted to release resource %d that does not exist.\n", processId + 1, resourceId + 1);
        return;
    }

    Resource* resource = &resourcesQueing[resourceId];

    pthread_mutex_lock(&resource->queueLock);

    if (!isEmpty(&resource->waitQueue)) {
        int nextProcess = dequeue(&resource->waitQueue);

        printf("Process %d: Released resource %d. Granting it to Process %d.\n", processId + 1, resourceId + 1, nextProcess + 1);
        sem_post(&resource->semaphore);
    } else {
        printf("Process %d: Released resource %d.\n", processId + 1, resourceId + 1);
        sem_post(&resource->semaphore);
    }

    if (processHeldPriorityQueing[processId] == resource->priority) {
        processHeldPriorityQueing[processId] = 0;
    }

    pthread_mutex_unlock(&resource->queueLock);
}

void* hierarchicalQueueingFunction(void* arg) {
    int processId = *(int*)arg;

    requestResourceHierarchicalQueing(processId, 1);
    sleep(1);
    requestResourceHierarchicalQueing(processId, 0);

    sleep(2);

    releaseResourceHierarchicalQueing(processId, 1);
    releaseResourceHierarchicalQueing(processId, 0);

    return NULL;
}


int main() {
    while (1) {
        printf("\n***********************************************************\n");
        printf("Choose from the following Deadlock Prevention Algorithms: \n");
        printf("1. One Shot Algorithm\n");
        printf("2. Multi Shot Algorithm\n");
        printf("3. Hierarchical Algorithm\n");
        printf("4. Hierarchical Algorithm with Queueing\n");
        printf("5. Exit\n");
        printf("Enter choice: ");
        int choice;
        scanf("%d", &choice);
        printf("***********************************************************\n");
        if(choice == 1){
            printf("Now simulating One Shot Algorithm\n");
            sleep(2);
            sem_init(&knife, 0, 1);
            sem_init(&spoon, 0, 1);
            pthread_t p1, p2;
            int id1 = 0, id2 = 1;
            pthread_create(&p1, NULL, oneShotFunction, &id1);
            pthread_create(&p2, NULL, oneShotFunction, &id2);
            pthread_join(p1, NULL);
            pthread_join(p2, NULL);
            sem_destroy(&knife);
            sem_destroy(&spoon);
        }

        else if(choice == 2){
            printf("Now simulating Multi Shot Algorithm\n");
            sleep(2);
            for (int i = 0; i < totalResources; i++) {
                sem_init(&resources[i], 0, 1);
            }
            pthread_t p1, p2;
            int id1 = 0, id2 = 1;

            pthread_create(&p1, NULL, multiShotFunction, &id1);
            pthread_create(&p2, NULL, multiShotFunction, &id2);

            pthread_join(p1, NULL);
            pthread_join(p2, NULL);

            for (int i = 0; i < totalResources; i++) {
                sem_destroy(&resources[i]);
            }
        }

        else if(choice == 3){
            printf("Now simulating Hierarchical Algorithm\n");
            sleep(2);
            resourcePriorities[0] = 2;
            resourcePriorities[1] = 1;

            for (int i = 0; i < totalResourcesHierarchical; i++) {
                sem_init(&resources[i], 0, 1);
            }

            pthread_t threads[NUM_PROCESSES];
            int processIds[NUM_PROCESSES] = {0, 1};

            for (int i = 0; i < NUM_PROCESSES; i++) {
                pthread_create(&threads[i], NULL, hierarchicalFunction, &processIds[i]);
            }

            for (int i = 0; i < NUM_PROCESSES; i++) {
                pthread_join(threads[i], NULL);
            }

            for (int i = 0; i < totalResourcesHierarchical; i++) {
                sem_destroy(&resources[i]);
            }
        }

        else if(choice == 4){
            printf("Now simulating Hierarchical Algorithm with Queueing\n");
            sleep(2);
            resourcesQueing[0] = (Resource){.id = 0, .priority = 2, .queueLock = PTHREAD_MUTEX_INITIALIZER};
            resourcesQueing[1] = (Resource){.id = 1, .priority = 1, .queueLock = PTHREAD_MUTEX_INITIALIZER};

            for (int i = 0; i < totalResourcesHierarchicalQueing; i++) {
                sem_init(&resourcesQueing[i].semaphore, 0, 1);
                initQueue(&resourcesQueing[i].waitQueue);
            }

            pthread_t threads[NUM_PROCESSES];
            int processIds[NUM_PROCESSES] = {0, 1};

            for (int i = 0; i < NUM_PROCESSES; i++) {
                pthread_create(&threads[i], NULL, hierarchicalQueueingFunction, &processIds[i]);
            }

            for (int i = 0; i < NUM_PROCESSES; i++) {
                pthread_join(threads[i], NULL);
            }

            for (int i = 0; i < totalResourcesHierarchicalQueing; i++) {
                sem_destroy(&resourcesQueing[i].semaphore);
            }
        }

        else if(choice == 5){
            return 0;
        }
    }
    return 0;
}
