#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

int main(int argc, char *argv[]) {
   	unsigned long num;
    
    if(argc<2){
    	printf("Unable to execute\n");
    	exit(1);
    }

    num = strtoul((argv[argc-1]), NULL, 10);
    
    unsigned long squareRoot = round(sqrt(num));



    if(argc==2){
    	printf("%lu\n", squareRoot);
    	return 0;
    }
    else{
    	char *args_new[argc];
    	for(int i=0;i<argc-2;i++){
    		args_new[i]=argv[i+1];
    	}

    	char arg_ans[64];

    	sprintf(arg_ans, "%lu", squareRoot);
       	args_new[argc-2] = arg_ans;
    	args_new[argc-1]=(char*)NULL;
    	
    	execv(args_new[0], args_new);
    	


    }
    
    
    return 0;
}
