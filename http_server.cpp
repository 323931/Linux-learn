//这个文件学习 epoll reactor的使用
//epoll里面有mmap吗？
//epoll是线程安全的吗？
//git remote add origin git@github.com:323931/Linux-learn.git
// git branch -M main 
//git push -u origin main
//wrk测试对比服务器性能
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>
#include <poll.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#define BUFFER_SIZE 1024
#define ENABLE_HTTP_RESPONSE 1
#define ROOT  "/srv/samba/share/cpp"

typedef int (*CALL_BACK)(int fd);

struct conn_item{
    int fd;
    char wbuffer[BUFFER_SIZE];
    int wlen;
    char rbuffer[BUFFER_SIZE];
    int rlen;
    CALL_BACK accept_callback;
    union{
    CALL_BACK accept_callback;
    CALL_BACK recv_callback;
    }recv_t;
    CALL_BACK send_callback;
   
};


#if ENABLE_HTTP_RESPONSE
typedef struct conn_item connection_t;

//http://192.168.244.128:2048/index.html
//http://192.168.244.128:2048/abc.html
int http_request(connection_t* conn){
    //GET /index.html HTTP/1.1

    return 0;
}
  

int http_response(connection_t* conn){
#if 1
    conn->wlen = sprintf(conn->wbuffer, 
        "HTTP/1.1 200 OK\r\n"
        "Accept-Ranges: bytes\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: 82\r\n"
        "Date: Wed, 19 Jun 2024 10:00:00 GMT\r\n\r\n"
        "<html><head><title>0voice.king</title></head><body><h1>King</h1></body></html>\r\n\r\n");
#else 
    int filefd = open("abc.html",O_RDONLY);
    if(filefd <0){
        std::cerr<<"open file error"<<std::endl;        
        return -1;
    }
    struct stat stat_buf;
    fstat(filefd,&stat_buf);
    conn->wlen = sprintf(conn->wbuffer, 
        "HTTP/1.1 200 OK\r\n"
        "Accept-Ranges: bytes\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %d\r\n"
        "Date: Wed, 19 Jun 2024 10:00:00 GMT\r\n\r\n"
        ,(int)stat_buf.st_size);
        
    int count = read(filefd,conn->wbuffer + conn->wlen,  BUFFER_SIZE-conn->wlen);
    conn->wlen += count;
    close(filefd);
#endif
    return conn->wlen;
}
#endif


int epfd =0;
conn_item conn_items[1024];
//listenfd 触发 epollin事件时执行accept_cb函数
//connfd 触发 epollin事件时执行recv_cb函数
//connfd 触发 epollout事件时执行send_cb函数
int accept_cb(int fd);
int recv_cb(int fd);
int send_cb(int fd);
int set_event(int fd,int event_type,int flag=0 );

int set_event(int fd,int event_type,int flag ){
    if(flag==0){
        struct epoll_event ev;
        ev.events = event_type ; //|EPOLLET; 
        ev.data.fd = fd;
        epoll_ctl(epfd,EPOLL_CTL_MOD,fd,&ev);
    }else{
        struct epoll_event ev;
        ev.events = event_type ; //|EPOLLET; 
        ev.data.fd = fd;
        epoll_ctl(epfd,EPOLL_CTL_ADD,fd,&ev);
    }
    return 0;
}

int accept_cb(int fd){
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int newfd = accept(fd, (struct sockaddr*)&client_addr, &client_len);
    if (newfd < 0) {    
        std::cerr << "Accept failed" << std::endl;
        return -1;   
    }
    std::cout<<"accept_cb:接收到新连接"<<newfd<<std::endl;
    conn_items[newfd].fd = newfd;
    memset(conn_items[newfd].wbuffer,0,sizeof(conn_items[newfd].wbuffer));
    conn_items[newfd].wlen  = 0;
    memset(conn_items[newfd].rbuffer,0,sizeof(conn_items[newfd].rbuffer));
    conn_items[newfd].rlen = 0;
    conn_items[newfd].recv_t.recv_callback = recv_cb;
    conn_items[newfd].send_callback = send_cb;

    set_event(newfd,EPOLLIN,1); //关注可读事件
    return newfd;
}

int recv_cb(int fd){
    char* buffer =conn_items[fd].rbuffer;
    int index = conn_items[fd].rlen;
    //当recv函数返回0，说明对端调用了close函数，连接已经关闭
    int len = recv(fd, buffer+index, BUFFER_SIZE-index, 0); //
    if(len == 0){
        //注意这里要把事件处理完后断开连接，并且从rdfs中移除
        close(fd);
        epoll_ctl(epfd,EPOLL_CTL_DEL,fd,nullptr); //从epoll中移除
        std::cout<<"recv_cb:连接关闭："<< fd <<std::endl;
        return -1;
    }else{
        conn_items[fd].rlen += len;
        std::cout << "recv_cb:Received from "<< fd <<": " << conn_items[fd].rbuffer << std::endl;
    }
    //把收到的数据拷贝到发送缓冲区
    // memcpy(conn_items[fd].wbuffer,conn_items[fd].rbuffer,conn_items[fd].rlen);
    // conn_items[fd].wlen = conn_items[fd].rlen;
    //清空接收缓冲区
#if 1
    http_response(&conn_items[fd]);
#endif
    set_event(fd,EPOLLOUT,0); //修改为关注可写事件
    return len;
}

int send_cb(int fd){
    char* buffer =conn_items[fd].wbuffer;
    int index = conn_items[fd].wlen;
    send(fd, buffer, index, 0); // Echo back the received data
    std::cout << "send_cb:Sent to "<< fd <<": " << conn_items[fd].wbuffer << std::endl;
    //发送完后，清空缓冲区
    memset(conn_items[fd].wbuffer,0,sizeof(conn_items[fd].wbuffer));
    conn_items[fd].wlen =0;

    set_event(fd,EPOLLIN,0); //修改为关注可读事件
    return 0;
}


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
    conn_items[sockfd].fd = sockfd;
    conn_items[sockfd].recv_t.accept_callback = accept_cb;
    std::cout<<sockfd<<std::endl;
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
#elif 0 //poll
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
#else
    //reactor epoll
    //为什么libevetn等网络库不适合多线程，很大程度上原因是底层实现上和这个类似，他的epfd,以及events列表是全局的，多个线程共享
    //当多个线程调用epoll_wait时，可能会出现惊群效应
    epfd =  epoll_create(1);   
    //参数 size : 历史遗留问题，一开始需要确定 蜂巢盒子的大小 ，但是计算机里面完全可以实现一个链表的效果，无需一开始指定，所以
    //我们只要大于0就行参数
    set_event(sockfd,EPOLLIN,1);
    epoll_event events[1024];
    //能不能把connfd在主循环中不用体现？我们只关注epollin/out?
    //能不能把select,poll,epoll代码做一个组件？
    //现在数组是1024大小，能不能动态扩展？
    //现在读写在一个buffer，怎么拆分出去？
    //reactor的主要思想就是对于一个io，我们只关心它的读buffer，写buffer，以及事件。
    while(1){
        int nready = epoll_wait(epfd,events,1024,-1);
        int i =0;
        for(i=0;i<nready;i++){
            int connfd = events[i].data.fd;
            /*if(connfd == sockfd){
                conn_items[connfd].accept_callback(connfd);
            }else*/ if(events[i].events & EPOLLIN){
                conn_items[connfd].recv_t.recv_callback(connfd);
            }else if(events[i].events & EPOLLOUT){
                conn_items[connfd].send_callback(connfd);
            }
        }
    }
#endif
    close(sockfd);
    return 0;
}