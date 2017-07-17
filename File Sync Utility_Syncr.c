//  syncr.c
//
//
//  Created by Chandan Troughia and Sidharth Prabhakaran on 5/06/17.
//  ******************************************************
//  || Details:  Name                    RCS Id         ||
//  ||           Chandan Troughia        trougc@rpi.edu ||
//  ||           Sidharth Prabhakaran    prabhs2@rpi.edu||
//  ******************************************************
//
//  Sync Utilitiy in C
//
//
//

#define __USE_XOPEN
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h> 
#include <arpa/inet.h>
#include <sys/param.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netdb.h>
#include <openssl/md5.h>
#include <time.h>
//#include <features.h>


/*=======================Code for Mac (you can remove #include <openssl/md5.h> in case you are using the code below)=========================*/
/*#if defined(__APPLE__)
#  define COMMON_DIGEST_FOR_OPENSSL
#  include <CommonCrypto/CommonDigest.h>
#  define SHA1 CC_SHA1
#else
#  include <openssl/md5.h>
#endif   */

#define MaxSize 1024
#define serv 1
#define cli 2
#define clien 3

/*Function Declarations*/
int server(int portno);
int client(int portno);
int listfileswithMD5(int sd, int flag);
char * calcmd5(char *filename);
int calcmd5Server(char *filename,FILE *makeFile);

void readFile(int, FILE * , int);
void writeFile(char *filename,int, FILE * , int, int);


/*Function definitions*/

int main(int argc, char *argv[]){

    if(argc!=3){
        printf("Invalid number of Command-Line arguments\n");
        printf("<arg1> server/client <arg2> port no.\n");
        return EXIT_FAILURE;
    }
    int port=atoi(argv[2]);
    if(strcmp(argv[1],"server")==0)
        server(port);
    else if(strcmp(argv[1],"client")==0)
        client(port);
    else{

        printf("Invalid option: %s\n",argv[1] );
        return EXIT_FAILURE;
    }       

    return EXIT_SUCCESS;
}


