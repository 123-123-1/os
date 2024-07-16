#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc ,char* argv[])
{
    char * newargv[MAXARG];
    if(argc<3){
        fprintf(2,"TRACE: wrong command\n");
        exit(1);
    }
    if(atoi(argv[1])<0){
        fprintf(2,"TRACE: wrong mask\n");
        exit(1);
    }
    for(int i=2;i<argc&&i<MAXARG;i++){
        newargv[i-2]=argv[i];
    }
    exec(newargv[0],newargv);
    exit(0);
}