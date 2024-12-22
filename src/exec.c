#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <limits.h>
#include <signal.h>

#include "exec.h"
#include "jobs.h"


#define PIPE_DELIM "|"
#define FIN_DELIM "<"
#define FOUT_DELIM ">"
#define FADD_DELIM ">>"
#define BKGD_DELIM "&"

#define PIPE_LIM 32
#define BUFFER_SIZE 1024
#define COMMAND_LENGTH 256

int pipefds[PIPE_LIM][2]; // makes pipe holding array global 

//this should implement execution, return a status of exec  
//this probably also needs to handle pipes on it's own ie the "main" of exec 
int quash_echo(int argc, char **argv);
int quash_export(int argc, char **argv);
int quash_cd(int argc, char **argv);
int quash_pwd(int argc, char **argv);
int quash_exit(int argc, char **argv);
int make_pipes(int argc, char ** argv);
void redirect(int argc, char ** argv);
char * expand_variables(const char * input);
int quash_kill(int argc, char **argv);

int quash_kill(int argc, char **argv){
    if (argc < 2){
        fprintf(stderr, "Usage: kill <PID> or kill %%<JOBID>\n");
        return 1;
    }

    pid_t pid;
    char* command = NULL;

    if (argv[1][0] == '%'){
        // Handle kill %<JOBID>
        char *job_id_str = &argv[1][1];
        if (job_id_str[0] == '\0'){
            fprintf(stderr, "kill: No JOBID specified after '%%'\n");
            return 1;
        }

        // Validate that JOBID consists of digits
        for (int i = 0; job_id_str[i] != '\0'; i++) {
            if (!isdigit(job_id_str[i])){
                fprintf(stderr, "kill: Invalid JOBID: %s\n", job_id_str);
                return 1;
            }
        }

        int job_id = atoi(job_id_str);
        if (job_id <= 0){
            fprintf(stderr, "kill: Invalid JOBID: %s\n", job_id_str);
            return 1;
        }
        pid = find_pid_by_job_id(job_id);
        if (pid == -1){
            fprintf(stderr, "kill: No such job: %s\n", argv[1]);
            return 1;
        }

        command = find_command_by_pid(pid);
    }
    else{
        // Handle kill <PID>
        char *pid_str = argv[1];
        if (pid_str[0] == '\0'){
            fprintf(stderr, "kill: No PID specified\n");
            return 1;
        }

        // Validate that PID consists of digits
        for (int i = 0; pid_str[i] != '\0'; i++) {
            if (!isdigit(pid_str[i])){
                fprintf(stderr, "kill: Invalid PID: %s\n", pid_str);
                return 1;
            }
        }

        pid = atoi(pid_str);
        if (pid <= 0){
            fprintf(stderr, "kill: Invalid PID: %s\n", pid_str);
            return 1;
        }
        command = find_command_by_pid(pid);
    }

    // Attempt to send SIGTERM to the target PID
    if (kill(pid, SIGTERM) == -1){
        perror("kill");
        return 1;
    }

    if (command != NULL){
        printf("Terminated: %s\n", command);
    }
    else{
        printf("Terminated: [Unknown Command]\n");
    }

    return 1;
}



int quash_cd(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "quash_cd: expected argument to \"cd\"\n");
        return 1;
    }

    char *expanded_path = expand_variables(argv[1]);


    if (chdir(expanded_path) != 0) {
        perror("quash_cd");
        return 1;
    }

    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));
    setenv("PWD",cwd,1);
    return 1;
}

int quash_exit(int argc, char **argv) {
    printf("Exiting Quash...\n");
    exit(0);
    return 0;
}

//helper function to expand string to find environment variables for use in built in functions
char* expand_variables(const char* input) {
    static char expanded[BUFFER_SIZE];
    int j = 0;

    for (int i = 0; input[i] != '\0' && j < BUFFER_SIZE - 1; i++) {
        if (input[i] == '$') {
            i++;
            if (input[i] == '\0') {
                expanded[j++] = '$';
                break;
            }

            char var_name[BUFFER_SIZE];
            int var_len = 0;

            while (isalnum(input[i]) || input[i] == '_') {
                var_name[var_len++] = input[i++];
                if (var_len >= BUFFER_SIZE - 1) break; 
            }
            var_name[var_len] = '\0';

            char *env_var = getenv(var_name);
            if (env_var != NULL) {
                int k = 0;
                while (env_var[k] != '\0' && j < BUFFER_SIZE - 1) {
                    expanded[j++] = env_var[k++];
                }
            }


            i--;
        } else {
            expanded[j++] = input[i];
        }
    }

    expanded[j] = '\0';
    return expanded;
}

