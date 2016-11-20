#include "GetFile.hpp"
#include "headers.hpp"
#include "http_client.hpp"
#include "user_agent.hpp"

#include <ss1x/asio/utility.hpp>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <tuple>
#include <memory>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <sss/Terminal.hpp>
#include <sss/util/PostionThrow.hpp>
#include <sss/debug/value_msg.hpp>
#include <sss/utlstring.hpp>

namespace ss1x {
namespace asio {

namespace detail {

/**
 * @brief 发送文件、信息
 *
 * @param headers
 * @param serverName
 * @param getCommand
 * @param port
 *
 * https://imququ.com/post/four-ways-to-post-data-in-http.html
 * 貌似只能一个一个文件地来，不是整体打包！
 * 相当于包装了一下post数据而已；
 *
 * 那么，相关的数据主要有：
 *
 * 1. 服务器域名+端口
 * 2. post命令
 * 3. 文件路径
 * Content-Type: multipart/form-data;
 * boundary=---------------------------2473929242097947597857883638
 * 另外，还可以post xml、json；以及url命令参数；
 * postXMLInner
 * postJSONInner
 * postParamsInner
 */
void postFileInner(ss1x::http::Headers* headers, const std::string& serverName,
                   const std::string& getCommand, int port)
{
    char service_port[24] = "";
    if (port <= 0) {
        port = 80;
    }
    if (port > 0) {
        sprintf(service_port, "%d", port);
    }

    // std::cout << __func__ << SSS_VALUE_MSG(serverName) << std::endl;
    // std::cout << __func__ << SSS_VALUE_MSG(getCommand) << std::endl;
    // std::cout << __func__ << SSS_VALUE_MSG(port) << std::endl;

    using namespace std;
    using namespace boost::asio;
    using boost::asio::ip::tcp;
    boost::asio::io_service io_service;

    // Get a list of endpoints corresponding to the server name.
    tcp::resolver resolver(io_service);
    tcp::resolver::query query(serverName, service_port);
    tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
    tcp::resolver::iterator end;

    // Try each endpoint until we successfully establish a connection.
    tcp::socket socket(io_service);
    boost::system::error_code error = boost::asio::error::host_not_found;
    while (error && endpoint_iterator != end) {
        socket.close();
        socket.connect(*endpoint_iterator++, error);
    }

    boost::asio::streambuf request;
    std::ostream request_stream(&request);

    request_stream << "POST " << getCommand << " HTTP/1.0\r\n";
    request_stream << "Host: " << serverName
                   << "\r\n";  // NOTE 实际使用中，端口号需要加在Host key后面
    request_stream << "Accept: */*\r\n";
    request_stream << "User-Agent: " << USER_AGENT_DEFAULT;
    request_stream << "Connection: keep-alive\r\n\r\n";

    // TODO FIXME post 需要加上 Referer吗？

    // Send the request.
    boost::asio::write(socket, request);

    // Read the response status line.
    boost::asio::streambuf response;
    boost::asio::read_until(socket, response, "\r\n");

    // Check that response is OK.
    std::istream response_stream(&response);
    std::string http_version;
    response_stream >> http_version;
    unsigned int status_code = 0u;
    response_stream >> status_code;
    std::string status_message;
    std::getline(response_stream, status_message);

    std::ostringstream oss;

#ifdef _ECHO_HTTP_RESPONSE_STATUS
    oss << SSS_VALUE_MSG(http_version) << "\n"
        << SSS_VALUE_MSG(status_code) << "\n"
        << SSS_VALUE_MSG(status_message) << "\n"
        << std::endl;

    std::cerr << oss.str();
#endif

    oss.str("");

    // Read the response headers, which are terminated by a blank line.
    boost::asio::read_until(socket, response, "\r\n\r\n");

    // Process the response headers.
    std::string header;
#ifdef _ECHO_HTTP_HEADERS
    oss << "header begin:" << std::endl;
    oss << sss::Terminal::debug.data();
#endif
    if (headers) {
        headers->status_code = status_code;
        headers->http_version = http_version;
    }
    while (std::getline(response_stream, header) && header != "\r") {
#ifdef _ECHO_HTTP_HEADERS
        oss << header << std::endl;
#endif
        if (!headers) {
            continue;
        }
        size_t colon_pos = header.find(':');
        if (colon_pos == std::string::npos) {
            continue;
        }
        size_t value_beg = header.find_first_not_of("\t ", colon_pos + 1);
        if (value_beg == std::string::npos) {
            value_beg = header.length();
        }
        (*headers)[header.substr(0, colon_pos)] = header.substr(value_beg);
    }
#ifdef _ECHO_HTTP_HEADERS
    oss << sss::Terminal::end.data();
    oss << "header end:" << std::endl;
    std::cout << oss.str() << std::endl;
#endif

    //// Write whatever content we already have to output.
    // if (response.size() > 0)
    //{
    //    outFile << &response;
    //}
    //// Read until EOF, writing data to output as we go.
    // while (boost::asio::read(socket, response,
    // boost::asio::transfer_at_least(1), error))
    //{
    //    outFile << &response;
    //}
}

void getFileInner(std::ostream& outFile, ss1x::http::Headers* headers,
                  const std::string& serverName, const std::string& getCommand,
                  int port)
{
    char service_port[24] = "";
    if (port <= 0) {
        port = 80;
    }
    if (port > 0) {
        sprintf(service_port, "%d", port);
    }

    std::unique_ptr<boost::asio::ssl::context> p_ctx;;
    bool use_ssl = (port == 443);
    if (use_ssl) {
        p_ctx.reset(new boost::asio::ssl::context(boost::asio::ssl::context::sslv23));
        p_ctx->set_default_verify_paths();
    }

    // std::cout << __func__ << SSS_VALUE_MSG(serverName) << std::endl;
    // std::cout << __func__ << SSS_VALUE_MSG(getCommand) << std::endl;
    // std::cout << __func__ << SSS_VALUE_MSG(port) << std::endl;

    using namespace std;
    using namespace boost::asio;
    using boost::asio::ip::tcp;
    boost::asio::io_service io_service;

    // Get a list of endpoints corresponding to the server name.
    tcp::resolver resolver(io_service);
    tcp::resolver::query query(serverName, service_port);
    tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
    tcp::resolver::iterator end;

    // Try each endpoint until we successfully establish a connection.
    // tcp::socket socket(io_service);
    ss1x::detail::socket_t socket;
    if (use_ssl) {
        socket.init(io_service, *p_ctx);
    }
    else {
        socket.init(io_service);
    }

    boost::system::error_code error = boost::asio::error::host_not_found;
    while (error && endpoint_iterator != end) {
        if (use_ssl) {
            // socket.get_ssl_socket().connect(*endpoint_iterator++, error);
        }
        else {
            socket.get_socket().close();
            socket.get_socket().connect(*endpoint_iterator++, error);
        }
    }

    boost::asio::streambuf request;
    std::ostream request_stream(&request);

    request_stream << "GET " << getCommand << " HTTP/1.0\r\n";
    request_stream << "Host: " << serverName << "\r\n";
    request_stream << "Accept: */*\r\n";
    request_stream << "User-Agent: " << USER_AGENT_DEFAULT;
    request_stream << "Connection: close\r\n\r\n";

    // Send the request.
    boost::asio::write(socket.get_socket(), request);

    // Read the response status line.
    boost::asio::streambuf response;
    boost::asio::read_until(socket.get_socket(), response, "\r\n");

    // Check that response is OK.
    std::istream response_stream(&response);
    std::string http_version;
    response_stream >> http_version;
    unsigned int status_code = 0;
    response_stream >> status_code;
    std::string status_message;
    std::getline(response_stream, status_message);

    std::ostringstream oss;

#ifdef _ECHO_HTTP_RESPONSE_STATUS
    oss << SSS_VALUE_MSG(http_version) << "\n"
        << SSS_VALUE_MSG(status_code) << "\n"
        << SSS_VALUE_MSG(status_message) << "\n"
        << std::endl;

    std::cerr << oss.str();
#endif

    oss.str("");

    // Read the response headers, which are terminated by a blank line.
    boost::asio::read_until(socket.get_socket(), response, "\r\n\r\n");

    // Process the response headers.
    std::string header;
#ifdef _ECHO_HTTP_HEADERS
    oss << "header begin:" << std::endl;
    oss << sss::Terminal::debug.data();
#endif
    if (headers) {
        headers->status_code = status_code;
        headers->http_version = http_version;
    }
    while (std::getline(response_stream, header) && header != "\r") {
#ifdef _ECHO_HTTP_HEADERS
        oss << header << std::endl;
#endif
        if (!headers) {
            continue;
        }
        size_t colon_pos = header.find(':');
        if (colon_pos == std::string::npos) {
            continue;
        }
        size_t value_beg = header.find_first_not_of("\t ", colon_pos + 1);
        if (value_beg == std::string::npos) {
            value_beg = header.length();
        }
        (*headers)[header.substr(0, colon_pos)] = header.substr(value_beg);
    }
#ifdef _ECHO_HTTP_HEADERS
    oss << sss::Terminal::end.data();
    oss << "header end:" << std::endl;
    std::cout << oss.str() << std::endl;
#endif

    // Write whatever content we already have to output.
    if (response.size() > 0) {
        outFile << &response;
    }
    // Read until EOF, writing data to output as we go.
    while (boost::asio::read(socket.get_socket(), response,
                             boost::asio::transfer_at_least(1), error)) {
        outFile << &response;
    }
}
}  // detail namespace


void getFile(std::ostream& outFile, const std::string& serverName,
             const std::string& getCommand, int port)
{
    detail::getFileInner(outFile, 0, serverName, getCommand, port);
}

void getFile(std::ostream& outFile, ss1x::http::Headers& header,
             const std::string& serverName, const std::string& getCommand,
             int port)
{
    detail::getFileInner(outFile, &header, serverName, getCommand, port);
}

void getFile(std::ostream& outFile, const std::string& url)
{
    std::string domain;
    int port = 80;
    std::string command;
    std::tie(std::ignore, domain, port, command) = ss1x::util::url::split(url);
    ss1x::asio::getFile(outFile, domain, command, port);
}

void getFile(std::ostream& outFile, ss1x::http::Headers& header,
             const std::string& url)
{
    std::string domain;
    int port = 80;
    std::string command;
    std::tie(std::ignore, domain, port, command) = ss1x::util::url::split(url);
    ss1x::asio::getFile(outFile, header, domain, command, port);
}

void proxyGetFile(std::ostream& outFile, const std::string& proxy_domain,
                  int proxy_port, const std::string& serverName,
                  const std::string& getCommand, int port)
{
    if (port <= 0) {
        port = 80;
    }

    std::string proxy_command = "http://";
    if (port == 443) {
        proxy_command = "https://";
    }
    proxy_command.append(serverName);
    if (port != 80) {
        char port_buffer[24] = "";
        sprintf(port_buffer, ":%d", port);
        proxy_command.append(port_buffer);
    }
    proxy_command.append(getCommand);

    ss1x::asio::getFile(outFile, proxy_domain, proxy_command, proxy_port);
}

void proxyGetFile(std::ostream& outFile, ss1x::http::Headers& header,
                  const std::string& proxy_domain, int proxy_port,
                  const std::string& serverName, const std::string& getCommand,
                  int port)
{
    if (port <= 0) {
        port = 80;
    }

    std::string proxy_command = "http://";
    if (port == 443) {
        proxy_command = "https://";
    }
    proxy_command.append(serverName);
    if (port != 80) {
        char port_buffer[24] = "";
        sprintf(port_buffer, ":%d", port);
        proxy_command.append(port_buffer);
    }
    proxy_command.append(getCommand);

    ss1x::asio::getFile(outFile, header, proxy_domain, proxy_command,
                        proxy_port);
}

void proxyGetFile(std::ostream& outFile, const std::string& proxy_domain,
                  int proxy_port, const std::string& url)
{
    ss1x::asio::getFile(outFile, proxy_domain, url, proxy_port);
}

void proxyGetFile(std::ostream& outFile, ss1x::http::Headers& header,
                  const std::string& proxy_domain, int proxy_port,
                  const std::string& url)
{
    ss1x::asio::getFile(outFile, header, proxy_domain, url, proxy_port);
}

void redirectHttpGet(std::ostream& out, ss1x::http::Headers& header,
                     const std::string& url)
{
    boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
    ctx.set_default_verify_paths();

    int max_attempt = 5;
    bool is_ok = false;
    boost::asio::io_service io_service;
    do {
        auto url_info = ss1x::util::url::split_port_auto(url);
        COLOG_DEBUG(ss1x::util::url::join(url_info));
        http_client c(io_service, std::get<2>(url_info) == 443 ? &ctx : nullptr);
        c.setOnResponce([&](boost::asio::streambuf& response)->void { out << &response; });
        c.http_get(std::get<1>(url_info), std::get<2>(url_info), std::get<3>(url_info));
        io_service.run();

        // 301,302
        //
        // src/varlisp/builtin_http.cpp:71: ( http-get : http-status code: 302 )
        // header : Connection: Close
        // header : Content-Length: 215
        // header : Content-Type: text/html
        // header : Date: Sat, 19 Nov 2016 02:20:36 GMT
        // header : Location: https://www.baidu.com/
        //
        // 10:21:14.596558552 src/varlisp/builtin_http.cpp:71: ( http-get : http-status code: 301 )
        // header : Connection: close
        // header : Content-Length: 178
        // header : Content-Type: text/html
        // header : Date: Sat, 19 Nov 2016 02:21:14 GMT
        // header : Location: https://movie.douban.com/subject/3578925/
        //
        // 另外google，建议，不要连续使用5次3xx跳转。
        //
        //! http://blog.sina.com.cn/s/blog_687344480100kd53.html
        // 3xx
        //　　要完成请求，需要进一步操作。通常，这些状态码用来重定向。Google
        //建议您在每次请求中使用重定向不要超过 5
        //次。您可以使用网站管理员工具查看一下 Googlebot
        //在抓取重定向网页时是否遇到问题。诊断下的网络抓取页列出了由于重定向错误导致
        //Googlebot 无法抓取的网址。
        //
        //　　300(多种选择)针对请求，服务器可执行多种操作。服务器可根据请求者
        //(user agent) 选择一项操作，或提供操作列表供请求者选择。
        //
        //　　301(永久移动)请求的网页已永久移动到新位置。服务器返回此响应(对 GET
        //或 HEAD 请求的响应)时，会自动将请求者转到新位置。您应使用此代码告诉
        //Googlebot 某个网页或网站已永久移动到新位置。
        //
        //　　302(临时移动)服务器目前从不同位置的网页响应请求，但请求者应继续使用原有位置来响应以后的请求。此代码与响应
        //GET 和 HEAD 请求的 301
        //代码类似，会自动将请求者转到不同的位置，但您不应使用此代码来告诉
        //Googlebot 某个网页或网站已经移动，因为 Googlebot
        //会继续抓取原有位置并编制索引。
        //
        //　　303(查看其他位置)请求者应当对不同的位置使用单独的 GET
        //请求来检索响应时，服务器返回此代码。对于除 HEAD
        //之外的所有请求，服务器会自动转到其他位置。
        //
        //　　304(未修改)自从上次请求后，请求的网页未修改过。服务器返回此响应时，不会返回网页内容。
        //
        //　　如果网页自请求者上次请求后再也没有更改过，您应将服务器配置为返回此响应(称为
        //If-Modified-Since HTTP 标头)。服务器可以告诉 Googlebot
        //自从上次抓取后网页没有变更，进而节省带宽和开销。


        switch (c.header().status_code) {
            case 200:
                header = c.header();
                is_ok = true;
                break;
            case 301: case 302:
                if (!c.header()["Location"].empty()) {
                    url_info = ss1x::util::url::split_port_auto(c.header()["Location"]);
                }
                else {
                    max_attempt = 0;
                }
                break;
            default:
                max_attempt = 0;
                break;
        }
    } while (!is_ok && max_attempt-- > 0);
}

void proxyRedirectHttpGet(std::ostream& out, ss1x::http::Headers& header,
                          const std::string& proxy_domain, int proxy_port,
                          const std::string& url)
{
    boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
    ctx.set_default_verify_paths();

    int max_attempt = 5;
    bool is_ok = false;
    boost::asio::io_service io_service;
    do {
        auto url_info = ss1x::util::url::split_port_auto(url);
        http_client c(io_service, std::get<2>(url_info) == 443 ? &ctx : nullptr);
        c.setOnResponce([&](boost::asio::streambuf& response)->void { out << &response; });
        // std::get<1>(url_info), std::get<2>(url_info), std::get<3>(url_info)
        std::string proxy_command = std::get<0>(url_info) + "://" + std::get<1>(url_info);
        if (std::get<2>(url_info) != 443 && std::get<2>(url_info) != 80) {
            proxy_command += ':';
            proxy_command.append(sss::cast_string(std::get<2>(url_info)));
        }
        proxy_command += std::get<3>(url_info);
        COLOG_DEBUG(proxy_command);
        c.http_get(proxy_domain, proxy_port, proxy_command);
        io_service.run();

        switch (c.header().status_code) {
            case 200:
                header = c.header();
                is_ok = true;
                break;
            case 301: case 302:
                if (!c.header()["Location"].empty()) {
                    url_info = ss1x::util::url::split_port_auto(c.header()["Location"]);
                }
                else {
                    max_attempt = 0;
                }
                break;
            default:
                max_attempt = 0;
                break;
        }
    } while (!is_ok && max_attempt-- > 0);
}

//! http://boost.2283326.n4.nabble.com/boost-asio-SSL-connection-thru-proxy-server-td2586048.html
// NOTE 这是一个，同步代码的https资源获取函数。
#if 0
void sync_https_get()
{
    using boost::asio::ip::tcp;

    using namespace std;

    using namespace boost;
    try
    {
        tcp::resolver *m_pResolver;
        tcp::socket *m_pSocket;
        boost::asio::streambuf request_;
        boost::asio::streambuf response_;
        boost::asio::io_service *m_pIOservice;
        boost::asio::ssl::context *m_pSSLContext;
        boost::asio::ssl::stream<boost::asio::ip::tcp::socket> *m_pSecureSocket;
        string m_host;//host url

        m_pIOservice = new boost::asio::io_service();

        tcp::resolver resolver(*m_pIOservice);
        m_pSSLContext = new boost::asio::ssl::context(*m_pIOservice, boost::asio::ssl::context::sslv23_client);
        m_pSSLContext->set_verify_mode(boost::asio::ssl::context::verify_none);

        //by default connect directly
        string proxyOrHost(m_host);
        //CInternetSettings netSetting;

        boost::system::error_code error = boost::asio::error::host_not_found;
        tcp::resolver::iterator end;

        tcp::resolver::query query(proxyOrHost , "https");
        tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
        // Try each endpoint until we successfully establish a connection.

        //boost::system::error_code error;
        m_pSecureSocket = new boost::asio::ssl::stream<boost::asio::ip::tcp::socket>(*m_pIOservice, *m_pSSLContext) ;
        //try to connect it directly first even though proxy is set, if fail try with proxy
        while (error && endpoint_iterator != end)
        {
            m_pSecureSocket->lowest_layer().close();
            m_pSecureSocket->lowest_layer().connect(*endpoint_iterator++, error);
            if (!error)
            {

                m_pSecureSocket->handshake(boost::asio::ssl::stream_base::client, error);
                if(!error)
                {
                }
                else{
                    boost::system::error_code code = error;
                    boost::system::system_error e(error);
                    long lastResult = code.value();
                    string errorString = e.what();
                    m_pSecureSocket->lowest_layer().close();
                    std::cout << "Handshake failed: " << error << "\n";
                    return ;
                }
            }
            else
            {
                boost::system::error_code code = error;
                boost::system::system_error e(error);
                long lastResult = code.value();
                string errorString = e.what();
                std::cout << "Connect failed: " << error << "\n";
                m_pSecureSocket->lowest_layer().close();
                return ;
            }
        }

        boost::asio::streambuf request;
        std::ostream request_stream(&request);

        request_stream << "POST " << urlPath << " HTTP/1.0\r\n";
        request_stream << "Host: " << m_host << "\r\n";
        request_stream << "Accept: */*\r\n";

        long contentLength = 0;
        const wchar_t *pPostParam;
        long paramLen;

        if(pPostParam && paramLen)
        {
            request_stream << "Content-Length: ";
            request_stream << boost::lexical_cast<string>(contentLength);
            request_stream << "\r\n";
        }
        else
        {

        }

        request_stream << "Cache-Control: no-cache\r\n";
        request_stream << "Connection: Close\r\n\r\n";

        // Send the request.
        boost::system::error_code error;
        boost::asio::write(*m_pSecureSocket, request );
        //Write post param

        if (error)
            throw boost::system::system_error(error);
    }
    catch(...)    {
    }
    catch (boost::system::system_error &e){
        //Handle Error
    }
    return S_OK;
}
#endif
}  // namespace asio
}  // namespace ss1x
