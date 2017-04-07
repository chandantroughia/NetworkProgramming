//
//  project1.c
//
//
//  Created by Chandan Troughia and Sidharth Prabhakaran on 3/3/17.
//  ******************************************************
//  || Details:  Name                    RCS Id         ||
//  ||           Chandan Troughia        trougc@rpi.edu ||
//  ||           Sidharth Prabhakaran    prabhs2@rpi.edu||
//  ******************************************************
//
//  
//
// Incorporated sample code from the book --> the definitive guide / Daniel Steinberg, Stuart Cheshire.
// Simple example (single event source only)
// of how to handle DNSServiceDiscovery events using a select() loop

#include <dns_sd.h>
#include <stdio.h>			// For stdout, stderr
#include <string.h>			// For strlen(), strcpy(), bzero()
#include <errno.h>          // For errno, EINTR
#include <time.h>
#include <stdlib.h>
#include <math.h>

#ifdef _WIN32
#include <process.h>
typedef	int	pid_t;
#define	getpid	_getpid
#define	strcasecmp	_stricmp
#define snprintf _snprintf
#else
#include <sys/time.h>		// For struct timeval
#include <unistd.h>         // For getopt() and optind
#include <arpa/inet.h>		// For inet_addr()
#endif


#define LONG_TIME 100000000

static volatile int stopNow = 0;
static volatile int timeOut = LONG_TIME;


int socket_desc , client_sock , length , message_size, convert;
struct sockaddr_in server , client, temp;
char message_buffer[80];    // Server reads the characters into this buffer from socket
char buffer_2[80];          // Same as message buffer --> copying everyting to buffer_2 from message_buffer
char guess[80];             //Char array to capture the string part of the user input i.e. 'guess'/'GUESS'
char number[10];            //Char array to capture the number part of the user input i.e. '40'/'50' etc.
int guess_int, i;           //i is the counter and guess_int used for getting the asci of the char input
int counter = 0;            //Counter to keep the track of no. of guesses.
int server_random_no;       //To hold the random number generated

/*===================Function to handle events after the DNS service registry====================*/
int HandleEvents(DNSServiceRef serviceRef)
{
    int dns_sd_fd = socket_desc;
    //DNSServiceRefSockFD(serviceRef);
    int nfds = dns_sd_fd + 1;
    fd_set readfds;
    struct timeval tv;
    int result;
   
    FD_ZERO(&readfds);
    FD_SET(dns_sd_fd, &readfds);
    tv.tv_sec = timeOut;
    tv.tv_usec = 0;
    
    //Select waits on the created server socket descriptor
    result = select(nfds, &readfds, (fd_set*)NULL, (fd_set*)NULL, &tv);
    if (result > 0)
    {
        DNSServiceErrorType err = kDNSServiceErr_NoError;
        if (FD_ISSET(dns_sd_fd, &readfds))
            err = DNSServiceProcessResult(serviceRef);
        if (err) { fprintf(stderr,
                           "DNSServiceProcessResult returned %d\n", err);
            stopNow = 1; }
    }
    else
    {
        printf("select() returned %d errno %d %s\n",
               result, errno, strerror(errno));
        if (errno != EINTR) stopNow = 1;
    }
 


    //Accept connection from an incoming client
    client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&length);
    if (client_sock < 0)
    {
        perror("accept failed");
        //return 1;
        exit(EXIT_FAILURE);
    }
    puts("Connection accepted");

    //Generate random number
    srand(time(0));
    server_random_no = rand() % 100 + 1;

    //Game Loop
    while(1)
    {

        //Clear Buffer
        bzero(message_buffer,80);
        //Receive a message from client
        message_size = read(client_sock, message_buffer, 79);
        //Initilize all the variables everytime when it comes to the loop.
        for(int l=0;l<80;l++){
            buffer_2[l]='\0';
            guess[l]='\0';
            if(l<10){
                number[l]='\0';
            }
        }

        //Copying message_buffer to buffer_2 for our own convenience
        strncpy(buffer_2,message_buffer,strlen(message_buffer));
        for( i = 0; i < strlen(buffer_2); i++){
            if(buffer_2[i] == ' '){
                break;
            }
            guess[i] = buffer_2[i];
        }
        guess[i] = '\0';    //Adding null termination character at the end of the string

        // Copy the number from the message_buffer
        if(i+1<strlen(buffer_2)){
            strncpy(number,&buffer_2[i+1],(strlen(buffer_2)-i)-3);
        }
        //Converting the number from ascii to integer
        guess_int = atoi(number);
        //Error Condition to check the input from the user: ERROR CONDITION 1
        if(strcmp(guess, "guess") == 0 || strcmp(guess, "GUESS") == 0 ){
            
            //error condition if the user input is more than 100 and less than 0, ERROR CONDITION 2
            /*if(guess_int > 100 || guess_int <0){
                write(client_sock , "???\n" , 4);
            }else{*/
            
        //printf("%d, %d\n",server_random_no, guess_int );
        //Condition to check if the random number generated is equal to the user input
        if(guess_int == server_random_no){
            write(client_sock , "CORRECT\n" , 8);
            counter++;
            if(counter < (log(100)/log(2)) - 1){
                write(client_sock , "GREAT GUESSING\n" , 15);
                break;
            }
            else if(counter > (log(100)/log(2)) - 1 && counter < (log(100)/log(2)) + 1){
                write(client_sock , "AVERAGE\n" , 8);
                break;
            }
            else{
                write(client_sock , "BETTER LUCK NEXT TIME\n" , 22);
                break;
            }
        }
            //If user input is greater than the server random number
        else if(guess_int > server_random_no){
            write(client_sock , "SMALLER\n" , 8);
            counter++;
        }
            //If user input is smaller than the server random number
        else{
            write(client_sock , "GREATER\n" , 8);
            counter++;
        }
            /*} ERROR CONDITION 2 ends here */
      }
           //If the 'ERROR CONDITION 1' fails
      else
      {
        write(client_sock , "???\n" , 4);
      }

        if(message_size == 0)
        {
            puts("Client disconnected");
            fflush(stdout);

        }
        else if(message_size == -1)
        {
            perror("recv failed");
        }

    }

    close(client_sock);

    return EXIT_SUCCESS;
}