//Server side of the program
int server(int portno){
    
    /* Create the temporary directory */
    FILE *fp = NULL;
    char template[] = "/tmp/tempdir.XXXXXX";
    char *tmp_dirname = mkdtemp (template);

    //struct stat fileInfoBuf;

    if(tmp_dirname == NULL)
    {
        perror ("tempdir: error: Could not create tmp directory");
        exit (EXIT_FAILURE);
    }
    //printf("%s\n",tmp_dirname);
    /*Change Directory*/
    if (chdir (tmp_dirname) == -1)
    {
        perror ("tempdir: error: ");
        exit (EXIT_FAILURE);
    }

    /*Create Sockets*/
    char message_buffer[MaxSize+1];
    char filename[256];
    int sockfd, newsockfd;
    socklen_t clilen;

    struct sockaddr_in serv_addr, cli_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0){
        perror( "socket() failed" );
        exit( EXIT_FAILURE );
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr,
             sizeof(serv_addr)) < 0)
        perror("ERROR on binding");

    listen(sockfd,5);

    //printf("Listen\n");

    clilen = sizeof(cli_addr);
    char *sendBuffer = calloc(MaxSize+1, sizeof(char));

    //char ack[15]={0};

    while (1) {
         //printf("Wait Accept\n");
        newsockfd = accept(sockfd,
                           (struct sockaddr *) &cli_addr, &clilen);
        //printf("Accept\n");
        //Clear Buffer
        bzero(message_buffer,MaxSize+1);
        //Receive a message from client
        int message_size = recv(newsockfd, message_buffer, MaxSize, 0); //Client sends the contents command
        //printf("%d\n",message_size);
        //while(message_size>0){
        if(message_size == 9 && strncmp(message_buffer, "contents",8) == 0){
            if( access( ".4220_file_list.txt", F_OK ) == -1 ) { //in case the content file is not yet created on the server
                bzero(sendBuffer,MaxSize+1);
                memcpy(sendBuffer, "getall\0", strlen("getall\0")); //signal client to send all the files to server
                send(newsockfd, sendBuffer, strlen(sendBuffer), 0); // This is done by sending a 'getall' signal to the cilent
                //printf("%s\n", sendBuffer);
                int msg_size = 0;
                while (1) {
                    //printf("Inside while 1\n");
                    bzero(message_buffer, MaxSize+1);
                    msg_size = recv(newsockfd, message_buffer, MaxSize, 0); // waiting for put command from client

                    if(msg_size <=0){
                        break;
                    }
                    message_buffer[msg_size] = '\0';
                    //printf("message_buffer %s\n", message_buffer);
                    if(strncmp(message_buffer, "put", 3) == 0) {


                        //printf("Inside while 1 message size: %d \n", msg_size);
                        bzero(sendBuffer, MaxSize+1);
                        bzero(filename, 256);
                        char num[7]={0};
                        strncpy(num,&message_buffer[40],6);
                        strncpy(filename, &message_buffer[46], msg_size - 46);
                        sprintf(sendBuffer, "get %s", filename);
                        //printf("NUM and Filename %s %s\n",num, filename);
                        int fsize=atoi(num);
                        //printf("Fsize %d\n",fsize );
                        fp = fopen(filename, "wb");

                        //int size = 0;
                        writeFile(filename,newsockfd,fp,fsize,serv);

                        
                    }

                }

            }
            //if the content file i.e. ".4220_file_list.txt" is present, then send the contents one-by-one to the cleint
            else{
                listfileswithMD5(newsockfd, serv);
                char filename[256] = {0};
                char hash[33]={0};
                FILE *fp=fopen(".4220_file_list.txt","r");
                char * line = NULL;
                size_t len = 0;
                ssize_t read;
                if (fp == NULL)
                    exit(EXIT_FAILURE);
                //int read=0;
                while ((read = getline(&line, &len, fp)) != -1) {  // get every line from the file
                    
                    //printf("Retrieved line of length: %zu \n", read);
                    //printf("%s", line);
                    bzero(message_buffer, MaxSize + 1);
                    bzero(filename, 256);
                    strncpy(filename, &line[36], read - 36-1);
                    strncpy(hash, line, 32);
                    FILE *fp = fopen(filename,"rb");


                    fseek(fp,0,SEEK_END);
                    int fsize = ftell(fp);
                    fseek(fp,0,SEEK_SET);
                    //printf("Fsize for file %s is %d\n",filename,fsize );
                    //sprintf(message_buffer,"put %s    %6d%s",fsize,hash,filename);
                    sprintf(message_buffer,"put %s    %6d%s",hash,fsize,filename);
                    //printf("message_buffer %s\n",message_buffer);
                    send(newsockfd,message_buffer,strlen(message_buffer),0);  // -->  send the line to client with the "put" command
                    recv(newsockfd, message_buffer, MaxSize, 0);                // It receives response for a put command
                    if(strncmp(message_buffer,"get", 3) == 0){                  // if response is "get" from the client side then send the file to the client

                        readFile(newsockfd,fp, fsize);
                        fclose(fp);

                    }
                    else if(strncmp(message_buffer,"skip", 4) == 0){            //If the response is "skip", then skip the transfer for that file
                        fclose(fp);
                        continue;
                    }
                    else {                                                      //If it enters here it means that the hash of the files differed
                        //fclose(fp);                                           //it needs to be queried -- >"query"
                        //if(strncmp(message_buffer,"query", 5) == 0)
                        struct stat fileInfoBuf;
                        //printf("*I SHOULDN'T BE HERE\n");
                        //printf("*Inside LSTAT\n");
                        bzero(sendBuffer,MaxSize+1);
                        bzero(message_buffer,MaxSize+1);
                        stat(filename,&fileInfoBuf);
                        char date[18];
                        strftime(date, 20, "%d-%m-%y %H:%M:%S", localtime(&(fileInfoBuf.st_mtime)));
                        sprintf(sendBuffer,"query %s   %s",date,filename);              // sends "query" command with date time and filename
                        //printf("*Inside LSTAT %s\n",sendBuffer);
                        send(newsockfd,sendBuffer,strlen(sendBuffer),0);
                        //printf("*Inside LSTAT af %s--fname: %s\n",sendBuffer,filename);
                        recv(newsockfd, message_buffer, MaxSize, 0);                        //if the response received is "get" the file, then it is sent to client
                        if(strncmp(message_buffer,"get",3) == 0){
                            
                            readFile(newsockfd,fp,fsize);
                            //fclose(f);
                            fclose(fp);
                        }
                        else if(strncmp(message_buffer,"put",3) == 0){                  //if the response is "put" the file, then it is sent to the server
                            fclose(fp);
                            FILE *f = fopen(filename, "wb");
                            fseek(f,0,SEEK_END);
                            int fsize=ftell(f);
                            fseek(f,0,SEEK_SET);
                            char tempName[7];
                            bzero(tempName, 7);
                            strncpy(tempName, &message_buffer[40], 6);
                            fsize = atoi(tempName);
                            //printf("* message_buffer %s\n",message_buffer );
                            //printf("Inside LSTAT else filename %s fsize %d\n",filename,fsize);
                            bzero(sendBuffer,MaxSize+1);
                            sprintf(sendBuffer, "get %s", filename);
                            send(newsockfd,sendBuffer,strlen(sendBuffer),0);
                            //printf("Inside LSTAT else again filename %s fsize %d\n",sendBuffer,fsize);
                            writeFile(filename,newsockfd,f,fsize,serv);
                            //fclose(f);
                        }
                    
                        
                    }
                }

                //fclose(fp);
                if (line)
                    free(line);
                char done[10]={0};
                sprintf(done,"done");                       // "Done" means that all the files from the .4220 file are synced and not the code below in this part
                send(newsockfd,done,strlen(done),0);        // it goes thorugh the client directoy and syncs all the files with the server, in case there is some new file

                //char filename[256]={0};
                //char hash[33]={0};
                int first=0;
                int bytes_read;
                bzero(message_buffer,MaxSize+1);
                bytes_read=recv(newsockfd,message_buffer,MaxSize,0);
                //printf("*************HALFWAY THROUGH THE TRANSFER*************\n");  //--> This is server sending everythiing it has to the client (case 5 in README.txt)
                if(strncmp(message_buffer, "put", 3) == 0){

                    for(;;){
                        //printf("Here Server infinite for\n");
                        if(first!=0){
                            bytes_read = recv(newsockfd, message_buffer, MaxSize, 0);    
                            if(bytes_read<=0)
                                break;   
                        }
                        if(strncmp(message_buffer, "done", 4) == 0){
                            break;
                        }
                        first=1;
                        int fsize = 0;
                        char tempName[7] = {0};
                        bzero(filename,256);
                        bzero(hash,33);
                        //bzero(receiveBuff,MaxSize+1);
                        strncpy(filename, &message_buffer[46], bytes_read - 46);
                        strncpy(hash, &message_buffer[4], 32);
                        //printf("outside for filename %s--%s--%s\n",filename,hash,message_buffer);
                        //file does not exist
                        if(access(filename, F_OK ) == -1){
                            //get file
                            //printf("infinite for file nox exist\n");
                            bzero(tempName, 7);
                            strncpy(tempName, &message_buffer[40], 6);
                            fsize = atoi(tempName);
                            bzero(sendBuffer,MaxSize+1);
                            sprintf(sendBuffer,"get %s",filename);
                            send(newsockfd,sendBuffer,strlen(sendBuffer),0);
                            //int size = 0;
                            FILE *fp = fopen(filename, "wb");
                            //printf("%s--%s--%d \n",filename,hash,fsize);
                            //fflush(NULL);

                            writeFile(filename,newsockfd,fp,fsize,serv);

                        }
                        //file exists, so compare hash
                        else{
                            //printf("infinite for file exist\n");
                            char *hash2 = calcmd5(filename);
                            if(strcmp(hash,hash2)==0){
                                bzero(sendBuffer,MaxSize+1);
                                sprintf(sendBuffer,"skip");
                                send(newsockfd,sendBuffer,strlen(sendBuffer),0);   
                            }
                            
                        }
                        bzero(message_buffer,MaxSize+1);
                        //recv(sd, receiveBuff, MaxSize, 0);  
                    }   
                }
            }
        }

        //printf("Done\n");
        listfileswithMD5(newsockfd, serv);
        //printf("No seg fault\n");
    }
    
    return EXIT_SUCCESS;
}
                 

