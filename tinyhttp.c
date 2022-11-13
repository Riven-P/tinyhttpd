/*tinyhttpd的c++版本*/

#include<stdio.h>
#include<sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>

#define SERVER_STRING "Server: rollehttpd/0.1.0\r\n"
#define PRINT(str) printf("[%s-%d]" #str "=%s\n", __func__, __LINE__, str);

int get_line(int, char *, int);
void *accept_request(void *);
void error_die(const char* str);//错误提示
int startup(u_short* port);//端口建立httpd服务
void unimplemented(int);
void not_found(int);
void server_file(int, const char *);
void execute_cgi(int, const char *, const char *, const char *);
void headers(int, const char *);
void cat(int, FILE *);
void bad_request(int);
void cannot_execute(int);
const char *getHeadType(const char *filename);
/*浏览器发起访问时，会向服务器端发送一个GET请求报文，GET报文请求格式如下
请求报文由4四个部分组成：请求行、请求头部行、空行、请求数据

GET / HTTP/1.1\n
Host: 127.0.0.1:8000\n
Connection: keep-alive\n
Cache-Control: max-age=0\n
Upgrade-Insecure-Requests: 1\n
User-Agent: Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/86.0.4240.198 Safari/537.36\n
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,* /*;q=0.8,application/signed-exchange;v=b3;q=0.9\n
Sec-Fetch-Site: none\n
Sec-Fetch-Mode: navigate\n
Sec-Fetch-User: ?1\n
Sec-Fetch-Dest: document\n
Accept-Encoding: gzip, deflate, br\n
Accept-Language: zh-CN,zh;q=0.9\n
\n
GET报文没有body
*/

void* accept_request(void* client1)
{
    int client = *(int *)client1;
    char buf[1024];
    int numchar;
    char method[255];
    char url[255];
    char path[512];
    size_t i, j;
    struct stat st;
    int cgi = 0;
    char *query_string = NULL;
    //将报文第一行存入
    numchar = get_line(client, buf, sizeof(buf));
    //第一行所有参数都在buf中 先看是什么方法
    i = 0, j = 0;
    while(isspace(buf[j])==0&&(i<sizeof(method)-1)){
        method[i++] = buf[j++];
    }
    method[i] = '\0';
    //判断是否是get或post 相等则为0 只有当两个都不是的情况下 才会进入if内部
    if(strcasecmp(method,"GET")&&strcasecmp(method,"POST")){
        unimplemented(client);
        return NULL;
    }

    //POST的时候开启CGI
    if(strcasecmp(method,"POST")==0){
        cgi = 1;
    }

    //读url地址
    i = 0;
    //跳过空格
    while(isspace(buf[j])&&j<sizeof(buf)){
        ++j;
    }
    while(!isspace(buf[j])&&(i<sizeof(url)-1)&&(j<sizeof(buf))){
        url[i++] = buf[j++];
    }
    url[i] = '\0';

    //处理GET方法
    if(strcasecmp(method,"GET")==0){
        //待处理请求为url
        query_string = url;
        while((*query_string!='?')&&(*query_string!='\0')){
            query_string++;

        }
        //GET方法特点:?后面为参数
        if(*query_string=='?'){
            //开启cgi
            cgi = 1;
            //从字符 ？ 处把字符串 url 给分隔会两份
            *query_string = '\0';
            query_string++;//使指针指向?后面的位置 即参数的位置
        }
    }
    sprintf(path, "htdocs%s", url);
    //默认情况下都是访问index.html
    if(path[strlen(path)-1]=='/'){
        //则需要我们给补上index.html
        strcat(path, "index.html");
    }
    //根据路径找到相应文件是否存在
    if(stat(path,&st)==-1){
        //如果不存在，那把这次 http 的请求后续的内容(head 和 body)全部读完并忽略
        while((numchar>0)&&strcmp(buf,"\n")){
            numchar = get_line(client, buf, sizeof(buf));  
        }
        //返回一个找不到文件
        not_found(client);
    }
    else{
        //文件存在，那去跟常量S_IFMT(文件类型的位遮罩)相与，相与之后的值可以用来判断该文件是什么类型的 
        if((st.st_mode&S_IFMT)==S_IFDIR){
            //如果这个文件是个目录，那就需要再在 path 后面拼接一个"/index.html"的字符串
            strcat(path, "index.html");
        }
        //如果这个文件是一个可执行文件，不论是属于4用户/组/其他这三者类型的，就将 cgi 标志变量置一
        if((st.st_mode&S_IXUSR)||
        (st.st_mode&S_IXGRP)||
        (st.st_mode&S_IXOTH)){
            cgi = 1;
        }
        if(!cgi){
            //如果不需要 cgi 机制的话 调研该函数给客户端返回请求内容
            server_file(client, path);
        }
        else{
            execute_cgi(client, path, method, query_string);
        }
    }
    //端开与客户端的连接
    close(client);
    return NULL;
}


