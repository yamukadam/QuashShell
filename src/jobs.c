#include "jobs.h"
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>

#define COMMAND_LENGTH 256

static Job job_list[MAX_JOBS];
static int next_job_id = 1;

void init_jobs (){
    for (int i =0; i < MAX_JOBS; i++){
        job_list[i].active = 0;

    }
    next_job_id = 1;
}

int add_job(pid_t pid, const char *command){
    for (int i =0; i < MAX_JOBS; i++){
        if (job_list[i].active == 0){
            job_list[i].job_id = next_job_id++;
            job_list[i].pid = pid;
            strncpy(job_list[i].command, command, COMMAND_LENGTH - 1);      
            job_list[i].command[COMMAND_LENGTH - 1] = '\0';
            job_list[i].active = 1;
            printf("Background job started: [%d] %d %s &\n", job_list[i].job_id, job_list[i].pid, job_list[i].command);
            return 1;
        }
    }
    fprintf(stderr, "Error: Maximum number of background jobs reached.\n");
    return 0;
}

void remove_job(pid_t pid){
    for (int i = 0; i < MAX_JOBS; i++){
        if (job_list[i].active && job_list[i].pid == pid){
            printf("\nCompleted: [%d] %d %s &\n", job_list[i].job_id, job_list[i].pid, job_list[i].command);
            job_list[i].active = 0;
            printf("[QUASH]$ ");
            fflush(stdout);
            return;
        }
    }
}

void print_jobs(){
    for (int i = 0; i < MAX_JOBS; i++){
        if (job_list[i].active == 1){
            printf("[%d] %d %s &\n", job_list[i].job_id, job_list[i].pid, job_list[i].command);
        }
    }
}

void sigchld_handler(int sig) {
    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        remove_job(pid);
    }
}

pid_t find_pid_by_job_id(int job_id){
    for (int i = 0; i < MAX_JOBS; i++){
        if (job_list[i].active && job_list[i].job_id == job_id){
            return job_list[i].pid;
        }
    }
    return -1; // Indicates that the job was not found
}

char* find_command_by_pid(int pid){
    for (int i = 0; i < MAX_JOBS; i++){
        if (job_list[i].active && job_list[i].pid == pid){
            return job_list[i].command;
        }
    }
    return -1; // Indicates that the job was not found
}