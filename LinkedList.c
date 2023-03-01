#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#define MAX_STRING 1000
#define MAX_ARGS 100

//initialize the linked list
struct jobs {
    int job_id;
    int pids[MAX_ARGS];
    char job_cmd[MAX_STRING];
    int status;
    int children;
    int group_id;
    struct jobs *next;
};

typedef struct jobs *job;

//initialize a new node 
job newJob(){
    job temp;
    temp = (job)malloc(sizeof(struct jobs));
    temp->next = NULL;
    temp->job_id = 1;
    return temp;
}

//add new job to the end of the linked list
job addJob(job head, int *pid_array, char *command, int cur_status, int group_id){
    job temp, p;
    int k = 0;
    temp = newJob();
    temp->status = cur_status; 
    temp->group_id = group_id;
    while (pid_array[k] > 0){
        temp->pids[k] = pid_array[k];
        k++;
    }
    temp->children = k;//num of children running
    temp->pids[k] = -100; //set the end of array to null
    for (int i = 0; i < MAX_STRING; i++){
        temp->job_cmd[i] = command[i];
        if (temp->job_cmd[i] == '\0' || temp->job_cmd[i] == '\n'){
            break;
        }
    }
    if (head == NULL){
        temp->job_id = 1; //update the job id
        head = temp;
        //free(temp);
    } else {
        p = head;
        while (p->next != NULL){
            p = p->next;
        }
        p->next = temp;
        temp->job_id = p->job_id + 1;
        //free(temp);
    }
    return head;
}

void printnPid(job node){
    int i = 0;
    while (node->pids[i] > 0){
        fprintf(stderr, "%d ", node->pids[i]);
        i++;
    }
    //printf("\n");
}

//renew the job id after delete a job
void renew_jobID(job head){
    job p = head;
    int i = 1;
    while (p != NULL){
        p->job_id = i;
        i++;
        p = p->next;
    }
}

//check if all process in the list are running bg process, if not, return the most recent stopped process's job id
int find_stop_fg(job head){
    job p = newJob(); //dummy head
    job temp = p;
    //int ret = 0;
    p->next = head;
    while (p->next != NULL){
        if (p->next->status == 0){ //if status is stopped
            free(temp);
            return p->next->job_id;
        }
        p = p->next;
    }
    free(temp);
    return p->job_id;
}

//check most recent stopped job 
int find_stop_bg(job head){
    job p = head;
    int ret = 0;
    while (p != NULL){
        if (p->status == 0){
            ret = p->job_id;
        }
        p = p->next;
    }
    return ret;
}


//delete a link with given job_id
job delete_job(job head, int id){
    if (id == 1){
        job p = head;
        head = p->next; 
        free(p);//free malloc
        if (head != NULL){
            renew_jobID(head);
        }
        return head;
    }
    job p = head;
    while (p->next != NULL){
        if (p->next->job_id == id){
            job temp = p->next;
            p->next = p->next->next;
            free(temp);
            break;
        }
        p = p->next;
    }
    if (head != NULL){
        renew_jobID(head); //update job id
    }
    return head;
}

//update the given node's pid array if there is certain child process finished
void removePid_at_array(job node, int pid){
    int j = 0, i = 0;
    for (i = 0; node->pids[i] > 0; i++){
        if (node->pids[i] == pid){
            j++; //remove cur node
        }
        node->pids[i] = node->pids[j];
        j++;
    }
    node->pids[j] = -100;
    node->children = node->children - 1;
}

//locate the specific job with job id, and return its command
job find_via_jobID(int id, job head){
    job p = head;
    while (p != NULL){
        if (p->job_id == id){
            return p;
        }
        p = p->next;
    }
    return NULL;
}

//insert a node at given location 
void insert_job(job head, job node, int location){
    if (location == 1){
        node->next = head;
        head = node;
    } else {
        job p = head;
        while (p->next->job_id != location){
            p = p->next;
        }
        node->next = p->next->next;
        p->next = node;
    }
    renew_jobID(head);
}

void treverse(job head){
    job p = head;
    while (p != NULL){
        printf("[%d] ", p->job_id);
        printf("%s  ", p->job_cmd);
        if (p->status < 0){
            printf("(Running)\n");
        }
        else if (p->status == 0){
            printf("(Stopped)\n");
        }
        else {
            printf("(terminated)\n");
        }
        p = p->next;
    }
}

