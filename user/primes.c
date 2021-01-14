//
// Created by lenovo on 2021/1/13.
//
#include "user.h"
#include "../kernel/types.h"

//int isPrime(int x){
//    for(int i=2;i<x;++i){
//        if(x%i == 0)
//            return 0;
//    }
//    return 1;
//}
//int main(int argc, char *argv[]){
//
//    int df[2];
//    if(pipe(df) < 0){
//        fprintf(2,"create pipe failed.");
//        exit(1);
//    }
//    int pid;
//    if((pid = fork())==0){
//        // child process
//        close(df[1]);
//        int j;
//        while(read(df[0],&j,sizeof(int)) != 0){
//            printf("prime %d\n",j);
//        }
//
//    }else{
//        // parent process
//        close(df[0]);
//        int i;
//        for(i=2;i<35;++i){
//            if(isPrime(i)){
//                write(df[1],&i, sizeof(int));
//            }
//
//        }
//
//    }
//    exit(0);
//}


#define STDDER_FILENO 2
#define READEND 0
#define WRITEEND 1

typedef int pid_t;

int main(void)
{
    int numbers[36], fd[2];
    int i, index = 0;
    pid_t pid;
    for (i = 2; i <= 35; i++)
    {
        numbers[index++] = i;
    }
    // 堆叠的管道形成一个大的素数筛
    while (index > 0)
    {
        // 只要仍有剩余的数就继续创建管道进行筛选
        pipe(fd);
        if ((pid = fork()) < 0)
        {
            fprintf(STDDER_FILENO, "fork error\n");
            exit(0);
        }
        else if (pid > 0)
        {
            // 父进程中 负责写数据
            close(fd[READEND]);
            for (i = 0; i < index; i++)
            {
                write(fd[WRITEEND], &numbers[i], sizeof(numbers[i]));
            }
            close(fd[WRITEEND]);
            wait((int *)0);
            exit(0);
        }
        else
        {
            // 子进程中 负责读出数据并打印
            close(fd[WRITEEND]);
            int prime = 0;
            int temp = 0;
            index = -1;

            while (read(fd[READEND], &temp, sizeof(temp)) != 0)
            {
                // 到达每一级管道的所有数中的第一个必是素数，其余数只有不被该数整除才能送入到下一级管道
                // the first number must be prime
                if (index < 0) prime = temp, index ++;
                else
                {
                    if (temp % prime != 0) numbers[index++] = temp;
                }
            }
            printf("prime %d\n", prime);
            // fork again until no prime
            close(fd[READEND]);
        }
    }
    exit(0);
}