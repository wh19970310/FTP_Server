/*fileserver.c*/
/* 所有的文件都已经以二进制到形式访问，所以打开读写文件都以二进制到形式进行 */
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include "sys/stat.h"
#include "pthread.h"
#include "dirent.h"
#include "fcntl.h"

#define BUFFER_SIZE 4096
#define MAX_QUE_CONN_NM 32
#define PORT 8520
#define MAXSOCKFD       10
#define FILE_NAME_MAX 512

char User[100] = "user:100###pwd:100";

char *myitoa(int value,char string[],int radix)   
{   
   int rt=0;   
   if(string==NULL)   
      return NULL;   
   if(radix<=0||radix>30)   
      return NULL;   
   rt=snprintf(string,radix,"%d",value);   
   if(rt>radix)   
      return NULL;   
   string[rt]='\0';   
   return string;   
}  

int file_size(char filename[])
{
    struct stat statbuf;
    stat(filename,&statbuf);
    int size=statbuf.st_size;
 
    return size;
}

int isPath(const char dir_path[]){
    char path[200];
    memset(path,'\0',100);
    strncpy(path,dir_path,(strlen(dir_path)-1));
    if(opendir(path)==NULL) //是文件
    {
        int size = file_size(path);
        return size;
    }
    else
    {
        return -1;
    }
}

int Comm(char comm[])
{
    if(strcmp(comm,User) == 0)
    {
        return 0;
    }
    if(strcmp(comm,"ls") == 0)
    {
        return 1;
    }
    if(strcmp(comm,"SendFile") == 0)
    {
        return 2;
    }
    if(strcmp(comm,"GetFile") == 0)
    {
        return 3;
    }
    if(strcmp(comm,"DelFile") == 0)
    {
        return 4;
    }
    if(strcmp(comm,"OpenFile") == 0)
    {
        return 5;
    }
    if(strcmp(comm,"BackFile") == 0)
    {
        return 6;
    }
    return -1;
}

void waitChild2(int sig){
    int status = 0;
    pid_t pid = 0;
    while((pid = waitpid(-1,&status,WNOHANG)) >0){
        //printf("end %d\n",pid);   
    }
}

void* Pthread_GetFile(void * arg);
void* Pthread_Accept(void *arg);
void* Pthread_SendFile(void * arg);

