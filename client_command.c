#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <poll.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <sys/time.h>

#include<stdio.h>

#define Q_LEN 20
#define BUFFSIZE 128

#define STDIN 0
#define STDOUT 1
#define STDERR 2

#define FALSE 0
#define TRUE 1

#define UPLOAD "upload"
#define DOWNLOAD "download"
#define DELIM " "
#define UP 1
#define DOWN 0

char BUFF[BUFFSIZE];

int PORT_X ;
int PORT_M ;
int PORT_Z;
const char* PATH_NAME;

struct sockaddr_in client_addr;
socklen_t client_addr_size;

struct sockaddr_in heart_addr;
socklen_t heart_addr_size;

struct sockaddr_in serv_addr;
socklen_t serv_addr_size;

struct sockaddr_in peer_addr;
socklen_t peer_addr_size;

int udpid;
int p2pid;
int findSize(const char *file_name){
    //return -1 in fail
    struct stat st; 
    /*get the size using stat()*/
    if(stat(file_name,&st)==0)
        return (st.st_size);
    else
        return -1;
}
void setup_dirctory(const char * path){
    // should be called after port number argument assignment
    struct stat st = {0};
    if (stat(path, &st) == -1)
        mkdir(path, 0700);
    chdir(path);
}
void setup_client_addr(){
    memset(&client_addr , 0 , sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(PORT_M);
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    client_addr_size = sizeof(client_addr);
}
void setup_heart_addr(){
    memset(&heart_addr , 0 , sizeof(heart_addr));
    heart_addr.sin_family = AF_INET;
    heart_addr.sin_port = htons(PORT_X);
    heart_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    heart_addr_size = sizeof(heart_addr);
}
void setup_peer_addr(){
    memset(&peer_addr , 0 , sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(PORT_Z);
    peer_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    peer_addr_size = sizeof(peer_addr);
}
void setup_serv_addr(int port){
    memset(&serv_addr , 0 , sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr_size = sizeof(serv_addr);
}

int make_brodcast_sock(struct sockaddr_in addr){
    int opt_value=1;
    socklen_t opt_size = sizeof(opt_value);

    int sockid;
    int status;

    // make the socket
    sockid = socket(PF_INET, SOCK_DGRAM, 0); //make socket
    if (sockid == -1){
        write(2,"erro making socket\n" , 19);
        exit(0);
    }
    // write(2,"socket made successfully\n" , 25);
    
    // setup options
    if(setsockopt(sockid , SOL_SOCKET , SO_BROADCAST ,&opt_value , opt_size) < 0){
        write(2,"fail to set option broadcast\n" , 29);
    }
    // write(2, "set option broadcast\n" ,21);
    if(setsockopt(sockid , SOL_SOCKET , SO_REUSEADDR ,&opt_value , opt_size) < 0){
        write(2,"fail to set option reuse\n" , 25);
    }
    // write(2, "set option reuse\n" ,17);

    //bind
    if(bind(sockid , ( struct sockaddr* ) & addr , sizeof(addr)) == -1){
        // printf("error code :%d\n" , errno);
        write(2,"error in bind procces!\n" , 23*sizeof(char));
        exit(0);
    }
    // write(2,"bind successfull\n" , 17*sizeof(char));
    return sockid;
}
int make_tcp_sock(int port){
    /*
    return -1 if gone wrong
    return sockid if successfull
    */
    if (port == -1){
        return -1;
    }
    int opt_value = 1;
    socklen_t opt_size = sizeof(opt_value);
    int sockid;
    int status;

    setup_serv_addr(port);

    // make the socket
    sockid = socket(PF_INET, SOCK_STREAM, 0); //make socket
    if (sockid == -1){
        write(2,"erro making socket\n" , 19*sizeof(char));
        return -1;
    }
    // write(2,"socket made successfully\n" , 25*sizeof(char));

    // set option reuse
    if(setsockopt(sockid , SOL_SOCKET , SO_REUSEADDR ,&opt_value , opt_size) < 0){
        write(2,"fail to set option reuse to broad cast socket\n" , 46);
        close(sockid);
        return -1;
    }
    // write(2, "set option reuse\n" ,17);

    //bind
    if(bind(sockid , ( struct sockaddr* ) & client_addr , sizeof(client_addr)) == -1){
        write(2,"error in bind procces!\n" , 23*sizeof(char));
        close(sockid);
        return -1;
    }
    // write(2,"bind successfull\n" , 17*sizeof(char));

    //connect
    status = connect(sockid , (struct sockaddr*)&serv_addr ,sizeof(serv_addr));
    if(status == -1){
        // printf("status number : %d" , status);
        write(2, "connect unsuccessfull\n" , 22);
        close(sockid);
        return -1;
    }
    // write(2, "connect successfull\n" , 20*sizeof(char));
    return sockid;
}
int find_serv(){
    /*
    returns tcp port of server or -1 in failiar
    */
    struct pollfd sd;
    int res;

    sd.fd = udpid;
    sd.events = POLLIN;
    res = poll(&sd, 1, 5000); // 5s timeout

    if (res <= 0)
    {
        return -1;
    }
    else
    {
        int nbytes;
        nbytes = recvfrom(udpid , BUFF , BUFFSIZE , 0 ,(struct sockaddr *) &heart_addr , &heart_addr_size);
        if(nbytes == -1)
            return -1;
        BUFF[nbytes] = '\0';
        return atoi(BUFF);
    }
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
    // printf("send file1\n");
    char* context = (char*)malloc(file_size);
    int fd = open(file_name , O_RDONLY);
    if(fd < 0){
        write(2 , "file could not be opened\n" , 25);
        return -1;
    }
    if((read(fd , context , file_size))< 0){
        write(2 , "could not read the file\n" , 24);
        write(2 , "try again\n" , 10);
        return -1;
    }
    // printf("send file2\n");
    // cast int to char*
    char file_size_arr[50];
    sprintf(file_size_arr , "%d" , file_size);
    // send file size
    if((send(sockid , file_size_arr , strlen(file_size_arr)+1 , 0)) < 0){
        write(2 , "file size send error\n" ,21);
        return -1;
    }
    // printf("send file3\n");
    // send context
    if((send(sockid , context , file_size , 0)) < 0){
        write(2 , "file send error\n" ,16);
        return -1;
    }
    // printf("send file4\n");
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
            write(2 , "AK was not sent\n" , 16);
            return -1;
        }
    }
    write(2 , "file sent\n" , 10);
    return 0;
}
int rcv_file(int sockid , char* file_name){
    // printf("rcv__note__1\n");
    /*
    return -1 is failier and 0 in success
    */
    //get the size of file that is going to be recv    
    char* buff = (char*)malloc(BUFFSIZE+1);
    memset(buff , 0 , BUFFSIZE);
    // poll for answer
    struct pollfd sd;
    int res;
    sd.fd = sockid;
    sd.events = POLLIN;
    res = poll(&sd, 1, 1000); // 1s timeout

    if (res <= 0)
    {
        write(2 ,"either connection lost or host doesn't have requested file\n" , 59);
        return -1;
    }
    else{
        //read file size
        if(read(sockid , buff , BUFFSIZE) <= 0){
            write(2 , "file size read error\n" , 21);
            return -1;
        }
    }
    // printf("rcv__note__2\n");
    int file_size = atoi(buff);

    char* context = (char*)malloc(file_size);
    memset(context , 0 , file_size);
    struct pollfd kd;
    kd.fd = sockid;
    kd.events = POLLIN;
    res = poll(&kd, 1, 3000); // 1s timeout

    if (res <= 0)
    {
        write(2 ,"connection was lost\n" , 20);
        return -1;
    }
    else{
        //read context of file
        if(read(sockid , context , file_size) <= 0){
            write(2 , "context read error\n" , 19);
            return -1;
        }
    }
    // printf("rcv__note__3\n");

    //creat a new file and unlink previous ones
    unlink(file_name);
    // make new file
    int fd = open(file_name , O_CREAT | O_WRONLY , S_IRUSR |S_IWUSR );
    // printf("fd is %d\n" , fd);
    // printf("and file name is\n");
    // write( 2 , file_name , strlen(file_name));
    if(fd < 0){
        close(fd);
        write(2 , "file open err\n" , 14);
        return -1;
    }
    
    if((write(fd , context , file_size)) < 0){
        write(2 , "write to file error\n" , 20);
        close(fd);
        return -1;
    }
    close(fd);
    // printf("rcv__note__4\n");
    // AK
    if((send(sockid , "AK" , 2 , 0)) < 0){
        write(2 , "file send error\n" ,16);
        return -1;
    }
    write(2 , "file recieved\n" ,14 );
    return 0;
}
int p2p_send_handler(char* file_name , int up_or_down){
    //1 is up , 0 is down
    // make the new msg: upload filenem.type portM
    // cast int to char* --> port number is in range of 0 to 65535
    char port_arr[6];
    sprintf(port_arr , "%d" , PORT_M);
    int msg_size;
    if(up_or_down)
        msg_size = strlen(UPLOAD) + strlen(file_name) + strlen(port_arr) + 2*strlen(DELIM) + 1;
    else
        msg_size = strlen(DOWNLOAD) + strlen(file_name) + strlen(port_arr) + 2*strlen(DELIM) + 1;
    
    char* msg = (char*)malloc(msg_size);
    if(up_or_down)
        strcpy(msg , UPLOAD);
    else
        strcpy(msg , DOWNLOAD);
    strcat(msg , DELIM);
    strcat(msg , file_name);
    strcat(msg , DELIM);
    strcat(msg , port_arr);

    // printf("^^^^^new mwssage size is: %d\n" , strlen(msg));
    // write(2 , msg ,strlen(msg));
    // write(2 , '\n' , 1);
    
    // make tcp port
    // make the socket
    int sockid , status;
    memset(&sockid , 0 , sizeof(int));
    int opt_value = 1;
    socklen_t opt_size = sizeof(opt_value);
    sockid = socket(PF_INET, SOCK_STREAM, 0); //make socket
    if (sockid == -1){
        write(2,"erro making socket\n" , 19);
        return -1;
    }
    // set option reuse
    if(setsockopt(sockid , SOL_SOCKET , SO_REUSEADDR ,&opt_value , opt_size) < 0){
        write(2,"fail to set option reuse to broad cast socket\n" , 46);
        close(sockid);
        return -1;
    }
    //bind
    if(bind(sockid , ( struct sockaddr* ) & client_addr , sizeof(client_addr)) == -1){
        write(2,"error in bind procces!\n" , 23);
        close(sockid);
        return -1;
    }
    // broadcast the message as p2pid is binded to port sender will detect it's own 
    // request too. this matter is handeled in recv routin
    sendto(p2pid ,msg,msg_size , 0 ,(struct sockaddr *) &peer_addr ,peer_addr_size);
    write(2 , "p2p request broadcasted\n" , 24);

    // //connect
    // status = connect(sockid , (struct sockaddr*)&serv_addr ,sizeof(serv_addr));
    // if(status == -1){
    //     printf("status number : %d" , status);
    //     write(2, "connect unsuccessfull\n" , 22*sizeof(char));
    //     return -1;
    // }


    //listen for new connection
    // set qlen to one so that only one peer could request
    // poll and time out
    struct sockaddr_in new_addr;
    int size;
    int peer;


    struct pollfd sd;
    int res;
    sd.fd = sockid;
    sd.events = POLLIN;
    res = poll(&sd, 1, 2000); // 1s timeout

    if (res <= 0)
    {
        write(2 ,"either connection lost or host doesn't have requested file\n" , 59);
        close(sockid);
        return -1;
    }
    else{
        //read file size
        write(2,"waiting for response...\n" ,24);
        if(listen(sockid, Q_LEN) == -1){
            write(2,"error in listen non-blocking system call!\n" , 42);
            close(sockid);
            return -1;
        }
        // printf("after listen\n");
        //accept new connection
        //make sure that all the memory is set to zero
    }

    struct pollfd hd;
    hd.fd = sockid;
    hd.events = POLLIN;
    res = poll(&hd, 1, 1000); // 1s timeout
    if (res <= 0)
    {
        write(2 ,"either connection lost or host doesn't have requested file\n" , 59);
        close(sockid);
        return -1;
    }
    else{
        memset(&new_addr , 0 , sizeof(new_addr));
        size = sizeof(new_addr);
        peer = accept(sockid , ( struct sockaddr* ) & new_addr ,(socklen_t *)& size );
        if(peer == -1){
            write(2 , "accpet fialed\n" , 14);
            close(sockid);
            close(peer);
            return -1;
        }
    }
    // printf("after accept\n");
    //send ro rcv  file
    if(up_or_down){
        status = send_file(peer , file_name); 
        close(sockid);
        close(peer);
        if(status == -1)
            return -1;
        return 0;
    }
    else{
        status = rcv_file(peer , file_name);
        close(sockid);
        close(peer);
        if(status == -1)
            return -1;
        return 0;
    }
}
int p2p_rcv_handler(){
    int nbytes;
    memset(BUFF , 0 , BUFFSIZE);
    nbytes = recvfrom(p2pid , BUFF , BUFFSIZE , 0 ,(struct sockaddr *) &heart_addr , &heart_addr_size);
    if(nbytes == -1){
        write("could not read broadcasted message\n" , 35);
        return -1; 
    }
    // write(2 , BUFF , BUFFSIZE);
    // parse message
    int up_or_down = 2; // 1 for up and 0 for down
    char* file_name;
    char* port_number;
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
        write(2 , "file name is not given\n" , 23);
        return -1;
    }
    file_name = ptr;

    ptr = strtok(NULL , DELIM);
    // file name is not given
    if(ptr == NULL){
        write(2 , "port number is not given\n" , 25);
        return -1;
    }
    port_number = ptr;
    // too many arguments
    if(strtok(NULL , DELIM)!= NULL){
        return -1;
        write(2 , "too many arguments\n" , 19);
    }
    // cast port number char* to int
    int port_number_i = atoi(port_number);
    if(port_number_i == PORT_M){
        return 0;
    }
    // printf("befor make tcp sock\n");
    // printf("port is :%d\n" , port_number_i);
    // make tcp connection with peer requestiong you. the peer is listening
    int peer = make_tcp_sock(port_number_i);
    if(peer == -1){
        write(2 , "tcp connect to peer failed\n" , 27);
        return -1;
    }

    // file_name[strlen(file_name)-1] = '\0'; //to ommit the new line char

    // then fix ruined BUFF
    // int i = 0;
    // while(BUFF[i] != '\0'){
    //     i++;
    // }
    // BUFF[i] = ' ';
    // while(BUFF[i] != '\0'){
    //     i++;
    // }
    // BUFF[i] = ' ';
    // now buffer contains the original message
    if(!up_or_down){
        if (!file_exist(file_name)){
            write( 2, "file not found\n" , 15);
            close(peer);
            return -1;
        }
        write(2,"download req detected\n" , 22);
        // printf("file name is : %s \n" , file_name);
        // printf("port name is : %s\n" , port_number);
        send_file(peer , file_name);
        close(peer);
    }
    else{
        write(2 , "upload req  detected\n" , 21);
        // printf("file name is : %s \n" , file_name);
        // printf("port name is : %s\n" , port_number);
        rcv_file(peer, file_name); 
        close(peer);
    }
    // connect to port
    // call appropriate file tranfer handler
    return 0;
}
int up_handler(char* file_name){
    /*
    return -1 in error and 0 o.w
    */
    int sockid;
    sockid = find_serv();
    // if server was down
    if(sockid == -1){
        write(2 , "server not found\n" , 17);
        write(2 , "peer 2 peer mode ...\n" , 21);
        return p2p_send_handler(file_name , UP);
    }
    sockid = make_tcp_sock(sockid);
    // check could make the connection
    // make the message to send
    char * msg;
    int msg_size =strlen(file_name) + strlen(UPLOAD) + 2;
    msg = (char*)malloc(msg_size);
    strcpy(msg , UPLOAD);
    strcat(msg , DELIM);
    strcat(msg , file_name);

    // send "upload filename.type" to server
    if(send(sockid,msg, msg_size , 0) == -1){
        write(2 , "error sending to socket\n" , 24);
        close(sockid);
        return -1;
    }
    send_file(sockid , file_name);
    close(sockid);
    return 0;
}
int down_handler(char* file_name){
     /*
    return -1 in error and 0 o.w
    */
    int sockid;
    sockid = find_serv();
    // check if server found
     if(sockid == -1){
        write(2 , "server not found\n" , 17);
        write(2 , "peer 2 peer mode ...\n" , 21);
        return p2p_send_handler(file_name , DOWN);
    }
    sockid = make_tcp_sock(sockid);
    // check could make the connection

    // make the message to send
    char * msg;
    int msg_size =strlen(file_name) + strlen(UPLOAD) + 3;
    msg = (char*)malloc(msg_size);
    strcpy(msg , DOWNLOAD);
    strcat(msg , DELIM);
    strcat(msg , file_name);

    // send "upload filename.type" to server
    if(send(sockid,msg, msg_size , 0) == -1){
        write(2 , "error sending to socket\n" , 24);
        close(sockid);
        return -1;
    }

    rcv_file(sockid , file_name);
    close(sockid);
    return 0;
}
int file_exist(char* file_name){
    struct stat st = {0};
    if (stat(file_name, &st) == -1)
        return 0;
    return 1;
}
int command_handler(){
    /*
    reuturns -1 on falilure
    return +1 in success
    */
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
        write(2 , "wrond command\n" , 14);
        return -1;
    }

    ptr = strtok(NULL , DELIM);
    // file name is not given
    if(ptr == NULL){
        write(2 , "file name is not given\n" , 23);
        return -1;
    }
    file_name = ptr;
    // too many arguments
    if(strtok(NULL , DELIM)!= NULL){
        return -1;
        write(2 , "too many arguments\n" , 19);
    }

    file_name[strlen(file_name)-1] = '\0'; //to ommit the new line char

    // then fix ruined BUFF
    int i = 0;
    while(BUFF[i] != '\0'){
        i++;
    }
    BUFF[i] = ' ';
    // now buffer contains the original message
    if(up_or_down){
        if (!file_exist(file_name)){
            write( 2, "file not found\n" , 15);
            return -1;
        }
        up_handler(file_name);
    }
    else
        down_handler(file_name);
    return 1;
}
int run_client(){
 
    fd_set read_fds;  // read file descriptor list for select()
    int fdmax = p2pid; //becuse we are only watching stdin and p2p socket
    int event;
    struct timeval tv;
    write(2 , "client up :))\n" , 14);
    // keep track of the biggest file descriptor
     while(TRUE)   
    {   
        //clear the socket set  
        FD_ZERO(&read_fds);   
        //add linstener socket to set  
        FD_SET(STDIN, &read_fds);
        //sould be p2pid
        FD_SET(p2pid, &read_fds);   

        //pretty terminal
        write(2 , ">> " , 3);
        //wait for an event on one of the sockets , with no timeout
        event = select( fdmax + 1 , &read_fds , NULL , NULL , NULL);
        if ((event < 0) && (errno!=EINTR))   
        {   
            write(2 , "select error\n" , 13);
            exit(0);   
        }   
        if(event == 0){
            
        }
        //if p2p socket is set then there is new request
        if (FD_ISSET(p2pid, &read_fds))   
        {   
            write( 2, "p2p detected \n" , 14);
            p2p_rcv_handler();            
        }
        // if STDIN is set get command
        if (FD_ISSET(STDIN, &read_fds))   
        {   memset(BUFF , 0 , BUFFSIZE);
            int nbytes;
            // printf("stdin detected\n");
            nbytes = read(STDIN , BUFF ,BUFFSIZE);
            command_handler();
        }           
    }  
    return 0; 
}
int main(int argc, char const *argv[]){
    // init ports
    PORT_X = atoi(argv[1]);
    PORT_M = atoi(argv[2]);
    PORT_Z = atoi(argv[3]);
    PATH_NAME = argv[2];

    // detup directory
    setup_dirctory(PATH_NAME);

    // setup addresses
    setup_client_addr();
    setup_heart_addr();
    setup_peer_addr();

    udpid = make_brodcast_sock(heart_addr);
    p2pid = make_brodcast_sock(peer_addr);

    run_client();

    close(udpid);
    close(p2pid);
    return(0);
}