// Client side of the program
int client(int portno){
    //char ack[15] = {0};
    char receiveBuff[MaxSize+1] = {0};
    char sendBuff[MaxSize+1] = {0};
      /* create TCP client socket (endpoint) */
      int sd = socket( PF_INET, SOCK_STREAM, 0 );

      if ( sd < 0 )
      {
        perror( "socket() failed" );
        exit( EXIT_FAILURE );
      }

    #if 0
      /* localhost maps to 127.0.0.1, which stays on the local machine */
      struct hostent * hp = gethostbyname( "localhost" );

      struct hostent * hp = gethostbyname( "128.113.126.29" );
    #endif

      struct hostent * hp = gethostbyname( "127.0.0.1" );


      if ( hp == NULL )
      {
        fprintf( stderr, "ERROR: gethostbyname() failed\n" );
        return EXIT_FAILURE;
      }

      struct sockaddr_in server;
      server.sin_family = AF_INET;
      memcpy( (void *)&server.sin_addr, (void *)hp->h_addr, hp->h_length );
      
     // unsigned short port = 9998;
      server.sin_port = htons(portno);

      //printf( "server address is %s\n", inet_ntoa( server.sin_addr ) );


      //printf( "connecting to server.....\n" );
      if ( connect( sd, (struct sockaddr *)&server, sizeof( server ) ) == -1 )
      {
        perror( "connect() failed" );
        return EXIT_FAILURE;
      }
    
    char * buffer=calloc(MaxSize+1,sizeof(char));
    memcpy(buffer,"contents\n",9);
    send(sd,buffer,strlen(buffer),0);
    char filename[256]={0};
    char hash[33]={0};
    int bytes_read;
    int first=0;
    
    bytes_read = recv(sd, receiveBuff, MaxSize, 0); // wait for get all from the server
    //printf("%s\n", receiveBuff);
    //printf("%d\n", bytes_read);
    
    if(bytes_read == 6 && strncmp(receiveBuff, "getall", 6) == 0){  // Client gets the "getall" from the server and sends all the files in the client directory to server temp dir.
        
        listfileswithMD5(sd, cli);
    }
    else if(strncmp(receiveBuff, "put", 3) == 0){       //if client receives "put" response then send the files one-by-one to the server
        for(;;){
            //printf("Here inside infinite for\n");
            //printf("outside for filename %s--%s--%s\n",filename,hash,receiveBuff);
            if(first!=0){
                bytes_read = recv(sd, receiveBuff, MaxSize, 0);    
                if(bytes_read<=0)
                    break;   
            }
            //printf("Here inside infinite for 2\n");
            if(strncmp(receiveBuff, "done", 4) == 0){
                break;
            }
             //printf("Here inside infinite for 3\n");
            first=1;
            int fsize = 0;
            char tempName[7] = {0};
            bzero(filename,256);
            bzero(hash,33);
            
            strncpy(filename, &receiveBuff[46], bytes_read - 46);
            strncpy(hash, &receiveBuff[4], 32);
            //printf("outside for filename %s--%s--%s\n",filename,hash,receiveBuff);
            //If the file does not exist
            if(access(filename, F_OK ) == -1){
                //get file
                //printf("infinite for file nox exist\n");
                bzero(tempName, 7);
                strncpy(tempName, &receiveBuff[40], 6);
                fsize = atoi(tempName);
                bzero(sendBuff,MaxSize+1);
                sprintf(sendBuff,"get %s",filename);            //send "get" to server so that it is ready to receive the file
                send(sd,sendBuff,strlen(sendBuff),0);
                //int size = 0;
                FILE *fp = fopen(filename, "wb");
                //printf("%s--%s--%d \n",filename,hash,fsize);
                fflush(NULL);

                writeFile(filename,sd,fp,fsize,cli);

            }
            //If file exists, so compare hash
            else{
                //printf("infinite for file exist\n");
                char *hash2 = calcmd5(filename);
                if(strcmp(hash,hash2)==0){
                    bzero(sendBuff,MaxSize+1);
                    sprintf(sendBuff,"skip");       //hashes are same the skip
                    send(sd,sendBuff,strlen(sendBuff),0);  

                }
                else{           //send "query" to the server to get the content of the file .4220 --> the code below will take care of the timestamp to do the trasnactions
                    struct stat fileInfoBuf;
                    char date[18] = {0};
                    bzero(tempName, 7);
                    strncpy(tempName, &receiveBuff[40], 6);
                    fsize = atoi(tempName);
                    char fname[256]= {0};

                    bzero(receiveBuff,MaxSize);
                    int bz=0;
                    bzero(sendBuff,MaxSize+1);
                    sprintf(sendBuff,"query");
                    send(sd,sendBuff,strlen(sendBuff),0);  
                    bz = recv(sd,receiveBuff,MaxSize,0);
                    if(strncmp(receiveBuff,"query",5) == 0){    //if the response is "query" along with the data timestamp, then transfer files accordingly
                        bzero(fname,256);
                        strncpy(fname,&receiveBuff[25], bz-26);
                        stat(filename,&fileInfoBuf);
                        strncpy(date,&receiveBuff[6], 17);
                        //printf("String received for stat %s : Filename %s--%s\n",date,fname,filename);
                        struct tm tm;
                        time_t t;
                        int seconds = 0;
                        //printf("Entire query: %s\n",receiveBuff);
                        //printf("Date %s\n",date );
                        strptime(date, "%d-%m-%y %H:%M:%S", &tm);
                        t = mktime(&tm);
                        //printf("Last modified time: %s -- %s", ctime(&fileInfoBuf.st_mtime),ctime(&t));
                        seconds = difftime( fileInfoBuf.st_mtime,t);
                        if(seconds>0){          //If the file is latest on the client side , client sends to server
                            //printf("My file is recent\n");
                            bzero(sendBuff,MaxSize+1);
                            FILE *fp = fopen(filename, "rb");
                            if(fp==NULL)
                                printf("File %s not found\n",filename);
                            fseek(fp,0,SEEK_END);
                            int fsize=ftell(fp);
                            fseek(fp,0,SEEK_SET);
                            bzero(hash,33);
                            strcpy(hash,calcmd5(filename));
                            //printf("Inside LSTAT if filename %s fsize %d\n",filename,fsize);
                            sprintf(sendBuff,"put %s    %6d%s",hash,fsize,filename);
                            //sprintf(sendBuff, "put %s", fname);
                            //FILE *f=fopen(fname,"rb");

                            send(sd,sendBuff,strlen(sendBuff),0);
                            bzero(receiveBuff,MaxSize+1);
                            recv(sd, receiveBuff, MaxSize, 0); 
                            readFile(sd,fp,fsize);
                            //send
                            fclose(fp);
                        }
                        else{       //it will send a "get" signal to server and receive the latest file.

                            //printf("Other file is recent\n");
                            //printf("Inside LSTAT else filename %s fsize %d\n",filename,fsize);
                            bzero(sendBuff,MaxSize+1);
                            sprintf(sendBuff, "get %s", fname);
                            FILE *f=fopen(filename,"wb");
                            
                            if(f==NULL)
                                printf("couldn't open file %s\n",filename);
                            send(sd,sendBuff,strlen(sendBuff),0);
                            writeFile(filename,sd,f,fsize,cli);
                            //fclose(f);

                        }
                    }
                   
                }
            }
            bzero(receiveBuff,MaxSize+1);
            //recv(sd, receiveBuff, MaxSize, 0);  
        }   
        listfileswithMD5(sd,clien);

        char done[10]={0};
        sprintf(done,"done");
        send(sd,done,strlen(done),0);    
        bzero(receiveBuff,MaxSize+1);
        
    }
    
    
      
      //printf("End\n");
      return EXIT_SUCCESS;
}

