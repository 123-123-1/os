#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int p_to_c[2];//公用的两个管道
int c_to_p[2];

int main(int argc, char *argv[])
{
    pipe(c_to_p);
    pipe(p_to_c);
    int id;
    char p_str[10];//接受字符串
    if(fork()==0){
       id=getpid();
       read(p_to_c[0],p_str,4);
       close(p_to_c[0]);
       printf("<%d>:received %s\n",id,p_str);
       write(c_to_p[1],"pong",4);
       close(c_to_p[1]);
    }
    else{
       id=getpid();
       write(p_to_c[1],"ping",4);
       close(p_to_c[1]);
       int status;
       wait(&status);
       read(c_to_p[0],p_str,4);
       close(c_to_p[0]);
       printf("<%d>:received %s\n",id,p_str);

    }
    exit(0);
}
