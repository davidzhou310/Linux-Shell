#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#define MAX_STRING 1000
#define MAX_ARGS 100

struct jobs{
    int job_id;

    int pids[MAX_ARGS];

    char job_cmd[MAX_STRING];

    int status;

    int children;

    int group_id;

    struct jobs *next;
};

typedef struct jobs *job;

job newJob();

job addJob(job head, int *pid_array, char *command, const int cur_status, int group_id);

int find_stop_fg(job head);

int find_stop_bg(job head);

void renew_jobID(job head);

void printnPid(job node);

void removePid_at_array(job node, int pid);

job delete_job(job head, int id);

void insert_job(job head, job node, int location);

job find_via_jobID(int id, job head);

void treverse(job head);