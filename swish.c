#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "job_list.h"
#include "string_vector.h"
#include "swish_funcs.h"

#define CMD_LEN 512
#define PROMPT "@> "

int main(int argc, char **argv) {
    // Task 4: Set up shell to ignore SIGTTIN, SIGTTOU when put in background
    // You should adapt this code for use in run_command().
    struct sigaction sac;
    sac.sa_handler = SIG_IGN;
    if (sigfillset(&sac.sa_mask) == -1) {
        perror("sigfillset");
        return 1;
    }
    sac.sa_flags = 0;
    if (sigaction(SIGTTIN, &sac, NULL) == -1 || sigaction(SIGTTOU, &sac, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    strvec_t tokens;
    strvec_init(&tokens);
    job_list_t jobs;
    job_list_init(&jobs);
    char cmd[CMD_LEN];

    printf("%s", PROMPT);
    while (fgets(cmd, CMD_LEN, stdin) != NULL) {
        // Need to remove trailing '\n' from cmd. There are fancier ways.
        int i = 0;
        while (cmd[i] != '\n') {
            i++;
        }
        cmd[i] = '\0';

        if (tokenize(cmd, &tokens) != 0) {
            printf("Failed to parse command\n");
            strvec_clear(&tokens);
            job_list_free(&jobs);
            return 1;
        }
        if (tokens.length == 0) {
            printf("%s", PROMPT);
            continue;
        }
        const char *first_token = strvec_get(&tokens, 0);

        if (strcmp(first_token, "pwd") == 0) {
            // TODO Task 1: Print the shell's current working directory
            // Use the getcwd() system call to get the current working directory 
            char buffer[CMD_LEN];
                   if(getcwd(buffer, 512)==NULL){
                perror("getcwd() error");
            }
            else{
                printf("%s\n",buffer);
            }
        }

        else if (strcmp(first_token, "cd") == 0) {
            // TODO Task 1: Change the shell's current working directory
            // Use the chdir() system call
            if (tokens.data[1]==NULL){
                
                chdir(getenv("/"));//change to the home directory by default if user called "cd"
            }
            else{  // If the user supplied an argument (token at index 1), change to that directory
                char *path;
                path=realpath(tokens.data[1],NULL); 
                if (path==NULL){ // if the directory the user gave was not found then return error
                    perror("chdir");
                    free(path);
                }
                else{
                    chdir(path); 
                    free(path);
                }
            }
        }

        else if (strcmp(first_token, "exit") == 0) {
            strvec_clear(&tokens);      
            break;
        }

        // Task 5: Print out current list of pending jobs
        else if (strcmp(first_token, "jobs") == 0) {
            int i = 0;
            job_t *current = jobs.head;
            while (current != NULL) {
                char *status_desc;
                if (current->status == JOB_BACKGROUND) {
                    status_desc = "background";
                } else {
                    status_desc = "stopped";
                }
                printf("%d: %s (%s)\n", i, current->name, status_desc);
                i++;
                current = current->next;
            }
        }

        // Task 5: Move stopped job into foreground
        else if (strcmp(first_token, "fg") == 0) {
            if (resume_job(&tokens, &jobs, 1) == -1) {
                printf("Failed to resume job in foreground\n");
            }
        }

        // Task 6: Move stopped job into background
        else if (strcmp(first_token, "bg") == 0) {
            if (resume_job(&tokens, &jobs, 0) == -1) {
                printf("Failed to resume job in background\n");
            }
        }

        // Task 6: Wait for a specific job identified by its index in job list
        else if (strcmp(first_token, "wait-for") == 0) {
            if (await_background_job(&tokens, &jobs) == -1) {
                printf("Failed to wait for background job\n");
            }
        }

        // Task 6: Wait for all background jobs
        else if (strcmp(first_token, "wait-all") == 0) {
            if (await_all_background_jobs(&jobs) == -1) {
                printf("Failed to wait for all background jobs\n");
            }
        }

        else { // If the user input does not match any built-in shell command,
            // treat the input as a program name and command-line arguments
            // USE THE run_command() to run what user inputed 
            
            int foreground = 1;

            //If the last token input by the user is "&", start the current
            // command in the background. and remove the & from token
            for(int j= 0; j<tokens.length;j++){   
            if(strcmp(tokens.data[j],"&") == 0){
               strvec_take(&tokens, j);
               foreground = 0;
            }
        }
            int status;
            pid_t child = fork(); // Use fork() to spawn a child process and have the child run the user input
            if(child ==0){
                if(run_command(&tokens)==-1){
                    return -1;
                }
            }
              //for background process
            // 2. Modify the code for the parent (shell) process: Don't use tcsetpgrp() or
            //    use waitpid() to interact with the newly spawned child process.
            // 3. Add a new entry to the jobs list with the child's pid, program name,
            //    and status JOB_BACKGROUND.

            else if(child> 0){ 
               if(foreground){   
                tcsetpgrp(STDIN_FILENO, child);// use tcsetpgrp() which will push that process into the foreground and Set the child process as the target of signals sent to the terminal via the keyboard.
                if(waitpid(child,&status,WUNTRACED) == -1){   //Wait specifically for the child just forked, and
                        perror("wait failed");                //use WUNTRACED as your third argument to detect if it has stopped from a signal
                        return -1;
                }
                if(WIFSTOPPED(status)){ //If the child status was stopped by a signal, add it to 'jobs', the terminal's jobs list.
                     if(job_list_add(&jobs, child, tokens.data[0], JOB_STOPPED) == -1){ // if the process stopped add it into the list of jobs that were stopped
                        return -1;
                     }
                }
                tcsetpgrp (STDIN_FILENO,getpid()); // Call tcsetpgrp() to restore the shell process to the foreground
                }
                else{   // add background job to the job list
                     if(job_list_add(&jobs, child, tokens.data[0], JOB_BACKGROUND) == -1){
                        return -1;
                     }
                }
            }
            else{
                perror("fork failed");
                return -1;
                
            }
        }

        strvec_clear(&tokens);
        printf("%s", PROMPT);
    }
    job_list_free(&jobs);
    return 0;
}