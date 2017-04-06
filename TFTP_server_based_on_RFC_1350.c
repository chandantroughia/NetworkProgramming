//  project2.c
//
//
//  Created by Chandan Troughia and Sidharth Prabhakaran on 3/26/17.
//  ******************************************************
//  || Details:  Name                    RCS Id         ||
//  ||           Chandan Troughia        trougc@rpi.edu ||
//  ||           Sidharth Prabhakaran    prabhs2@rpi.edu||
//  ******************************************************
//
//  UDP server for file transfer based on RFC 1350
//
//  
//


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>


#define MAXDATA 512
#define RRQ 1
#define WRQ 2
#define DATA 3
#define ACK 4
#define ERROR 5
#define NOFILE 1
#define FILE_ALREADY 6
#define WRONG_MODE 0 //self defined errro to check the mode is binary(octet) other wise return an error packet.
#define ON 1
#define OFF 0


int EXIT = 0;
int createSocket();
struct sockaddr_in bindSocket(int sockfd);
void writeRequest(int sock_des, struct sockaddr_in socAddress,struct sockaddr_in child_addr, char filename[]);
int readRequest(int sock_des, struct sockaddr_in socAddress,struct sockaddr_in child_addr, char filename[]);
void error(int errorCode, int sock_des, struct sockaddr_in socAddress, unsigned char finalPacket[] );
static void create_alarm(int);


/*Error Codes

   Value     Meaning

   0         Not defined, see error message (if any).
   1         File not found.
   6         File already exists.
*/

void error(int errorCode, int sock_des, struct sockaddr_in socAddress, unsigned char finalPacket[]){
  
    if(errorCode == NOFILE)
    {
        fflush(NULL);
        sendto(sock_des, finalPacket, 1024, 0, (struct sockaddr *) &socAddress, sizeof(socAddress));
        char errorbuf[30] = {"File not found.\n"}; //While reading if client asks for the file which doesnot exist
        int length=strlen(errorbuf);
        errorbuf[length] = '\0';
        sprintf((char *) finalPacket, "%c%c%c%c", 0x00, 0x05, 0x00, 0x01);
        memcpy(& (finalPacket[4]), errorbuf, length);
        sendto(sock_des, finalPacket, length + ACK, 0, (struct sockaddr *) &socAddress, sizeof(socAddress));
        
    }
    else if(errorCode == FILE_ALREADY) //while writing if file already exists prevents overwrite
    {
        char errorbuf[30] = {"File already exists\n"};
        int length=strlen(errorbuf);
        errorbuf[length] = '\0';
        sprintf((char *) finalPacket, "%c%c%c%c", 0x00, 0x05, 0x00, 0x06);
        memcpy((char *) finalPacket + ACK, errorbuf, length);
        sendto(sock_des, finalPacket, length + ACK, 0, (struct sockaddr *) &socAddress, sizeof(socAddress));
    }

    else if(errorCode == WRONG_MODE) //sends error packet if mode is not binary(octet)
    {
        char errorbuf[30] = {"Set mode to Binary\n"};
        int length=strlen(errorbuf);
        errorbuf[length] = '\0';
        sprintf((char *) finalPacket, "%c%c%c%c", 0x00, 0x05, 0x00, 0x00);
        memcpy((char *) finalPacket + ACK, errorbuf, length);
        sendto(sock_des, finalPacket, length + ACK, 0, (struct sockaddr *) &socAddress, sizeof(socAddress));
    }

}

/*--------------------------Creating Socket - Calling This Function From Main-------------------------*/
int createSocket(){
    
    int sockfd;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd==-1)
        perror("Failed to create socket");
    return sockfd;
    
}

/*--------------Binding The Socket - Calling This Function From Main - After Creating Socket-------------*/
struct sockaddr_in bindSocket(int sockfd){
    
    struct sockaddr_in temp;
    struct sockaddr_in serv_addr;
    
    
    
    
    socklen_t len;
    
    if(sockfd < 0){
        perror("ERROR opening socket");
    }
    
    bzero((char *) &serv_addr, sizeof(serv_addr));      /*bzero() sets all values in a buffer to zero. It takes two argument
                                                         (1.) Pointer to the buffer (2.) Size of the buffer --> This line initializes serv_addr to zeros.*/
    //portno = atoi(argv[1]);                             // port number on which server listens, passed as an argument --> atoi converts string to integer.
    
    serv_addr.sin_family = AF_INET;                     /*The strusture has 4 fields. The firs field is short sin_family, which contains a code for a family.
                                                         It sould always be set to the symbolic costant of AF_INET.*/
    
    serv_addr.sin_port = htons(0);                 //*Second field in the structure is unsigned short sin_port, which contais the port number.
    
    serv_addr.sin_addr.s_addr = INADDR_ANY;             //*Third field is a structure type in_addr which contains a single unsigned long s_addr.
    
