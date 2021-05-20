#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#define BUF_SIZE 100
#define MAX_PEER 10
#define MAX_NEIGHBOR 3
void error_handling(char *message);
int unregister_neighbor(struct sockaddr_in *peers, fd_set *set, int fd);
int get_random_neighbors(int req_fd, struct sockaddr_in *peers, struct sockaddr_in *neighbors);
void shuffle(int *arr, int num);
int starts_with(const char *pre, const char *str);

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

    struct sockaddr_in serv_adr, peer_list[MAX_PEER], neighbors_to_send[MAX_NEIGHBOR], client_adr;

    socklen_t clnt_adr_sz;

    fd_set fds, cpy_fds;

    if (argc < 2)
    {
        printf("Usage: %s <port>", argv[0]);
        exit(1);
    }

    memset(&peer_list, 0, sizeof(peer_list));

    serv_sd = socket(PF_INET, SOCK_STREAM, 0);

    //server address을 null 로 초기화
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sd, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");

    if (listen(serv_sd, MAX_PEER) == -1)
        error_handling("listen() error");

    printf("[Bind and listen success]\n");

    // Initialize FD table for IO multiplexing
    FD_ZERO(&fds);
    FD_SET(serv_sd, &fds);
    fd_max = serv_sd;

    while (1)
    {
        cpy_fds = fds;
        // select: fd table 에 등록된 애들 중 event 생긴 socket 수 return
        if ((fd_num = select(fd_max + 1, &cpy_fds, NULL, NULL, NULL) == -1))
        {
            printf("select error");
            break;
        }

        // Iterate through the FD table and check for events
        for (int i = 0; i < fd_max + 1; i++)
        {
            if (FD_ISSET(i, &cpy_fds))
            {
                if (i == serv_sd)
                {
                    clnt_adr_sz = sizeof(peer_list[fd_max]);
                    client_sd = accept(serv_sd, (struct sockaddr *)&client_adr, &clnt_adr_sz);
                    inet_ntop(AF_INET, &(client_adr.sin_addr), buf, INET_ADDRSTRLEN);

                    printf("-------------\nWelcome! %s", buf);
                    // save client address to peer list
                    peer_list[client_sd] = client_adr;

                    FD_SET(client_sd, &fds);
                    if (fd_max < client_sd)
                    {
                        fd_max = client_sd;
                    }
                }
                else
                {
                    memset(message,0,sizeof(message));
                    // read message
                    if ((str_len = recv(i, message, BUF_SIZE, 0)) == -1)
                    {
                        error_handling("ERROR: receiving message from peers");
                    }
                    printf("Received message : %s\n", message);

                    // ALIVE messages come with a port number of the client
                    // This port number needs to be saved in the NEIGHBOR_LIST to enable peer connections
                    if (starts_with("ALIVE", message) == 1)
                    {
                        strcpy(message, &message[5]);

                        // save port number in PEER_LIST
                        peer_list[i].sin_port = htons(atoi(message));
                        printf(" %d\n",ntohs(peer_list[i].sin_port));
                        // assign neighbors to the peer
                        neighbor_size = get_random_neighbors(i, peer_list, &neighbors_to_send);

                        // ENCODE MESSAGE
                        sprintf(buf, "/%d/", neighbor_size);
                        strcpy(message, nlist_message);
                        strcat(message, buf);
                        printf("Generating neighbor list for %d: [",ntohs(peer_list[i].sin_port));
                        for (int n = 0; n < neighbor_size; n++)
                        {
                            inet_ntop(AF_INET, &(neighbors_to_send[n].sin_addr), buf, INET_ADDRSTRLEN);
                            sprintf(temp, "/%d", ntohs(neighbors_to_send[n].sin_port));
                            strcat(buf, temp);

                            printf(" %s, ", buf);
                            strcat(message, buf);
                            strcat(message, n == (neighbor_size - 1) ? "" : "/");
                        }
                        printf("]\n");
                        send(i, message, sizeof(message), 0);
                    }
                    else
                    {
                        printf("invalid message\n");
                    }
                }
            }
        }
    }

    close(serv_sd);
    return 0;
}

// starts_with
// : Check if the str starts with pre
int starts_with(const char *pre, const char *str)
{
    if (strncmp(pre, str, strlen(pre)) == 0)
        return 1;
    else
        return 0;
}

// shuffle
// : shuffles the elements in arr in random order
void shuffle(int *arr, int num)
{
    srand(time(NULL));
    int temp;
    int rn;
    for (int i = 0; i < num - 1; i++)
    {
        rn = rand() % (num - i) + i;
        temp = arr[i];
        arr[i] = arr[rn];
        arr[rn] = temp;
    }
}

// get_random_neighbors
// : Among the neighbors, get at most MAX_PEER neighbors randomly
int get_random_neighbors(int req_fd, struct sockaddr_in *peers, struct sockaddr_in *neighbors)
{
    int candidate_idx[MAX_PEER];
    int idx = -1;
    struct sockaddr_in invalid_addr;

    memset(&invalid_addr, 0, sizeof(invalid_addr));

    for (int i = 0; i < MAX_PEER; i++)
    {
        if (i == req_fd)
            continue;
        if (memcmp(&peers[i], &invalid_addr, sizeof(invalid_addr)) == 0)
            continue;
        candidate_idx[++idx] = i;
    }
    // randomize candidate list
    shuffle(candidate_idx, idx + 1);

    // set first N elements in <neighbors>
    for (int i = 0; i < ((idx + 1 > MAX_NEIGHBOR) ? MAX_NEIGHBOR : (idx + 1)); i++)
    {
        neighbors[i] = peers[candidate_idx[i]];
    }
    return idx + 1;
}

void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}