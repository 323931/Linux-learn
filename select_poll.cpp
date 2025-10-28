//这个文件学习 io多路复用：select/poll/epoll 的使用
//以及reactor的学习
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>
#include <poll.h>
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
#elif 0
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
#elif 0 //select
      //select(maxfd,rset,wset,eset,timeout);  

      fd_set rdfs,rset;
      FD_ZERO(&rdfs);
      FD_SET(sockfd,&rdfs);
      int maxfd = sockfd;
      while(1){
        //这里涉及到应用层和内核，select调用后，会把rest进行拷贝到内核
        rset = rdfs;
        int nready = select(maxfd+1,&rset,nullptr,nullptr,nullptr);

        if(FD_ISSET(sockfd,&rset)){ //有新的连接到来
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int newsockfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);
            if (newsockfd < 0) {
                std::cerr << "Accept failed" << std::endl;
                continue;
            }
            std::cout<<"接受到了新连接："<< newsockfd <<std::endl;
            FD_SET(newsockfd,&rdfs);
            if(newsockfd > maxfd){
                maxfd = newsockfd;
            }
        }

        //检查已经连接的socket是否有数据到来
        for(int i =sockfd+1;i<=maxfd;++i){
            if(FD_ISSET(i,&rset)){
                char buffer[128];
                memset(buffer,0,sizeof(buffer));
                //当recv函数返回0，说明对端调用了close函数，连接已经关闭
                int len = recv(i, buffer, 128, 0); // Receive data into buffer
                if(len <= 0){
                    //注意这里要把事件处理完后断开连接，并且从rdfs中移除
                    close(i);
                    FD_CLR(i,&rdfs);
                    std::cout<<"连接关闭："<< i <<std::endl;
                }else{
                    send(i, buffer, strlen(buffer), 0); // Echo back the received data
                    std::cout << "Received from "<< i <<": " << buffer << std::endl;
                }
            }
        }
      }
#else //poll

      struct pollfd fds[1024] = {0};
      fds[sockfd].fd = sockfd;
      fds[sockfd].events = POLLIN;
      int maxfd = sockfd;
     while(1){
        int nready = poll(fds, maxfd + 1, -1);
        if(fds[sockfd].revents & POLLIN){ //有新的连接到来
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int newsockfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);
            if (newsockfd < 0) {
                std::cerr << "Accept failed" << std::endl;
                continue;   
            }
            std::cout<<"接受到了新连接："<< newsockfd <<std::endl;
            fds[newsockfd].fd = newsockfd;
            //这里event要设置为POLLIN，表示关注可读事件
            fds[newsockfd].events = POLLIN;
            if(newsockfd > maxfd){
                maxfd = newsockfd;
            }
        }
        int i=0;
        for(int i = sockfd+1;i<=maxfd;++i){
            if(fds[i].revents & POLLIN){
                char buffer[128];
                memset(buffer,0,sizeof(buffer));
                //当recv函数返回0，说明对端调用了close函数，连接已经关闭
                int len = recv(i, buffer, 128, 0); // Receive data into buffer
                if(len <= 0){
                    //注意这里要把事件处理完后断开连接，并且从rdfs中移除
                    close(i);
                    fds[i].fd = -1; //表示这个位置可以被重用
                    std::cout<<"连接关闭："<< i <<std::endl;
                }else{
                    send(i, buffer, strlen(buffer), 0); // Echo back the received data
                    std::cout << "Received from "<< i <<": " << buffer << std::endl;
                }
            }
        }
    }
        
#endif
    close(sockfd);
    return 0;
}