//
// Created by lenovo on 2021/1/13.
//
#include "user.h"
#include "../kernel/types.h"
#include "../kernel/param.h"

#define STDIN_FILENO 0
#define MAXLINE 1024

int main(int argc, char *argv[])
{
    char line[MAXLINE];
    char* params[MAXARG];
    int n, args_index = 0;
    int i;
    // example: echo hello too | xargs echo bye
//    printf("argc is %d\n",argc); // 3
//    printf("argv0 is %s\n",argv[0]); // xargs  argv[0]指向输入的程序路径及名称
    char* cmd = argv[1]; // echo
//    printf("current cmd is %s.\n", cmd);
    for (i = 1; i < argc; i++) params[args_index++] = argv[i];
//    printf("params are:\n");
//    int k = 0;
//    for(int i=1;i<argc;++i)
//        printf("%s\n",params[k++]);
    // 管道符前边的命令会直接执行完 并生成一个结果集
    // 从结果集中每读出一行命令 都fork()一个子进程去执行它
    while ((n = read(STDIN_FILENO, line, MAXLINE)) > 0)
    {
//        printf("line: %s\n",line); // hello too
        if (fork() == 0) // child process
        {
            char *arg = (char*) malloc(sizeof(line));
            int index = 0;
            for (i = 0; i < n; i++)
            {
                if (line[i] == ' ' || line[i] == '\n')
                {
                    arg[index] = 0;
                    params[args_index++] = arg;
//                    printf("add params: %s\n",params[args_index-1]);
                    index = 0;
                    arg = (char*) malloc(sizeof(line));
                }
                else arg[index++] = line[i];
            }
            arg[index] = 0;
            params[args_index] = 0;
//            for(int i=0;i<args_index;++i)
//                printf("%s\n",params[i]);
            exec(cmd, params);
        }
        else wait((int*)0);
    }
    exit(0);
}