/*The function listfileswithMD5() is used to iterate through the files in the direcroty*/
//The file type should match DT_REG
//When ever this function is called in client or server, it means that it is iterating thought the files in the respective directory

int listfileswithMD5(int sock,int flag)
{
    DIR *d;
    struct dirent *dir;
    char * filename=".4220_file_list.txt";
    int nsize = strlen(filename);
    FILE *makeFile; 
    //= fopen(".4220_file_list.txt", "w");
    
    if(flag == serv){
        makeFile = fopen(".4220_file_list.txt", "w");
    }
    
    char * buffer=calloc(MaxSize+1,sizeof(char));
    char * finalbuffer=calloc(MaxSize+1,sizeof(char));
    d = opendir(".");
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            //printf("%s %s %d\n",dir->d_name, filename,strcmp(dir->d_name, filename));
            if(strncmp(dir->d_name, filename,nsize)==0){
                continue;
            }
            if(dir->d_type == DT_REG && flag==serv ){
                
                calcmd5Server(dir->d_name,makeFile);
            }
            else if(dir->d_type == DT_REG && (flag==cli || flag==clien) ){
                char *mdhash=calcmd5(dir->d_name);
                if(mdhash==NULL){
                    printf("Error calculating hash for file %s\n",dir->d_name );
                    continue;
                }
                //printf("Hash Value: %s %s\n", mdhash,dir->d_name);
                
                bzero(buffer,MaxSize+1);
                
                FILE *fp=fopen(dir->d_name,"rb");



                fseek(fp,0,SEEK_END);
                int fsize=ftell(fp);
                fseek(fp,0,SEEK_SET);
                //printf("Fsize :: %d\n",fsize);
                /* Not sure whether professor wants us to neglect empty files of transfer them
                    so commenting this block which will allow empty files to be transferred.
                if(fsize==0){
                    fclose(fp);
                    continue;
                }
                */
                sprintf( buffer, "put %s    %6d%s", mdhash,fsize ,dir->d_name );
                //sprintf(message_buffer,"put %s    %6d%s",hash,fsize,filename);
                //printf("I'm from list func %s\n",buffer );
                int size=strlen(buffer);
                buffer[size]='\0';
                bzero(finalbuffer,MaxSize+1);
                memcpy(finalbuffer,buffer,size);
                send(sock,finalbuffer,strlen(finalbuffer),0); //sending put command with the filename to the server
                bzero(finalbuffer,MaxSize+1);
                if(flag==2)
                    readFile(sock, fp, fsize);
                else{

                    char recvbuff[MaxSize+1]={0};
                    bzero(recvbuff,0);
                    recv(sock,recvbuff,MaxSize,0); //waiting for get or skip
                    if(strncmp(recvbuff,"get",3)==0){
                        readFile(sock, fp, fsize);
                        fclose(fp);
                    }
                    else if(strncmp(recvbuff,"skip", 4) == 0){
                        //printf("Skipped file %s\n", dir->d_name );
                        continue;
                    }
                    
                }

                
            }
            
        }
        
        closedir(d);
        if(flag==1)
            fclose(makeFile);
        
    }
    
    return(0);
}

