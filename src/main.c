#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <sys/wait.h>
#include <signal.h>

#include "exec.h"
#include "jobs.h"



#define BUFFER_SIZE  1024
#define TOKEN_SIZE   64
#define TOKEN_DELIM " \t\r\n\a"

char * read_in();
void read_command();
int parse();

char  input_buf[BUFFER_SIZE]; // creates a global input buffer of size buffer size 
char * argv[TOKEN_SIZE]; //creates a global array of arguments of size token size (ie 64 args)

//this will read commands from the line and exec them, should wipe out the job queue 
int main() {
  //wipe_jobs();
  init_jobs();
  struct sigaction sa;
  sa.sa_handler = &sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;

    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

  read_command();
  return 0;
}


//this grabs and execs commands from the terminal  
void read_command(){

    char * in; //holds the line inputted into the term 
    int argc; // holds the args from each line, ie the split of the line -- might need to type check this 
    int stat; //holds the status of the command, ie whether the command concluded to end the loop
    int background; // flag to check for background job
    

    do{
      printf("[QUASH]$ "); //prints prompt 
      in = read_in(); //grabs content from term 
      if (in == NULL){ //checks if term handed nothing, if so just keep going 
        continue;
      }else{
        background = 0;
      argc = parse(in,&background); //parses the arguments grabbed from the term 
      stat = run(argc,argv, background); // run the arguments 
      }

    }while (stat); // end the loop when stat exits 
}

/*i lied we don't need this maybe 
//grabs any char signal, stops ctrl c and grabs it 
void signal_handle(){
  signal(SIGINT, signal_handle);
  getchar();
}
*/

//this should grab args from the term
char * read_in(){
  if (fgets(input_buf, sizeof(input_buf),stdin)== NULL){ //puts input into input buffer, exits if empty 
    exit(0);
  } 
  if (input_buf[0]== '\n'){ // checks if an empty new line has been input 
    return NULL;
  }

  input_buf[strcspn(input_buf,"\n")] = '\0'; //gets rid of the new line char, replace w null 
  return input_buf;
}

// this tokenizes, leaves tokens in argv and returns the num of tokens 
int parse(char * linein, int *background){
  memset(argv,0,sizeof(argv)); //clears the memory in argv 
  int index = 0; //initializes index counter or "argc"
  char * token; //makes an array for tokens to sit in 

  token = strtok(linein, TOKEN_DELIM); // filters input, stores the tokens in token 
  while(token != NULL){ //while the token string is not null 
    if (strcmp(token, "&") == 0){
      *background = 1;
    }
    else{
    argv[index]= token; //put the token char array at index of argv 
    index++; //increment index 
    if (index >= TOKEN_SIZE){ // if we have maxed out the amount of tokens allowed, ie the size of argv 
      fprintf(stderr, "too many args \n"); //say we have too many args, error out 
      exit(0); 
    }
    }
    token = strtok(NULL, TOKEN_DELIM); // grab the next token, loop until no tokens left 
  }
  argv[index] = NULL; //terminate argv w null 
  return index; 
}