static void MyRegisterCallBack(DNSServiceRef service,
                               DNSServiceFlags flags,
                               DNSServiceErrorType errorCode,
                               const char * name,
                               const char * type,
                               const char * domain,
                               void * context)
{
#pragma unused(flags)
#pragma unused(context)

    if (errorCode != kDNSServiceErr_NoError)
        fprintf(stderr, "MyRegisterCallBack returned %d\n", errorCode);
    else
        printf("%-15s %s.%s%s\n","REGISTER", name, type, domain);
    //return EXIT_SUCCESS;
}
//DNS service registry below
static DNSServiceErrorType MyDNSServiceRegister(int port)
{
    DNSServiceErrorType error;
    DNSServiceRef serviceRef;

    error = DNSServiceRegister(&serviceRef,
                               0,                  // no flags
                               0,                  // all network interfaces
                               "Sid's game",       // name
                               "_gtn._tcp",        // service type
                               "local",            // register in default domain(s)
                               NULL,               // use default host name
                               htons(port),        // port number
                               0,                  // length of TXT record
                               NULL,               // no TXT record
                               MyRegisterCallBack, // call back function
                               NULL);              // no context

    if (error == kDNSServiceErr_NoError)
    {
        printf("Registered Sid's game under _gtn._tcp service type");
        //Game will not end untill stopped with ^C --> Added as per professors instructions
        while(1){
            HandleEvents(serviceRef);

        }
        DNSServiceRefDeallocate(serviceRef);

    }

    return error;
}


//main
int main (int argc, const char * argv[])
{


    socklen_t len=sizeof(temp);



    //Clear Buffer
    bzero((char *) &socket_desc, sizeof(socket_desc));




    //Create socket
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1)
    {
        printf("Could not create socket");
    }
    puts("Socket created");

    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( 0 );

    //Bind
    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
        //print the error message
        perror("bind failed. Error");
        return 1;
    }
    //puts("bind done");
    getsockname(socket_desc,(struct sockaddr *) &temp,&len);

    //Listen
    listen(socket_desc , 3);

    //Accept and incoming connection
    puts("Waiting for incoming connections...");
    length = sizeof(struct sockaddr_in);

    //printf("server number %d\n", server_random_no);

    DNSServiceErrorType error = MyDNSServiceRegister(ntohs(temp.sin_port));
    //fprintf(stderr, "DNSServiceDiscovery returned %d\n", error);

    return 0;
}