/*
POST /color.cgi HTTP/1.1\n
Host: 127.0.0.1:8000\n
Connection: keep-alive\n
Content-Length: 9\n
Cache-Control: max-age=0\n
Upgrade-Insecure-Requests: 1\n
Origin: http://127.0.0.1:8000\n
Content-Type: application/x-www-form-urlencoded\n
User-Agent: Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/86.0.4240.198 Safari/537.36\n
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,* /*;q=0.8,application/signed-exchange;v=b3;q=0.9\n
Sec-Fetch-Site: same-origin\n 
Sec-Fetch-Mode: navigate\n
Sec-Fetch-User: ?1\n
Sec-Fetch-Dest: document\n
Referer: http://127.0.0.1:8000/\n
Accept-Encoding: gzip, deflate, br\n
Accept-Language: zh-CN,zh;q=0.9\n
\n
color=red  //这就是body的内容
*/
/**********************************************************************/
/* Execute a CGI script.  Will need to set environment variables as
 * appropriate.
 * Parameters: client socket descriptor
 *             path to the CGI script */
/**********************************************************************/
void execute_cgi(int client,const char* path,const char* method,const char* query_string){
    //管道通信知识
    char buf[1024];
    int cgi_output[2];
    int cgi_input[2];
    pid_t pid;
    int status;
    int i;
    char c;
    int numchars = 1;
    int content_length = -1;

    //给buf加点东西进入while循环
    buf[0] = 'A';
    buf[1] = '\0';
    //如果是 http 请求是 GET 方法的话读取并忽略请求剩下的内容
    if(strcasecmp(method,"GET")==0){
        while(numchars>0&&strcmp(buf,"\n")){
            numchars = get_line(client, buf, sizeof(buf));
        }
    }
    //POST
    else{
        //只有POST方法可以继续读取内容
        numchars = get_line(client, buf, sizeof(buf));
        //这个循环的目的是读出指示 body 长度大小的参数，并记录 body 的长度大小。其余的 header 里面的参数一律忽略
        //注意这里只读完 header 的内容，body 的内容没有读
        while(numchars>0&&strcmp(buf,"\n")){
            //找出Content-Length
            buf[15] = '\0';
            //如果读到Content-Length行了
            if(strcasecmp(buf,"Content-Length:")==0){
                content_length = atoi(&buf[16]);//记录body的长度
            }
            //没有则继续读
            numchars = get_line(client, buf, sizeof(buf));
        }
        //如果head读完了还没有 则报错
        if(content_length==-1){
            //错误请求
            bad_request(client);
            return;
        }
    }
    //正确 发送状态码200
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);

    //下面这里创建两个管道，用于两个进程间通信
    //成功的pipe会在cgi_output中返回两个打开的文件描述符：一个表示管道的读取端（cgi_output[0]), 另一个表示管道的写入端(cgi_output[1])
    if(pipe(cgi_output)<0){
        cannot_execute(client);
        return;
    }

    if(pipe(cgi_input)<0){
        cannot_execute(client);
        return;
    }

    //建立子进程
    if((pid=fork())<0){
        cannot_execute(client);
        return;
    }

    if(pid==0){
        //子进程
        char meth_env[255];//设置request_method 的环境变量
        char query_env[255];//GET 的话设置 query_string 的环境变量
        char length_env[255];//POST 的话设置 content_length 的环境变量

        //dup2创建一个newfd，newfd指向oldfd的位置
        //dup2()包含<unistd.h>中，参读《TLPI》P97
        //将子进程的输出由标准输出重定向到 cgi_ouput 的管道写端上 也就是输出内容将会输出到output写入
        dup2(cgi_output[1], 1);
        //将子进程的输出由标准输入重定向到 cgi_inout 的管道读端上 也就是将从input[0]读内容到input缓冲
        dup2(cgi_input[0], 0);
        close(cgi_output[0]);
        close(cgi_input[1]);

        //设置环境变量
        sprintf(meth_env, "REQUEST_METHOD=%s", method);
        putenv(meth_env);

        if(strcasecmp(method,"GET")==0){
            //设置query_string的环境变量
            sprintf(query_env, "QUERY_STRING=%s", query_string);
            putenv(query_env);
        }
        else{
            sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
            putenv(length_env);
        }
        //最后将子进程替换成另一个进程并执行 cgi 脚本
        // 函数说明:  execl()用来执行参数path字符串所代表的文件路径，接下来的参数代表执行该文件时传递过去的argv(0)、argv[1]……，最后一个参数必须用空指针(NULL)作结束。
        execl(path, path, NULL);
        exit(0);
    }
    else{
        //父进程
        close(cgi_input[0]);
        close(cgi_output[1]);
        if(strcasecmp(method,"POST")==0){
            for (int i = 0; i < content_length;++i){
                recv(client, &c, 1, 0);
                //把POST的body数据写入cgi_input，让子进程去读 在我们的例子中为color=red
                write(cgi_input[1], &c, 1);
            }
        }
        //从cgi_output管道中读取子进程的输出 并发送给客户端
        while(read(cgi_output[0],&c,1)>0){
            send(client, &c, 1, 0);
        }
        close(cgi_input[1]);
        close(cgi_output[0]);
        //等待子进程结束
        waitpid(pid, &status, 0);
    }
}