int quash_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];
        char *expanded = expand_variables(arg);
        printf("%s ", expanded);
    }
    printf("\n");
    return 1;
}

// quash_export: Set environment variables
int quash_export(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "quash_export: expected argument in the form VAR=VALUE\n");
        return 1;
    }

    char *arg = strdup(argv[1]);
    char *var = strtok(arg, "=");
    char *value = strtok(NULL, "=");

    if (var == NULL || value == NULL) {
        fprintf(stderr, "quash_export: invalid format. Use VAR=VALUE\n");
        free(arg);
        return 1;
    }

    char *expanded_value = expand_variables(value);

    if (setenv(var, expanded_value, 1) != 0) {
        perror("quash_export");
    }

    free(arg);
    return 1; 
}

int quash_pwd(int argc, char **argv) {
    char cwd[BUFFER_SIZE];

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("quash_pwd");
    }

    return 1;
}

int quash_jobs(int argc, char **argv){
  print_jobs();
  return 1;

}

int is_builtin(char *cmd){
    if (strcmp(cmd, "echo") == 0 ||
        strcmp(cmd, "export") == 0 ||
        strcmp(cmd, "cd") == 0 ||
        strcmp(cmd, "pwd") == 0 ||
        strcmp(cmd, "quit") == 0 ||
        strcmp(cmd, "exit") == 0 ||
        strcmp(cmd, "jobs") == 0 ||
        strcmp(cmd, "kill") == 0) // Added "kill"
    {
        return 1;
    }
    return 0;
}


int execute_builtin(int argc, char ** argv){
  if (strcmp(argv[0], "echo") == 0) {
        return quash_echo(argc, argv);
    } 
    else if (strcmp(argv[0], "export") == 0) {
      return quash_export(argc,argv);
    }
    else if (strcmp(argv[0], "cd") == 0) {
        return quash_cd(argc, argv);
    } 
    else if (strcmp(argv[0], "pwd") == 0) {
        return quash_pwd(argc, argv);
    }
    else if (strcmp(argv[0], "exit") == 0 || strcmp(argv[0], "quit") == 0) {
        return quash_exit(argc, argv);
    }
    else if (strcmp(argv[0], "jobs") == 0){
        return quash_jobs(argc,argv);
    }
    else if (strcmp(argv[0], "kill") == 0){
        return quash_kill(argc,argv);
    }
    return 1;
}


//counts the pipes in array argv, returns the count 
int count_pipes(char **argv) {
    int counter = 0;
    for (int i = 0; argv[i] != NULL; i++) {
        if (strcmp(argv[i], PIPE_DELIM) == 0) {
            if (argv[i + 1] == NULL) {
                return -1; 
            }
            counter++;
        }
    }
    return counter;
}

//closes all the pipes in the global pipe array 
void close_pipes(int count){
  for(int i =0; i<count;i++){
    close(pipefds[i][0]);
    close(pipefds[i][1]);
  }
}


int run(int argc, char ** argv, int background){
  if (argc == 0){ // concludes if there are no args in argv 
    return 1;
  }

  if(count_pipes(argv)>0){ //counts pipes, if there are more than 0 pipes
    make_pipes(argc,argv); //makes the pipes 
    return 1;
  }

  int has_redirection = 0;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], ">") == 0 || strcmp(argv[i], ">>") == 0 || strcmp(argv[i], "<") == 0) {
            has_redirection = 1;
            break;
        }
    }

  if (is_builtin(argv[0])) {
        if (has_redirection) {
            // Save the current stdout and stdin
            int saved_stdout = dup(STDOUT_FILENO);
            if (saved_stdout == -1) {
                perror("dup");
                return 1;
            }
            int saved_stdin = dup(STDIN_FILENO);
            if (saved_stdin == -1) {
                perror("dup");
                close(saved_stdout);
                return 1;
            }

            // Perform redirection
            redirect(argc, argv);

            // Recompute argc after redirection
            int new_argc = 0;
            while (argv[new_argc] != NULL) {
                new_argc++;
            }
            // Execute built-in command
            int status = execute_builtin(new_argc, argv);

            // Restore original stdout and stdin
            if (dup2(saved_stdout, STDOUT_FILENO) == -1) {
                perror("dup2");
            }
            if (dup2(saved_stdin, STDIN_FILENO) == -1) {
                perror("dup2");
            }
            close(saved_stdout);
            close(saved_stdin);

            return status;
        } else {
            // No redirection; execute built-in normally
            return execute_builtin(argc, argv);
        }
    }

  pid_t pid = fork();
  if (pid == 0){ // child process
    redirect(argc,argv);
    if (execvp(argv[0], argv)<0 ){
      perror("error");
      exit(EXIT_FAILURE);
    }
    }
    else if (pid < 0){
      perror("quash");
    }
    else{
      if(background){
        int total_length = 0;
            for(int i = 0; i < argc; i++) {
                total_length += strlen(argv[i]) + 1; // +1 for space or null terminator
            }
            char cmd[COMMAND_LENGTH];
            cmd[0] = '\0';
            for(int i = 0; i < argc; i++) {
                strcat(cmd, argv[i]);
                if (i < argc -1) strcat(cmd, " ");
            }
            add_job(pid, cmd);
      }
      else{
      int status;
        do {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
      }
    }
    return 1;
  }


