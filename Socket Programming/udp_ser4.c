/**************************************
udp_ser.c: the source file of the server in udp transmission
**************************************/
#include "headsock.h"

void str_ser1(int sockfd);                                                           // transmitting and receiving function

int main(int argc, char *argv[])
{
	int sockfd;
	struct sockaddr_in my_addr;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {			//create socket
		printf("error in socket");
		exit(1);
	}

	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(MYUDP_PORT);//which converts a port number in host byte order to a port number in network byte order.
	my_addr.sin_addr.s_addr = INADDR_ANY; //The third field of sockaddr_in is a structure of type struct in_addr which contains only a single field unsigned long s_addr. This field contains the IP address of the host. 
	bzero(&(my_addr.sin_zero), 8);
	if (bind(sockfd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr)) == -1) {           //bind socket
		printf("error in binding");
		exit(1);
	}
	printf("start receiving\n");
	while(1) {
		str_ser1(sockfd);                        // send and receive
	}
	close(sockfd);
	exit(0);
}

void str_ser1(int sockfd)
{
	char buf[BUFSIZE];
	FILE *fp;
	struct pack_so recvpkt;
	struct ack_so ack;
	struct sockaddr_in addr;
	socklen_t len = sizeof(addr);
	int endflag =0 ; 
	long lseek=0;
	int n=0;
	int du =0 ;
	
	while(!endflag)
	{
		if ((n= recvfrom(sockfd, &recvpkt, BUFSIZE, 0,(struct sockaddr *)&addr, &len))==-1)                                   //receive the packet
		{
			printf("error when receiving\n");
			exit(1);
		}

	
		if ((du%2) == 0) {                     //send the ack if even 
		ack.num = 1;
		ack.len = 0;
		sendto(sockfd, &ack, 2, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) ;
		} 
		    
		/* Check for end of of file */
		if (recvpkt.data[n-HEADLEN-1] == '\0'){ //reaches EOY
		endflag = 1;
		recvpkt.len--;     //n -- to account for addition \0
		}

		memcpy((buf+lseek), recvpkt.data, n-HEADLEN);   //recvpkt conatins headlen
		lseek += recvpkt.len;
		du = (du+1)%3;
		printf("%d of data has been received and recvpkt.len is %d\n",n,(int)recvpkt.len);
		
	
    }


	if ((fp = fopen ("mytest.txt","wt")) == NULL)
	{
		printf("File doesn't exit\n");
		exit(0);
	}

	
	fwrite (buf , 1 , lseek , fp);					//write data into file
	fclose(fp);
	printf("The file has been successfully received!\nthe total data received is %d bytes\n", (int)lseek);
	
}

