# tinyhttpd
经典的tinyhttpd项目，配套有中文注释   测试CGI时需要本机安装PERL，同时安装perl-cgi

本项目是学习经典的tinyhttpd项目，对于网络编程的初学者来说是很不错的练手项目
该项目作出了改进，能够传送图片，主要改进地方在headers函数、getHeadType函数和cat函数
并且修改了tinyhttpd项目多线程出错的问题

建议源码阅读顺序： main -> startup -> accept_request -> execute_cgi

运行步骤：
cd tinyhttpd
./tinyhttp

![head](https://user-images.githubusercontent.com/107916833/201566822-5aa966d0-dd25-4d1d-9f0f-be1029b3b80e.jpg)
