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
	int serv_sd, clnt_sd[MAX_CLIENT], client_sd;
    int fd_max;
    int activity, strlen;
	char buf[BUF_SIZE];
	
	struct sockaddr_in serv_adr, clnt_adr;
	socklen_t clnt_adr_sz;

    fd_set readfds;
	
    //client 소켓 초기화
    for (int i=0;i<MAX_CLIENT;i++) {
        clnt_sd[i] = 0;
    }
    //소켓 생성
	serv_sd = socket(PF_INET, SOCK_STREAM, 0);   
	
    //server address을 null 로 초기화
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_adr.sin_port = htons(atoi(argv[1]));
	

	bind(serv_sd, (struct sockaddr*)&serv_adr, sizeof(serv_adr));
	listen(serv_sd, 10);

    FD_ZERO(&readfds);
    //server socket fd에 등록
    FD_SET(serv_sd, &readfds);
    fd_max = serv_sd;

    while(1){
        if((activity = select(fd_max+1, &readfds, NULL,NULL,NULL))==-1) {
            print("select error");
            break;
        }

        for(int i=0;i<fd_max+1;i++){
            if(FD_ISSET(i, &readfds))
            {
                //server socket에서 일어난 일
                if(i==serv_sd) {
                    clnt_adr_sz = sizeof(clnt_adr);    
	                client_sd = accept(serv_sd, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);
                    
                    FD_SET(client_sd, &readfds);
                    if(fd_max<client_sd)
                    {
                        fd_max = client_sd;
                    }
                }
                else {
                    //event occured to one of the clients
                    strlen = read(i,buf,BUF_SIZE);
                    if(str_len==0){
                        //close reqest??
                    } else {
                        //생존 신고 보내왔음
                        printf("%s\n",buf);
                    }
                }
            }
        }
        
	}
	
	close(serv_sd);
	return 0;
} 

void error_handling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}
