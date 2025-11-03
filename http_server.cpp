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
#include <sys/time.h>
#define BUFFER_SIZE 1024
#define ENABLE_HTTP_RESPONSE 1
#define ROOT  "/srv/samba/share/cpp"
#define TIME_DIFF(tv1,tv2)   ((tv1.tv_sec - tv2.tv_sec)*1000 + (tv1.tv_usec - tv2.tv_usec)/1000)
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

timeval tv_start;
int epfd =0;
conn_item conn_items[1048576];
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

    if(newfd % 1000 == 999){
        struct timeval tv_cur;
        gettimeofday(&tv_cur,nullptr);
        int time_used = TIME_DIFF(tv_cur,tv_start);
        memcpy(&tv_start,&tv_cur,sizeof(tv_cur));
        std::cout << time_used << " ms to accept 1000 connections" << std::endl;
    }
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
        std::cout << "recv_cb:Received from "<< fd <<": " << conn_items[fd].rbuffer+index << std::endl;
    }
    //把收到的数据拷贝到发送缓冲区
    // memcpy(conn_items[fd].wbuffer,conn_items[fd].rbuffer,conn_items[fd].rlen);
    // conn_items[fd].wlen = conn_items[fd].rlen;
    //清空接收缓冲区
#if 0
    http_response(&conn_items[fd]);
#elif 1
    //把收到的数据拷贝到发送缓冲区
    memcpy(conn_items[fd].wbuffer,conn_items[fd].rbuffer,conn_items[fd].rlen);
    //!!!!!!这里就是直接把要发送的数据长度设置为接收的数据长度
    //然后逻辑上将接收缓存区情况，因为rlen为0，下次新接数据就是直接覆盖原本数据！！
    conn_items[fd].wlen = conn_items[fd].rlen;    
    conn_items[fd].rlen -= conn_items[fd].rlen;
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

int init_server(unsigned short port);
int init_server(unsigned short port){
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    //开放服务器多端口
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Bind failed" << std::endl;
        return -1;
    }

    listen(sockfd, 5);
    return sockfd;
}


int main(){
    //参数 size : 历史遗留问题，一开始需要确定 蜂巢盒子的大小 ，但是计算机里面完全可以实现一个链表的效果，无需一开始指定，所以
    //我们只要大于0就行参数
    epfd =  epoll_create(1);
    int port_count = 10;
    unsigned short start_port = 2048;
    for(int i =0;i<port_count;++i){
        int sockfd = init_server(start_port + i);
        conn_items[sockfd].fd = sockfd;
        conn_items[sockfd].recv_t.accept_callback = accept_cb;
        set_event(sockfd,EPOLLIN,1);
        std::cout<<sockfd<<std::endl;
    }
    gettimeofday(&tv_start,nullptr);


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
    for(int i =0;i<port_count;++i){
        int sockfd = start_port + i;
        close(sockfd);
    }
    return 0;
}
//目标：让服务器跑到百万连接 
//一个socket 
//: fd ->对应一个具体的tcp网络连接：tcp control block (TCB)
//  tcb : sip ,dip ,sport,dport,protocl
//  所以客户端ip,目标ip，目标端口，协议类型确认情况下，和客户端端口数有关系。
//  端口是 unsigned short类型，占2个字节，范围0-65535,0-1023系统默认，不能随机分配

//设置客户端的端口范围：
///etc/sysctl.conf : net.ipv4.ip_local_port_range = 1024 65535,否则跑不了那么多端口，

//设置客户端的fd最大值：
///etc/sysctl.conf ：fs.file-max = 1048576 ,否则即使设置了ulimit -n 65536，
//内核也不允许一个进程打开那么多文件描述符
//接着执行sudo sysctl -p 使配置生效
//这里第一次执行会报错： sysctl: 无法获取/proc/sys/et/ipv4/ip_local_port_range 的文件状态(stat): 没有那个文件或目录
//解决方法： sudo modprobe ip_conntrack

//同样需要在 ulimit  -a 里面把  open files  设置大一些，这个值就是一个进程能够打开的fd数量，
//所以我们要一个进程建立六万多个连接，肯定要设置大一些，命令 ulimit -n  65536

//设置服务端端口数：
//这里我们开了10个端口，2048-2057
//所以一个客户端最多可以建立 10 * 64512 = 645120 个连接

//ps:之所以一开始3个客户端的情况下，建立1000个连接的时间会有突增，就是因为
//有两个客户端在65999的时候停止了，导致消耗时间变成几乎之前的三倍，这是正常的，
//设置了 fs.file-max = 1048576 后，这个问题就不存在了


//===================问题！！
// 1. 为什么客户端程序退出，服务端也跟着退出了？
// 2. 直接手动设置成 1048576的数组，是不是有点太粗糙？
// 3. 服务端跑到九十万的时候，直接崩溃退出了为什么？
// 答案： 这个问题还是要到 /etc/sysctl.conf 里去修改
// net.ipv4.tcp_mem = 262144 524288 1048576  这里的单位是页，4k，所以就是1gb,2gb,3gb,所以当超过3gb后，
// 内核就直接杀掉进程了,所以要设置大一些，比如524288 1048576 2097152 这样就是2gb,4gb,8gb，一定能跑到百万连接
// net.ipv4.tcp_rmem = 2048 2048 4096
// net.ipv4.tcp_wmem = 2048 2048 4096