    if(bind(sockfd, (struct sockaddr *) &serv_addr,     //bind() binds a socket to an address, in this case the address of the current host and port number on whcih the server
            sizeof(serv_addr)) < 0)                     //will run. It takes 3 arguments, the socket file descriptor, the address to which is bound, and size of address to bound
        perror("ERROR on binding");                      //The 2nd argument is a pointer to a structure of type sockaddr, but what is passed in it is a structure os type sockaddr_in, and so this must be cast to the correct type. This can fail for a no. of reasons, the most obvious is that this socket is already in use on the machine*/
        
        len=(sizeof(temp));
        getsockname(sockfd,(struct sockaddr *) &temp,&len);
        
        
        return temp;
}

/*-----------------------------------------MAIN------------------------------------------*/
int main(int argc, char *argv[]){
    
    int sockfd, childSock; 
    int n, opcodeLength = 2, byte = 1;
    struct sockaddr_in serv_addr, cli_addr,child_addr;
    char mode[20]={0}, filename[256] = {0};
    socklen_t fromLength;
    char opcode;
    char octet[] = "octet";
    char buffer[1024];
    pid_t pid;

    sockfd=createSocket();
    serv_addr=bindSocket(sockfd);
    fromLength = sizeof(struct sockaddr_in);
    printf("%d\n",ntohs(serv_addr.sin_port) );
    while (1) {
        //printf("While started!\n");
        n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&cli_addr, &fromLength);
	
	
        buffer[strlen(buffer)] = '\0';          //add terminating char at the end of the buffer
        //printf("Opcode is: %d%d\n",buffer[0],buffer[1]);
        if(n < 0){
            perror("ERROR recvfrom");
        }
	
    
	    opcode = buffer[1];
        strcpy(filename,&buffer[2]);
        //printf("%s\n",filename );
        int modestart=strlen(filename)+opcodeLength+byte; // gives us the point where the mode starts in the datagram
        strcpy(mode,&buffer[modestart]);
        
        fflush(stdout);

        if(strcmp(mode, octet) == 0){ //if mode is octet


            if(fork()==0){  //creating child processes
        	    childSock=createSocket(); //creating individual sockets for each child to communicate
    	       	child_addr=bindSocket(childSock);  //binding that socket
            if(opcode == RRQ){ //if opcode is RRQ i.e. 1
            	writeRequest(childSock, cli_addr,child_addr, filename);
                }
            else if(opcode == WRQ){   //if opcode is RRQ i.e. 2
            	readRequest(childSock, cli_addr,child_addr, filename);
        	   }
	  	    exit(0);
    	   }
        }
    
        else{ // if mode is not octet
            unsigned char errorBuffer[1024];
            error(WRONG_MODE, sockfd, cli_addr, errorBuffer);
            exit(0);
        }
        //break;
    }
    while ((pid = waitpid(-1, NULL, 0))) {  //Wait for childs to exit
   if (errno == ECHILD) {
      break;
   }
}
    
    close(sockfd);   //close socket
    return EXIT_SUCCESS;
}





/*------------------------------Client GET Request (RRQ)-----------------------------------------*/
void writeRequest(int sock_des, struct sockaddr_in socAddress,struct sockaddr_in child_addr, char filename[]){
    FILE *fp;
    unsigned char finalPacket[1024];
    unsigned char receiveAck[1024];
    socklen_t recv_size;
    int count = 1;
    int size = 0;
    char dataBuffer[MAXDATA];

    finalPacket[2] = 0;
    finalPacket[3] = 0;
    receiveAck[2] = 0;
    receiveAck[3] = 0;
    if( access( filename, F_OK ) == -1 ) {//If there is no such file --> call error function
        error(NOFILE, sock_des, socAddress, finalPacket);
        exit(1);
       //exit(1);
    }
    else{ 
        fp = fopen(filename, "rb");
        signal(SIGALRM,create_alarm); //handling SIGALRM
        while(!feof(fp)){
            
            if((finalPacket[2] == receiveAck[2] && finalPacket[3] == receiveAck[3])) //check for the data count --> Handling Sorcerer's Apprentice Syndrome

            {
            size = fread(dataBuffer, 1, MAXDATA, fp);
            dataBuffer[strlen(dataBuffer)] = '\0';
            int len = ACK + size;
            sprintf((char *) finalPacket, "%c%c%c%c", 0x00, 0x03, 0x00, 0x00);
            memcpy((char *) finalPacket + ACK, dataBuffer, size);
            finalPacket[2] = (count & 0xFF00) >> 8;
            finalPacket[3] = (count & 0x00FF);
            //printf("Sent : %d - %d\n",finalPacket[2], finalPacket[3] );
            count++;
            if (sendto(sock_des, finalPacket, len, 0, (struct sockaddr *) &socAddress, sizeof(socAddress)) != len)
                puts("SENDING FAILED!");
            }
            memset(receiveAck, 0, 1024);
            alarm(ON);
            //int n = recvfrom(sock_des, receiveAck, 1024, 0, (struct sockaddr *) &socAddress, &recv_size);
            int n;
            /*Code to check working of SIGALARM*/
             #if 0
                sleep(10);
                n=-1;
            #endif
            if((n = recvfrom(sock_des, receiveAck, 1024, 0, (struct sockaddr *) &socAddress, &recv_size)) < 0){  //Handle Timeout
                
                if(errno == EINTR){
                     fprintf(stderr,"Timeout\n");
                     printf("%d\n",EXIT );
                     if(EXIT<10){
                        int len = ACK + size;
                        if (sendto(sock_des, finalPacket, len, 0, (struct sockaddr *) &socAddress, sizeof(socAddress)) != len){
                        puts("SENDING FAILED!");
                        }    
                     }
                    else if(EXIT>=10){
                        printf("Timeout\n");
                        close(sock_des);
                        exit(1);
                    }
                }
                else{
                    printf("Error\n");
                }
            }
            else{

                alarm(OFF);
                EXIT = 0;
            }
            //printf("Received: %d - %d\n", receiveAck[2], receiveAck[3]);
        if(size < MAXDATA){
                break;
            }
    }

    fclose(fp);
    }
    close(sock_des);
    exit(0);
}


