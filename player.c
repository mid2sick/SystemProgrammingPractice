#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>

int main(int argc, char *argv[])
{
    int player = atoi(argv[1]), i, k;
    char money[10], from_host[10];
    snprintf(money, sizeof(money), "%d %d\n", player, player * 100);
    if (argc != 2) {
        fprintf(stderr, "usage: parameters error\n");
        exit(1);
    }
    for(i = 0; i < 10; i++)
    {
        if(i)
        {
            do
            {
                k = read(STDIN_FILENO, from_host, sizeof(from_host));
            }while(!k);
        }
        write(STDOUT_FILENO, money, strlen(money));
    }
    exit(0);
}