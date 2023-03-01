#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <wait.h>
#include <fcntl.h>
#include <ctype.h>
#include "parser.h"
#include "LinkedList.h"
#define MAX_INPUT 1024
#define MAX_LIST 100


//initialize global variable
//int group_id = 1;
pid_t pid_array[MAX_LIST];
job head = NULL; //linked list of background process 
//status:
int running_status = -1;
int stop_status = 0;
int terminated_status = 1;


// handle the CTRL_C 
void sigint_handler(int signo){
    if (signo == SIGINT){
        if (pid_array[0] < 0){
            write(1, "\n", sizeof("\n"));
            write(STDOUT_FILENO, PROMPT, sizeof(PROMPT));
        } else {
            int i = 0;
            while (pid_array[i] > 0){
                kill(pid_array[i], SIGKILL);
                i++;
            }
            write(1, "\n", sizeof("\n"));
        }
    }
}

//handle CTRL_Z
void sigtstp_handler(int signo){
    if (signo == SIGTSTP){
        if (pid_array[0] < 0){
            write(1, "\n", sizeof("\n"));
            write(STDOUT_FILENO, PROMPT, sizeof(PROMPT));
        } else {
            int i = 0;
            while (pid_array[i] > 0){
                if (kill(pid_array[i], SIGSTOP) == -1){
                    perror("stop error");
                }
                i++;
            }
            //write(1, "\n", sizeof("\n"));
        }
    }
}

//register the signal 
void sig_handler(){
    if (signal(SIGINT, sigint_handler) == SIG_ERR){
        perror("Unable to catch SIGINT\n");
        //exit(EXIT_FAILURE);
    }       
    if (signal(SIGTTOU, SIG_IGN) == SIG_ERR){
        perror("Unable to ignore SIGTTOU\n");
        //exit(EXIT_FAILURE);
    }
    if (signal(SIGTSTP, sigtstp_handler) == SIG_ERR){
        perror("Unable to catch SIGTSTP\n");
        //exit(EXIT_FAILURE);
    }
}


//redirect stdin from a file
void redirectIn(const char *filename){
    int fd;
    if ((fd = open(filename, O_RDONLY)) == -1){
        perror("open error");
        //exit(EXIT_FAILURE);
    }
    if (dup2(fd, STDIN_FILENO) < 0) perror("dup2 error");
    close(fd);
}

//redirect stdout in a file
void redirectOut(const char *filename){
    int fd;
    if ((fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644)) == -1){
        perror("open error");
        //exit(EXIT_FAILURE);
    }
    if (dup2(fd, STDOUT_FILENO) < 0) perror("dup2 error");
    close(fd);
}

//keep writing to an existing file (stdout)
void redirectOut_add(const char *filename){
    int fd;
    if ((fd = open(filename, O_WRONLY | O_APPEND, 0644)) == -1){
        perror("open error");
        //exit(EXIT_FAILURE);
    }
    if (dup2(fd, STDOUT_FILENO) < 0) perror("dup2 error");
    close(fd);
}


//reset the pid_array
void reset_pid(int *array){
    int i = 0;
    while (pid_array[i]){
            pid_array[i] = -100;
            i++;
        } 
}


//renew the pid_array to the given array
void renew_pidArray(int *array){
    int k = 0;
    while (array[k] > 0){
        pid_array[k] = array[k];
        k++;
    }
    pid_array[k] = -100; //set the end of array to null
}

//copy array to a new array
void array_copy(int *from, int *to){
    int k = 0;
    while (from[k] > 0){
        to[k] = from[k];
        k++;
    }
    to[k] = -100;
}

//remove finished pid in pipe
void removePid(int pid){
    int j = 0, i;
    for (i = 0; pid_array[i] > 0; i++){
        if (pid_array[i] == pid){
            j++;
        }
        j++;
        pid_array[i] = pid_array[j];
    }
    pid_array[j] = -100;
}

void printPid(){
    int i = 0;
    while (pid_array[i] > 0){
        fprintf(stderr, "%d | ", pid_array[i]);
        i++;
    }
    printf("\n");
}

//run pipe
int run_pipe(char *Arg, char *Args[], int *fd, int group_id, int index){
    pid_t pid;
    if ((pid = fork()) < 0) {
        perror("fork error");
        return 0;
        //exit(EXIT_FAILURE);
    }
    else if (pid == 0){
        if (dup2(fd[1], STDOUT_FILENO) < 0) perror("dup2 error in pipe"); 
        close(fd[1]);
        close(fd[0]); //close unused pipe
        execvp(Arg, Args);
        perror(Arg);
        return 0;
    } else {
        pid_array[index] = pid; //store pid
        if (group_id == 0){ //means it is the leading process 
            setpgid(pid, pid); 
            group_id = pid;
        } else {
            setpgid(pid, group_id);  //set it to the same job group of the leading process 
        }
        close(fd[1]); 
        if (dup2(fd[0], STDIN_FILENO) < 0) perror("dup2 error in pipe");
        close(fd[0]);
        close(fd[1]);
        tcsetpgrp(STDIN_FILENO, group_id); // hand the terminal control to the child
        return group_id;
    }
}

