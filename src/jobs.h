#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>

#define MAX_JOBS 100
#define COMMAND_LENGTH 256

typedef struct Job {
    int job_id;
    pid_t pid;
    char command[COMMAND_LENGTH];
    int active;
} Job;

void init_jobs();

int add_job(pid_t pid, const char *command);

void remove_job(pid_t pid);

void print_jobs();

void sigchld_handler(int sig);

pid_t find_pid_by_job_id(int job_id);

char* find_command_by_pid(int pid);

//void cleanup_jobs();

#endif