/*This funtion gets the file pointer and sends it to the other end of the socket.
 Used by both the client and server */

void readFile(int socket, FILE *fp , int fsize){


    char * finalbuffer=calloc(MaxSize+1,sizeof(char));
    char *ack=calloc(15,sizeof(char));
    //printf("readFile %d -- %p -- %d\n",socket,fp,fsize);
    if(fsize>MaxSize){
        int sent=0;
        int bread = 0;

        while(sent<fsize){

            bzero(finalbuffer,MaxSize+1);
            if(fsize-sent>=1024)
                bread = fread(finalbuffer,1,MaxSize,fp);
            else
                bread = fread(finalbuffer,1,fsize-sent,fp);
            //sleep(2);
            //printf("bread %d\n", bread);
            sent += send(socket,finalbuffer,bread,0); // sending file contents to the server
            //printf("%d :: %d\n", bread ,sent);
            bzero(ack,15);
            recv(socket,ack,15,0);
            //printf("ACK: %s\n",ack );
            //recv();
        }
        //free(ack);
    }
    else{
        int nread = 0;
        bzero(finalbuffer,MaxSize+1);
        nread = fread(finalbuffer,1,fsize,fp);
        //printf("%s\n", finalbuffer);
        send(socket,finalbuffer,nread,0); // special case where file contents is less than 1024 bytes
        bzero(ack,15);
        recv(socket,ack,15,0);
        //printf("ACK: %s\n",ack );
    }
}