//loop through the linked list and see if any bg process is done, form a new linked list by removing the finished process
void check_done(){
    int status, pid;
    int temp_array[MAX_ARGS];
    job p, new_head, temp;
    new_head = newJob(); //dummy head
    temp = new_head;
    p = head;
    while (p != NULL){
        array_copy(p->pids, temp_array);
        for (int i = 0; temp_array[i] > 0; i++) {
            pid = waitpid(temp_array[i], &status, WUNTRACED | WNOHANG);
            if (pid > 0){
                removePid_at_array(p, pid); //if one of the process finished in pipe, remove it from linked list
            }
            else if (pid == 0){
                continue;
            }
        }
        //fprintf(stderr, "childnum: %d with pid: \n", p->children);
        if (p->children == 0){ //if all children finished
            fprintf(stderr, "Finished: %s\n", p->job_cmd);
            job temp1 = p;
            p = p->next;
            free(temp1); //free malloc 
        } else {
            temp->next = p;
            temp = temp->next;
            p = p->next;
        }
    }
    temp->next = p;
    head = new_head->next;
    if (head != NULL){
        renew_jobID(head); //update the job id
    }
    reset_pid(temp_array);
    free(new_head);
}

//get user_command without &
void get_command(char *string){
    int a = 0, b = 0;
    while (string[a] != '\n'){
        if (isspace(string[a])){
            if (b > 0 && !isspace(string[b - 1])){
                string[b++] = ' ';
            }
        } else{
            string[b++] = string[a];
            }
        a++; 
    }
    if (isalnum(string[b - 1])){
        string[b] = '\0'; //not bg, exclude \n
    } else {
        string[b - 1] = '\0'; //bg, exclude &
    }
}

//run last command and return group_id
int run(char *Arg, char *Args[], int group_id, int index){
    pid_t pid;
    if ((pid = fork()) < 0) {
        perror("fork error");
        //exit(EXIT_FAILURE);
    }
    else if (pid == 0){
        execvp(Arg, Args);
        perror(Arg);
        //exit(EXIT_FAILURE);
    }
    //parents
    pid_array[index - 1] = pid; //store pid
    pid_array[index] = -100; //set the end of array to null
    if (group_id == 0){
        setpgid(pid, pid);
        group_id = pid;
    } else {
        setpgid(pid, group_id);
    }
    tcsetpgrp(STDIN_FILENO, group_id);
    return group_id;
}

//check the first command is the builtin command jobs, fg or bg
int check_command(const struct parsed_command *const cmd){
    if (strcmp(cmd -> commands[0][0], "jobs") == 0){
        if (head){
            treverse(head);
        }
        return 1;  //return 1 means ignore the following command and reprompt 
    }
    else if (strcmp(cmd -> commands[0][0], "fg") == 0){
        int id;
        if (cmd -> commands[0][1] == NULL){
            id = find_stop_fg(head);
        } else {
            id = atoi(cmd -> commands[0][1]);
        }
        if (!head){
            fprintf(stderr, "no job");
            return 1; //if empty process list, reprompt
        }
        job node = find_via_jobID(id, head); //locate the process 
        if (!node) { 
            perror("invalid id");
            return 1; //if invalid job id , reprompt
        }
        if (node->status == 0) { //prompt the restarting message if its status is stopped or its command line only if its a bg process
            fprintf(stderr, "Restarting: %s\n", node->job_cmd);
        } else {
            fprintf(stderr, "%s\n", node->job_cmd);
        }
        int status, pid;
        renew_pidArray(node->pids); //renew the pid array for the following operation in case ctrl z issued 
        //printPid();
        //job temp = node; //record the information of cur node
        for (int i = 0; i < node->children; i++){
            if (kill(node->pids[0], SIGCONT) == -1){
                perror("error occurs when continuing the process");
                //exit(EXIT_FAILURE);
            }
        }
        for (int j = 0; pid_array[j] > 0; j++){
            pid = waitpid(pid_array[j], &status, WUNTRACED);
            if (pid > 0 && WIFEXITED(status)){
                removePid_at_array(node, pid); //remove the finished child at node (in case to not break for loop, we just update node pid here)
            }
        }
        renew_pidArray(node->pids); //then we update the global pid_array 
        if (WIFSTOPPED(status)){ //if ctrl z
            fprintf(stderr, "\nStopped: %s\n", node->job_cmd);
            node->status = stop_status;
        } else {
            head = delete_job(head, node->job_id);  //remove process from bg list 
        }
        reset_pid(pid_array); //reset the pid array
        return 1;
    }
    else if (strcmp(cmd -> commands[0][0], "bg") == 0){
        int id;
        if (cmd -> commands[0][1] == NULL){
            id = find_stop_bg(head);
            if (id == 0){
                fprintf(stderr, "all jobs are running\n");
                return 1;
            }
        } else {
            id = atoi(cmd -> commands[0][1]);
        }
        if (!head){
            fprintf(stderr, "no job");
            return 1;
        }
        job node = find_via_jobID(id, head);
        if (!node || node->status != 0) { 
            perror("invalid id\n");
            return 1; //if invalid job id , reprompt
        }
        int status;
        for (int i = 0; i < node->children; i++){
            if (kill(node->pids[0], SIGCONT) == -1){
                perror("error occurs when continuing the process");
                //exit(EXIT_FAILURE);
            }
        }
        for (int j = 0; j < node->children; j++){
            waitpid(-1, &status, WUNTRACED | WNOHANG); //non-stop wait
        }
        node->status = running_status;
        fprintf(stderr, "Running: %s\n", node->job_cmd);
        return 1;
    }
    return 0;
}

