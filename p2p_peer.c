// Client side C/C++ program to demonstrate Socket programming
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#define BUF_SIZE 100
#define MAX_NEIGHBOR 3

int get_nlist(int sockfd, struct sockaddr_in *neighbor_list);

int main(int argc, char const *argv[])
{
    int serv_sd, client_sd, tracker_sd;
    int fd_max;
    int fd_num, str_len;
    int clnt_adr_sz;
    int neighbor_size;
    struct sockaddr_in serv_addr, client_adr, tracker_addr;
    struct sockaddr_in neighbor_list[3];

    char *alive_message = "ALIVE";
    char buffer[BUF_SIZE];
    char *message_type;
    char message[BUF_SIZE];
    struct timeval timeout;

    fd_set fds;

    if (argc != 4)
    {
        printf("Usage: %s <tracker IP> <tracker port> <own port>\n", argv[0]);
        exit(1);
    }

    //소켓 생성
    serv_sd = socket(PF_INET, SOCK_STREAM, 0);
    tracker_sd = socket(PF_INET, SOCK_STREAM, 0);

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[3]));

    memset(&tracker_addr, 0, sizeof(tracker_addr));
    tracker_addr.sin_family = AF_INET;
    tracker_addr.sin_addr.s_addr = inet_addr(argv[1]);
    tracker_addr.sin_port = htons(atoi(argv[2]));

    int result = connect(tracker_sd, (struct sockaddr*)&tracker_addr, sizeof(tracker_addr));

    bind(serv_sd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    listen(serv_sd, MAX_NEIGHBOR+2);    

    // fd table 초기화
    FD_ZERO(&fds);
    FD_SET(serv_sd, &fds);
    FD_SET(tracker_sd, &fds);
    fd_max = serv_sd;

    //TODO: timeout 설정
    timeout.tv_sec = 1;
    timeout.tv_usec = 1000;
    while (1)
    {
        sleep(1);
        send(tracker_sd, alive_message, strlen(alive_message), 0);
        printf("ALIVE message sent\n");

        if ((fd_num = select(fd_max + 1, &fds, NULL, NULL, &timeout)) == -1)
        {
            printf("select error");
            break;
        }
        else if(fd_num==0)
        {
            printf("timeout\n");
        }

        for (int i = 0; i < (fd_max + 1); i++)
        {
            if (FD_ISSET(i, &fds))
            {
                if (i == serv_sd)
                {
                    clnt_adr_sz = sizeof(client_adr);
                    client_sd = accept(serv_sd, (struct sockaddr *)&client_adr, &clnt_adr_sz);

                    FD_SET(client_sd, &fds);
                    if(fd_max<client_sd)
                        fd_max = client_sd;
                }
                else {
                    //read message
                    if ((str_len = recv(i, buffer, BUF_SIZE, 0)) == -1)
                    {
                        error_handling("ERROR: receiving message");
                    }
                    strcpy(message, buffer);
                    message_type = strtok(message, '/');
                    
                    // tracker가 neighbor list를 보내줌 : type == NLIST ?
                    if (strcmp(message_type,"NLIST")==0)
                    {
                        // parse message
                        neighbor_size = get_nlist(i, neighbor_list);

                        // update fd table (reset)
                        for (int j=0;j<fd_max;j++){
                            if(j!=0 && j!=serv_sd){
                                FD_CLR(j,&fds);
                                close(j);
                            }
                        }
                        // update neighbor address list


                        // request chunk list to neighbors
                        
                    }
                    // neighbor가 보유 chunk 목록을 요청 : type == CRQST ?
                    else if (strcmp(message_type,"LRQST")==0) {
                        // 소유 chunk offset list를 serialize
                        
                        // serialized된 list를 neighbor에 보내기
                    }
                    // neighbor가 특정 chunk 데이터를 요청 : type == DRQST ?
                    else if (strcmp(message_type, "DRQST")==0){
                        
                    }
                    // neighbor가 chunk 데이터 목록을 응답해옴 : type == LRESP ?
                    else if (strcmp(message_type, "LRESP")==0){
                        
                    }
                    // neighbor가 요청한 데이터를 응답해옴 : type == DRESP ?
                    else if (strcmp(message_type, "DRESP")==0){
                        
                    }
                    else {
                        // invalid message
                        error_handling("ERROR: survival from peer");
                    }
                }
            }
        }

        
    }

    close(tracker_sd);
    close(serv_sd);
    return 0;
}

int get_nlist(int sockfd, struct sockaddr_in *neighbor_list) {
    char message[BUF_SIZE];
    char * buf;
    int neighbor_size;
    int n;

    recv(sockfd, message, BUF_SIZE, 0);
    buf = strtok(message, "/");
    neighbor_size = atoi(buf);
    for (n = 0; n < neighbor_size; n++) {
        buf = strtok(NULL, "/");
        printf("neighbor[%d]: %s\n", n, buf);
        inet_pton(AF_INET, buf, &(neighbor_list[n].sin_addr));
    }

    return neighbor_size;
}

void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}