int make_pipes(int argc,char ** argv){
  int pipe_count = count_pipes(argv); //stores the number of pipes to be made 
  char * commands[PIPE_LIM+1][BUFFER_SIZE]; //an array that will hold the commands that get split up by pipes 


  int command_ind = 0; 
  int arg_ind =0;
  for (int i=0; i<argc;i++){
    if(strcmp(argv[i],PIPE_DELIM)==0){
      commands[command_ind][arg_ind] = NULL; //if the token is a pipe, set the val at the arg index to null 
      command_ind++; //go to the next "command"
      arg_ind = 0; //reset the args 
    }else{
      commands[command_ind][arg_ind] = argv[i]; //if token is not a pipe, set the val at arg index to be the token
      arg_ind++;
    }
  }
  commands[command_ind][arg_ind] = NULL; //null termination 

  
  // make the pipes 
  for( int i=0;i<pipe_count;i++ ){
    if(pipe(pipefds[i])==-1){
      perror("pipe");
      exit(EXIT_FAILURE);
    }
  }

  for(int i = 0; i<= pipe_count; i++){
    pid_t pid = fork(); // make a child process for every pipe 
    if (pid ==0){
      //for the child proc
      if (i>0){
        //if not the first command, read from the previous pipe 
        dup2(pipefds[i-1][0],STDIN_FILENO);
        close(pipefds[i-1][0]);
        close(pipefds[i-1][1]);
      }
      if (i< pipe_count){
        //if not the last command, write to the pipe 
        dup2(pipefds[i][1],STDOUT_FILENO);
        close(pipefds[i][1]);
        close(pipefds[i][0]);
      }
      //close all pipes in child proc 
      for(int j =0; j <pipe_count;j++){
        close(pipefds[j][0]);
        close(pipefds[j][1]);
        
      }

      if(execvp(commands[i][0],commands[i]) == -1){
        perror("execvp");
        exit(EXIT_FAILURE);
      }
    }else if (pid <0){
      perror("fork");
      exit(EXIT_FAILURE);
    }
  }
  
  //close parent fds 
  for(int i =0; i <pipe_count; i++){
    close(pipefds[i][0]);
    close(pipefds[i][1]);
  }
  
  //wait for child procs to complete 
  for(int i =0; i <= pipe_count; i++){
    int status;
    waitpid(-1, &status, 0);
  }

}

//break this apart into multiple execs, for each redirect, then redirect the executed ones together inside of here 
void redirect(int argc, char ** argv){
  for( int i =0 ; i< argc; i++ ){
    if(strcmp(argv[i],FOUT_DELIM)==0){
      if(argv[i+1] == NULL){
        exit(EXIT_FAILURE);
      }
      argv[i] = NULL; // null termination
      int fd = open(argv[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd == -1){
        perror("file opening");
        exit(EXIT_FAILURE);
      }
      dup2(fd,STDOUT_FILENO); //redirect stdout to the fd 
      close(fd);
    }else if (strcmp(argv[i],FIN_DELIM)==0){
      if(argv[i+1] == NULL){
        exit(EXIT_FAILURE);
      }
      argv[i] = NULL; //null termination
      int fd = open(argv[i+1], O_RDONLY);
      if (fd == -1){
        perror("file opening");
        exit(EXIT_FAILURE);
      }
      dup2(fd,STDIN_FILENO); //redirect stdin to the fd 
      close(fd);
    }else if (strcmp(argv[i],FADD_DELIM)==0){
      if(argv[i+1] == NULL){
        exit(EXIT_FAILURE);
      }
      argv[i] = NULL;
      int fd = open(argv[i+1], O_WRONLY|O_CREAT|O_APPEND, 0644);
      if (fd ==-1){
        perror("open");
        exit(EXIT_FAILURE);
      }
      dup2(fd,STDOUT_FILENO); //redirect stdout to the fd 
      close(fd);
    }
  }
  return;
}