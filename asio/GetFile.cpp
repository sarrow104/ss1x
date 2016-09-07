#include "GetFile.hpp"
#include "headers.hpp"

#include <ss1x/asio/utility.hpp>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <tuple>

#include <boost/asio.hpp>

#include <sss/Terminal.hpp>

#define USER_AGENT_DEFAULT                                         \
    "Mozilla/5.0 (Windows; U; Windows NT 5.1; zh-CN; rv:1.9.1.6) " \
    "Gecko/20091201 Firefox/3.5.6\r\n"

namespace {

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

    // std::cout << __func__ << VALUE_MSG(serverName) << std::endl;
    // std::cout << __func__ << VALUE_MSG(getCommand) << std::endl;
    // std::cout << __func__ << VALUE_MSG(port) << std::endl;

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
    unsigned int status_code;
    response_stream >> status_code;
    std::string status_message;
    std::getline(response_stream, status_message);

    std::ostringstream oss;

#ifdef _ECHO_HTTP_RESPONSE_STATUS
#ifndef VALUE_MSG
#define VALUE_MSG(a) #a << " = `" << a << "`"
#endif
    oss << VALUE_MSG(http_version) << "\n"
        << VALUE_MSG(status_code) << "\n"
        << VALUE_MSG(status_message) << "\n"
        << std::endl;

    std::cerr << oss.str();
#ifdef VALUE_MSG
#undef VALUE_MSG
#endif
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

    // std::cout << __func__ << VALUE_MSG(serverName) << std::endl;
    // std::cout << __func__ << VALUE_MSG(getCommand) << std::endl;
    // std::cout << __func__ << VALUE_MSG(port) << std::endl;

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

    request_stream << "GET " << getCommand << " HTTP/1.0\r\n";
    request_stream << "Host: " << serverName << "\r\n";
    request_stream << "Accept: */*\r\n";
    request_stream << "User-Agent: " << USER_AGENT_DEFAULT;
    request_stream << "Connection: close\r\n\r\n";

    // Send the request.
    boost::asio::write(socket, request);

    // Read the response status line.
    boost::asio::streambuf response;
    boost::asio::read_until(socket, response, "\r\n");

    // Check that response is OK.
    std::istream response_stream(&response);
    std::string http_version;
    response_stream >> http_version;
    unsigned int status_code;
    response_stream >> status_code;
    std::string status_message;
    std::getline(response_stream, status_message);

    std::ostringstream oss;

#ifdef _ECHO_HTTP_RESPONSE_STATUS
#ifndef VALUE_MSG
#define VALUE_MSG(a) #a << " = `" << a << "`"
#endif
    oss << VALUE_MSG(http_version) << "\n"
        << VALUE_MSG(status_code) << "\n"
        << VALUE_MSG(status_message) << "\n"
        << std::endl;

    std::cerr << oss.str();
#ifdef VALUE_MSG
#undef VALUE_MSG
#endif
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
    while (boost::asio::read(socket, response,
                             boost::asio::transfer_at_least(1), error)) {
        outFile << &response;
    }
}
}  // namespace

namespace ss1x {
namespace asio {

#define VALUE_MSG(a) #a << " = `" << a << "`"

void getFile(std::ostream& outFile, const std::string& serverName,
             const std::string& getCommand, int port)
{
    ::getFileInner(outFile, 0, serverName, getCommand, port);
}

void getFile(std::ostream& outFile, ss1x::http::Headers& header,
             const std::string& serverName, const std::string& getCommand,
             int port)
{
    ::getFileInner(outFile, &header, serverName, getCommand, port);
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

}  // namespace asio
}  // namespace ss1x
