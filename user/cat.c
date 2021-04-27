#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

char buf[512];

void
cat(int fd)
{
  int n;
  //利用while循环读入
  while((n = read(fd, buf, sizeof(buf))) > 0) {
      //标准输出到显示器
    if (write(1, buf, n) != n) {
      fprintf(2, "cat: write error\n");
      exit(1);
    }
  }
  if(n < 0){
    fprintf(2, "cat: read error\n");
    exit(1);
  }
}

//cat（英文全拼：concatenate）命令用于连接文件并打印到标准输出设备上
int
main(int argc, char *argv[])
{
  int fd, i;

  if(argc <= 1){
      //参数个数为1时 标准输入从键盘读入
    cat(0);
    exit(0);
  }

  //从参数中指定的各个文件读取
  for(i = 1; i < argc; i++){
    if((fd = open(argv[i], 0)) < 0){
      fprintf(2, "cat: cannot open %s\n", argv[i]);
      exit(1);
    }
    cat(fd);
    close(fd);
  }
  //exit(0)表示正常退出
  exit(0);
}
