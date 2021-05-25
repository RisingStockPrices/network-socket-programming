// Client side C/C++ program to demonstrate Socket programming
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#define BUF_SIZE 100
#define MAX_NEIGHBOR 3
#define FILE_COUNT 4
#define FILE_SIZE 33

void encode_message(char *message, char *type);
int get_nlist(int sockfd, char *message, struct sockaddr_in *neighbor_list);
void error_handling(char *message);
int find_neighbor_idx(int sd, int neighbor_size, int *neighbor_sd);
int starts_with(const char *pre, const char *str);

int main(int argc, char const *argv[])
{
    int serv_sd, client_sd, tracker_sd, neighbor_sd[MAX_NEIGHBOR];
    int fd_max, fd_num;
    int clnt_addr_sz;
    int offset;
    int neighbor_size = 0;
    int has_all_files = 0;
    int file_size = 0;
    struct sockaddr_in serv_addr, clnt_addr, tracker_addr;
    struct sockaddr_in neighbor_list[MAX_NEIGHBOR];
    fd_set fds, cpy_fds;
    FILE *file;

    // Message types for P2P system
    char alive_message[BUF_SIZE] = "ALIVE";
    char *lrqst_message = "LRQST";
    char *drqst_message = "DRQST";
    char *lresp_message = "LRESP/";
    char *dresp_message = "DRESP";

    char *temp;
    char buffer[BUF_SIZE];
    char message[BUF_SIZE];

    // FILENAMES is the pool of files to share between peers
    // If a peer has a file matching any of the names in the pool,
    // it may be read and saved in its neighbor's file system
    char *filenames[FILE_COUNT] = {
        "1.txt",
        "2.txt",
        "3.txt",
        "4.txt",
    };
    char has_file[FILE_COUNT];
    char is_requesting[FILE_COUNT];
    char data_list[FILE_COUNT][FILE_SIZE];

    // Check if the current peer has files specified in the FILENAMES pool
    // if yes, save the data in the DATA_LIST array
    printf("PEER %s's files: [",argv[3]);
    for (int i = 0; i < FILE_COUNT; i++)
    {
        if ((file = fopen(filenames[i], "rb")) != NULL)
        {
            fseek(file, 0, SEEK_END);
            file_size = ftell(file);
            fseek(file, 0, SEEK_SET);
            has_file[i] = 1;
            is_requesting[i] = 0;
            fread(data_list[i], 1, FILE_SIZE, file);
            memset(&(data_list[i][file_size]), 0, 1);
            printf(" %s ,", filenames[i]);
            fclose(file);
        }
        else{
            has_file[i] = 0;
            is_requesting[i] = 0;
        }
            
    }
    printf("]\n");

    // Once the peer starts execution,
    // it tries to initiate the connection with the tracker (<tracker IP>:<tracker port>),
    // and listen on its <own port>
    if (argc != 4)
    {
        printf("Usage: %s <tracker IP> <tracker port> <own port>\n", argv[0]);
        exit(1);
    }

    // Create sockets for the peer itself and the tracker
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
    listen(serv_sd, MAX_NEIGHBOR + 2);

    // Peers act as a client to the tracker, which is why we need to use connect()
    connect(tracker_sd, (struct sockaddr *)&tracker_addr, sizeof(tracker_addr));
    strcat(alive_message, argv[3]);

    // Once the connection is established, send an ALIVE message to the tracker,
    // as a way of telling the tracker to join the p2p system
    send(tracker_sd, alive_message, sizeof(alive_message), 0);
    printf("ALIVE message sent to tracker\n");

    // fd table initialization
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

        for (int i = 0; i < (fd_max + 1); i++)
        {
            if (FD_ISSET(i, &cpy_fds))
            {
                // Handle initial connections from neighbors and tracker
                if (i == serv_sd)
                {
                    clnt_addr_sz = sizeof(clnt_addr);
                    client_sd = accept(serv_sd, (struct sockaddr *)&clnt_addr, &clnt_addr_sz);

                    FD_SET(client_sd, &fds);
                    if (fd_max < client_sd)
                        fd_max = client_sd;
                }
                // Handle events in already registered sockets
                else
                {

                    memset(buffer, 0, sizeof(buffer));
                    // Store incoming message in the BUFFER variable
                    if (recv(i, buffer, sizeof(buffer), 0) == -1)
                    {
                        error_handling("ERROR: receiving message");
                    }

                    if(strlen(buffer)==0)
                        {
                            printf("Lost connection with %d\n",i);
                            close(i);
                            FD_CLR(i, &fds);
                        }
                    // Optional: use print statement below for debugging
                    //printf("Received message from socket %d: %s\n", i, buffer);

                    // Messages in our P2P system:
                    // There are four types of messages a peer can receive from another peer (neighbor)
                    // -> LRQST, LRESP, DRQST, DRSP
                    // and one type a peer can receive from the TFRACKER -> NLIST

                    // NLIST messages : Once the peer sends an ALIVE message to the tracker,
                    // the tracker responds with a NLIST type message, encoding information of its neighbors
                    // ACTION: LRQST should be sent for each extracted neighbor
                    // FORMAT: "NLIST/(number of neighbors)/(N1 IP)/(N1 PORT)/(N2 IP)/(N2 PORT)/...//"
                    if (starts_with("NLIST", buffer) == 1)
                    {
                        for(int k=0;k<FILE_COUNT;k++){
                            if(has_file[k]==0)
                                break;
                            else if (k==FILE_COUNT-1)
                                {
                                    printf("No files to request!\n");
                                    has_all_files = 1;
                                }
                        }
                        // DECODE NLIST message sent by tracker
                        // get_nlist returns number of neighbors, and stores neighbor info in NEIGHBOR_LIST
                        if(has_all_files==0){
                        strcpy(buffer, &buffer[6]);
                        neighbor_size = get_nlist(tracker_sd, buffer, neighbor_list);

                        // for all extracted neighbors, open up a socket and initiate the connection
                        for (int j = 0; j < neighbor_size; j++)
                        {
                            neighbor_sd[j] = socket(PF_INET, SOCK_STREAM, 0);


                            if (connect(neighbor_sd[j], (struct sockaddr *)&(neighbor_list[j]), sizeof(neighbor_list[j])) == -1)
                            {
                                sprintf(message,"Connection failed with neighbor %d\n", j);
                                error_handling(message);
                            }

                            printf("-------------\nWelcoming peer %d\n", ntohs(neighbor_list[j].sin_port));
                            FD_SET(neighbor_sd[j], &fds);
                            if (fd_max < neighbor_sd[j])
                            {
                                fd_max = neighbor_sd[j];
                            }

                            // Send a LRQST message to each neighbor, requesting the list of files of the neighbor
                            strcpy(message, lrqst_message);
                            send(neighbor_sd[j], message, sizeof(message), 0);
                            memset(buffer, 0, sizeof(buffer));
                            memset(message, 0, sizeof(message));
                        }
                        }
                    }
                    // LRQST messages : Once the peer gets a list of neighbors from the tracker,
                    // it sends out a LRQST message requesting a list of all files in the FILENAMES list currently in the neighbor's file system
                    // ACTION: LRESP should be sent back to the requester, returning what the LRQST message asks for
                    // FORMAT: "LRQST"
                    else if (starts_with("LRQST", buffer) == 1)
                    {
                        // ENCODE the LRESP message and send back to the requester
                        strcpy(message, lresp_message);
                        char tmp[5];
                        for (int k = 0; k < FILE_COUNT; k++)
                        {
                            if (has_file[k] == 1)
                            {
                                sprintf(tmp, "%d/", k);
                                strcat(message, tmp);
                            }
                        }
                        strcat(message, "//");
                        send(i, message, sizeof(buffer), 0);
                    }
                    // LRESP messages : Get file list from neighbor, and send DRQST for absent file.
                    // FORMAT: "LRESP/(file index 1)/(file index 2)/...//"
                    else if (starts_with("LRESP", buffer) == 1)
                    {
                        strcpy(message, drqst_message);
                        temp = strtok(buffer, "/");
                        while (1)
                        {
                            temp = strtok(NULL, "/");
                            if (temp == NULL)
                            {
                                printf("No files to receive from neighbor %d\n", ntohs(neighbor_list[find_neighbor_idx(i, neighbor_size, neighbor_sd)].sin_port));
                                break;
                            }

                            offset = atoi(temp);
                            if ((has_file[offset] != 1) && (is_requesting[offset] != 1))
                            {
                                memset(temp, 0, sizeof(temp));
                                sprintf(temp, "/%d", offset);
                                strcat(message, temp);
                                is_requesting[offset] = 1;
                                send(i, message, BUF_SIZE, 0);
                                break;
                            }
                        }
                        
                    }

                    // DRQST messages: Check which file the peer is requesting,
                    // and send the corresponding file data to the peer
                    // FORMAT: "DRQST/(requesting file index)/(file data)//"
                    else if (starts_with("DRQST", buffer) == 1)
                    {
                        strcpy(buffer, &buffer[6]);
                        strcpy(message, dresp_message);
                        temp = strtok(buffer, "/");
                        offset = atoi(temp);
                        strcat(message, "/");
                        strcat(message, temp);
                        strcat(message, "/");
                        strcat(message, data_list[offset]);

                        printf("PEER is requesting file [%s]...\n", filenames[offset]);
                        if ((send(i, message, sizeof(message), 0)) != -1) {
                            printf("Successfully sent file [%s] to PEER\n", filenames[offset], ntohs(neighbor_list[find_neighbor_idx(i, neighbor_size, neighbor_sd)].sin_port));
                        }
                        
                    }
                    // DRESP messages: Receive file data from the neighbor, and write file
                    // FORMAT: "DRESP/(receiving file index)/(file data)"
                    else if (starts_with("DRESP", buffer) == 1)
                    {
                        // get the receiving file index
                        strcpy(buffer, &buffer[6]);
                        offset = atoi(strtok(buffer, "/"));
                        temp = strtok(NULL, "/");

                        // update data_list for the receiving file,
                        // and write file
                        strcpy(data_list[offset], temp);
                        has_file[offset] = 1;
                        is_requesting[offset] = 0;
                        file = fopen(filenames[offset], "wb");
                        fwrite(data_list[offset], 1, strlen(data_list[offset]), file);
                        printf("[RECEIVED FILE %s from PEER %d]\n", filenames[offset], ntohs(neighbor_list[find_neighbor_idx(i, neighbor_size, neighbor_sd)].sin_port));
                        fclose(file);
                    }
                    else
                    {
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

// starts_with
// : Check if the str starts with pre
int starts_with(const char *pre, const char *str)
{
    if (strncmp(pre, str, strlen(pre)) == 0)
        return 1;
    else
        return 0;
}

// get_nlist
// : Extract neighbor list from the message
int get_nlist(int sockfd, char *message, struct sockaddr_in *neighbor_list)
{
    char *buf;
    int neighbor_size;
    int n;

    buf = strtok(message, "/");
    neighbor_size = atoi(buf);
    for (n = 0; n < neighbor_size; n++)
    {
        memset(&neighbor_list[n], 0, sizeof(neighbor_list[n]));
        neighbor_list[n].sin_family = AF_INET;
        buf = strtok(NULL, "/");
        inet_pton(AF_INET, buf, &(neighbor_list[n].sin_addr));
        buf = strtok(NULL, "/");
        neighbor_list[n].sin_port = htons(atoi(buf));
    }

    return neighbor_size;
}

int find_neighbor_idx(int sd, int neighbor_size, int *neighbor_sd) {
    int requester_idx = -1;
    for(int k=0;k<neighbor_size; k++){
        if(neighbor_sd[k]==sd)
        {
            requester_idx = k;
            break;
        }
    }
    return requester_idx;
}

void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