int getCommand(int sockfd,int *fd,int Stat,int *acc,int *count)
{
    pthread_t tid;
    struct sockaddr_in client_sockaddr;
    int client_fd , sin_size = sizeof(struct sockaddr);
    char Command[1024];
    int n = 0;
    char Out[BUFFER_SIZE];
    int loginstat = 1;

    while(1)
    {
        if ((client_fd = accept(sockfd, (struct sockaddr *)&client_sockaddr, (socklen_t *)&sin_size)) == -1)
        {
            printf("E Accept\n");
        }
        *acc += 1;
        loginstat = 1;
        if((*count - *acc) <=1)
        {
            char Add[9] = "Add";
            write(fd[1],Add,strlen(Add));
        }
        while(1)
        {
            memset(Command,'\0',1024);
            n = recv(client_fd,Command,1024-1,0);
            if(n <= 0 || (strcmp(Command,"Quit") == 0))
            {
                close(client_fd);
                *acc -= 1;
                break;
            }
            int comm = Comm(Command);
            if(comm == 0)
            {
                pthread_create(&tid, NULL, Pthread_Accept, &client_fd);
                pthread_join(tid,NULL);
            }
            switch(comm)
            {
                case -1:
                {
                    loginstat = 0;
                    char E[10] = "Err";
                    send(client_fd,E,strlen(E),0);
                    char End[5] = "\n";
                    send(client_fd,End,strlen(End),0);
                    break;
                }

                case 1:
                {
                    FILE *fp;
                    if((fp = popen("ls","r")) == NULL)
                    {
                        printf("E popen\n");
                    }
                    memset(Out,'\0',BUFFER_SIZE);
                    while(fgets(Out,BUFFER_SIZE-1,fp) != NULL)
                    {
                        int ispath = isPath(Out);
                        if(ispath == -1)
                        {
                            char File[1024] = "文件夹#";
                            strcat(File,Out);
                            send(client_fd,File,strlen(File),0);
                            memset(File,'\0',1024);
                        }
                        else
                        {
                            char File[1024];
                            char *p = File;
                            myitoa(ispath,File,10);
                            char c[20] = "文件#";
                            strcat(File,c);
                            strcat(File,Out);
                            send(client_fd,File,strlen(File),0);
                            memset(File,'\0',1024);
                        }
                        memset(Out,'\0',BUFFER_SIZE);
                    }
                    char End[5] = "\n";
                    send(client_fd,End,strlen(End),0);
                    pclose(fp);
                    break;
                }
                case 2:
                {
                    pthread_create(&tid, NULL, Pthread_GetFile, &client_fd);   
                    pthread_join(tid,NULL);
                    break;
                }
                case 3:
                {
                    pthread_create(&tid,NULL,Pthread_SendFile,&client_fd);
                    pthread_join(tid,NULL);
                    loginstat = 0;
                    break;
                }
                case 4:
                {
                    bzero(Command,1024);
                    recv(client_fd,Command,1024-1,0);
                    if(remove(Command) == 0)
                    {
                        printf("OK del\n");
                    }
                    break;
                }
                case 5:
                {
                    bzero(Command,1024);
                    recv(client_fd,Command,1024-1,0);
                    if(chdir(Command) == 0)
                    {
                        FILE *fp;
                        if((fp = popen("ls","r")) == NULL)
                        {
                            printf("E popen\n");
                        }
                        memset(Out,'\0',BUFFER_SIZE);
                        while(fgets(Out,BUFFER_SIZE-1,fp) != NULL)
                        {
                            int ispath = isPath(Out);
                            if(ispath == -1)
                            {
                                char File[1024] = "文件夹#";
                                strcat(File,Out);
                                send(client_fd,File,strlen(File),0);
                                memset(File,'\0',1024);
                            }
                            else
                            {
                                char File[1024];
                                char *p = File;
                                myitoa(ispath,File,10);
                                char c[20] = "文件#";
                                strcat(File,c);
                                strcat(File,Out);
                                send(client_fd,File,strlen(File),0);
                                memset(File,'\0',1024);
                            }
                            memset(Out,'\0',BUFFER_SIZE);
                        }
                        char End[5] = "\n";
                        send(client_fd,End,strlen(End),0);
                        pclose(fp);
                    }
                    break;
                }
                case 6:
                {
                    if(chdir("..") == 0)
                    {
                        FILE *fp;
                        if((fp = popen("ls","r")) == NULL)
                        {
                            printf("E popen\n");
                        }
                        memset(Out,'\0',BUFFER_SIZE);
                        while(fgets(Out,BUFFER_SIZE-1,fp) != NULL)
                        {
                            int ispath = isPath(Out);
                            if(ispath == -1)
                            {
                                char File[1024] = "文件夹#";
                                strcat(File,Out);
                                send(client_fd,File,strlen(File),0);
                                memset(File,'\0',1024);
                            }
                            else
                            {
                                char File[1024];
                                char *p = File;
                                myitoa(ispath,File,10);
                                char c[20] = "文件#";
                                strcat(File,c);
                                strcat(File,Out);
                                send(client_fd,File,strlen(File),0);
                                memset(File,'\0',1024);
                            }
                            memset(Out,'\0',BUFFER_SIZE);
                        }
                        char End[5] = "\n";
                        send(client_fd,End,strlen(End),0);
                        pclose(fp);
                    }
                    break;
                }
            }
            if(loginstat == 0)
            {
                continue;
            }
        }
        if(Stat == 0)
        {
            break;
        }
    }
    return 0;
}

void Cli_Accept(int sockfd)
{
    int *fd;
    fd = (int *)malloc(sizeof(int)*2);  
    signal(SIGCHLD,waitChild2);
    struct sockaddr_in client_sockaddr;
    int client_fd , sin_size = sizeof(struct sockaddr);
    pid_t pid;
    int GetAcc = 0;
    int *acc = &GetAcc;
    int Child_Stat;
    int Child_Count = 4;
    int *count = &Child_Count;

    if((pipe(fd)) < 0)
    {
        printf("E PIPE\n");
    }

    for(int i = 0 ;i < 4 ;i++)
    {
        Child_Stat = 1;
        if((pid = fork()) == 0)
        {  
            int r = getCommand(sockfd,fd,Child_Stat,acc,count);
            if(r == 0)
            {
                exit(0);
            }
        }
    }
    while(1)
    {
        char op[9];
        int n = read(fd[0],op,9);
        if(strcmp(op,"Add") == 0)
        {
            for(int i = 0;i < 3;i++)
            {
                Child_Count++;
                Child_Stat = 0;
                if((pid = fork()) == 0)
                {  
                    int r = getCommand(sockfd,fd,Child_Stat,acc,count);
                    if(r == 0)
                    {
                        exit(0);
                    }
                }
            }
        }
    }
}

