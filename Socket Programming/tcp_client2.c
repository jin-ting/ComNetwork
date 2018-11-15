/*******************************
udp_client.c: the source file of the client in udp
********************************/

#include "headsock.h"

float str_cli1(FILE *fp, int sockfd, struct sockaddr *addr, int addrlen, long *len);             
void tv_sub(struct timeval *out, struct timeval *in); 

int main(int argc, char *argv[])
{
	int sockfd; //sockfd and newsockfd are file descriptors, i.e. array subscripts into the file descriptor table . These two variables store the values returned by the socket system call and the accept system call.
	struct sockaddr_in ser_addr; //A sockaddr_in is a structure containing an internet address. This structure is defined in netinet/in.h.
	char **pptr;
	struct hostent *sh;
	struct in_addr **addrs;
    FILE *fp; //file descriptor
	float ti, rt;
	long len;

	if (argc!= 2)
	{
		printf("parameters not match.");
		exit(0);
	}

	if ((sh=gethostbyname(argv[1]))==NULL) {             //get host's information
		printf("error when gethostbyname");
		exit(0);
	}

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);             //create socket y taking in addrerss domianm, type of socket.The third argument is the protocol. If this argument is zero (and it always should be except for unusual circumstances), the operating system will choose the most appropriate protocol. It will choose TCP for stream sockets and UDP for datagram sockets.The socket system call returns an entry into the file descriptor table (i.e. a small integer). This value is used for all subsequent references to this socket. If the socket call fails, it returns -1.
	if (sockfd<0)
	{
		printf("error in socket");
		exit(1);
	}

	addrs = (struct in_addr **)sh->h_addr_list; //A pointer to a list of network addresses for the named host.  Host addresses are returned in network byte order.
	printf("canonical name: %s\n", sh->h_name);
	for (pptr=sh->h_aliases; *pptr != NULL; pptr++)
		printf("the aliases name is: %s\n", *pptr);			//printf socket information
	switch(sh->h_addrtype)
	{
		case AF_INET:
			printf("AF_INET\n");
		break;
		default:
			printf("unknown addrtype\n");
		break;
	}

	ser_addr.sin_family = AF_INET;
	ser_addr.sin_port = htons(MYUDP_PORT);
	memcpy(&(ser_addr.sin_addr.s_addr), *addrs, sizeof(struct in_addr));
	bzero(&(ser_addr.sin_zero), 8); //The function bzero() sets all values in a buffer to zero. It takes two arguments, the first is a pointer to the buffer and the second is the size of the buffer. Thus, this line initializes serv_addr to zeros. ----

    if((fp = fopen ("myfile.txt","r+t")) == NULL)
	{
		printf("File doesn't exit\n");
		exit(0);
	}
	
	printf("Opening file\n");

    ti = str_cli1(fp, sockfd, (struct sockaddr *)&ser_addr, sizeof(struct sockaddr_in), &len);                  
	rt = (len/(float)ti);               //caculate the average transmission rate
	printf("Time(ms) : %.3f, Data sent(byte): %d\nData rate: %f (Kbytes/s)\n", ti, (int)len, rt);

	close(sockfd );
	fclose(fp);

	exit(0);
}

float str_cli1(FILE *fp, int sockfd, struct sockaddr *addr, int addrlen, long *len)
{
	char *buf;
	long lsize, ci;
	struct ack_so ack;
	struct pack_so pkt; 
	struct sockaddr_in senderaddr;
	socklen_t saddrlen = sizeof(senderaddr);
	float time_inv = 0.0;
	struct timeval sendt, recvt;
	int n =0;
	int du = 0;
	int DATALEN [2] = {DULEN, 2*DULEN};
	

	fseek (fp , 0 , SEEK_END);
	lsize = ftell (fp);
	rewind (fp);
	printf("The file length is %d bytes\n", (int)lsize);
	printf("the packet length for 1 DU is %d bytes\n",DATALEN[0]);
	printf("the packet length for 2DU is %d bytes\n",(DATALEN[1]) );

  //allocate memory to contain the whole file.
	buf = (char *) malloc (lsize);
	if (buf == NULL) exit (2);

  // copy the file into the buffer.
	fread (buf,1,lsize,fp);

  /*** the whole file is loaded in the buffer. ***/
	buf[lsize] ='\0';	//append the end byte

									
	gettimeofday(&sendt, NULL);	 //get the current time

    /*** Transmitting file ***/
	while(ci<= lsize)
	{ 
		/*To determine the size of data sent.Send before checking for ack */
		if ((lsize+1-ci) <= DATALEN[du]) {
			pkt.len =  lsize+1-ci; //send remaining data
		}
		else {
			pkt.len = DATALEN[du] ; //sent data based on du size
		}	
		memcpy(pkt.data, (buf+ci), pkt.len); //*
		printf("datalen is %d\n",DATALEN[du]);
		
		if (ci == 0 ) {     //First occurence send w/o waiting for ackn
		    n = sendto(sockfd, &pkt, pkt.len + HEADLEN, 0, addr ,addrlen);
		}
		/*check for acknowledgement from sender*/
		else {
			if (recvfrom(sockfd, &ack, 2, 0,(struct sockaddr *)&senderaddr, &saddrlen)==-1) {                                  //receive the ack{
				printf("ack error");
				exit(1);
			}
			else {
				n = sendto(sockfd, &pkt, pkt.len + HEADLEN, 0, addr ,addrlen);
			}
		}
		if(n == -1) {
			printf("send error!");								//chck for error during tranmission of data
			exit(1);
		}
		printf("%d  of message is sent successfully! and len is %d\n\n",n,(int) pkt.len);			
		ci += pkt.len;
		du = (du+1)%2 ;
	}

	gettimeofday(&recvt, NULL);
	*len= ci; 
	                                               //get current time
	tv_sub(&recvt, &sendt);                                                                 // get the whole trans time
	time_inv += (recvt.tv_sec)*1000.0 + (recvt.tv_usec)/1000.0;
	return(time_inv);
}

void tv_sub(struct  timeval *out, struct timeval *in)
{
	if ((out->tv_usec -= in->tv_usec) <0)
	{
		--out ->tv_sec;
		out ->tv_usec += 1000000;
	}
	out->tv_sec -= in->tv_sec;
}