#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>

#define shm_name "/shm_for_shell_and_schdr"

#define ERROR(msg) do { printf("%s\n", msg); } while(0)

typedef struct {
    char name[256];
    pid_t pid;
    int c_t;  
    int w_t;  
} p_info;

typedef struct {
    int ncpu;
    int t_slice;
    pid_t r_q[1024];
    int q_sz;
    p_info p[1024];
    int p_c;
} shared;

shared *shm;
pid_t sc_pid; 
volatile sig_atomic_t r_state = 1;

void handle_sigusr1(int sig) {}

volatile sig_atomic_t ctrl_c_ = 0;

void ctrl_c(int sig) {
    if (getpid() == getpgrp() && !ctrl_c_) {
        ctrl_c_ = 1;
        printf("\nEnter exit to terminate");
        printf("\nProcess summary:\n");
        for (int i = 0; i < shm->p_c; i++) {
            printf("Name: %s, PID: %d, Completion Time: %d, Wait Time: %d\n",
                   shm->p[i].name, shm->p[i].pid, shm->p[i].c_t , shm->p[i].w_t );
        }
        if (munmap(shm, sizeof(shared)) == -1) {
            ERROR("Error unmapping shared memory");
        }
        if (shm_unlink(shm_name) == -1) {
            ERROR("Error unlinking shared memory");
        }
        exit(0);
    }
}

void b_ctrl_c() {
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        ERROR("Error setting signal handler for SIGINT");
    }
}

void schd() {
    int f = 0;
    while (r_state || shm->q_sz > 0) {
        if (shm->q_sz == 0) {
            pause();
            continue;
        }

        int p_2_r = (shm->ncpu < shm->q_sz) ? shm->ncpu : shm->q_sz;
        pid_t runn_pid[p_2_r];

        for (int i = 0; i < p_2_r; i++) {
            int idx = (f + i) % shm->q_sz;
            pid_t pid = shm->r_q[idx];
            runn_pid[i] = pid;
            if (kill(pid, SIGCONT) == -1) {
                ERROR("Error continuing process");
            }
        }

        usleep(shm->t_slice * 1000);

        for (int j = 0; j < shm->p_c; j++) {
            for (int k = 0; k < p_2_r; k++) {
                if (shm->p[j].pid == runn_pid[k]) {
                    shm->p[j].c_t += shm->t_slice;  
                }
            }
        }

        for (int i = 0; i < shm->p_c; i++) {
            int is_runn = 0;
            for (int j = 0; j < p_2_r; j++) {
                if (shm->p[i].pid == runn_pid[j]) {
                    is_runn = 1;  
                    break;
                }
            }
            if (!is_runn) {
                shm->p[i].w_t += shm->t_slice;  
            }
        }

        for (int i = 0; i < p_2_r; i++) {
            int idx = (f + i) % shm->q_sz;
            pid_t pid = shm->r_q[idx];

            if (waitpid(pid, NULL, WNOHANG) > 0) {
                shm->q_sz--;  
                for (int j = idx; j < shm->q_sz; j++) {
                    shm->r_q[j] = shm->r_q[j + 1];  
                }
                i--;  
            } 
            else {
                if (kill(pid, SIGSTOP) == -1) {
                    ERROR("Error stopping process");
                }
                shm->r_q[(f + shm->q_sz) % 1024] = pid;  
            }
        }

        if (shm->q_sz > 0) {
            f = (f + 1) % 1024;  
        }
    }
}

void strt_shm(int ncpu, int t_slice) {
    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        ERROR("Error opening shared memory");
        exit(EXIT_FAILURE);
    }
    if (ftruncate(shm_fd, sizeof(shared)) == -1) {
        ERROR("Error truncating shared memory");
        exit(EXIT_FAILURE);
    }
    shm = mmap(0, sizeof(shared), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED) {
        ERROR("Error mapping shared memory");
        exit(EXIT_FAILURE);
    }
    shm->ncpu = ncpu;
    shm->t_slice = t_slice;
    shm->q_sz = 0;
    shm->p_c = 0;
}

void run_cmd(char *cmd) {
    pid_t pid = fork();
    if (pid == 0) {  
        b_ctrl_c();
        pause();  
        execl(cmd, cmd, NULL);
        ERROR("exec failed");  
        exit(EXIT_FAILURE);
    } 
    else if (pid > 0) {  
        strcpy(shm->p[shm->p_c].name, cmd);
        shm->p[shm->p_c].pid = pid;
        shm->p[shm->p_c].c_t = 0;
        shm->p[shm->p_c].w_t = 0;
        shm->r_q[shm->q_sz++] = pid;
        shm->p_c++;
        printf("Process '%s' with PID %d Submitted.\n", cmd, pid);

        if (kill(sc_pid, SIGUSR1) == -1) {
            ERROR("Error sending SIGUSR1 to scheduler");
        }

        if (fork() == 0) {  
            int pp_f[2];
            if (pipe(pp_f) == -1) {
                ERROR("Error creating pipe");
                exit(EXIT_FAILURE);
            }
            if (fork() == 0) {  
                close(pp_f[0]);
                dup2(pp_f[1], STDOUT_FILENO);
                execl(cmd, cmd, NULL);
                ERROR("exec failed");
                exit(EXIT_FAILURE);
            } 
            else {  
                close(pp_f[1]);
                char bfr[1024];
                ssize_t nbt;
                while ((nbt = read(pp_f[0], bfr, sizeof(bfr) - 1)) > 0) {
                    bfr[nbt] = '\0';
                    printf("%s", bfr);
                    fflush(stdout);
                }
                close(pp_f[0]);
                wait(NULL);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <NCPU> <TSLICE>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int ncpu = atoi(argv[1]);
    int t_slice = atoi(argv[2]);
    strt_shm(ncpu, t_slice);
    signal(SIGINT, ctrl_c);
    signal(SIGUSR1, handle_sigusr1);
    if ((sc_pid = fork()) == 0) {
        b_ctrl_c();
        schd();  
        if (munmap(shm, sizeof(shared)) == -1) {
            ERROR("Error unmapping shared memory");
        }
        exit(0);
    }
    char cmd[256];
    while (1) {
        printf("OnePiece-Shell$>>> ");
        if (!fgets(cmd, sizeof(cmd), stdin)) break;
        cmd[strcspn(cmd, "\n")] = 0; 
        if (strcmp(cmd, "exit") == 0) break;
        else if (strncmp(cmd, "submit ", 7) == 0) {
            run_cmd(cmd + 7);
        }
    }
    kill(sc_pid, SIGTERM);
    waitpid(sc_pid, NULL, 0);
    return 0;
}
