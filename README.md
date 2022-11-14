# tinyhttpd

经典的tinyhttpd项目，配套有中文注释   测试CGI时需要本机安装PERL，同时安装perl-cgi

本项目是学习经典的tinyhttpd项目，对于网络编程的初学者来说是很不错的练手项目
该项目作出了改进，能够传送图片，主要改进地方在headers函数、getHeadType函数和cat函数
并且修改了tinyhttpd项目多线程出错的问题

建议源码阅读顺序： main -> startup -> accept_request -> execute_cgi

## 运行步骤：

git clone https://github.com/RolleXXX/tinyhttpd.git
cd tinyhttpd
make
./tinyhttp



## 运行效果展示：

<img src="https://user-images.githubusercontent.com/107916833/201567258-43fa899f-e112-48e0-9e99-f414d2464a16.png" alt="微信图片_20221114111123" style="zoom:80%;" />
<img src="https://user-images.githubusercontent.com/107916833/201567260-fe804f8b-7512-4e8d-aabd-e11b41c34cb0.png" alt="微信图片_20221114111126" style="zoom:80%;" />



## 注意事项：

端口号设置为8000，可在main函数中修改，若将port设为0，则自动分配端口

HTTP请求页面默认为htdocs文件夹下的index.html文件和.cgi文件



## 基本流程图
![20200402164118320](https://user-images.githubusercontent.com/107916833/201569194-853808fd-f38e-434c-8712-a9d470dbcb83.png)


[服务器项目--Tinyhttpd_with_threadpool_epoll](https://blog.csdn.net/qq_39751437/article/details/105265301)
