#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 30
#define MAX_CLIENT 10
void error_handling(char *message);

int main(int argc, char *argv[])
{

    //각 소켓의 번호
    int serv_sd, client_sd;
    int fd_max;
    int activity, strlen;
    char buf[BUF_SIZE];
    char message[BUF_SIZE];

    struct sockaddr_in serv_adr, clnt_adr[MAX_CLIENT];

    //list of all peers
    struct sockaddr_in registeredPeers[MAX_CLIENT];

    socklen_t clnt_adr_sz;

    fd_set readfds;

    if (argc < 2)
    {
        printf("Usage: %s <port>", argv[0]);
        exit(1);
    }

    //소켓 생성
    serv_sd = socket(PF_INET, SOCK_STREAM, 0);

    //server address을 null 로 초기화
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    bind(serv_sd, (struct sockaddr *)&serv_adr, sizeof(serv_adr));
    listen(serv_sd, 10);

    FD_ZERO(&readfds);
    //server socket fd에 등록
    FD_SET(serv_sd, &readfds);
    fd_max = serv_sd;

    while (1)
    {
        if ((activity = select(fd_max + 1, &readfds, NULL, NULL, NULL)) == -1)
        {
            printf("select error");
            break;
        }

        for (int i = 0; i < fd_max + 1; i++)
        {
            if (FD_ISSET(i, &readfds))
            {
                //server socket에서 일어난 일
                if (i == serv_sd)
                {
                    clnt_adr_sz = sizeof(clnt_adr[fd_max]);
                    //incoming peer일때 address 받아옴,,,??
                    client_sd = accept(serv_sd, (struct sockaddr *)&clnt_adr[fd_max], &clnt_adr_sz);

                    //save client address

                    //send neighbor list
                    if ((strlen = recv(client_sd, buf, BUF_SIZE, 0)) == -1)
                    {
                        error_handling("ERROR: receiving message from peers");
                    }
                    strcpy(message, buf);

                    printf("%s\n", message);
                    if (strcmp("ALIVE", message) == 0)
                    {
                        // update registeredPeeers - 이미 하고 있는 듯? (clnt_adr[] list)

                        // 새로운 neightbor list 보내주기
                    }
                    else
                    {
                        error_handling("ERROR: survival from peer");
                    }

                    FD_SET(client_sd, &readfds);
                    if (fd_max < client_sd)
                    {
                        fd_max = client_sd;
                    }
                }
                else
                {
                    //event occured to one of the clients
                    strlen = read(i, buf, BUF_SIZE);
                    if (strlen == 0)
                    {
                        //close reqest??
                    }
                    else
                    {
                        //생존 신고 보내왔음
                        printf("%s\n", buf);
                    }
                }
            }
        }
    }

    close(serv_sd);
    return 0;
}

// int send_neighbor_list(struct sockaddr_in peer, struct sockaddr_in registered, fd_set)
// {

// }

void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
