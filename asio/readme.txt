* asio-readme

date:2016-03-01

======================================================================

���֧�� �Ӵ����������ȡ���ݣ�

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

����֮��������ʹ�ô����ʱ���ô�����������������˿ںţ�����ԭ��Ŀ��������Ͷ�
�ںţ�ֻ�������·��������Ŀ�������url·�����桪����Ȼ������ж˿ںŵĻ���Ҳ��
Ҫ���϶˿ںţ�

----------------------------------------------------------------------

** ���� getFile()

Ӧ�ó����ǣ�������

����html�ı��󣬷�����һ�����ӣ�����������⼸�������
1. ��һ���������ӣ�����Э�顢�������˿ڣ��Լ�����·������
2. ��һ��������ӣ�ֻ�����·����ʽ������·����
3. ��һ��ʡ����Э�顢�������Լ��˿ںŵľ�������·����

�������1������˵����Ϣ����������ʹ�ã�

�������2�����3������Ҫһ��Referer��Ϣ��

https://en.wikipedia.org/wiki/HTTP_referer
http://smerity.com/articles/2013/where_did_all_the_http_referrers_go.html

����֮������ Referer �򣬿���Я��������Ϣ�����磬ĳЩ�ؼ����ֶΣ����û���������
�������뵽uri��ַ���棩

----------------------------------------------------------------------

** �ο�

��HTTP Made Really Easy��

http://www.jmarshall.com/easy/http/

��ʹ��ssl��proxy��asio��������ʵ���Դ��롻
http://lists.boost.org/boost-users/2010/08/62116.php

HTTP Request fields
https://www.w3.org/Protocols/HTTP/HTRQ_Headers.html
https://www.w3.org/Protocols/

URL �� URL������
https://www.zhihu.com/question/21950864