/*This funtion receives the contents of the file and writes in the directory from where it is called (client/server).
 Used by both the client and server */

void writeFile(char *filename,int sd, FILE *fp, int fsize, int flag){
    int size = 0;
    char *receiveBuff = calloc(MaxSize+1,sizeof(char));
    char *ack=calloc(15,sizeof(char));
    //printf("writeFile %d -- %p -- %d\n",sd,fp,fsize);
    if(fsize>MaxSize){
        int received=0;
        int check = 0;
        char *ack=calloc(15,sizeof(char));

        while(received<fsize){
            bzero(receiveBuff,MaxSize+1);
            if(fsize-received>=1024){
                check=recv(sd, receiveBuff, MaxSize, 0);
                received += check;
                fwrite(receiveBuff, 1, check, fp);

            }

            else{
                //printf("client function inside else\n");
                check= recv(sd, receiveBuff, fsize-received, 0);
                //printf("recvbuff: %s\n",receiveBuff);
                received += check;
                fwrite(receiveBuff, 1, check, fp);
            }
            //sleep(2);
            //printf("%s\n", finalbuffer);
            //sent += send(sock,finalbuffer,bread,0); // sending file contents to the server
            //printf("%d :: %d\n", received, fsize-received);
            //recv();
            bzero(ack,15);
            sprintf(ack,"ack %d",check);
            send(sd,ack,strlen(ack),0);
            //printf("ACK: %s\n",ack);
        }
        //free(ack);
    }
    else{

        size = recv(sd, receiveBuff, fsize, 0);
        fwrite(receiveBuff, 1, size, fp);
        bzero(ack,15);
        sprintf(ack,"ack %d",size);
        send(sd,ack,strlen(ack),0);
        //printf("Else client function ACK: ack %s\n",ack);
    }
    char *md5hash=calcmd5(filename);
    if(flag==1){
        printf("[server] Detected different & newer file: %s \n",filename);
        printf("[server] Downloading %s: %s\n",filename,md5hash);
    }
    else
    {
        printf("[client] Detected different & newer file: %s \n",filename);
        printf("[client] Downloading %s: %s\n",filename,md5hash);
    }
    
    fclose(fp);
    fp=NULL;

}

