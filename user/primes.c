#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int fd[2][2];
int first;//存储第一个值，即素数

int main()
{
    pipe(fd[0]);
    for(int i=2;i<=35;i++){
        write(fd[0][1],&i,sizeof(i));
    }
    close(fd[0][1]);
    if(fork()==0){
       do{
            pipe(fd[1]);//创建右侧管道
            int i=0;
            read(fd[0][0],&first,sizeof(first));
            printf("prime %d\n",first);
            while(read(fd[0][0],&i,sizeof(i))){
               if(i%first!=0){
                  write(fd[1][1],&i,sizeof(i));
               }
            }
            close(fd[0][0]);//关闭左手的读 
            close(fd[1][1]);//关闭右手的写
            fd[0][0]=fd[1][0];//右手导左手
          
            if(i==0){
              close(fd[1][0]);
              break;
            }
        }while(fork()==0);//产生子进程继续干活，父进程休息
    }
    int status;
    wait(&status);
    exit(0);
}