// 主要处理发生在执行 cgi 程序时出现的错误。
void cannot_execute(int client){
    char buf[1024];
    //回应给客户端 cgi无法执行
    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution\r\n");
    send(client, buf, strlen(buf), 0);
}

//返回给客户端这是个错误请求，HTTP 状态吗 400 BAD REQUEST.
void bad_request(int client){
    char buf[1024];
    //回应给客户端 cgi无法执行
    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request,");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(client, buf, sizeof(buf), 0);
}

//调用 cat 把服务器文件返回给浏览器
void server_file(int client,const char* filename){
    PRINT("server_file begin\n");
    int numchars = 1;
    char buf[1024];
    FILE *resource = NULL;
    //读取并丢弃header
    buf[0] = 'A';
    buf[1] = '\n';
    while ((numchars > 0) && strcmp("\n", buf)){
        numchars = get_line(client, buf, sizeof(buf));
        PRINT(buf);
    } /* read & discard headers */
    if(strcmp(filename,"htdocs/index.html")==0){
        //打开服务器的文件 
        resource = fopen(filename, "r");
    }
    else{
        //对于图片文件需要用rb的方式读取
        resource = fopen(filename, "rb");
        printf("imgae rb\n");
    }

    if(resource==NULL){
        //文件读取失败
        not_found(client);
    }
    else{
        //打开成功后，将这个文件的基本信息封装成 response 的头部(header)并返回
        
        headers(client, getHeadType(filename));
        //接着把这个文件的内容读出来作为 response 的 body 发送到客户端
        cat(client, resource);
    }
    //关闭文件
    fclose(resource);
}

//把http的响应头部写到套接字
void headers(int client,const char* type){
    
    char buf[1024];
    (void)type;

    

    //HTTP的头部
    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    //服务器信息
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: %s\r\n",type);
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}

//读取传入的文件是什么类型 方便整合头部信息发送出去
const char* getHeadType(const char* filename){
    const char *ret = "text/html";//基础的文本类型
    const char *p = strrchr(filename, '.');
    if(!p){
        return ret;
    }
    p++;
    if(strcmp(p,"css")==0){
        ret = "text/css";
    }
    else if(strcmp(p,"jpg")==0){
        ret = "image/jpeg";
    }
    else if(strcmp(p,"png")==0){
        ret = "image/png";
    }
    else if(strcmp(p,"js")==0){
        ret = "application/x-javascript";
    }
    printf(ret);
    printf("\n");
    return ret;
}

