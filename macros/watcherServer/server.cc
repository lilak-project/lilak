/* A simple server in the internet domain using TCP
   The port number is passed as an argument */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 10205

using namespace std;

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
    if (argc!=2) {
        std::cout << "== Usage: " << std::endl;
        std::cout << "./watcherServer.exe  /path/to/test/data" << std::endl;
        return true;
    }
    string infname = argv[1];

    int sockfd, newsockfd, portno;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    int n;

    ifstream in;
    int size_buffer;
    char *buffer;
    in.open(infname.c_str(),std::ios::binary | std::ios::in);
    if(!in) {
        std::cout << "Could not open input file!" << std::endl; 
        return 0;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = PORT;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) error("ERROR on binding");
    listen(sockfd,5);
    clilen = sizeof(cli_addr);

    std::cout << "..." << std::endl;
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    if (newsockfd < 0)
        error("ERROR on accept");

    size_buffer = 512;

    in.seekg(0, ios::end);
    //int file_size = in.tellg();
    int file_size = size_buffer*40000;
    n = write(newsockfd,&file_size,sizeof(int));

    in.seekg(0, ios::beg);
    buffer = (char *) malloc (size_buffer);
    while(!in.eof()) {
        in.read(buffer,size_buffer);
        n = write(newsockfd,buffer,size_buffer);
        std::cout << "sending" << std::endl;
        if (n < 0) error("ERROR writing to socket");
    }

    close(newsockfd);
    close(sockfd);
    in.close();
    return 0; 
}
