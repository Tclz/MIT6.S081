#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

//echo命令用于字符串的输出
int
main(int argc, char *argv[])
{
  int i;

  for(i = 1; i < argc; i++){
      //写入到标准输出
    write(1, argv[i], strlen(argv[i]));
    //多个字符串之间增加一个" "符
    if(i + 1 < argc){
      write(1, " ", 1);
    } else {
        //写完后换行
      write(1, "\n", 1);
    }
  }
  exit(0);
}
