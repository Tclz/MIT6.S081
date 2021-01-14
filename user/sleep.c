//
// Created by lenovo on 2021/1/12.
//

#include "../kernel/types.h"  // gcc不认unit类型 用户态程序需要手动加这行
#include "user.h"
int
main(int argc, char *argv[]){
    // argv第一个参数是可执行程序名
    // 实际参数从第二个开始
    // sleep 命令只需要一个参数
    if(argc != 2){
        fprintf(2, "Usage: sleep ticks...\n");
        exit(1);
    }
    sleep(atoi(argv[1]));
    exit(0);
}

