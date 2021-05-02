// init: The initial user-level program

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/spinlock.h"
#include "kernel/sleeplock.h"
#include "kernel/fs.h"
#include "kernel/file.h"
#include "user/user.h"
#include "kernel/fcntl.h"

char *argv[] = { "sh", 0 };

int
main(void)
{
  int pid, wpid;

  if(open("console", O_RDWR) < 0){
    mknod("console", CONSOLE, 0);
    open("console", O_RDWR);
  }
  dup(0);  // stdout
  dup(0);  // stderr

  for(;;){
    printf("init: starting sh\n");
    pid = fork();
    if(pid < 0){
      printf("init: fork failed\n");
      exit(1);
    }
    if(pid == 0){
      exec("sh", argv);
      printf("init: exec sh failed\n");
      exit(1);
    }

    for(;;){
      // this call to wait() returns if the shell exits,
      // or if a parentless process exits.
      //init进程一直wait到shell进程退出
      wpid = wait((int *) 0);
      if(wpid == pid){
          //如果shell进程退出了 重启它
        // the shell exited; restart it.
        break;
      } else if(wpid < 0){
          //返回的进程号小于0，出错
        printf("init: wait returned an error\n");
        exit(1);
      } else {
          //否则，有一个孤儿进程退出了，do nothing
        // it was a parentless process; do nothing.
      }
    }
  }
}
