#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<time.h>
#include<signal.h>
#include<errno.h>
#include<sys/wait.h>
#include"queue.h"

#define MAX_COMMAND_LEN 100
#define MAX_JOBS 100
#define CORES 8

enum STATUS {RUNNING, WAITING, SUCCESS, FAILED};

struct JOB{
    int id;
    char command[MAX_COMMAND_LEN];
    enum STATUS status;
    time_t starttime;
    time_t endtime;
    pid_t pid;
} *alljobs[MAX_JOBS];
struct JOB *runningjobs[CORES];
struct JOB completedjobs[MAX_JOBS];
int filled = -1, completed = -1, P = -1;
queue *waitingjobs;
pid_t childid = -1;

void submithistory(struct JOB jobs[]){
    printf("%2s %20s %30s %30s %10s\n", "jobid", "command", "startime", "endtime", "status");
    char status[20];
    struct tm * time_info;
    for(int i=0;i<=completed;i++){
        char time1[50], time2[50];
        strcpy(time1, ctime(&(jobs[i].starttime)));
        time1[strlen(time1)-1] = '\0';
        strcpy(time2, ctime(&(jobs[i].endtime))); 
        time2[strlen(time2)-1] = '\0';
        
        if(jobs[i].status == SUCCESS){
            strcpy(status, "Success");
            printf("%2d %20s %30s %30s %10s\n", jobs[i].id, jobs[i].command, time1, time2, status);
        }else if(jobs[i].status == FAILED){
            strcpy(status, "Failed");
            printf("%2d %20s %30s %30s %10s\n", jobs[i].id, jobs[i].command, time1, time2, status);
        }
    }
}

void showjobs(queue *waitingjobs, struct JOB *runningjobs[], int P){
    printf("%2s %30s %30s\n", "jobid", "command", "status");
    char status[20];
    for(int i=waitingjobs->start;i<waitingjobs->end;i++){
        strcpy(status, "Waiting");
        printf("%2d %30s %30s\n", alljobs[waitingjobs->buffer[i]]->id, alljobs[waitingjobs->buffer[i]]->command, status);
    }
    for(int i=0;i<P;i++){
        strcpy(status, "Running");
        if(runningjobs[i] != NULL){
            printf("%2d %30s %30s\n", runningjobs[i]->id, runningjobs[i]->command, status);
        }
    }
}

int startprocess(struct JOB *t){
    int running = 0;
    for(int i=0;i<P;i++){
        if(runningjobs[i] == NULL){
            running = 1;
            t->status = RUNNING;
            t->starttime = time(NULL);
            runningjobs[i] = t;
            childid = fork();
            if(childid == 0){
                printf("\rJob %d Started.\nEnter command> ", t->id);
                fflush(stdout);
                char temp[MAX_COMMAND_LEN+50];
                strcpy(temp, t->command);
                sprintf(temp, "%s > ./out/%d.out 2> ./out/%d.err", t->command, t->id, t->id);
                system(temp);                
                exit(0);
            }
            runningjobs[i]->pid = childid;
            break;
        }
    }
    return running;
}

void child_handler(int sig) {
    pid_t p;
    int status;

    while (1) {
       /* retrieve child process ID (if any) */
       p = waitpid(-1, &status, WNOHANG);

       /* check for conditions causing the loop to terminate */
        if (p == -1) {
            if (errno == EINTR) {continue;}
            break;
        }else if(p == 0) {break;}

        for(int i=0;i<P;i++){
            if(runningjobs[i] != NULL && runningjobs[i]->pid == p){
                runningjobs[i]->endtime = time(NULL);
                runningjobs[i]->status = SUCCESS;
                completedjobs[++completed] = *runningjobs[i];
                runningjobs[i] = NULL;
                break;
            }
        }
        int jobid = queue_delete(waitingjobs);
        if(jobid == -1) continue;
        struct JOB *jobtorun = alljobs[jobid];
        alljobs[jobid] = NULL;
        startprocess(jobtorun);
    }   
}

int main(int argc, char *argv[]){
    if(argc < 2){
        printf("USAGE: ./scheduler P\n");
        exit(0);
    }
    signal(SIGCHLD, child_handler);
    P = atoi(argv[1]);
    if(P > CORES){
        P = CORES;
    }
    char command[MAX_COMMAND_LEN];
    waitingjobs = queue_init(MAX_JOBS);
    for(int i=0;i<P;i++){
        runningjobs[i] = NULL;
    }
    while(1){
        printf("\rEnter command> ");
        bzero(command, MAX_COMMAND_LEN);
        fgets(command, MAX_COMMAND_LEN, stdin);
        // if(command[strlen(command)-1] == '\n')
        command[strlen(command)-1] = '\0';
        if(strncmp(command, "submithistory", 13) == 0){
            submithistory(completedjobs);
        }else if(strncmp(command, "submit", 6) == 0){
            struct JOB *t = malloc(sizeof(struct JOB));
            strncpy(t->command, &command[7], strlen(command) - 7);
            filled++;
            t->id = filled;
            if(startprocess(t)) continue;
            for(int i=0;i<MAX_JOBS;i++){
                if(alljobs[i] == NULL){
                    alljobs[i] = t;
                    queue_insert(waitingjobs, i);
                    printf("Job %d added to the queue\n", t->id);
                    break;
                }
            }


        }else if(strncmp(command, "showjobs", 8) == 0){
            showjobs(waitingjobs, runningjobs, P);
        }else if(strncmp(command, "exit", 4) == 0){
            printf("Exiting. Bye!\n");
            exit(0);
        }else{
            if(strlen(command) > 0)
                printf("Unknown Command! \n");
        }
    }

    return 0;
}