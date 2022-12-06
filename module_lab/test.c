#include<stdio.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
int main(void){
	int i,testgetdev;
	char buf[10];
	testgetdev=open("/dev/chardev",O_RDWR);
	if(testgetdev==-1){
		printf("I can't open the file\n");
		exit(0);
	}

	read(testgetdev, buf, 10);
	for(i=0;i<10;i++) {
		//buf[i] = 7; 
		printf("No.%d character is %d\n",i+1,buf[i]);
	}
		
    
    write(testgetdev, buf, 10);
	close(testgetdev);
    
	return 0;
}
