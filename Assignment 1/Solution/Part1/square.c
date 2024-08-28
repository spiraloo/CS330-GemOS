#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
   	unsigned num;
    
    if(argc<2){
    	printf("Unable to execute\n");
    	exit(1);
    }

    num = strtoul(argv[argc-1], NULL, 10);
    
    unsigned long square = num * num;

    if(argc==2){
    	printf("%lu\n", square);

    }
    else{
    	char *args_new[argc];
    	for(int i=0;i<argc-2;i++){
    		args_new[i]=argv[i+1];
    	}

    	char arg_ans[64];

    	sprintf(arg_ans, "%lu", square);
       	args_new[argc-2] = arg_ans;
    	args_new[argc-1]=(char*)NULL;
    	
    	execv(args_new[0], args_new);
    }
    
    
    return 0;
}
