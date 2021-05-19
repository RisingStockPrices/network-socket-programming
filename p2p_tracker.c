#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#define BUF_SIZE 30
#define MAX_CLIENT 10
#define MAX_NEIGHBOR 3
#define TIME_OUT 1000
void error_handling(char *message);

int startsWith(const char *pre, const char *str);
int main(int argc, char *argv[])
{
    int serv_sd, client_sd;
    int fd_max;
    int fd_num, str_len;
    int neighbor_size;
    char temp[BUF_SIZE];
    char buf[BUF_SIZE];
    char message[BUF_SIZE];
    char *nlist_message = "NLIST";

    struct sockaddr_in serv_adr, peer_list[MAX_CLIENT], neighbors_to_send[MAX_NEIGHBOR], client_adr;
    struct timeval timeout;

    
    socklen_t clnt_adr_sz;

    fd_set fds, cpy_fds;


    if (argc < 2)
    {
        printf("Usage: %s <port>", argv[0]);
        exit(1);
    }

    memset(&peer_list,0,sizeof(peer_list));
    //소켓 생성
    serv_sd = socket(PF_INET, SOCK_STREAM, 0);

    //server address을 null 로 초기화
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    // welcoming socket (최대 10개까지 listen)
    if( bind(serv_sd, (struct sockaddr *)&serv_adr, sizeof(serv_adr))==-1){
        error_handling("bind() error");
    }
    
    // Listen the connection setup request from clients.
    if (listen(serv_sd, 10) == -1)
        error_handling("listen() error");

    printf("bind and listen success\n");

    // fd table 초기화
    FD_ZERO(&fds);
    FD_SET(serv_sd, &fds);
    fd_max = serv_sd;

    timeout.tv_sec = 50;
    timeout.tv_usec = 5000;

    while (1)
    {
        cpy_fds = fds;
        // select: fd table 에 등록된 애들 중 event 생긴 socket 수 return
        if ((fd_num = select(fd_max + 1, &cpy_fds, NULL, NULL, NULL) == -1))
        {
            printf("select error");
            break;
        }
        else if (fd_num == 0) //timeout
        {
            printf("No peers requesting!\n");
        }

        // fd table 돌면서 event 생겼는지 확인
        for (int i = 0; i < fd_max + 1; i++)
        {
            if (FD_ISSET(i, &cpy_fds))
            {
                //welcoming socket에서 생긴 event
                if (i == serv_sd)
                {
                    /*
                    새 connection 을 받아들여, fd table 에 자리 내줌
                    */
                    clnt_adr_sz = sizeof(peer_list[fd_max]);
                    client_sd = accept(serv_sd, (struct sockaddr *)&client_adr, &clnt_adr_sz);
                    inet_ntop(AF_INET, &(client_adr.sin_addr), buf, INET_ADDRSTRLEN);
                    

                    printf("Welcome! %s %d\n", buf, client_adr.sin_port);
                    //save client address to peer list
                    peer_list[client_sd] = client_adr;

                    //update fd table
                    FD_SET(client_sd, &fds);
                    if (fd_max < client_sd)
                    {
                        fd_max = client_sd;
                    }
                }
                else
                {
                    // read message
                    if ((str_len = recv(i, message, BUF_SIZE, 0)) == -1)
                    {
                        error_handling("ERROR: receiving message from peers");
                    }
                    printf("message: %s///\n", message);
                    //memset(&message[5], 0, 1);

                    if (startsWith("ALIVE", message) == 1) //strcmp("ALIVE", message) == 0)
                    {
                        strcpy(message, &message[5]);
                        peer_list[i].sin_port = htons(atoi(message));
                        // 새 nlist 뽑아주기
                        // serialize message
                        // 보냄 : NLIST/(수)/(IP1)/(IP2)/...

                        // 새 nlist 뽑아주기
                        // serialize message
                        // 보냄 : NLIST/(수)/(IP1)/(IP2)/...

                        neighbor_size = get_random_neighbors(i, peer_list, &neighbors_to_send);
                        
                        printf("neighbor size is %d\n",neighbor_size);
                        sprintf(buf, "/%d/", neighbor_size);
                        strcpy(message, nlist_message);
                        strcat(message, buf);
                        for (int n = 0; n < neighbor_size; n++) {
                            inet_ntop(AF_INET, &(neighbors_to_send[n].sin_addr), buf, INET_ADDRSTRLEN);
                            sprintf(temp, "/%d", ntohs(neighbors_to_send[n].sin_port));
                            strcat(buf, temp);

                            printf("neighbor [%d]: %s\n",n, buf);

                            strcat(message, buf);
                            strcat(message, n == (neighbor_size - 1) ? "" : "/");
                        }
                        //printf("message: %s\n", message);
                        str_len = send(i, message, sizeof(message), 0);
                        //printf("string length: %d\n",str_len);
                       
                    }
                    else
                    {
                        printf("invalid message\n");
                        // invalid message
                        //error_handling("ERROR: survival from peer");
                    }
                }
            }
        }
    }

    close(serv_sd);
    return 0;
}

int startsWith(const char *pre, const char *str)
{
    if (strncmp(pre, str, strlen(pre)) == 0)
        return 1;
    else
        return 0;
}
// 배열을 섞는 함수 
void shuffle(int *arr, int num) { 
    srand(time(NULL)); 
    int temp; 
    int rn; 
    for (int i=0; i < num-1; i++) {
         rn = rand() % (num - i) + i; // i 부터 num-1 사이에 임의의 정수 생성 
         temp = arr[i]; 
         arr[i] = arr[rn]; 
         arr[rn] = temp; 
    } 
}

int get_random_neighbors(int req_fd, struct sockaddr_in* peers, struct sockaddr_in* neighbors){
    int candidate_idx[MAX_CLIENT];
    int idx=-1;
    struct sockaddr_in invalid_addr;
   
    memset(&invalid_addr,0,sizeof(invalid_addr));

    for(int i=0;i<MAX_CLIENT;i++){
        if(i==req_fd)
            continue;
        if(memcmp(&peers[i],&invalid_addr,sizeof(invalid_addr))==0)
            continue;
        candidate_idx[++idx] = i;
    }
    //randomize candidate list
    shuffle(candidate_idx,idx+1);
    
    printf("Debugging... print randomized candidates idx\n");
    for(int i=0;i<=idx;i++){
        printf("%d ",candidate_idx[i]);
    }
    //set first N elements in <neighbors>
    for(int i=0;i<((idx+1>MAX_NEIGHBOR)?MAX_NEIGHBOR:(idx+1));i++){
        neighbors[i] = peers[candidate_idx[i]];
    }
    return idx+1;
}


int unregister_neighbor(struct sockaddr_in *peers, fd_set *set, int fd)
{
    FD_CLR(fd, set);
    memset(&peers[fd], 0, sizeof(peers[fd]));
}

void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
