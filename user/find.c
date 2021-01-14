//
// Created by lenovo on 2021/1/13.
//
#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "../kernel/fs.h"
#include "user.h"
#include "../kernel/types.h"

#define STDDER_FILENO 2
#define READEND 0
#define WRITEEND 1
#define O_RDONLY 0

char*
fmtname(char *path)
{
    static char buf[DIRSIZ+1];
    char *p;

    // Find first character after last slash.
    // 找到最后一个'/'后面的第一个字符
    for(p=path+strlen(path); p >= path && *p != '/'; p--)
        ;
    p++;

    if(strlen(p) >= DIRSIZ)
        return p;
    // void *memmove(void *str1, const void *str2, size_t n) 从 str2 复制 n 个字符到 str1
    memmove(buf, p, strlen(p));
    // modify Here.
//    memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
    // no padding, just the file name is ok.
    buf[strlen(p)] = 0;
    return buf;
}

void
find(char *path, char *fileName)
{
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if((fd = open(path, O_RDONLY)) < 0){
        fprintf(STDDER_FILENO, "find: cannot open %s\n", path);
        return;
    }

    if(fstat(fd, &st) < 0){
        fprintf(STDDER_FILENO, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch(st.type){
        case T_FILE: {
            // 如果是文件，判断该文件是否是要寻找的目标文件
            char *f = fmtname(path);
            if (strcmp(f, fileName) == 0) {
//                printf("%s %d %d %l\n", fileName, st.type, st.ino, st.size);
                  // 打印文件全路径 不只是文件名
                  printf("%s\n",path);
            }
            break;
        }
            // 如果是目录，则递归使用find进行查找
        case T_DIR:
            if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
                printf("find: path too long\n");
                break;
            }

            // add '/'
            strcpy(buf, path);
            p = buf+strlen(buf);
            *p++ = '/';

            // 读取该目录下所有内容
            while(read(fd, &de, sizeof(de)) == sizeof(de)){
                if(de.inum == 0)
                    continue;
                // add de.name to path
                // 待搜索路径拼接上当前扫描到的目录
                memmove(p, de.name, DIRSIZ);
                p[DIRSIZ] = 0;

//                if(stat(buf, &st) < 0){
//                    printf("find: cannot stat %s\n", buf);
//                    continue;
//                }
//                printf("%s %d %d %d\n", fmtname(buf), st.type, st.ino, st.size);
                // don't find find . and ..
                if(!strcmp(de.name,".") || !strcmp(de.name, ".."))
                    continue;
                // recursive call find
                find(buf, fileName);
            }
            break;
    }
    close(fd);
}

int
main(int argc, char *argv[])
{
//    int i;
    // find 命令接收两个参数
    if(argc != 3){
        // 一般打印该命令用法的提示信息
        fprintf(STDDER_FILENO, "usage: find <path> <fileName>\n");
        exit(0);
    }
    // search_path and target_file
    find(argv[1],argv[2]);
    exit(0);
}
