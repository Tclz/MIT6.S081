//
// Created by lenovo on 2021/1/12.
//
#include "user.h"
//#include <unistd.h>
#include "../kernel/types.h"
//#include "string.h"
#define buffer_size 10
#define STDDER_FILENO 2
#define READEND 0
#define WRITEEND 1
int main(int argc, char *argv[]){
    // ping pong 命令无需参数
    if(argc != 1){
        fprintf(2,"Usage:ping-pong a byte between two processes.");
    }
    // two pipes
    int pfd[2], cfd[2];
    // 创建管端，每个管道如p分别具有读端p[0]和写端p[1]
    if(pipe(pfd) < 0 || pipe(cfd) < 0) {
        fprintf(STDDER_FILENO, "create pipe failed.");
        exit(1);
    }
    // send messages
//    char *send_msg1 = "ping";
//    char accept_msg1[buffer];
//    char *send_msg2 = "pong";
//    char accept_msg2[buffer];
    // received buffer
    char buffer[buffer_size];

    int pid = fork();
    // 创建好进程后，父子进程同时运行。
    // fork的三种可能的返回值 0说明在子进程中；新创建的子进程的id；负值创建失败
    if(pid < 0) {
        fprintf(STDDER_FILENO,"fork failed.");
        exit(1);
    }else if(pid == 0){
        // in child process
        close(pfd[WRITEEND]);
        close(cfd[READEND]);

        // 子进程读出最多buffer_size大小字节的内容
        read(pfd[READEND], buffer, buffer_size);
        printf("%d: received %s\n",getpid(),buffer);
        write(cfd[WRITEEND],"pong",strlen("pong"));

        close(cfd[WRITEEND]);
        close(pfd[READEND]);
    }else{
        // in parent process
        close(pfd[READEND]);
        close(cfd[WRITEEND]);

        write(pfd[WRITEEND],"ping",strlen("ping"));
        close(pfd[WRITEEND]);

        read(cfd[READEND], buffer, buffer_size);
        close(cfd[READEND]);
        printf("%d: received %s\n",getpid(),buffer);

    }
    exit(0);
}

