#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include<stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <poll.h>


#define Q_LEN 20
#define STDIN 0
#define MAX_CLIENT 20
#define TRUE 1
#define FALSE 0
#define DELIM " "
#define UPLOAD "upload"
#define DOWNLOAD "download"
// while(TRUE);
size_t BUFFSIZE= 128;
char BUFF[128];
int clients[MAX_CLIENT];

int opt_value = 1;
socklen_t opt_size = sizeof(opt_value);
struct sockaddr_in serv_addr;
socklen_t serv_addr_size;

int PORT_X; //for broadcasting
int PORT_Y = 5050; //for TCP connection (listener)
char* PORT_Y_MSG = "5050";
int udpid;
int tcpid;

__sighandler_t heart_beat(int sig){
    sendto(udpid ,PORT_Y_MSG,4 , 0 ,(struct sockaddr *) &serv_addr ,serv_addr_size);
    alarm(1);
}
int findSize(const char *file_name){
    //return -1 in fail
    struct stat st; 
    /*get the size using stat()*/
    if(stat(file_name,&st)==0)
        return (st.st_size);
    else
        return -1;
}
void setup_dirctory(char * path){
    // should be called after port number argument assignment
    struct stat st = {0};
    if (stat(path, &st) == -1) 
        mkdir(path, 0700);
    chdir(path);
}
int broadcast_socket(int portx)
{
    int sockid;
    // setup serv_addr
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portx);
    serv_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    serv_addr_size = sizeof(serv_addr);
    //make socket
    sockid = socket(PF_INET, SOCK_DGRAM,IPPROTO_UDP); 
    if (sockid == -1){
        write(2,"erro making socket\n" , 19*sizeof(char));
        exit(0);
    }
    // write(2,"broadcast socket made successfully\n", 33);
    //set up socket options broad cast
    if(setsockopt(sockid , SOL_SOCKET , SO_BROADCAST ,&opt_value , opt_size) < 0){
        write(2,"fail to set option broadcast to broadcast socket\n" , 49);
    }
    // write(2, "set option broadcast to broadcast socket\n" ,39);
    //set up socket option reuse
    if(setsockopt(sockid , SOL_SOCKET , SO_REUSEADDR ,&opt_value , opt_size) < 0){
        write(2,"fail to set option reuse to broad cast socket\n" , 46);
    }
    // write(2, "set option reuse\n" ,17);
    write(2 , "broadcasting...\n" , 16);
    return sockid;
}
int tcp_listener(int porty){
    int sockid;
    struct sockaddr_in listener_addr;
    // struct sockaddr_in client_addr;

    //make sure that all the memory is set to zero
    memset(&listener_addr , 0 , sizeof(listener_addr));
    // memset(&client_addr , 0 , sizeof(client_addr));

    //make socket
    sockid = socket(PF_INET, SOCK_STREAM, 0); 
    if (sockid == -1){
        write(2,"erro making listener socket\n" , 28);
        exit(0);
    }
    // write(2,"listener socket made successfully\n" , 34);
    
    //bind the socket to Y port
    listener_addr.sin_family = AF_INET;
    listener_addr.sin_port = htons(porty);
    listener_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    //bind
    if(bind(sockid , ( struct sockaddr* ) & listener_addr , sizeof(listener_addr)) == -1){
        write(2,"error in bind tcp socket\n" , 25);
        exit(0);
    }
    // write(2,"tcp socket bind successfull\n" , 28);

    //listen
    if(listen(sockid , Q_LEN) == -1){
        write(2,"error in listen non-blocking system call!\n" , 42);
        exit(0);
    }
    write(2,"listening ...\n" ,14);
    return sockid;
}
int file_exist(char* file_name){
    struct stat st = {0};
    if (stat(file_name, &st) == -1)
        return 0;
    return 1;
}
int send_file(int sockid , char* file_name){
    /*
    return -1 is failier and 0 in success
    */
    int file_size = findSize(file_name);
    if(file_size <= 0){
        write( 2, "file size not accessible\n" , 25);
        return -1;
    }
    char* context = (char*)malloc(file_size);
    int fd = open(file_name , O_RDONLY);
    if(fd < 0){
        write(2 , "could not open file\n" , 20);
        return -1;
    }
    if((read(fd , context , file_size))< 0){
        write(2 , "could not read the file\n" , 24);
        return -1;
    }
    // cast in t to char*
    char file_size_arr[50];
    sprintf(file_size_arr , "%d" , file_size);
    // send file size
    if((send(sockid , file_size_arr , strlen(file_size_arr)+1 , 0)) < 0){
        write(2 , "could not send the size of file to server\n" ,42);
        return -1;
    }
    // send context
    if((send(sockid , context , file_size , 0)) < 0){
        write(2 , "could not send the context of file\n" ,35);
        return -1;
    }
     // AK
    struct pollfd sd;
    int res;
    sd.fd = sockid;
    sd.events = POLLIN;
    res = poll(&sd, 1, 1000); // 1s timeout

    if (res <= 0)
    {
        write(2 ,"send unsuccessfull\n" , 19);
        return -1;
    }
    else{
        //read file size
        if(read(sockid , BUFF , BUFFSIZE) <= 0){
            write(2 , "file size read error\n" , 21);
            return -1;
        }
    }
    write(2 , "file sent\n" , 10);
    return 0;
}
int rcv_file(int sockid , char* file_name){
    /*
    return -1 is failier and 0 in success
    */
    //get the size of file that is going to be recv    
    char* buff = (char*)malloc(BUFFSIZE+1);
    memset(buff , 0 , BUFFSIZE);

    //read file size
    if(read(sockid , buff , BUFFSIZE) <= 0){
        write(2 , "could not read the size of file\n" , 32);
        return -1;
    }
    int file_size = atoi(buff);
    char* context = (char*)malloc(file_size);
    memset(context , 0 , file_size);
    if(read(sockid , context , file_size) <= 0){
        write(2 , "could not read the context of file\n" , 33);
        return -1;
    }
    //creat a new file and unlink previous ones
    unlink(file_name);
    // make new file
    int fd = open(file_name , O_CREAT | O_WRONLY , S_IRUSR |S_IWUSR );
    if(fd < 0){
        close(fd);
        write(2 , "could not creat a new file\n" , 27);
        return -1;
    }
    
    if((write(fd , context , file_size)) < 0){
        write(2 , "could not write to file\n" , 24);
        close(fd);
        return -1;
    }
    close(fd);
     if((send(sockid , "AK" , 2 , 0)) < 0){
        write(2 , "file send error\n" ,16);
        return -1;
    }
    write(2 , "file uploaded\n" ,14 );
    return 0;
}
int send_handler(int sockid , char* file_name){
    /* rturn -1 in fail and return 0 in success*/
    return send_file(sockid , file_name);
}
int rcv_handler(int sockid, char* file_name){
    /*
    return -1 in error and return 0 in success
    */
    return rcv_file(sockid , file_name);
}
int communication_handler(int client , int* clients , int i){
    /* return -1 if sth went wrong and 
     return 0 if socket is closes at the oher end
     return 1 if everything went ok*/
    int status;
    //zero out all the buffer before read
    memset(BUFF , 0 , BUFFSIZE);
    //  read date and check if closed 
    if ((status = read( client , BUFF, BUFFSIZE)) == 0)   
    {  
        //Close the socket and mark as 0 in list clients  
        close( client);   
        clients[i] = 0;
        return 0;   
    }
    if(status < 0)
        return -1;

    int up_or_down = 2; // 1 for up and 0 for down
    char* file_name;
    char* ptr = strtok(BUFF , DELIM);
    // length of the command is 1
    if (ptr == NULL){
        return -1;
    }

    // check what is command and if commadn is valid
    if (strcmp(ptr , UPLOAD) == 0)
        up_or_down = 1;
    else if(strcmp(ptr , DOWNLOAD) == 0)
        up_or_down = 0;
    else{
        write(2 , "wrong command\n" , 14);
        return -1;
    }
    ptr = strtok(NULL , DELIM);
    // file name is not given
    if(ptr == NULL){
        return -1;
    }
    file_name = ptr;
    // too many arguments
    if(strtok(NULL , DELIM)!= NULL){
        return -1;
        write(2 , "too many arguments\n" , 19);
    }
    // then fix ruined BUFF
    int j = 0;
    while(BUFF[j] != '\0'){
        j++;
    }
    BUFF[j] = ' ';
    // now buffer contains the original message
    if(up_or_down){
        write(2 , "upload request detected\n" , 24);
        return rcv_handler(client , file_name);
    }
    else{
        write(2 , "download request detected\n" , 26);
        if (!file_exist(file_name)){
            write( 2, "file not found\n" , 15);
        }
        return send_handler(client , file_name);
    }
    return 1;
}
int accept_tcp_connection(){
    struct sockaddr_in client_addr;
    int size;
    //make sure that all the memory is set to zero
    memset(&client_addr , 0 , sizeof(client_addr));
    size = sizeof(client_addr);
    return accept(tcpid , ( struct sockaddr* ) & client_addr ,(socklen_t *)& size );
}
int run_server(){
    memset(clients ,0 ,MAX_CLIENT); //zero out all clients
    fd_set read_fds;  // read file descriptor list for select()
    int fdmax;        // maximum file descriptor number in read list
    int client;
    int event;
    int i;
    struct timeval tv;
    write(2 , "server up :))\n" , 14);
     while(TRUE)   
    {
        // set up timeout
        tv.tv_sec = 60;
        tv.tv_usec = 0;
        //clear the socket set  
        FD_ZERO(&read_fds);   
     
        //add linstener socket to set  
        FD_SET(tcpid, &read_fds);   
        fdmax = tcpid;   
             
        //add child sockets to set  
        for ( i = 0 ; i < MAX_CLIENT ; i++)   
        {   
            //socket descriptor  
            client = clients[i];   
                 
            //if valid socket descriptor then add to read list  
            if(client > 0)   
                FD_SET( client , &read_fds);   
                 
            //highest file descriptor number, need it for the select function  
            if(client > fdmax)   
                fdmax = client;   
        }   
     
        //wait for an event on one of the sockets , with no timeout
        event = select( fdmax + 1 , &read_fds , NULL , NULL , NULL);       
        if ((event < 0) && (errno!=EINTR))   
        {   
            write(2 , "select error\n" ,13);   
        }   

        //if listener socket is set then there is new connection
        if (FD_ISSET(tcpid, &read_fds))   
        {   write(2 , "new connect request\n" , 20);
            if ((client =accept_tcp_connection())<0)   
            {   
                perror("accept");   
                exit(EXIT_FAILURE);   
            }   
                 
            //add new client's socket to array of sockets  
            for (i = 0; i < MAX_CLIENT; i++)   
            {   
                //if position is empty  
                if( clients[i] == 0 )   
                {   
                    clients[i] = client;     
                    break;   
                }   
            }   
        }   
             
        //else its some IO operation on some other socket 
        for (i = 0; i < MAX_CLIENT; i++)   
        {   
            client = clients[i];   
                 
            if (FD_ISSET( client , &read_fds))   
            {   write(2 , "new IO request\n" , 15);
                // tcp connection handler
                communication_handler(client , clients , i);
                //read data or update sockets in case of clozed intrupt
            }   
        }   
    }  
    return 0; 
}
int main(int argc, char const *argv[]){
    memset(&serv_addr , 0 , sizeof(serv_addr)); //zero out all the structure
    PORT_X = atoi(argv[1]); //get the x port from argv
    setup_dirctory(PORT_Y_MSG); //setup directory
    udpid = broadcast_socket(PORT_X);
    tcpid = tcp_listener(PORT_Y); //the listener socket

    // set up signal
    signal(SIGALRM , heart_beat);
    alarm(1);
    //run server
    run_server();
    // close sockets
    close(udpid);
    close(tcpid);
    return 0;
}