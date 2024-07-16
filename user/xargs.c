#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

int main(int argc,char* argv[])
{
    char* buf[MAXARG];
    if(argc>MAXARG){
        printf("Too many param for xargs");
        exit(1);
    }
    for(int i=1;i<argc;i++){
       buf[i-1]=argv[i];
    }
    
    for(;;){
        char p[512];
        gets(p,512);

        if(strlen(p)==0){
            break;
        }

        int j=2;
        char* q=p;

        while(*q){
            if(*q==' '||*q=='\n'){
                *q++='\0';
            }
            else {
                buf[j]=q;
                j++;
                if(j>=MAXARG-1){
                    printf("Too many params for %s",argv[1]);
                    exit(1);
                }
                while(*q!=' '&&*q!='\n'&&*q!=0)
                  q++;
            }
        }
        buf[j]=0;

        if(fork()>0){
            int status;
            wait(&status);
        }
        else{
            exec(argv[1],buf);
            exit(0);
        }
    }
    exit(0);
}