/*------------------------------Client PUT Request (WRQ)-----------------------------------------*/
int readRequest(int sock_des, struct sockaddr_in socAddress,struct sockaddr_in child_addr, char filename[]){
    FILE *fp;  //File Pointer
    int nCount = 0;
    unsigned char sendAck[4];  //Acknowledgement Buffer
    unsigned char finalPacketW[1024];  //Final packet that we are sending
    unsigned char receiveW[1024]; //Packet that we are reading from recvfrom()
    socklen_t r_size;

    sprintf((char *) sendAck, "%c%c%c%c",  0x00, 0x04, 0x00, 0x00);  //Sending first Ack to client fro PUT request
    sendAck[2] = (nCount & 0xFF00) >> 8;
    sendAck[3] = (nCount & 0x00FF);
    
    if(sendto(sock_des, sendAck, ACK, 0, (struct sockaddr *) &socAddress, sizeof(socAddress)) != ACK) 
        puts("Sending Failed!");
    
    memset(finalPacketW, 0, sizeof(finalPacketW));
    char first = 0;
    char second = 0;
    
    if( access( filename, F_OK ) != -1 ) { //if filename is aleady present --> call error
        error(FILE_ALREADY, sock_des, socAddress, finalPacketW);
        exit(1);
    }           
    fp = fopen(filename, "wb");
    signal(SIGALRM, create_alarm);  //Handle SIGALRM
    while(1){
        
        
        int n;
        alarm(ON);
        //int n = recvfrom(sock_des, receiveW, sizeof(receiveW), 0, (struct sockaddr *) &socAddress, &r_size);
        /*Debug code for SIGALRM Working*/
        #if 0
        sleep(10);
        n=-1;
        #endif
        if((n = recvfrom(sock_des, receiveW, sizeof(receiveW), 0, (struct sockaddr *) &socAddress, &r_size)) < 0){ //TIme Out Condidtion
            if(errno == EINTR){
                fprintf(stderr, "Timeout\n");
                if(EXIT < 10){
                    fprintf(stderr, "resent\n" );
                    if(sendto(sock_des, sendAck, ACK, 0, (struct sockaddr *) &socAddress, sizeof(socAddress)) != ACK)
                        puts("Not Sent");
                }
                else if(EXIT>=10){
                        printf("Timeout\n");
                        close(sock_des);
                        exit(1);
                    }
            }
            else{
                printf("Receivefrom Error\n");
            }
        }
        else{
            alarm(OFF);
            EXIT = 0;
        }
        
 
        //printf("Received: %d - %d\n", receiveW[2], receiveW[3]);
        nCount++;
        
        if(receiveW[1] == 3){
            if((first < receiveW[2]) || (first == receiveW[2] && second < receiveW[3])){ //Handling Sorcerer's Apprentice Syndrome
                first  = receiveW[2];
                second = receiveW[3];
                memcpy(finalPacketW, receiveW + ACK, MAXDATA);
                fprintf(fp, "%s", finalPacketW);
                memset(finalPacketW, 0 , sizeof(finalPacketW));
                sprintf((char *) sendAck, "%c%c%c%c", 0x00, 0x04, 0x00, 0x00);
                sendAck[2] = receiveW[2];
                sendAck[3] = receiveW[3];
                //printf("Send : %d - %d\n", sendAck[2], sendAck[3]);
                if (sendto(sock_des, sendAck, ACK, 0, (struct sockaddr *) &socAddress, sizeof(socAddress)) != ACK)
                        puts("Failure");
                if(n < 516){
                    break;

                }

        }
        memset(receiveW, 0, sizeof(receiveW));
    }
       
    }
    
    fclose(fp);
    close(sock_des);

    return EXIT_SUCCESS;
}

static void create_alarm(int sig){
    EXIT++;
    return;
}




