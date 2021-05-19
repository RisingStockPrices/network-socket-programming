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
    char *lresp_message = "LRESP/";
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
        for(int i=0;i<MAX_CHUNK_NUMBER;i++){
            chunk_offset_list[i] = 1;
        }
        //memset(&chunk_offset_list, 1, sizeof(chunk_offset_list));
    } else {
        // 파일 없으므로 CHUNK_OFFSET_LIST 0으로 초기화
        for(int i=0;i<MAX_CHUNK_NUMBER;i++){
            chunk_offset_list[i] = 0;
        }
        //memset(&chunk_offset_list, 0, sizeof(chunk_offset_list));
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

    strcat(alive_message, argv[3]);
    send(tracker_sd, alive_message, sizeof(alive_message), 0);
    printf("ALIVE message sent\n");
    
    // fd table 초기화
    FD_ZERO(&fds);
    FD_SET(serv_sd, &fds);
    FD_SET(tracker_sd, &fds);
    fd_max = tracker_sd;

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

                    printf("Welcoming peer\n");
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
                    
                    printf("Received message from socket %d: %s\n", i, buffer);

                    if(startsWith("NLIST",buffer)==1){
                        strcpy(buffer,&buffer[6]);

                        for(int j=0;j<neighbor_size;j++){
                            FD_CLR(neighbor_sd[j],&fds);
                            close(neighbor_sd[j]);
                        }
                        
                        // DECODE MESSAGE
                        neighbor_size = get_nlist(tracker_sd,buffer, neighbor_list);
                        
                        for (int j=0; j<neighbor_size; j++) {
                            neighbor_sd[j] = socket(PF_INET, SOCK_STREAM, 0);

                            if (connect(neighbor_sd[j], (struct sockaddr*)&(neighbor_list[j]), sizeof(neighbor_list[j])) == -1) {
                                // 받은 neighbor랑 연결이 안되면?
                                printf("failed to connect\n");
                            }
\
                            FD_SET(neighbor_sd[j],&fds);
                            // neighbor에게 LRQST 요청
                            
                            strcpy(message, lrqst_message);
                            send(neighbor_sd[j], message, sizeof(message), 0);
                            printf("Sent LRQST message to socket fd %d\n",neighbor_sd[j]);
                        }
                    }
                    else if(startsWith("LRQST",buffer)==1){
                        // 소유 chunk offset list를 serialize
                        // 메시지 format: LRESP/(ofset1)/(offset2)/../(마지막 offset)///
                        strcpy(message, lresp_message);
                        char tmp[5];
                        for(int k=0;k<MAX_CHUNK_NUMBER;k++){
                            if(chunk_offset_list[k]==1){
                                sprintf(tmp,"%d/",k);
                                strcat(message,tmp);
                            }
                        }
                        strcat(message,"//");
                        printf("message to be sent is : %s", message);
                        send(i, message,sizeof(buffer), 0);
                        printf("Sent LRESP message\n");
                        // serialized된 list를 neighbor에 보내기
                    }
                    else if (startsWith("LRESP", buffer)==1){
                        // parse list
                        printf("Received LRQST message\n");
                        strtok(message,"/");
                        do{
                            temp = strtok(NULL, "/");
                            printf("%d\n",temp);
                        }while(strcmp(temp,"/")!=0);
                        
                        //memset(&buffer,0, sizeof(buffer));
                        //sprintf(buffer,"DRQST/");
                        //request data
                    }
                    else if (startsWith("DRQST", buffer)==1){
                        
                    }
                    else if (startsWith("DRESP", buffer)==1){
                        
                    }
                    else {
                        error_handling("ERROR: unknown command");
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
        memset(&neighbor_list[n],0,sizeof(neighbor_list[n]));
        neighbor_list[n].sin_family = AF_INET;

        buf = strtok(NULL, "/");
        //printf("neighbor[%d]: %s\n", n, buf);
        
        inet_pton(AF_INET, buf, &(neighbor_list[n].sin_addr));
        buf = strtok(NULL, "/");
        neighbor_list[n].sin_port = htons(atoi(buf));
        //inet_pton(AF_INET, buf, &(neighbor_list[n].sin_port));
    }

    return neighbor_size;
}

void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

int startsWith(const char *pre, const char *str)
{
    if (strncmp(pre, str, strlen(pre)) == 0)
        return 1;
    else
        return 0;
}