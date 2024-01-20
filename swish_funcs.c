#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "job_list.h"
#include "string_vector.h"
#include "swish_funcs.h"

#define MAX_ARGS 10

int tokenize(char *s, strvec_t *tokens) {
    // TODO Task 0: Tokenize string s
    // Assume each token is separated by a single space (" ")
    // Use the strtok() function to accomplish this
    // Add each token to the 'tokens' parameter (a string vector)
    // Return 0 on success, -1 on error

    char *word;
    word=strtok(s, " ");
    while (word!=NULL){
        if(strvec_add(tokens, word) == -1){
            return -1;
        }
        //put in vector
        word=strtok(NULL, " ");
    }
    return 0;
}

int run_command(strvec_t *tokens) {
    // TODO Execute the specified program (token 0) with the
    // specified command-line arguments
   
    int fd;
    int i=0;
    int j;
    int size = 0;
    char *execArray[tokens->length+1];  
    for( j= 0; j<tokens->length;j++){ // Build a string array from the 'tokens' vector and pass this into execvp()
        execArray[j] = tokens->data[j];
        size++;

        if(strcmp(tokens->data[j],">") == 0 || strcmp(tokens->data[j],"<") == 0 || strcmp(tokens->data[j],">>") == 0){
            break;  // exit the loop bc now we have an array we want, an array without the <,>,>>
        }
    }
    execArray[j] = NULL; // add NULL to be able to pass into exec() // should look like ls, -l, NULL 

    if((i=strvec_find(tokens,">"))!=-1){ //  redirects the output of program(ls for example) from standard output to the file 
        fd=open(tokens->data[i+1], O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR);
        
        if (fd==-1){   
            perror("Failed to open input file");
            return -1;
        }
        dup2(fd, STDOUT_FILENO);    //any output from program should be redirected into the file 
    }

    fd = 0;
    if((i=strvec_find(tokens,">>"))!=-1){ // redirects the output of program from standard output to the file and its not overwritten if file already exists
        fd=open(tokens->data[i+1], O_CREAT|O_WRONLY|O_APPEND, S_IRUSR|S_IWUSR);
        
        if (fd==-1){
            perror("Failed to open input file");
            return -1;
        }
        dup2(fd, STDOUT_FILENO);  //any output from program should be redirected into the file 
    }

    fd = 0;
    if((i=strvec_find(tokens,"<"))!=-1){    //redirects input from the file into the program
        fd=open(tokens->data[i+1], O_RDONLY, S_IRUSR|S_IWUSR);
        if (fd==-1){
            perror("Failed to open input file");
            return -1;
        }    
        dup2(fd,STDIN_FILENO);
    }

  
    //Restore the signal handlers for SIGTTOU and SIGTTIN to their defaults since main sets these signals (SIGTTIN and SIGTTOU) 
    //to run the SIG_IGN whenever the signals are called
    struct sigaction new_action;
    new_action.sa_handler = SIG_DFL; // use this handler to change the signals to their defaults
    if (sigfillset(&new_action.sa_mask) == -1) {
        perror("sigfillset");
        return -1;
    }
    if (sigaction(SIGTTIN, &new_action, NULL) == -1 || sigaction(SIGTTOU, &new_action, NULL) == -1) { //  use sigaction() to set the handlers to the SIG_DFL value
        perror("sigaction");
        return -1;
    }
    // Change the process group of this process (a child of the main shell).
    // Call getpid() to get its process ID then call setpgid() and use this process
    // ID as the value for the new process group ID
    if(setpgid( getpid(),0 ) == -1){
            perror("setpid");
            return -1;
    }

    // use exec to execute the user command
    if(execvp(execArray[0], execArray) == -1){
        perror("exec");
        return -1;
    }

    return 0;
}

int resume_job(strvec_t *tokens, job_list_t *jobs, int is_foreground) {

    int idx;
    idx=atoi(tokens->data[1]);  // get the idx for which process to resume running

    //Look up the relevant job information (in a job_t) from the jobs list
    //  using the index supplied by the user (in tokens index 1)
    job_t *job;
    if((job=job_list_get(jobs, idx))==NULL){
         fprintf(stderr, "Job index out of bounds\n");
         return -1;
    }

    if(is_foreground == 1){         // if user wants to foreground then foreground the process using tcsetpgrp with the jobs pid
      tcsetpgrp(STDIN_FILENO, job->pid);}

    if(kill(job->pid,SIGCONT) == -1){  // resumes the program to run by Sending the process the SIGCONT signal with the kill() system call
        perror("kill fail");
        return -1;
    }
    if(is_foreground == 1){  
        int status;
        if(waitpid(job->pid,&status,WUNTRACED)==-1){ //wait specifically for the job process using its pid
            perror("wait failed");
            return-1;
        }
        if(WIFSTOPPED(status)==0){ //  If the job has terminated (not stopped), remove it from the 'jobs' list
            job_list_remove(jobs,idx);
        }
            //printf("length of job list: %d", jobs->length);
        tcsetpgrp (STDIN_FILENO,getpid());
        }
    else{
        job->status = JOB_BACKGROUND; // if user wants to background the process change the status value of the job from JOB_STOPPED to JOB_BACKGROUND (as it was JOB_STOPPED before this)
    }                                  // also do not call tcsetpgrp() to move the process into fg bc its a bg process
    
    return 0;
}

int await_background_job(strvec_t *tokens, job_list_t *jobs) {
    int idx;
    idx=atoi(tokens->data[1]);  // get the job idx for which process is running in the background that u want to wait for in this function
    job_t *job;
    if((job=job_list_get(jobs, idx))==NULL){
         fprintf(stderr, "Job index out of bounds\n");
         return -1;
    }

   // Make sure the job's status is JOB_BACKGROUND (no sense waiting for a stopped job)
    if(job->status != JOB_BACKGROUND){ 
        fprintf(stderr, "Job index is for stopped process not background process\n");
        return -1;
    }
    int status;
    if(waitpid(job->pid,&status,WUNTRACED) == -1){//  Use waitpid() to wait for the background job to terminate
        perror("wait failed");
        return -1;
    }
    if(WIFSTOPPED(status)==0){
       job_list_remove(jobs,idx); // If the background process terminates (is not stopped by a signal) remove it from the jobs list
    }

    return 0;
}

int await_all_background_jobs(job_list_t *jobs) {
    int status;
    job_t *job = jobs->head;
    for(int i = 0; i<jobs->length; i++){ // Iterate through the jobs list, ignoring any stopped jobs
        if(job->status != JOB_STOPPED){ // wait for all background jobs so they can finish running, ignore the stoped jobs 
            if(waitpid(job->pid,&status,WUNTRACED) == -1){ //  Use waitpid() to wait for the job to terminate
                perror("wait failed");
                return -1;
            }
            if(WIFSTOPPED(status)){ // If the job has stopped (check with WIFSTOPPED), change its status to JOB_STOPPED but If the job has terminated, do nothing
                job->status = JOB_STOPPED; 
            }
        }
        job = job->next;
    }
    job_list_remove_by_status(jobs, JOB_BACKGROUND); // Remove all background jobs (which have all just terminated) from jobs list.
    return 0;
}
