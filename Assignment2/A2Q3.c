#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>

//global variables shared between threads
static int data_set=0;
static sem_t rw_mutex;
static sem_t write_mutex;
static sem_t mutex;
static sem_t read_try;
static int read_count = 0;
static int write_count = 0;
static int NUMBER_OF_WRITES;
static int NUMBER_OF_READS;
clock_t read_min_t=99999999, read_max_t=0, read_total_t=0, write_min_t=99999999, write_max_t=0, write_total_t=0;
int read_times=0;
int write_times=0;

static void *readerFunc(void *arg)
{
    clock_t start_t, end_t, dif,start_t2,end_t2;
    int loops = *((int *) arg);
    int j;

    for (j = 0; j < loops; j++) 
    {
        if (sem_wait(&read_try) == -1)
            exit(2);
        

        start_t=clock();
      
        if (sem_wait(&mutex) == -1)
            exit(2);

        read_count++;
        if (read_count == 1)
            if (sem_wait(&rw_mutex) == -1)
                exit(2);

        if (sem_post(&mutex) == -1)
            exit(2);        
            
        if (sem_post(&read_try) == -1)
            exit(2);

        end_t=clock();


        //read    
        int cur_data=data_set;

        if (sem_wait(&mutex) == -1)
            exit(2);


        dif=end_t-start_t;
        if (read_max_t<dif)
            read_max_t=dif;
        if (read_min_t>dif)
            read_min_t=dif;
        read_total_t+=dif;
        read_times+=1;

        read_count--;
        
        if (read_count == 0)
             if (sem_post(&rw_mutex) == -1)
                exit(2);

        if (sem_post(&mutex) == -1)
            exit(2);



        usleep(rand()%100000);

    }
    return NULL;
}

static void *writerFunc(void *arg)
{
    clock_t start_t, end_t, dif;

    int loops = *((int *) arg);
    int loc, j;
    for (j = 0; j < loops; j++) 
    {
        if (sem_wait(&write_mutex) == -1)
            exit(2);
        write_count++;
        if(write_count==1)
            if (sem_wait(&read_try) == -1)
                exit(2);
        if (sem_post(&write_mutex) == -1)
            exit(2);

        start_t=clock();

        if (sem_wait(&rw_mutex) == -1)
            exit(2);

        end_t=clock();
        

        loc = data_set;
        loc+=10;
        data_set = loc;

        if (sem_post(&rw_mutex) == -1)
            exit(2);
        

        dif=end_t-start_t;
        if (write_max_t<dif)
            write_max_t=dif;
        if (write_min_t>dif)
            write_min_t=dif;
        write_total_t+=dif;
        write_times+=1;

        if (sem_wait(&write_mutex) == -1)
            exit(2);
        write_count--;
        if(write_count==0)
            if (sem_post(&read_try) == -1)
                exit(2);
        if (sem_post(&write_mutex) == -1)
            exit(2);

        usleep(rand()%100000);
    }
    return NULL;
}

int main(int argc, char *argv[]) 
{
    //Initialization//
    NUMBER_OF_WRITES=atoi(argv[1]);
    NUMBER_OF_READS=atoi(argv[2]);
    pthread_t writers[10];
    pthread_t readers[500];
    
    if (sem_init(&mutex, 0, 1) == -1) 
    {
        printf("Error, init semaphore\n");
        exit(1);
    }
    if (sem_init(&write_mutex, 0, 1) == -1) 
    {
        printf("Error, init semaphore\n");
        exit(1);
    }
    if (sem_init(&read_try, 0, 1) == -1) 
    {
        printf("Error, init semaphore\n");
        exit(1);
    }
    if (sem_init(&rw_mutex, 0, 1) == -1)
    {
        printf("Error, init semaphore\n");
        exit(1);
    }
    //Initialization//


    //Creating writer threads//
    for (int i=0;i<10;i++)
    {
        int s = pthread_create(&writers[i], NULL, writerFunc, &NUMBER_OF_WRITES);

        if (s != 0) {
            printf("Error, creating threads\n");
            exit(1);
        }
    }
    //Creating writer threads//


    //Creating reader threads//
    for (int i=0;i<500;i++)
    {
        int s = pthread_create(&readers[i], NULL, readerFunc, &NUMBER_OF_READS);

        if (s != 0) {
            printf("Error, creating threads\n");
            exit(1);
        }
    }
    //Creating reader threads//



    //Joining writer threads//
    for (int i=0;i<10;i++)
    {
        int s = pthread_join(writers[i], NULL);
        if (s != 0) {
            printf("Error, creating threads\n");
            exit(1);
        }

    }
    //Joining writer threads//



    //Joining reader threads//
    for (int i=0;i<500;i++)
    {
        int s = pthread_join(readers[i], NULL);
        if (s != 0) {
            printf("Error, creating threads\n");
            exit(1);
        }

    }
    //Joining reader threads//


    

    //printf("%ld,%ld,%ld\n", write_min_t, write_total_t/write_times, write_max_t);
    printf("Write min waiting time:%ld, avg time:%ld, max time:%ld\nTotal write operations:%d\n", write_min_t, write_total_t/write_times, write_max_t,write_times);
    printf("Read min waiting time:%ld, avg time:%ld, max time:%ld\nTotal read operations:%d\n", read_min_t, read_total_t/read_times, read_max_t,read_times);

    

    printf("Finished.\n");
    return 0;
}