void check_background(const struct parsed_command *const cmd, char *inputBuffer, int *pid_array, int group_id){
    int status, pid;
    if (cmd -> is_background){
        int status;
        for (int i = 0; i < cmd->num_commands; i++){
            waitpid(-1, &status, WUNTRACED | WNOHANG); //no stop waiting
        }
        get_command(inputBuffer); //get the command exclude &
        head = addJob(head, pid_array, inputBuffer, running_status, group_id); //add the background process to linked list
        fprintf(stderr, "Running: %s\n", inputBuffer); 
    } else {
        int temp_array[MAX_ARGS];
        array_copy(pid_array, temp_array);
        for (int i = 0; temp_array[i] > 0; i++){
            pid = waitpid(temp_array[i], &status, WUNTRACED);
            if (pid > 0 && WIFEXITED(status)){
                removePid(pid); //if one of the child process finished, remove it from pid array
            }
        }
        get_command(inputBuffer);
        if (WIFSTOPPED(status)){  //if the child is being stopped by ^Z
            head = addJob(head, pid_array, inputBuffer, stop_status, group_id); //add the process to bg list
            fprintf(stderr, "\nStopped: %s\n", inputBuffer); //prompt out the stop message
        } 
    }
    tcsetpgrp(STDIN_FILENO, getpgid(0));//parent take over the terminal control 
}


int main(int argc, char *argv[]){
    int index;
    int interactive = 0;
    struct parsed_command *cmd;
    pid_array[0] = -100; //set the default value of pid array showing which has the same function as NULL
    interactive = isatty(STDIN_FILENO); //check if interactive 
    sig_handler();
    while(1){
        if (interactive){
            write(STDERR_FILENO, PROMPT, sizeof(PROMPT));
        }
        char *inputBuffer = (char *)malloc(MAX_INPUT);
        int n = read(STDIN_FILENO, inputBuffer, MAX_INPUT);
        if (head != NULL){
            check_done();
        }
        if (n == 0  && inputBuffer[0] != '\n'){
            free(inputBuffer);
            while(head != NULL){
                job temp = head;
                head = head->next;
                free(temp);
            }
            exit(0);
        }
        else if (n > 1 && inputBuffer[n - 1] != '\n'){
            free(inputBuffer);
            write(1, "\n", sizeof("\n"));
            continue;
        }
        int i = parse_command(inputBuffer, &cmd);
        if (i < 0) {
            perror("parse_command");
            free(cmd);
            free(inputBuffer);
            continue;
        }
        if (i > 0) {
            fprintf(stderr, "syntax error: %d\n", i);
            free(cmd);
            free(inputBuffer);
            continue;
        }
        if (cmd -> num_commands == 0){  //empty input, reprompt
            free(cmd); //free malloc
            free(inputBuffer);
            continue;
        }
        if (check_command(cmd) == 1){
            free(cmd);
            free(inputBuffer); //free malloc
            continue;
        }
        if (cmd -> stdin_file != NULL){
            redirectIn(cmd -> stdin_file);
        }
        if (cmd -> stdout_file != NULL){
            if (cmd -> is_file_append){
                redirectOut_add(cmd -> stdout_file);
            } else {
                redirectOut(cmd -> stdout_file);
            }
        }
        int group_id = 0; //set the group id
        for (index = 0; index < cmd -> num_commands - 1; index++){
            int fd[2];
            if (pipe(fd) < 0) perror ("pipe error");
            group_id = run_pipe(cmd -> commands[index][0], cmd -> commands[index], fd, group_id, index);
        } 
        group_id = run(cmd -> commands[index][0], cmd -> commands[index], group_id, cmd->num_commands);
        check_background(cmd, inputBuffer, pid_array, group_id);
        free(cmd);
        free(inputBuffer); //free malloc
        redirectIn("/dev/tty");
        redirectOut("/dev/tty"); //reset the STD input and output
        reset_pid(pid_array); //reset all value in pid array to NULL
        if (!interactive){
            while(head != NULL){
                job temp = head;
                head = head->next;
                free(temp);
            }
            exit(0);
        }
    }
    return 0;
}

