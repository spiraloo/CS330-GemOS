#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdint.h> // for uint64_t

//first is my free blocks list
void *first=NULL; 
#define chunk (unsigned long)(4*1024*1024)

void *memalloc(unsigned long size) 
{
	printf("memalloc() called\n");
	if(size==0){
		return NULL;
	}
	void* ans=NULL;
    	// allot_size is 8 bytes added to the size
	unsigned long allot_size= size+8;
	
	
	// checking if the allot_size is multiple of 8 or not
	if(allot_size%8!=0){
		allot_size = allot_size - (allot_size%8) + 8;
	}

	// ensuring it is minimum 24
	if(allot_size<=24){
		allot_size=24;
	}


    void* curr=first;
    uint64_t block_size = 0;
    // if curr is not null we are travesing to find appropriate block_size
    while(curr!=NULL){
    	block_size = *((uint64_t *)curr);
	    if(block_size>=allot_size){
	    	break;
	    }
	    else{
	    	curr=*((void **)(curr+8));
	    }

    }


    unsigned long new_size;
    // here we are comparing it with chunk size which is 4MB and making new_size a multiple of 4MB
    if(allot_size>chunk && allot_size%chunk!=0){
    	new_size=allot_size-(allot_size%chunk) + chunk;
    }
    else if(allot_size%chunk!=0){
    	new_size=chunk;
    }
    else{
    	new_size=allot_size;
    }


    
	
    if(curr==NULL){
    	// allocating memory if curr is NULL
    	curr = mmap(NULL, new_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    	if (curr == MAP_FAILED) {
		    perror("mmap");
		}
    	unsigned long b= new_size-allot_size;

    	if(b<24){
			ans=curr+8;
    	}
    	else{	
    		//traversing the current by adding 8 for front and 16 for previous 
    		*((uint64_t *)curr)= allot_size;

    		ans=(curr+8);

    		curr=(curr+allot_size);
    		*((uint64_t *)curr)=b;

    		*((void **)(curr+8))=first;
    		*((void **)(curr+16))=NULL;

    		if(first!=NULL)
    			*((void **)(first+16))=curr;

    		first=curr;
    	}

    }



    else{
    	// if curr is not NULL we are taking size of currrent as block size and calculating b
    	block_size = *((uint64_t *)curr);
    	unsigned long b= block_size-allot_size;
	//taking 2 cases of b as mentioned in the assignment 
    	if(b<24){
    		void* prev= *((void **)(curr+16));
    		if(prev!=NULL) 
    			*((void **)(prev+8)) = *((void **)(curr+8));
    		void* next= *((void **)(curr+8));
    		if(next!=NULL)
    			*((void **)(next+16))=prev;


    		if(prev==NULL){
    			first= *((void **)(first+8));
    		}


    		ans=curr+8;

    	}
    	else{

    		void* prev= *((void **)(curr+16));
    		if(prev!=NULL) 
    			*((void **)(prev+8)) = *((void **)(curr+8));
    		void* next= *((void **)(curr+8));
    		if(next!=NULL)
    			*((void **)(next+16))=prev;


    		if(prev==NULL){
    			first= *((void **)(first+8));
    		}
    		


    		*((uint64_t *)curr)= allot_size;

    		ans=(curr+8);

    		curr=(curr+allot_size);
    		*((uint64_t *)curr)=b;

    		*((void **)(curr+8))=first;
    		*((void **)(curr+16))=NULL;

    		if(first!=NULL)
    			*((void **)(first+16))=curr;

    		first=curr;

    	}

    }



    return ans;
}

int memfree(void *ptr)
{
	printf("memfree() called\n");

	void* new=ptr-8;

	uint64_t block_size = *((uint64_t *)new);

	void* search_1 = new+block_size;
	void* search_2 = new;

	// making 2 pointers to traverse the free block list for the right or left side presence
	void* curr=first;
	while(search_1!= curr && curr!=NULL){
		curr=*((void**)(curr+8));
	}

	void* new_curr=first;
	uint64_t block_size_1 = *((uint64_t *)new_curr);

	while((new_curr+block_size_1)!=search_2 && new_curr!=NULL){
		new_curr=*((void**)(new_curr+8));
	}
	// making 4 cases either both won't be free, one would be fre right or left and last case is both are free.
	if(curr==NULL && new_curr==NULL){
		if(first!=NULL){
			*((void**)(first+16))=new;
			*((void**)(new+8))=first;
			first=*((void**)(first+16));
		}

	}
	else if(curr!=NULL && new_curr==NULL){
		*((uint64_t *)new)= block_size+ *((uint64_t *)new);

		void* prev= *((void **)(curr+16));
    	if(prev!=NULL) 
    		*((void **)(prev+8)) = *((void **)(curr+8));
    	void* next= *((void **)(curr+8));
    	if(next!=NULL)
    		*((void **)(next+16))=prev;

    	if(first!=NULL){
			*((void**)(first+16))=new;
			*((void**)(new+8))=first;
			first=*((void**)(first+16));
		}	
			

	}
	else if(curr==NULL && new_curr!=NULL){
		*((uint64_t *)new)= block_size_1+ *((uint64_t *)new);

		void* prev= *((void **)(new_curr+16));
    	if(prev!=NULL) 
    		*((void **)(prev+8)) = *((void **)(new_curr+8));
    	void* next= *((void **)(new_curr+8));
    	if(next!=NULL)
    		*((void **)(next+16))=prev;

    	if(first!=NULL){
			*((void**)(first+16))=new;
			*((void**)(new+8))=first;
			first=*((void**)(first+16));
		}	

	}
	else{
		*((uint64_t *)new)= block_size_1+ *((uint64_t *)new)+ block_size;

		void* prev_1= *((void **)(new_curr+16));
    	if(prev_1!=NULL) 
    		*((void **)(prev_1+8)) = *((void **)(new_curr+8));
    	void* next_1= *((void **)(new_curr+8));
    	if(next_1!=NULL)
    		*((void **)(next_1+16))=prev_1;


    	void* prev_2= *((void **)(curr+16));
    	if(prev_2!=NULL) 
    		*((void **)(prev_2+8)) = *((void **)(curr+8));
    	void* next_2= *((void **)(curr+8));
    	if(next_2!=NULL)
    		*((void **)(next_2+16))=prev_2;

    	if(first!=NULL){
			*((void**)(first+16))=new;
			*((void**)(new+8))=first;
			first=*((void**)(first+16));
		}
	}

	return 0;
}	

