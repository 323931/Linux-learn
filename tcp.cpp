#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <unistd.h>
//这个文件主要是演示tcp的基本使用，然后最终实现是采用多线程，一个连接对应一个线程，
//所以这里引入了 io多路复用机制，来避免太多线程引起的系统资源消耗过大
//1.尽量不要让服务器端断开连接，否则会有大量的time_wait状态的连接存在，影响服务器性能
//2.服务器端可以使用io多路复用技术来避免过多线程
//3.recv函数返回0说明对端调用了close()关闭了连接
void *client_thread(void* clientfd){
    int sock = *((int*)clientfd);
    char buffer[128];
    while(1){
        memset(buffer,0,sizeof(buffer));
        //当recv函数返回0，说明对端调用了close函数，连接已经关闭
        int len = recv(sock, buffer, 128, 0); // Receive data into buffer
        if(len <= 0){
            break;
        }
        send(sock, buffer, strlen(buffer), 0); // Echo back the received data
        std::cout << "Received: " << buffer << std::endl;
    }
    close(sock);
    return nullptr;     
}


int main(){
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(2048);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Bind failed" << std::endl;
        return -1;
    }

    listen(sockfd, 5);
    std::cout<<sockfd<<std::endl;
    /*struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int newsockfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);
    if (newsockfd < 0) {
        std::cerr << "Accept failed" << std::endl;
        return -1;
    }

    
    std::cout<<newsockfd<<std::endl;*/
#if 0
    char buffer[128];
    recv(newsockfd, buffer, sizeof(buffer), 0); // Receive data into buffer
    send(newsockfd, buffer, strlen(buffer), 0); // Echo back the received data
    std::cout << "Received: " << buffer << std::endl;

    getchar(); // Wait for user input before closing
    close(newsockfd);
#else 
    /*while(1){
        char buffer[128];
        memset(buffer,0,sizeof(buffer));
        //当recv函数返回0，说明对端调用了close函数，连接已经关闭
        int len = recv(newsockfd, buffer, 128, 0); // Receive data into buffer
        if(len <= 0){
            break;
        }
        send(newsockfd, buffer, strlen(buffer), 0); // Echo back the received data
        std::cout << "Received: " << buffer << std::endl;
    }*/
    while(1){
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int newsockfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);
        if (newsockfd < 0) {
            std::cerr << "Accept failed" << std::endl;
            continue;
        }
        std::cout<<newsockfd<<std::endl;
        pthread_t tid;
        pthread_create(&tid, nullptr, client_thread, (void*)&newsockfd);
        pthread_detach(tid);
    }
#endif
    close(sockfd);
    return 0;
}