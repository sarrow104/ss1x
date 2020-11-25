#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include "GetFile.hpp"
#include "headers.hpp"
#include "http_client.hpp"
#include "proxy_tunnel_client.hpp"
#include "user_agent.hpp"

#include <ss1x/asio/utility.hpp>

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>

#include <sss/Terminal.hpp>
#include <sss/debug/value_msg.hpp>
#include <sss/util/PostionThrow.hpp>
#include <sss/utlstring.hpp>

namespace ss1x {
namespace asio {

bool & ptc_colog_status()
{
    return ::detail::ss1x_asio_ptc_colog_status();
}

int & ptc_deadline_timer_wait()
{
    return ::detail::ss1x_asio_ptc_deadline_wait_secends();
}

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
    request_stream << "User-Agent: " << USER_AGENT_DEFAULT << "\r\n";
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

    std::unique_ptr<boost::asio::ssl::context> p_ctx;
    ;
    bool use_ssl = (port == 443);
    if (use_ssl) {
        p_ctx.reset(
            new boost::asio::ssl::context(boost::asio::ssl::context::sslv23));
        p_ctx->set_default_verify_paths();
    }

    // std::cout << __func__ << SSS_VALUE_MSG(serverName) << std::endl;
    // std::cout << __func__ << SSS_VALUE_MSG(getCommand) << std::endl;
    // std::cout << __func__ << SSS_VALUE_MSG(port) << std::endl;

    using namespace std;
    using namespace boost::asio;
    using boost::asio::ip::tcp;
    boost::asio::io_service io_service;
    boost::asio::io_service::work work(io_service);

    // Get a list of endpoints corresponding to the server name.
    tcp::resolver resolver(io_service);
    tcp::resolver::query query(serverName, service_port);
    tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
    tcp::resolver::iterator end;