//将要读取的服务器上的该文件写到套接字中发送给客户
void cat(int client,FILE* resource){

    char buff[4096];
	int count = 0;
	while (1) {
		int ret = fread(buff, sizeof(char), sizeof(buff), resource);
		if (ret <= 0) {
			break;
		}
		send(client, buff, ret, 0);
		count += ret;
	}
	printf("一共发送了[%d]字节给浏览器\n", count);

    // char buf[1024];

    // //从文件描述符中读取指定内容
    // //从指定的流resource读取一行，并把它存储在buf所指向的字符串内
    // fgets(buf, sizeof(buf), resource);
    // //feof()是检测流上的文件结束符的函数，如果文件结束，则返回非0值，否则返回0
    // while(!feof(resource)){
    //     //先把开始读的一行发出去
    //     send(client, buf, strlen(buf), 0);
    //     //再读一行
    //     fgets(buf, sizeof(buf), resource);
    // }
}

//主要处理找不到请求的文件时的情况。
void not_found(int client){
    char buf[1024];
    //回应给客户端 cgi无法执行
    sprintf(buf, "HTTP/1.0 404 Not Found\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<img src=\"404.png\"/>");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

//返回给浏览器表明收到的 HTTP 请求所用的 method 不被支持
void unimplemented(int client){
    //告诉客户端提出的方法服务器不支持
    char buf[1024];

    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    //服务器信息（头部）
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);

    //正文信息
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

//读取套接字的一行，把回车换行等情况都统一为换行符结束。
int get_line(int sock,char *buf,int size){
    int i = 0;
    char c = 0;
    int n = 0;
    while((i<size-1)&&(c!='\n')){
        //一次读一个字节
        n = recv(sock, &c, 1, 0);
        if(n>0){
            if(c=='\r'){//因为报文结束是\r\n
                n = recv(sock, &c, 1, MSG_PEEK); //喵一眼
                if((c=='\n')&&(n>0)){
                    recv(sock, &c, 1, 0);
                }
                else{
                    //如果最后结尾不是\n 强制设为\n
                    c = '\n';
                }
            }
            //读到了 存进buf
            buf[i] = c;
            ++i;
        }
        else{
            //读取失败 直接跳出while
            c = '\n';
        }
    }
    buf[i] = '\0';//结束符
    return i;
}

//把错误信息写到 perror 并退出。
void error_die(const char* str){
    perror(str);
    exit(1);
}

//初始化 httpd 服务，包括建立套接字，绑定端口，进行监听等。
int startup(u_short* port){
    int httpd=0;//要返回的套接字
    struct sockaddr_in name;
    httpd=socket(AF_INET,SOCK_STREAM,0);
    if(httpd==-1){
        //连接失败 报错处理
        error_die("startup socket");
    }
    //初始化name
    memset(&name,0,sizeof(name));
    name.sin_family=AF_INET;
    name.sin_port=htons(*port);
    name.sin_addr.s_addr=htonl(INADDR_ANY);

    //绑定套接字与该服务器
    if(bind(httpd,(struct sockaddr*)&name,sizeof(name))==-1){
        error_die("bind");
    }

    //随机分配端口号
    if(*port==0){
        int namelen=sizeof(name);
        if(getsockname(httpd,(struct sockaddr*)&name,&namelen)==-1){
            error_die("getsockname");
        }
        *port=ntohs(name.sin_port);
    }

    //监听
    if(listen(httpd,5)==-1){
        error_die("listen");
    }

    return(httpd);
}


int main(void){
    int server_sock = -1; //服务器套接字
    u_short port = 8000;  //端口号
    int client_sock = -1; //客户端套接字
    struct sockaddr_in client_name;
    socklen_t client_name_len = sizeof(client_name);
    pthread_t newthread;

    //对应端口建立http服务
    server_sock = startup(&port);
    printf("httpd running on port%d\n", port);

    while(1){
        //服务器端套接字等待客户端进行连接 生成一个专门用来服务该客户的套接字
        client_sock = accept(server_sock,
                       (struct sockaddr *)&client_name,
                       &client_name_len);

        if (client_sock == -1)
            error_die("accept");

        // accept_request(client_sock);
        if (pthread_create(&newthread , NULL, accept_request, (void*)&client_sock) != 0)
            perror("pthread_create");
    }
    close(server_sock);
}