int main(int argc,char* argv[])
{
    int sockfd;
    int sin_size = sizeof(struct sockaddr);
    struct sockaddr_in server_sockaddr, client_sockaddr;
    int i = 1;/*使得重复使用本地地址与套接字进行绑定 */
    int listenfd;

    /*建立socket连接*/
    if ((sockfd = socket(AF_INET,SOCK_STREAM,0))== -1)
    {
        perror("socket");
        exit(1);
    }
    //printf("Socket id = %d\n",sockfd);

    /*设置sockaddr_in 结构体中相关参数*/
    server_sockaddr.sin_family = AF_INET;
    server_sockaddr.sin_port = htons(PORT);
    server_sockaddr.sin_addr.s_addr = INADDR_ANY;
    bzero(&(server_sockaddr.sin_zero), 8);

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));

    /*绑定函数bind*/
    if (bind(sockfd, (struct sockaddr *)&server_sockaddr, sizeof(struct sockaddr))== -1)
    {
        perror("bind");
        exit(1);
    }
    //printf("Bind success!\n");

    /*调用listen函数*/
    if (listen(sockfd, MAX_QUE_CONN_NM) == -1)
    {
        perror("listen");
        exit(1);
    }
    //printf("Listening....\n");
    Cli_Accept(sockfd);
}

void* Pthread_Accept(void * arg)
{
    int client_fd;
    client_fd = *(int *)arg;
    int n;
    char Accept[10];
    char End[5] = "\n";
    memset(Accept,'\0',10);
    strcpy(Accept,"Accept");
    n = send(client_fd,Accept,strlen(Accept),0);
    if(n > 0)
    {
        n = send(client_fd,End,strlen(End),0);
    }
    pthread_exit(0);
}

void* Pthread_GetFile(void * arg)
{
    
    //recv file imformation
    int client_fd;

    char buff[BUFFER_SIZE];
    char filename[FILE_NAME_MAX];
    int count;
    bzero(buff,BUFFER_SIZE);
    client_fd = *(int *)arg;
    //把接受到到字符放在长度为BUFFER_SIZE的buff地址上，接收成功返回接收到到字节数目
    count=recv(client_fd,buff,BUFFER_SIZE,0);   
    if(count<0)
    {
        perror("recv");
        exit(1);
    }
    //把filename地址上的内容复制到地址buff上，第三个参数表明复制多少个字节
    strncpy(filename,buff,strlen(buff)>FILE_NAME_MAX?FILE_NAME_MAX:strlen(buff));

    //printf("Preparing recv file : %s\n",filename );

    //recv file
    //告诉函数库，打开的是一个二进制到可写文件，地址在指针filename
    FILE *fd=fopen(filename,"wb+"); 
    bzero(filename,FILE_NAME_MAX);
    if(NULL==fd)
    {
        perror("open");
        exit(1);
    }
    bzero(buff,BUFFER_SIZE); //缓冲区清0

    int length=0;
    while(length=recv(client_fd,buff,BUFFER_SIZE,0)) //这里是分包接收，每次接收4096个字节
    {
        if(length<0)
        {
            perror("recv");
            exit(1);
        }
        if(strcmp(buff,"end") == 0)
        {
            break;
        }
        //把从buff接收到的字符写入（二进制）文件中
        int writelen=fwrite(buff,sizeof(char),length,fd);
        if(writelen<length)
        {
            perror("write");
            exit(1);
        }
        bzero(buff,BUFFER_SIZE); //每次写完缓冲清0，准备下一次的数据的接收
        
    }
    //printf("Receieved file:%s finished!\n",filename );
    fclose(fd);
    pthread_exit(0);
}

void* Pthread_SendFile(void * arg)
{
    
    //recv file imformation
    int client_fd;

    char buff[BUFFER_SIZE];
    char file_name[FILE_NAME_MAX];
    int count;
    bzero(buff,BUFFER_SIZE);
    client_fd = *(int *)arg;
    //把接受到到字符放在长度为BUFFER_SIZE的buff地址上，接收成功返回接收到到字节数目
    count=recv(client_fd,buff,BUFFER_SIZE,0);   
    if(count<0)
    {
        perror("recv");
        exit(1);
    }
    bzero(file_name, sizeof(file_name));
    //把filename地址上的内容复制到地址buff上，第三个参数表明复制多少个字节
    strncpy(file_name,buff,strlen(buff)>FILE_NAME_MAX?FILE_NAME_MAX:strlen(buff));
    //printf("Preparing send file : %s\n",file_name );
    FILE *fp = fopen(file_name, "r");
    if (fp == NULL)
    {
        printf("File:\t%s Not Found!\n", file_name);
    }
    else
    {
        bzero(buff, BUFFER_SIZE);
        int file_block_length = 0;
        while( (file_block_length = fread(buff, sizeof(char), BUFFER_SIZE, fp)) > 0)
        {
            // 发送buffer中的字符串到new_server_socket,实际上就是发送给客户端
            if (send(client_fd, buff, file_block_length, 0) < 0)
            {
                printf("Send File:\t%s Failed!\n", file_name);
                break;
            }
            bzero(buff, sizeof(buff));
        }
        fclose(fp);
    }
    close(client_fd);
    //printf("send file:%s finished!\n",file_name );
    pthread_exit(0);
}