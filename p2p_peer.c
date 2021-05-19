// Client side C/C++ program to demonstrate Socket programming
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#define BUF_SIZE 100
#define MAX_NEIGHBOR 3
#define MAX_CHUNK_NUMBER 100
#define CHUNK_SIZE 32

int get_nlist(int sockfd, char* message, struct sockaddr_in *neighbor_list);

int main(int argc, char const *argv[])
{
    int serv_sd, client_sd, tracker_sd, neighbor_sd[MAX_NEIGHBOR];
    int fd_max;
    int fd_num, str_len;
    int clnt_adr_sz;
    int neighbor_size;
    struct sockaddr_in serv_addr, client_adr, tracker_addr;
    struct sockaddr_in neighbor_list[3];

    char alive_message[BUF_SIZE] = "ALIVE";
    char *lrqst_message = "LRQST";
    char *drqst_message = "DRQST";
    char *sresp_message = "LRESP";
    char *dresp_message = "DRESP";
    char *temp_filename = "happy.txt";
    FILE* file;
    char buffer[BUF_SIZE];
    char *message_type;
    char* temp;
    char message[BUF_SIZE];
    struct timeval timeout;

    char chunk_offset_list[MAX_CHUNK_NUMBER];
    char* data_list[MAX_CHUNK_NUMBER];

    fd_set fds, cpy_fds;

    if((file = fopen(temp_filename,"r")!=NULL))
    {
        // 다 있는 걸로 CHUNK_OFFSET_LIST 췤
        memset(&chunk_offset_list, 1, sizeof(chunk_offset_list));
    } else {
        // 파일 없으므로 CHUNK_OFFSET_LIST 0으로 초기화
        memset(&chunk_offset_list, 0, sizeof(chunk_offset_list));
    }

    
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

    bind(serv_sd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    listen(serv_sd, MAX_NEIGHBOR+2);    

    connect(tracker_sd, (struct sockaddr*)&tracker_addr, sizeof(tracker_addr));

    // fd table 초기화
    FD_ZERO(&fds);
    FD_SET(serv_sd, &fds);
    FD_SET(tracker_sd, &fds);
    fd_max = tracker_sd;

    strcat(alive_message, argv[3]);
    send(tracker_sd, alive_message, sizeof(alive_message), 0);
    printf("ALIVE message sent\n");

    while (1)
    {
        cpy_fds = fds;

        if ((fd_num = select(fd_max + 1, &cpy_fds, NULL, NULL, NULL)) == -1)
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
            
            if (FD_ISSET(i, &cpy_fds))
            {
                //printf("fd %d is set\n", i);
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
                    if ((str_len = recv(i, buffer, sizeof(buffer), 0)) == -1)
                    {
                        error_handling("ERROR: receiving message");
                    }
                    
                    strcpy(message, buffer);
                    //printf("event happened\n");
                    printf("message: %s\n", message);

                    message_type = strtok(message, "/");
                   // printf("message_type: %s\n", message_type);
                    
                    // tracker가 neighbor list를 보내줌 : type == NLIST ?
                    if (strcmp(message_type,"NLIST")==0)
                    {
                        // parse message
                        neighbor_size = get_nlist(i, strtok(NULL, ""), neighbor_list);

                        // update fd table (reset)
                        for (int j=0;j<fd_max;j++){
                            if(j!=0 && j!=serv_sd){
                                FD_CLR(j,&fds);
                                close(j);
                            }
                        }
                        for (int j=0; j<neighbor_size; j++) {
                            neighbor_sd[j] = socket(PF_INET, SOCK_STREAM, 0);
                            
                            /*
                            if (connect(neighbor_sd[j], (struct sockaddr*)&neighbor_list[j], sizeof(neighbor_list[j])) == -1) {
                                // 받은 neighbor랑 연결이 안되면?
                            }*/

                            // neighbor에게 LRQST 요청
                            strcpy(message, lrqst_message);
                            send(neighbor_sd[j], message, sizeof(message), 0);
                            printf("Sent LRQST message to socket fd %d\n",neighbor_sd[j]);
                        }
                        
                    }
                    // neighbor가 보유 chunk 목록을 요청 : type == CRQST ?
                    else if (strcmp(message_type,"LRQST")==0) {
                        printf("Received LRQST message from %s %d", neighbor_list[i].sin_addr, neighbor_list[i].sin_port);
                        // 소유 chunk offset list를 serialize
                        // 메시지 format: LRESP/(ofset1)/(offset2)/../(마지막 offset)///
                        memset(&buffer,0, sizeof(buffer));
                        sprintf(buffer,"LRESP/");
                        for(int i=0;i<MAX_CHUNK_NUMBER;i++){
                            if(chunk_offset_list[i]==1)
                                sprintf(buffer,"%d/",i);
                        }
                        sprintf(buffer,"//");
                        send(i, buffer,sizeof(buffer), 0);
                        printf("Sent LRESP message to %s %d", neighbor_list[i].sin_addr, neighbor_list[i].sin_port);
                        // serialized된 list를 neighbor에 보내기
                    }
                    // neighbor가 특정 chunk 데이터를 요청 : type == DRQST ?
                    else if (strcmp(message_type, "DRQST")==0){
                        
                    }
                    // neighbor가 chunk 데이터 목록을 응답해옴 : type == LRESP ?
                    else if (strcmp(message_type, "LRESP")==0){
                        // parse list
                        strtok(message,"/");
                        do{
                            temp = strtok(NULL, "/");
                            printf("%d\n",temp);
                        }while(strcmp(temp,"/")!=0);
                        
                        //memset(&buffer,0, sizeof(buffer));
                        //sprintf(buffer,"DRQST/");
                        //request data
                    }
                    // neighbor가 요청한 데이터를 응답해옴 : type == DRESP ?
                    else if (strcmp(message_type, "DRESP")==0){
                        
                    }
                    else {
                        printf("invalid message\n");
                        //error_handling("ERROR: survival from peer");
                    }
                }
            }
        }

        
    }

    close(tracker_sd);
    close(serv_sd);
    return 0;
}

int get_nlist(int sockfd, char* message, struct sockaddr_in *neighbor_list) {
    // char message[BUF_SIZE];
    char * buf;
    int neighbor_size;
    int n;

    printf("message from get_nlist: %s\n", message);
    buf = strtok(message, "/");
    neighbor_size = atoi(buf);
    //printf("neighbor size: %d\n", neighbor_size);
    for (n = 0; n < neighbor_size; n++) {
        buf = strtok(NULL, "/");
        //printf("neighbor[%d]: %s\n", n, buf);
        inet_pton(AF_INET, buf, &(neighbor_list[n].sin_addr));
        buf = strtok(NULL, "/");
        //printf("neighbor port [%d]: %s\n", n, buf);
        inet_pton(AF_INET, buf, &(neighbor_list[n].sin_port));
    }

    return neighbor_size;
}

void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}