/*The function calcmd5Server() is called to calculte and write MD5 hash into ".4220_file_list.txt"*/

int calcmd5Server(char *filename,FILE *makeFile){

    unsigned char c[MD5_DIGEST_LENGTH];
    int i;

    FILE *inFile = fopen (filename, "rb");
    
    MD5_CTX mdContext;
    char m[MD5_DIGEST_LENGTH+1]={0};
    int bytes;
    unsigned char data[1024];

    if (inFile == NULL) {
        printf ("%s can't be opened.\n", filename);
        return 0;
    }

    MD5_Init (&mdContext);
    while ((bytes = fread (data, 1, 1024, inFile)) != 0)
        MD5_Update (&mdContext, data, bytes);
    MD5_Final (c,&mdContext);

    for(i = 0; i < MD5_DIGEST_LENGTH; i++) {

        fprintf(makeFile, "%02x", c[i]);
        sprintf(&m[i*2], "%02x", (unsigned int)c[i]);
    }
    //printf ("%s    %s\n",m, filename);
    fprintf(makeFile, "    %s\n", filename);

    fclose (inFile);
    //fclose(makeFile);
    return 0;
}

/*This function takes the filename as an argument and returns the Hash of that file*/

char * calcmd5(char * filename){

    unsigned char c[MD5_DIGEST_LENGTH];

    //unsigned char *hash;
    int i;
    FILE *inFile = fopen (filename, "rb");

    MD5_CTX mdContext;
    int bytes;
    unsigned char data[1024];

    if (inFile == NULL) {
        printf ("%s can't be opened.\n", filename);

        return NULL;
    }
    MD5_Init (&mdContext);
    while ((bytes = fread (data, 1, 1024, inFile)) != 0)
        MD5_Update (&mdContext, data, bytes);
    MD5_Final (c,&mdContext);

    char* buf_str = (char*) malloc (2*MD5_DIGEST_LENGTH + 1);
    char* buf_ptr = buf_str;

    for(i=0; i<MD5_DIGEST_LENGTH;i++){
        buf_ptr += sprintf(buf_ptr, "%02x", c[i]);
    }
    *(buf_ptr + 1) = '\0';

    return buf_str;
    
}
