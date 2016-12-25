#ifndef __GETFILE_HPP_1456798559__
#define __GETFILE_HPP_1456798559__

#include <iosfwd>
#include <functional>

namespace boost {
namespace system {
class error_code;
} // namespace system
} // namespace boost

namespace ss1x {
namespace http {
class Headers;
}  // namespace http
namespace asio {

void getFile(std::ostream& outFile, const std::string& serverName,
             const std::string& getCommand, int port = 80);

void getFile(std::ostream& outFile, ss1x::http::Headers& header,
             const std::string& serverName, const std::string& getCommand,
             int port = 80);

void getFile(std::ostream& outFile, ss1x::http::Headers& header,
             const std::string& url);

void getFile(std::ostream& outFile, const std::string& url);

void proxyGetFile(std::ostream& outFile, const std::string& proxy_domain,
                  int proxy_port, const std::string& serverName,
                  const std::string& getCommand, int port = 80);

void proxyGetFile(std::ostream& outFile, const std::string& proxy_domain,
                  int proxy_port, const std::string& url);

void proxyGetFile(std::ostream& outFile, ss1x::http::Headers& header,
                  const std::string& proxy_domain, int proxy_port,
                  const std::string& serverName, const std::string& getCommand,
                  int port = 80);

void proxyGetFile(std::ostream& outFile, ss1x::http::Headers& header,
                  const std::string& proxy_domain, int proxy_port,
                  const std::string& url);

typedef std::function<std::string(const std::string& domain, const std::string& path)>
     CookieFunc_t;

boost::system::error_code redirectHttpGetCookie(std::ostream& out, ss1x::http::Headers& header,
                     const std::string& url, CookieFunc_t&&);

boost::system::error_code redirectHttpGet(std::ostream& out, ss1x::http::Headers& header,
                     const std::string& url);

// NOTE 关于通过本地http proxy，获取https资源
// 关键词：https asio Tunneling proxy connect
// 相关网页：
//! http方法说明 https://www.w3.org/Protocols/rfc2616/rfc2616-sec9.html
//! http状态码说明 https://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
//! http://stackoverflow.com/questions/16532028/https-tunneling-through-my-proxy
//! https://en.wikipedia.org/wiki/HTTP_tunnel#HTTP_CONNECT_Tunneling
//! https://tools.ietf.org/html/draft-luotonen-ssl-tunneling-03
//! http://www.codeproject.com/Articles/7991/HTTP-Tunneling-HTTP-Proxy-Socket-Client
//
// 简言之，http的tcp通信，是基于文本的；可解析，于是代理服务器会知晓通信内容。
// 而https的tcp通信，是二进制，proxy不知道。
// 于是，https-proxy的使用方式，比较特殊，是使用http的connect命令——这个命令，我在昨
// 天之前(2016-11-20)根本就不知道。只知道常用的POST,GET。
// 这种方式的数据交互简言之：
//
// 1. Connect to Proxy Server first.
// 2. Issue CONNECT Host:Port HTTP/1.1<CR><LF>.
// 3. Issue <CR><LF>.
// 4. Wait for a line of response. If it contains HTTP/1.X 200, the connection
// is successful.
// 5. Read further lines of response until you receive an empty line.
// 6. Now, you are connected to the outside world through a proxy. Do any data
//    exchange you want.
// 这是源自 codeproject的那个页面的说法；相关代码是：
//! /home/sarrow/project/proxy-downloader/HttpProxySocket.h:31
// 	virtual void ConnectTo(LPCTSTR lpszHost , int nPort)
// 	{
// 
// 		// If No Http Proxy Server Specified then connect to host directly..
// 		if(HttpServer.IsEmpty())
// 		{
// 			CSocketClient::ConnectTo(lpszHost,nPort);
// 			return;
// 		}
// 
// 		CSocketClient::ConnectTo(HttpServer,Port);
// 		
// 		CString Line;
// 		
// 		Line.Format("CONNECT %s:%d HTTP/1.0",lpszHost,nPort);
// 		(*this)<<Line;
// 		Line.Format("host: %s:%d",lpszHost,nPort);
// 		(*this)<<Line;
// 		Line.Empty();
// 		(*this)<<Line;
// 
// 		(*this)>>Line;
// 
// 		int i = Line.Find(' ');
// 		if(i!=-1)
// 			Line = Line.Mid(i+1);
// 
// 
// 		if(Line.Left(3)!="200")
// 		{
// 		    ...
// 		}
// 可见，就是用CONNECT与proxy进行简单的链接——获取一个200的正常编码之后，就把这个
// 当做通道，完成后面的https交互流程——代理服务器，不会知道实际的交流内容。
// 上例用的不是MFC代码，而是对winsocket2的保证。另外，重载了>>和<<操作符，以便同时完成读写操作。
//
// 没有理解错误的话，与proxy联系(CONNECT)动作的时候，用的是普通的socket；正常之后，
// 用的是ssl的socket。也就是说，我需要一个"提升"的操作。在原有的socket基础智商，
// 创建一个新的ssl-socket出来！upgrade_to_ssl()

// 相关代码：

//! http://boost.2283326.n4.nabble.com/boost-asio-SSL-connection-thru-proxy-server-td2586048.html
//! https://github.com/Microsoft/cpprestsdk/blob/master/Release/src/http/client/http_client_asio.cpp
boost::system::error_code proxyRedirectHttpGetCookie(std::ostream& out, ss1x::http::Headers& header,
                                               const std::string& proxy_domain, int proxy_port,
                                               const std::string& url, CookieFunc_t&&);

boost::system::error_code proxyRedirectHttpGet(std::ostream& out, ss1x::http::Headers& header,
                                               const std::string& proxy_domain, int proxy_port,
                                               const std::string& url);
}  // namespace asio
}  // namespace ss1x

#endif /* __GETFILE_HPP_1456798559__ */
