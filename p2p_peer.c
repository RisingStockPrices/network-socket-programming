// Client side C/C++ program to demonstrate Socket programming
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#define BUF_SIZE 100

int main(int argc, char const *argv[])
{
    int sock = 0, valread;
    int serv_sd, client_sd;
    int fd_max;
    int activity, str_len;
    int client_addr_size;
    struct sockaddr_in serv_addr, client_addr;
    struct sockaddr_in neighborList[3];

    char *alive_message = "ALIVE";
    char buffer[BUF_SIZE];
    char *message_type;
    char message[BUF_SIZE];

    fd_set fds;

    if (argc != 3)
    {
        printf("Usage: %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    sock = socket(PF_INET, SOCK_STREAM, 0);

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    //이부분 야매로 함 바꿀것
    bind(serv_sd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    listen(serv_sd, 4);

    //INITIALIZE FD
    FD_ZERO(&fds);
    FD_SET(serv_sd, &fds);
    fd_max = serv_sd;
    while (1)
    {

        if ((activity = select(fd_max + 1, &fds, NULL, NULL, NULL)) == -1)
        {
            printf("select error");
            break;
        }

        for (int i = 0; i < (fd_max + 1); i++)
        {
            if (FD_ISSET(i, &fds))
            {
                // 새로오는 걸 받는 거니까
                // tracker에서 오는 거면 neighbor list를 업데이트

                if (i == serv_sd)
                {
                    client_addr_size = sizeof(client_addr);
                    client_sd = accept(serv_sd, (struct sockaddr *)&client_addr, &client_addr_size);

                    if ((str_len = recv(client_sd, buffer, BUF_SIZE, 0)) == -1)
                    {
                        error_handling("ERROR: receiving message");
                    }
                    strcpy(message, buffer);
                    message_type = strtok(message, '/');
                }
            }
        }

        send(sock, alive_message, strlen(alive_message), 0);
        printf("ALIVE message sent\n");

        //get updated neighbor list

        //이웃들한테 chunk list request 메시지 보내기
        for (int i = 0; i < 3; i++)
        {
            //send LISTREQ

            //RECEIVE
        }

        sleep(1000);
    }

    close(sock);
    // valread = read( sock , buffer, 1024);
    // printf("%s\n",buffer );
    return 0;
}

void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}