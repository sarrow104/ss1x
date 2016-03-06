* asio-readme

date:2016-03-01

======================================================================

如何支持 从代理服务器获取数据？

http://stackoverflow.com/questions/11523829/how-to-add-proxy-support-to-boostasio

======================================================================

date:Jul 18 '12 at 8:51

author:RED SOFT ADAIR

----------------------------------------------------------------------

I found the answer myself. It's quite simple:

http://www.jmarshall.com/easy/http/#proxies gives quite a brief and clear description how http proxies work.

All i had to do is add the following code to the asio sync_client sample sample :

std::string myProxyServer = ...;
int         myProxyPort   = ...;

void doDownLoad(const std::string &in_server, const std::string &in_path, std::ostream &outstream)
{
    std::string server      = in_server;
    std::string path        = in_path;
    char serice_port[255];
    strcpy(serice_port, "http");

    if(! myProxyServer.empty())
    {
        path   = "http://" + in_server + in_path;
        server = myProxyServer;
        if(myProxyPort    != 0)
            sprintf(serice_port, "%d", myProxyPort);
    }
    tcp::resolver resolver(io_service);
    tcp::resolver::query query(server, serice_port);

...

----------------------------------------------------------------------

简言之，就是在使用代理的时候，用代理服务器的域名、端口号，代替原有目标的域名和端
口号；只有请求的路径，则用目标的完整url路径代替——当然，如果有端口号的话，也需
要加上端口号；

----------------------------------------------------------------------

** 关于 getFile()

应用场景是，这样；

解析html文本后，发现有一个链接；这个链接有这几种情况：
1. 是一个绝对链接（包含协议、域名、端口，以及命令路径）；
2. 是一个相对链接（只有相对路径方式的命令路径）
3. 是一个省略了协议、域名，以及端口号的绝对命令路径；

对于情况1，不用说，信息完整，可以使用；

对于情况2、情况3，就需要一个Referer信息；

https://en.wikipedia.org/wiki/HTTP_referer
http://smerity.com/articles/2013/where_did_all_the_http_referrers_go.html

简言之，由于 Referer 域，可能携带敏感信息（比如，某些关键的字段，如用户名、密码
，被编码到uri地址里面）

----------------------------------------------------------------------

** 参考

『HTTP Made Really Easy』

http://www.jmarshall.com/easy/http/

『使用ssl的proxy，asio下载器，实验性代码』
http://lists.boost.org/boost-users/2010/08/62116.php

HTTP Request fields
https://www.w3.org/Protocols/HTTP/HTRQ_Headers.html
https://www.w3.org/Protocols/

URL 与 URL的区别
https://www.zhihu.com/question/21950864
