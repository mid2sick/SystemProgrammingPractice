#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#define MAXN 15
#define Q_NUM 3100
#define RANDMAX 65536

int host_num, player_num, data_cnt = 0;
char myfifo[80];
char Host_FIFO[MAXN][80];
char data[Q_NUM][30];

struct p
{
    int id, score, rank;
}player[60];

int comp_score(const void *a, const void *b)
{
    struct p *a1 = (struct p *)a;
    struct p *b1 = (struct p *)b;
    if(a1->score != b1->score) return a1->score < b1->score;
    else return a1->id > b1->id;
}

int comp_ID(const void *a, const void *b)
{
    struct p *a1 = (struct p *)a;
    struct p *b1 = (struct p *)b;
    return a1->id > b1->id;
}

void err_sys(const char* x) 
{ 
    perror(x);
    exit(1); 
}

void run(int cur, int cnt, int a[])
{
    int i;
    char arr[256];
    a[cnt++] = cur;
    if(cnt == 8)
    {
        snprintf(arr, sizeof(arr), "%d %d %d %d %d %d %d %d\n", a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
        strncpy(data[data_cnt], arr, sizeof(arr));
        data_cnt++;
        return;
    }
    for(i = cur+1; i <= player_num; i++)
        run(i, cnt, a);
}

int main(int argc,char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "usage: too many or too few parameters\n");
        exit(1);
    }

    int avail[Q_NUM]; //impliment a queue that records which host is available
    int fnt = 0, rear = 0, cnt = 0; // record queue's front and rear and elements in queue
    int a[8], host_rand[MAXN], host_fd[MAXN], fd, fifo_left = 0;
    int avail_host, host_fifo, i, k;
    char arr[80], num_to_child[10], rand_to_child[10];
    FILE * Host_FIFO_file; // file descriptor for Host.FIFO
    pid_t pid;

    host_num = atoi(argv[1]);
    player_num = atoi(argv[2]);

    for(i = 1; i <= host_num; i++)
    {
        avail[rear] = i;
        rear++;
        cnt++;
    }

    for(i = 1; i <= player_num; i++)
    {
        player[i].id = i;
        player[i].score = 0;
    }
    player[0].id = 0;  //avoid player[0] be sorted later
    player[0].score = -1;

    snprintf(myfifo, sizeof(myfifo), "Host.FIFO");
    if( mkfifo(myfifo, 0666) == -1) err_sys("make Host.FIFO error");

    for(i = 1; i <= host_num; i++)
    {
        snprintf(Host_FIFO[i], sizeof(Host_FIFO[i]), "Host%d.FIFO", i);
        mkfifo(Host_FIFO[i], 0666);
        host_rand[i] = rand() % RANDMAX;  //host_rand records the rand number of each root host
        snprintf(num_to_child,sizeof(num_to_child), "%d", i);
        snprintf(rand_to_child, sizeof(rand_to_child), "%d", host_rand[i]);
        if((pid = fork()) < 0)
            err_sys("fork error");
        else if(pid == 0)
        {
            char* argv2[] = {"./host", num_to_child, rand_to_child, "0", NULL};
            if(execv("./host", argv2) < 0)
            {
                err_sys("exec error");
                exit(0);
            }
        }
        host_fd[i] = open(Host_FIFO[i], O_WRONLY);
        unlink(Host_FIFO[i]);
    }
    for(i = 1; i <= player_num; i++)  //C(player_num, 8)
        run(i, 0, a);
    Host_FIFO_file = fopen(myfifo, "r");
    unlink(myfifo);
    host_fifo = fileno(Host_FIFO_file);

    while(data_cnt)  //when there is still some competitions haven't been given to hosts
    {
        int player_ID, player_rank;
        char *arr2;
        if(cnt > 0)  //if there is available host to get a new competition
        {
            fifo_left++;  //fifo_left records the number of hosts that haven't send data to bidding system
            data_cnt--;
            avail_host = avail[fnt]; //get the available host from queue
            fnt++, cnt--;
            write(host_fd[avail_host], data[data_cnt], strlen(data[data_cnt]));
        }
        else
        {
            fifo_left--;
            memset(arr, 0, sizeof(arr));
    
            fgets(arr, sizeof(arr), Host_FIFO_file);
            int find_host, find_rand = atoi(arr);

            for(i = 1; i <= host_num; i++) // find the host by the random number
            {
                if(host_rand[i] == find_rand)
                {
                    find_host = i;
                    break;
                }
            }
            avail[rear++] = find_host; // put the host into the queue
            cnt++;
            

            for(i = 0; i < 8; i++)  // read the data send by the host
            {
                fgets(arr, sizeof(arr), Host_FIFO_file);
                //printf("at bidding system arr = %s", arr);
                arr2 = strtok(arr, " ");
                player_ID = atoi(arr2);
                arr2 = strtok(NULL, " ");
                player_rank = atoi(arr2);
                if(player_rank < 8) player[player_ID].score += (8 - player_rank);
            }
        }
    }

    while(fifo_left--)  //though all the data is given to hosts, there are still some hosts' feedback hasn't been read
    {
        int player_ID, player_rank;
        char *arr2;
        memset(arr, 0, sizeof(arr));
        fgets(arr, sizeof(arr), Host_FIFO_file);
        int find_host, find_rand = atoi(arr);
        for(i = 1; i <= host_num; i++)
        {
            if(host_rand[i] == find_rand)
            {
                find_host = i;
                break;
            }
        }

        for(i = 0; i < 8; i++)
        {
            memset(arr, 0, sizeof(arr));
            fgets(arr, sizeof(arr), Host_FIFO_file);
            //printf("at bidding system arr = %s", arr);
            arr2 = strtok(arr, " ");
            player_ID = atoi(arr2);
            arr2 = strtok(NULL, " ");
            player_rank = atoi(arr2);
            if(player_rank < 8) player[player_ID].score += (8 - player_rank);
        }
    }

    qsort(player, player_num + 1, sizeof(player[0]), comp_score); //sort the player structure by score to find their rank

    player[0].rank = 1;
    for(i = 1; i < player_num; i++)
    {
        if(player[i].score == player[i-1].score) player[i].rank = player[i-1].rank;
        else player[i].rank = player[i-1].rank + 1;
    }

    qsort(player, player_num + 1, sizeof(player[0]), comp_ID);  //sort the player structure by ID

    for(i = 1; i <= player_num; i++)
        printf("%d %d\n", player[i].id, player[i].rank);

    snprintf(arr, sizeof(arr), "-1 -1 -1 -1 -1 -1 -1 -1\n");
    for(i = 1; i <= host_num; i++)
    {
        write(host_fd[i], arr, strlen(arr));
        wait(NULL);
    }

    close(host_fifo);
    waitpid(-1, NULL, WNOHANG);
    return 0;
}