    // Try each endpoint until we successfully establish a connection.
    // tcp::socket socket(io_service);
    ss1x::detail::socket_t socket(io_service);
    if (use_ssl) {
        socket.upgrade_to_ssl(*p_ctx);
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
    request_stream << "User-Agent: " << USER_AGENT_DEFAULT << "\r\n";
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

boost::system::error_code redirectHttpPostCookie(
    std::ostream&              out,
    ss1x::http::Headers&       header,
    const std::string&         url,
    const std::string&         post_content,
    CookieFunc_t&&             cookieFun,
    const ss1x::http::Headers& request_header)
{
    boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
    ctx.set_default_verify_paths();
    auto url_info = ss1x::util::url::split_port_auto(url);

    boost::asio::io_service io_service;
    boost::asio::io_service::work work(io_service);

    proxy_tunnel_client c(io_service, nullptr);
    // c.max_redirect(0);
    c.upgrade_to_ssl(ctx);
    c.request_header() = request_header;
    c.setOnFinished(
        [&io_service]()->void{
            io_service.stop();
        });
    c.setOnContent(
        [&out](sss::string_view response) -> void { out << response; });
    c.setCookieFunc(std::move(cookieFun));
    c.http_post(url, post_content);
    // 现有的实现，有少许问题，不能立即检测到此种连接方式(prox-https)的eof。
    // 于是，额外提供了一种检查机制，以便快速返回。
    // c.setOnEndCheck([](sss::string_view sv)-> bool { return
    // sv.find("</html>") != sss::string_view::npos; });
    io_service.run();
    COLOG_DEBUG(SSS_VALUE_MSG(c.header().status_code));
    header = c.header();

    return c.error_code();
}

boost::system::error_code redirectHttpPost(
    std::ostream& out, ss1x::http::Headers& header, const std::string& url,
    const std::string& post_content,
    const ss1x::http::Headers& request_header)
{
    CookieFunc_t cookieFun;
    return
        redirectHttpPostCookie(
            out,
            header,
            url,
            post_content,
            std::move(cookieFun),
            request_header);
}

boost::system::error_code redirectHttpGet(std::ostream& out,
                                          ss1x::http::Headers& header,
                                          const std::string& url,
                                          const ss1x::http::Headers& request_header)
{
    CookieFunc_t cookieFun;
    return redirectHttpGetCookie(out, header, url, std::move(cookieFun), request_header);
}

boost::system::error_code redirectHttpGetCookie(std::ostream& out,
                                                ss1x::http::Headers& header,
                                                const std::string& url,
                                                CookieFunc_t&& cookieFun,
                                                const ss1x::http::Headers& request_header)
{
    boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
    ctx.set_default_verify_paths();
    auto url_info = ss1x::util::url::split_port_auto(url);

    boost::asio::io_service io_service;
    boost::asio::io_service::work work(io_service);

    proxy_tunnel_client c(io_service, nullptr);
    // c.max_redirect(0);
    c.upgrade_to_ssl(ctx);
    c.request_header() = request_header;
    c.setOnFinished(
        [&io_service]()->void{
            io_service.stop();
        });
    c.setOnContent(
        [&out](sss::string_view response) -> void {
            out << response;
        });
    c.setCookieFunc(std::move(cookieFun));
    c.http_get(url);
    // 现有的实现，有少许问题，不能立即检测到此种连接方式(prox-https)的eof。
    // 于是，额外提供了一种检查机制，以便快速返回。
    // c.setOnEndCheck([](sss::string_view sv)-> bool { return
    // sv.find("</html>") != sss::string_view::npos; });
    io_service.run();
    COLOG_DEBUG(SSS_VALUE_MSG(c.header().status_code));
    header = c.header();

    return c.error_code();
}

boost::system::error_code proxyRedirectHttpGet(
    std::ostream& out, ss1x::http::Headers& header,
    const std::string& proxy_domain, int proxy_port, const std::string& url,
    const ss1x::http::Headers& request_header)
{
    CookieFunc_t cookieFun;
    return proxyRedirectHttpGetCookie(out, header, proxy_domain, proxy_port,
                                      url, std::move(cookieFun), request_header);
}

boost::system::error_code proxyRedirectHttpGetCookie(
    std::ostream& out, ss1x::http::Headers& header,
    const std::string& proxy_domain, int proxy_port, const std::string& url,
    CookieFunc_t&& cookieFun, const ss1x::http::Headers& request_header)
{
    // TODO 可以用跟踪法，看看avhttp，是如何使用proxy的。
    boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
    ctx.set_default_verify_paths();
    auto url_info = ss1x::util::url::split_port_auto(url);

    boost::asio::io_service io_service;
    boost::asio::io_service::work work(io_service);

    proxy_tunnel_client c(io_service, nullptr);
    c.upgrade_to_ssl(ctx);
    c.request_header() = request_header;
    c.setOnFinished(
        [&io_service]()->void{
            io_service.stop();
        });
    c.setOnContent(
        [&out](sss::string_view response) -> void {
            out << response;
        });
    c.setCookieFunc(std::move(cookieFun));
    c.setSetCookieFunc(ss1x::cookie::set);
    // 现有的实现，有少许问题，不能立即检测到此种连接方式(prox-https)的eof。
    // 于是，额外提供了一种检查机制，以便快速返回。
#if 1
    c.setOnEndCheck([](sss::string_view sv) -> bool {
        return sv.find("</html>") != sss::string_view::npos;
    });
#endif
    c.ssl_tunnel_get(proxy_domain, proxy_port, url);
    io_service.run();
    COLOG_DEBUG(SSS_VALUE_MSG(c.header().status_code));
    header = c.header();

    return c.error_code();
}

boost::system::error_code proxyRedirectHttpPost(
    std::ostream& out, ss1x::http::Headers& header,
    const std::string& proxy_domain, int proxy_port, const std::string& url,
    const std::string& post_content,
    const ss1x::http::Headers& request_header)
{
    CookieFunc_t cookieFun;
    return
        proxyRedirectHttpPostCookie(
            out,
            header,
            proxy_domain,
            proxy_port,
            url,
            post_content,
            std::move(cookieFun),
            request_header);
}

boost::system::error_code proxyRedirectHttpPostCookie(
    std::ostream& out, ss1x::http::Headers& header,
    const std::string& proxy_domain, int proxy_port, const std::string& url,
    const std::string& post_content,
    CookieFunc_t&& cookieFun, const ss1x::http::Headers& request_header)
{
    // TODO 可以用跟踪法，看看avhttp，是如何使用proxy的。
    boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
    ctx.set_default_verify_paths();
    auto url_info = ss1x::util::url::split_port_auto(url);

    boost::asio::io_service io_service;
    boost::asio::io_service::work work(io_service);

    proxy_tunnel_client c(io_service, nullptr);
    c.upgrade_to_ssl(ctx);
    c.request_header() = request_header;
    c.setOnFinished(
        [&io_service]()->void{
            io_service.stop();
        });
    c.setOnContent(
        [&out](sss::string_view response) -> void {
            out << response;
        });
    c.setCookieFunc(std::move(cookieFun));
    c.setSetCookieFunc(ss1x::cookie::set);
    // 现有的实现，有少许问题，不能立即检测到此种连接方式(prox-https)的eof。
    // 于是，额外提供了一种检查机制，以便快速返回。
#if 1
    c.setOnEndCheck([](sss::string_view sv) -> bool {
        return sv.find("</html>") != sss::string_view::npos;
    });
#endif
    c.ssl_tunnel_post(proxy_domain, proxy_port, url, post_content);
    io_service.run();
    COLOG_DEBUG(SSS_VALUE_MSG(c.header().status_code));
    header = c.header();

    return c.error_code();

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
