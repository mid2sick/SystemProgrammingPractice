#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <wait.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>

void err_sys(const char* x) 
{ 
    perror(x);
    exit(1); 
}

int main(int argc, char *argv[])
{
    int host_ID = atoi(argv[1]);
    int host_fifo_fd, host0_fifo_fd, depth, cnt = 1, child[2], len;
    int i, j, cur_host;
    FILE *host0_file, *host_file;
    int write_pipe[2][2], read_pipe[2][2];
    char arr[80], host_fifo[80];
    char buf1[80], buf2[80];
    if (argc != 4) {
        fprintf(stderr, "usage: do not have enough parameters\n");
        exit(1);
    }
    depth = atoi(argv[3]);
    snprintf(host_fifo, sizeof(host_fifo), "Host%d.FIFO", host_ID);

    for(i = 0; i < 2; i++)
    {
        pipe(read_pipe[i]);
        pipe(write_pipe[i]);
    }

    if(depth == 0)
    {
        if( (child[0] = fork()) == 0 || (child[1] = fork()) == 0 ) //child_host
        {
            argv[3] = "1";
            if(!child[0]) cur_host = 0;
            else cur_host = 1;

            close(read_pipe[cur_host][0]);
            dup2(read_pipe[cur_host][1], STDOUT_FILENO);
            close(read_pipe[cur_host][1]);

            close(write_pipe[cur_host][1]);
            dup2(write_pipe[cur_host][0], STDIN_FILENO);
            close(write_pipe[cur_host][0]);
            if(execv("./host", argv) < 0)
            {
                err_sys("exec error");
                exit(0);
            }
        }
        else //root_host
        {
            host_file = fopen(host_fifo, "r");
            host_fifo_fd = fileno(host_file);
            host0_file = fopen("Host.FIFO", "w");
            host0_fifo_fd = fileno(host0_file);
            for(i = 0; i < 2; i++)
            {
                close(read_pipe[i][1]);
                close(write_pipe[i][0]);
            }
            char ori[80], w[5], to_fifo[80], *split;
            int winner_ID, player[8];
            while(fgets(arr,sizeof(arr), host_file))
            {
                memset(ori, 0, sizeof(ori));
                strncpy(ori, arr, sizeof(arr));
                split = strtok(arr, " ");
                for(i = 0; i < 8; i++)
                {
                    player[i] = atoi(split);
                    split = strtok(NULL, " ");
                }
                
                snprintf(buf1, sizeof(buf1), "%d %d %d %d\n", player[0], player[1], player[2], player[3]);
                snprintf(buf2, sizeof(buf2), "%d %d %d %d\n", player[4], player[5], player[6], player[7]);

                if(player[0] == -1)
                {
                    write(write_pipe[0][1], "-1 -1 -1 -1\n", strlen("-1 -1 -1 -1\n"));
                    write(write_pipe[1][1], "-1 -1 -1 -1\n", strlen("-1 -1 -1 -1\n"));
                    fflush(NULL);
                    wait(NULL);
                    exit(0);
                }
                write(write_pipe[0][1], buf1, strlen(buf1));
                write(write_pipe[1][1], buf2, strlen(buf2));
                char get[2][10], winner[20], win_money[10], *p[2], temp[20];
                int p_money[2];
                for(i = 1; i < 10; i++)
                {
                    memset(get, 0, sizeof(get));
                    read(read_pipe[0][0], get[0], sizeof(get[0]));
                    read(read_pipe[1][0], get[1], sizeof(get[1]));
                    for(j = 0; j < 2; j++)
                    {
                        p[j] = strtok(get[j], " ");
                        p[j] = strtok(NULL, " ");
                        p_money[j] = atoi(p[j]);
                    }
                    if(p_money[0] > p_money[1]) strncpy(winner, get[0], sizeof(get[0]));
                    else strncpy(winner, get[1], sizeof(get[1]));
                    strncpy(w, winner, 2);
                    strcat(w, "\n");
                    winner_ID = atoi(w);
                    write(write_pipe[0][1], w, strlen(w));
                    write(write_pipe[1][1], w, strlen(w));
                    fflush(NULL);
                }
                memset(get, 0, sizeof(get));
                read(read_pipe[0][0], get[0], sizeof(get[0]));
                read(read_pipe[1][0], get[1], sizeof(get[1]));
                int rank[8];
                for(i = 0; i < 8; i++)
                {
                    if(player[i] == winner_ID) rank[i] = 1;
                    else rank[i] = 2;
                }
                memset(to_fifo, 0 ,sizeof(to_fifo));
                strcpy(to_fifo, argv[2]);
                strcat(to_fifo, "\n");
                for(i = 0; i < 8; i++)
                {
                    snprintf(temp, sizeof(temp), "%d %d\n", player[i], rank[i]);
                    strcat(to_fifo, temp);
                }
                write(host0_fifo_fd, to_fifo, strlen(to_fifo));
                fflush(NULL);
                memset(arr, 0, sizeof(arr));
            }    
        }
    }

    if(depth == 1)
    {
        if((child[0] = fork()) == 0 || (child[1] = fork()) == 0)
        {
            argv[3] = "2";
            if(!child[0]) cur_host = 0;
            else cur_host = 1;

            close(read_pipe[cur_host][0]);
            dup2(read_pipe[cur_host][1], STDOUT_FILENO);
            close(read_pipe[cur_host][1]);
            

            close(write_pipe[cur_host][1]);
            dup2(write_pipe[cur_host][0], STDIN_FILENO);
            close(write_pipe[cur_host][0]);

            if(execv("./host", argv) < 0)
            {
                err_sys("exec error");
                exit(0);
            }
        }
        else
        {
            for(i = 0; i < 2; i++)
            {
                close(read_pipe[i][1]);
                close(write_pipe[i][0]);
            }
            memset(arr, 0, sizeof(arr));
            
            while(read(STDIN_FILENO, arr, sizeof(arr)))
            {
                char *split;
                int player[4];
                split = strtok(arr, " ");
                for(i = 0; i < 4; i++)
                {
                    player[i] = atoi(split);
                    split = strtok(NULL, " ");
                }

                if(player[0] == -1)
                {
                    write(write_pipe[0][1], "-1 -1\n", strlen("-1 -1\n"));
                    write(write_pipe[1][1], "-1 -1\n", strlen("-1 -1\n"));
                    wait(NULL);
                    exit(0);
                }
                
                snprintf(buf1, sizeof(buf1), "%d %d\n", player[0], player[1]);
                snprintf(buf2, sizeof(buf2), "%d %d\n", player[2], player[3]);

                write(write_pipe[0][1], buf1, strlen(buf1));
                write(write_pipe[1][1], buf2, strlen(buf2));

                char get[2][10];

                read(read_pipe[0][0], get[0], sizeof(get[0]));
                read(read_pipe[1][0], get[1], sizeof(get[1]));

                char *p[2], winner[20], cp[2][10];
                int p_money[2];
                strncpy(cp[0], get[0], sizeof(get[0]));
                strncpy(cp[1], get[1], sizeof(get[1]));
                for(j = 0; j < 2; j++)
                {
                    p[j] = strtok(get[j], " ");
                    p[j] = strtok(NULL, " ");
                    p_money[j] = atoi(p[j]);
                }
                if(p_money[0] > p_money[1]) strncpy(winner, cp[0], sizeof(cp[0]));
                else strncpy(winner, cp[1], sizeof(cp[1]));
                write(STDOUT_FILENO, winner, strlen(winner));

                for(i = 1; i < 10; i++)
                {
                    memset(arr, 0, sizeof(arr));
                    read(STDIN_FILENO, arr, sizeof(arr));
                    write(write_pipe[0][1], arr, strlen(arr));
                    write(write_pipe[1][1], arr, strlen(arr));
                    memset(p, 0, sizeof(p));
                    memset(winner, 0, sizeof(winner));
                    memset(get, 0, sizeof(get));
                    memset(cp, 0, sizeof(cp));
                    read(read_pipe[0][0], get[0], sizeof(get[0]));
                    read(read_pipe[1][0], get[1], sizeof(get[1]));
                    strncpy(cp[0], get[0], sizeof(get[0]));
                    strncpy(cp[1], get[1], sizeof(get[1]));

                    for(j = 0; j < 2; j++)
                    {
                        p[j] = strtok(get[j], " ");
                        p[j] = strtok(NULL, " ");
                        p_money[j] = atoi(p[j]);
                    }
                    if(p_money[0] > p_money[1]) strncpy(winner, cp[0], sizeof(cp[0]));
                    else strncpy(winner, cp[1], sizeof(cp[1]));

                    write(STDOUT_FILENO, winner, strlen(winner));
                    
                }
            }
        }        
    }

    if(depth == 2)
    {
        if(!child[0]) cur_host = 0;
        else cur_host = 1;
        char get[2][10];
        while(read(STDIN_FILENO, arr, sizeof(arr)))
        {
            int rp[2][2], wp[2][2];
            for(i = 0; i < 2; i++)
            {
                pipe(rp[i]);
                pipe(wp[i]);
            }
            if(atoi(arr) == -1) exit(0);

            if((child[0] = fork()) == 0 || (child[1] = fork()) == 0) // generate player process
            {
                char send_num[10];
                if(!child[0]) cur_host = 0;
                else cur_host = 1;

                close(rp[cur_host][0]);
                dup2(rp[cur_host][1], STDOUT_FILENO);
                close(rp[cur_host][1]);

                close(wp[cur_host][1]);
                dup2(wp[cur_host][0], STDIN_FILENO);
                close(wp[cur_host][0]);

                read(STDIN_FILENO, send_num, sizeof(send_num));

                char* argv2[] = {"./player", send_num, NULL};
                if(execv("./player", argv2) < 0)
                {
                    err_sys("exec error");
                    exit(0);
                }
            }
            else
            {
                
                for(i = 0; i < 2; i++)
                {
                    close(rp[i][1]);
                    close(wp[i][0]);
                }
                
                char *split;
                int player[2];
                split = strtok(arr, " ");
                for(i = 0; i < 2; i++)
                {
                    player[i] = atoi(split);
                    split = strtok(NULL, " ");
                }

                snprintf(buf1, sizeof(buf1), "%d\n", player[0]);
                snprintf(buf2, sizeof(buf2), "%d\n", player[1]);
                waitpid(-1, NULL,WNOHANG);
                write(wp[0][1], buf1, strlen(buf1));
                write(wp[1][1], buf2, strlen(buf2));

                memset(get, 0, sizeof(get));
                read(rp[0][0], get[0], sizeof(get[0]));
                read(rp[1][0], get[1], sizeof(get[1]));
                char *p[2], winner[20], cp[2][20];
                int p_money[2];
                strncpy(cp[0], get[0], sizeof(get[0]));
                strncpy(cp[1], get[1], sizeof(get[1]));
                for(j = 0; j < 2; j++)
                {
                    p[j] = strtok(get[j], " ");
                    p[j] = strtok(NULL, " ");
                    p_money[j] = atoi(p[j]);
                }

                if(p_money[0] > p_money[1]) strncpy(winner, cp[0], sizeof(cp[0]));
                else strncpy(winner, cp[1], sizeof(cp[1]));
                write(STDOUT_FILENO, winner, strlen(winner));

                
                for(i = 1; i < 10; i++)
                {
                    memset(arr, 0, sizeof(arr));
                    read(STDIN_FILENO, arr, sizeof(arr));
                    write(wp[0][1], arr, strlen(arr));
                    write(wp[1][1], arr, strlen(arr));
                    memset(get, 0, sizeof(get));
                    memset(p, 0, sizeof(p));
                    memset(winner, 0, sizeof(winner));
                    memset(cp, 0, sizeof(cp));
                    read(rp[0][0], get[0], sizeof(get[0]));
                    read(rp[1][0], get[1], sizeof(get[1]));
                    strncpy(cp[0], get[0], sizeof(get[0]));
                    strncpy(cp[1], get[1], sizeof(get[1]));

                    for(j = 0; j < 2; j++)
                    {
                        p[j] = strtok(get[j], " ");
                        p[j] = strtok(NULL, " ");
                        p_money[j] = atoi(p[j]);
                    }

                    if(p_money[0] > p_money[1]) strncpy(winner, cp[0], sizeof(cp[0]));
                    else strncpy(winner, cp[1], sizeof(cp[1]));

                    write(STDOUT_FILENO, winner, strlen(winner));

                }
            }
        }
        
    }
}
