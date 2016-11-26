#pragma once

#include "socket_t.hpp"
#include "user_agent.hpp"

#include <cctype>

#include <vector>
#include <string>

#include <iostream>
#include <istream>
#include <ostream>

#include <stdexcept>

#include <functional>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>

#include <sss/colorlog.hpp>
#include <sss/debug/value_msg.hpp>
#include <sss/util/PostionThrow.hpp>
#include <sss/string_view.hpp>

#include <ss1x/asio/headers.hpp>
#include <ss1x/asio/error_codec.hpp>
#include <ss1x/asio/utility.hpp>

struct streambuf_t
{
    typedef boost::asio::streambuf value_type;
    explicit streambuf_t(const value_type& streambuf) : m_streambuf(streambuf)
    {
    }
    void print(std::ostream& o) const
    {
        int buf_size = m_streambuf.size();
        boost::asio::streambuf::const_buffers_type::const_iterator begin(
            m_streambuf.data().begin());
        const char* ptr = boost::asio::buffer_cast<const char*>(*begin);
        sss::string_view buf(ptr, buf_size);
        o << '[' << buf.size() << "; "
          << sss::raw_string(sss::string_view(ptr, buf_size)) << ']';
    }
    const value_type& m_streambuf;
};

inline std::ostream& operator << (std::ostream& o, const streambuf_t& b)
{
    b.print(o);
    return o;
}

streambuf_t streambuf_view(const boost::asio::streambuf& stream)
{
    return streambuf_t{stream};
}

struct pretty_ec_t
{
    explicit pretty_ec_t(const boost::system::error_code& ec)
        : m_ec(ec)
    {}
    const boost::system::error_code& m_ec;
    void print(std::ostream& o) const
    {
        o << '{' << m_ec.category().name() << "," << m_ec.value() << ','
          << sss::raw_string(m_ec.message()) << '}';
    }
};

inline std::ostream& operator << (std::ostream& o, const pretty_ec_t& pec)
{
    pec.print(o);
    return o;
}

pretty_ec_t pretty_ec(const boost::system::error_code& ec)
{
    return pretty_ec_t{ec};
}

namespace detail {

template <typename Iterator>
bool parse_http_status_line(Iterator begin, Iterator end,
	int& version_major, int& version_minor, int& status)
{
	enum
	{
		http_version_h,
		http_version_t_1,
		http_version_t_2,
		http_version_p,
		http_version_slash,
		http_version_major_start,
		http_version_major,
		http_version_minor_start,
		http_version_minor,
		status_code_start,
		status_code,
		reason_phrase,
		linefeed,
		fail
	} state = http_version_h;

	Iterator iter = begin;
	std::string reason;
	while (iter != end && state != fail)
	{
		char c = *iter++;
		switch (state)
		{
		case http_version_h:
			state = (c == 'H') ? http_version_t_1 : fail;
			break;
		case http_version_t_1:
			state = (c == 'T') ? http_version_t_2 : fail;
			break;
		case http_version_t_2:
			state = (c == 'T') ? http_version_p : fail;
			break;
		case http_version_p:
			state = (c == 'P') ? http_version_slash : fail;
			break;
		case http_version_slash:
			state = (c == '/') ? http_version_major_start : fail;
			break;
		case http_version_major_start:
			if (std::isdigit(c))
			{
				version_major = version_major * 10 + c - '0';
				state = http_version_major;
			}
			else
				state = fail;
			break;
		case http_version_major:
			if (c == '.')
				state = http_version_minor_start;
			else if (std::isdigit(c))
				version_major = version_major * 10 + c - '0';
			else
				state = fail;
			break;
		case http_version_minor_start:
			if (std::isdigit(c))
			{
				version_minor = version_minor * 10 + c - '0';
				state = http_version_minor;
			}
			else
				state = fail;
			break;
		case http_version_minor:
			if (c == ' ')
				state = status_code_start;
			else if (std::isdigit(c))
				version_minor = version_minor * 10 + c - '0';
			else
				state = fail;
			break;
		case status_code_start:
			if (std::isdigit(c))
			{
				status = status * 10 + c - '0';
				state = status_code;
			}
			else
				state = fail;
			break;
		case status_code:
			if (c == ' ')
				state = reason_phrase;
			else if (std::isdigit(c))
				status = status * 10 + c - '0';
			else
				state = fail;
			break;
		case reason_phrase:
			if (c == '\r')
				state = linefeed;
			else if (std::iscntrl(c))
				state = fail;
			else
				reason.push_back(c);
			break;
		case linefeed:
			return (c == '\n');
		default:
			return false;
		}
	}
	return false;
}
} // namespace detail

//! http://www.cnblogs.com/lzjsky/archive/2011/05/12/2044460.html
// 异步调用专用。
// boost::asio::placeholders::error
// boost::asio::placeholders::bytes_transferred
class proxy_tunnel_client {
public:
    typedef std::function<void(sss::string_view responce)> onResponce_t;
    typedef std::function<bool(sss::string_view mark)> onEndCheck_t;

public:
    proxy_tunnel_client(boost::asio::io_service& io_service,
                        boost::asio::ssl::context* p_ctx = nullptr)
        : m_resolver(io_service), m_socket(io_service), m_has_eof(false),
          m_max_redirect(5),
          m_request(2048), m_response(2048)
    {
        
        COLOG_ERROR(m_request.max_size());
        COLOG_ERROR(m_response.max_size());
        if (p_ctx) {
            m_socket.upgrade_to_ssl(*p_ctx);
        }
    }

    void upgrade_to_ssl(boost::asio::ssl::context& ctx)
    {
        m_socket.upgrade_to_ssl(ctx);
    }

    void                    setOnContent(onResponce_t&& func) { m_onContent = std::move(func);        }
    void                    setOnEndCheck(onEndCheck_t&& func) { m_onEndCheck = std::move(func);       }

    ss1x::http::Headers&    header()                           { return m_headers;                     }
    bool                    eof() const                        { return m_has_eof;                     }

    const std::string       get_url() const                    { return m_redirect_urls.back();        }
    const boost::system::error_code& error_code() const        { return m_ec;                          }

    size_t                  max_redirect() const               { return m_max_redirect;                }

    void                    max_redirect(int mr)               { if (mr >= 0) { m_max_redirect = mr; } }


    void http_get(const std::string& url)
    {
        m_redirect_urls.resize(0);
        m_redirect_urls.push_back(url);
        http_get_impl();
    }

    bool is_need_ssl(const decltype(ss1x::util::url::split_port_auto("")) & url_info)
    {
        return std::get<2>(url_info) == 443;
    }

    void http_get_impl()
    {
        auto url_info = ss1x::util::url::split_port_auto(get_url());
        if (is_need_ssl(url_info)) {
            m_socket.upgrade_to_ssl();
        }
        else {
            m_socket.disable_ssl();
        }
        http_get_impl(std::get<1>(url_info), std::get<2>(url_info), std::get<3>(url_info));
    }

    void http_get_impl(const std::string& server, int port, const std::string& path)
    {
        COLOG_DEBUG(sss::raw_string(server), port, sss::raw_string(path));

        std::ostream request_stream(&m_request);
        request_stream << "GET " << path << " HTTP/1.0\r\n";
        request_stream << "Host: " << server << "\r\n";
        request_stream << "Accept: */*\r\n";
        request_stream << "User-Agent: " << USER_AGENT_DEFAULT;
        request_stream << "Connection: close\r\n\r\n";

        char port_buf[10];
        std::sprintf(port_buf, "%d", port <= 0 ? 80 : port);

        boost::asio::ip::tcp::resolver::query query(server, port_buf);
        COLOG_DEBUG(SSS_VALUE_MSG(server));
        COLOG_DEBUG(SSS_VALUE_MSG(port));
        m_resolver.async_resolve(
            query, boost::bind(&proxy_tunnel_client::handle_resolve, this,
                               boost::asio::placeholders::error,
                               boost::asio::placeholders::iterator));
    }

    void ssl_tunnel_get(const std::string& proxy_domain, int proxy_port,
                        const std::string& url)
    {
        m_redirect_urls.resize(0);
        m_redirect_urls.push_back(url);
        m_proxy_hostname = proxy_domain;
        m_proxy_port = proxy_port;
        ssl_tunnel_get_impl();
    }

private:
    bool is_exceed_max_redirect() const
    {
        return m_redirect_urls.size() > m_max_redirect + 1;
    }
    void ssl_tunnel_get_impl()
    {
        COLOG_DEBUG(sss::raw_string(m_proxy_hostname), m_proxy_port);
        auto url_info = ss1x::util::url::split_port_auto(get_url());
        std::ostream request_stream(&m_request);
        request_stream << "CONNECT " << std::get<1>(url_info) << ':'
                       << std::get<2>(url_info) << " HTTP/1.1\r\n";
        request_stream << "Host: " << m_proxy_hostname << ':' << m_proxy_port
                       << "\r\n";
        request_stream << "\r\n";  // empty line

        char port_buf[10];
        std::sprintf(port_buf, "%d", m_proxy_port <= 0 ? 80 : m_proxy_port);

        boost::asio::ip::tcp::resolver::query query(m_proxy_hostname, port_buf);
        // NOTE 这async_resolve()之后，是异步完成；如果现在是normal-socket，
        // 那么，我在什么时候，才能upgrade_to_ssl() 呢？
        // 还是说，我额外设计一组流程。只不过，事先就update；然后内部的异步handle调用中，
        // 选用合适的layer，完成通信就好？
        // 两次握手。分别是proxy_server，remote-target-server
        // 或者这样，同一个 proxy_tunnel_client ，调用两次，分别是http_tunnel()
        // 和 http_get()
        // 前者，只是获取一个200；后者完成一般的通信；
        async_https_proxy_connect(m_socket.get_socket(), query);
    }

    template<typename Stream, typename Query>
    void async_https_proxy_connect(Stream& sock, Query& query)
    {
        m_resolver.async_resolve(
            query, boost::bind(&proxy_tunnel_client::async_https_proxy_resolve<Stream>, this,
                               boost::asio::placeholders::error,
                               boost::asio::placeholders::iterator, boost::ref(sock)));
    }

    template <typename Stream>
    void async_https_proxy_resolve(const boost::system::error_code& err,
                                   boost::asio::ip::tcp::resolver::iterator endpoint_iterator,
                                   Stream& sock)
    {
        COLOG_DEBUG("");
        if (err)
        {
            COLOG_ERROR("Connect to http proxy \'", m_proxy_hostname, ":",
                        m_proxy_port, "\', error message \'", err.message(),
                        "\'");
            set_error_code(err);
            return;
        }
        // 开始异步连接代理.
        boost::asio::async_connect(
            sock.lowest_layer(), endpoint_iterator,
            boost::bind(
                &proxy_tunnel_client::handle_connect_https_proxy<Stream>, this,
                boost::ref(sock), endpoint_iterator,
                boost::asio::placeholders::error));
    }

    template <typename Stream>
    void handle_connect_https_proxy(
        Stream& sock,
        boost::asio::ip::tcp::resolver::iterator endpoint_iterator,
        const boost::system::error_code& err)
    {
        COLOG_DEBUG(pretty_ec(err));
        if (err)
        {
            endpoint_iterator++;
            boost::asio::ip::tcp::resolver::iterator end;
            if (endpoint_iterator == end)
            {
                COLOG_ERROR("Connect to http proxy \'" , m_proxy_hostname , ":" , m_proxy_port , 
                    "\', error message \'" , err.message() , "\'");
                set_error_code(err);
                return;
            }

            // 继续尝试连接下一个IP.
            boost::asio::async_connect(
                sock.lowest_layer(), endpoint_iterator,
                boost::bind(&proxy_tunnel_client::handle_connect_https_proxy<Stream>,
                            this, boost::ref(sock), endpoint_iterator,
                            boost::asio::placeholders::error));

            return;
        }
        discard(m_response);
        std::ostream request_stream(&m_request);
        auto url_info = ss1x::util::url::split_port_auto(get_url());
        request_stream << "CONNECT " << std::get<1>(url_info) << ":"
                       << std::get<2>(url_info) << " HTTP/1.1\r\n";
        request_stream << "Host: " << std::get<1>(url_info) << "\r\n";
        request_stream << "Accept: text/html, application/xhtml+xml, */*\r\n";
        request_stream << "User-Agent: " << USER_AGENT_DEFAULT;
#if 1
        // NOTE 貌似 是否close，对于结果没啥影响
        request_stream << "Connection: close\r\n\r\n";
#endif
        request_stream << "\r\n";

        COLOG_DEBUG(streambuf_view(m_request));

        boost::asio::async_write(
            sock, m_request, boost::asio::transfer_exactly(m_request.size()),
            boost::bind(
                &proxy_tunnel_client::handle_https_proxy_request<Stream>, this,
                boost::ref(sock), boost::asio::placeholders::error));
    }

    template<typename Stream>
    void handle_https_proxy_request(Stream& sock, const boost::system::error_code& err)
    {
        COLOG_DEBUG(pretty_ec(err));
        if (err)
        {
            COLOG_ERROR("Connect to http proxy ", sss::raw_string(m_proxy_hostname),
                        ":", m_proxy_port, "error message ", err.message());
            set_error_code(err);
            return;
        }
        boost::asio::async_read_until(sock, m_response, "\r\n",
          boost::bind(&proxy_tunnel_client::handle_https_proxy_status<Stream>,
                      this,
                      boost::ref(sock), boost::asio::placeholders::error));
    }

    template<typename Stream>
    void handle_https_proxy_status(Stream& sock, const boost::system::error_code& err)
    {
        COLOG_DEBUG(pretty_ec(err));
        if (err)
        {
            COLOG_ERROR("Connect to http proxy ", sss::raw_string(m_proxy_hostname),
                        ":", m_proxy_port, "error message ", err.message());
            set_error_code(err);
            return;
        }

        boost::system::error_code ec;

        // 解析状态行，
        // 检查http状态码；
        int version_major = 0;
        int version_minor = 0;
        int status_code = 0;
        if (!detail::parse_http_status_line(
                std::istreambuf_iterator<char>(&m_response),
                std::istreambuf_iterator<char>(), version_major, version_minor,
                status_code)) {
            ec = ss1x::errc::malformed_status_line;
            COLOG_ERROR("Connect to http proxy ", sss::raw_string(m_proxy_hostname),
                        ":", m_proxy_port, "error message ", err.message());
            return;
        }
        COLOG_DEBUG("proxy_status:http version: ", version_major, '.', version_minor);

        boost::asio::async_read_until(
            sock, m_response, "\r\n\r\n",
            boost::bind(&proxy_tunnel_client::handle_https_proxy_header<Stream>,
                        this, boost::ref(sock),
                        boost::asio::placeholders::bytes_transferred,
                        boost::asio::placeholders::error));
    }

    template<typename Stream>
    void handle_https_proxy_header(Stream & sock, int bytes_transferred, const boost::system::error_code& err)
    {
        COLOG_DEBUG(SSS_VALUE_MSG(bytes_transferred), pretty_ec(err));
        if (err)
        {
            COLOG_ERROR("Connect to http proxy ", sss::raw_string(m_proxy_hostname),
                        ":", m_proxy_port, "error message ", err.message());
            set_error_code(err);
            return;
        }
        boost::system::error_code ec;
        // Process the response headers from proxy server.
        std::istream response_stream(&m_response);
        std::string header;
        while (std::getline(response_stream, header) && header != "\r") {
            size_t colon_pos = header.find(':');
            if (colon_pos == std::string::npos) {
                continue;
            }
            size_t value_beg =
                header.find_first_not_of("\t ", colon_pos + 1);
            if (value_beg == std::string::npos) {
                value_beg = header.length();
            }
            // NOTE descard the last '\r'
            size_t len = header.back() == '\r'
                ? header.length() - value_beg - 1
                : header.length() - value_beg;
            m_headers[header.substr(0, colon_pos)] =
                header.substr(value_beg, len);

            // NOTE TODO 或许，应该用vector来保存header；
            COLOG_DEBUG(sss::raw_string(header));
        }

        // Write whatever content we already have to output.
        if (m_response.size() > 0 && this->header().status_code == 200) {
            COLOG_DEBUG(SSS_VALUE_MSG(m_response.size()));
            consume_content(m_response);
        }
        else {
            discard(m_response);
        }

        COLOG_DEBUG("Connect to http proxy \'", m_proxy_hostname, ":", m_proxy_port, "\'.");

        // ssl的handshake过程，是底层封装好了的。这里必须交给ssl_stream的socket对象
        // 的async_handshake（或者同步版handshake()函数）来完成。
        // 传入的回调函数，名为xxx_handshake，其实和handshake没啥关系，只是底层在完
        // 成了handshake之后，给予用户的一个通知结果的接口。
        // 另外需要注意的是，一旦成功handshake之后，后续交流用到的socket，一定是ssl
        // 版！即，需要底层库，完成加密解密后，用户代码才能看到（对于用户透明，但是
        // 有带宽以及运算延时的损耗）。
        m_socket.get_ssl_socket().async_handshake(
            boost::asio::ssl::stream_base::client,
            boost::bind(&proxy_tunnel_client::handle_https_proxy_handshake, this,
                        boost::asio::placeholders::error));
    }

    void handle_https_proxy_handshake(const boost::system::error_code& err)
    {
        COLOG_DEBUG(pretty_ec(err));
        if (err)
        {
            COLOG_ERROR("Connect to http proxy ", sss::raw_string(m_proxy_hostname),
                        ":", m_proxy_port, "error message ", err.message());
            set_error_code(err);
            return;
        }

        COLOG_DEBUG("Handshake to ", sss::raw_string(get_url()));

        COLOG_DEBUG("m_response.consume(", m_response.size(), ")");
        // 清空接收缓冲区.
        discard(m_response);

        // 发起异步请求.
        async_request();
    }

    void async_request()
    {
        COLOG_DEBUG("");
        // GET / HTTP/1.1
        // Host: www.google.co.jp
        // Accept: text/html, application/xhtml+xml, */*
        // User-Agent: avhttp/2.9.9
        // Connection: close

        discard(m_response);
        std::ostream request_stream(&m_request);
        auto url_info = ss1x::util::url::split_port_auto(get_url());
        request_stream << "GET " << std::get<3>(url_info) << " HTTP/1.1\r\n";
        request_stream << "Host: " << std::get<1>(url_info) << "\r\n";
        request_stream << "Accept: text/html, application/xhtml+xml, */*\r\n";
        request_stream << "User-Agent: " << USER_AGENT_DEFAULT;
        request_stream << "Connection: " << "close" << "\r\n";
        request_stream << "\r\n";

        COLOG_DEBUG(streambuf_view(m_request));
        boost::asio::async_write(
            m_socket, m_request,
            boost::asio::transfer_exactly(m_request.size()),
            boost::bind(&proxy_tunnel_client::handle_request, this,
                        boost::asio::placeholders::error));
    }

    void handle_request(const boost::system::error_code& err)
    {
        COLOG_DEBUG(pretty_ec(err));
        if (err)
        {
            COLOG_ERROR("Send request, error message: ", err.message());
            set_error_code(err);
            return;
        }

        discard(m_response);
        // 异步读取Http status.
        boost::asio::async_read_until(
            m_socket, m_response, "\r\n",
            boost::bind(&proxy_tunnel_client::handle_read_status, this,
                        boost::asio::placeholders::bytes_transferred,
                        boost::asio::placeholders::error));
    }

    void handle_read_status(int bytes_transferred, const boost::system::error_code& err)
    {
        COLOG_DEBUG(bytes_transferred, pretty_ec(err));
        if (err)
        {
            COLOG_ERROR("Read status line, error message: ", err.message());
            set_error_code(err);
            return;
        }

        // 复制到新的streambuf中处理首行http状态, 如果不是http状态行, 那么将保持m_response中的内容,
        // 这主要是为了兼容非标准http服务器直接向客户端发送文件的需要, 但是依然需要以malformed_status_line
        // 通知用户, malformed_status_line并不意味着连接关闭, 关于m_response中的数据如何处理, 由用户自己
        // 决定是否读取, 这时, 用户可以使用read_some/async_read_some来读取这个链接上的所有数据.
        boost::asio::streambuf tempbuf;
        // int response_size = m_response.size();
        boost::asio::buffer_copy(tempbuf.prepare(bytes_transferred), m_response.data());
        tempbuf.commit(bytes_transferred);
        COLOG_DEBUG(streambuf_view(tempbuf));

        // 检查http状态码, version_major和version_minor是http协议的版本号.
        int version_major = 0;
        int version_minor = 0;
        int status_code = 0;
        if (!detail::parse_http_status_line(
                std::istreambuf_iterator<char>(&tempbuf),
                std::istreambuf_iterator<char>(),
                version_major, version_minor, status_code))
        {
            COLOG_ERROR("Malformed status line");
            set_error_code(ss1x::errc::malformed_status_line);
            return;
        }
        this->header().status_code = status_code;

        COLOG_DEBUG(version_major, '.', version_minor, status_code);
        discard(m_response, bytes_transferred);

        // NOTE
        //
        // 对于大段大段的header，如何处理安全点呢？
        // 还是分开的好。
        // 但是，仍然无法避免过长的header攻击。
        // 或者垃圾header字段，让你抽取不到有用的信息。
        // 另外，header如果要解析的话，是不区分大小写的（key-value的key）
        //
        // TODO FIXME 唯一正确的处理办法，是状态机！
        // 这样，就不用保持数据，一直扩大缓存，也能处理各种 async_read_until 的结束条件
        boost::asio::async_read_until(
            m_socket, m_response, "\r\n\r\n",
            boost::bind(&proxy_tunnel_client::handle_read_header, this,
                        boost::asio::placeholders::bytes_transferred,
                        boost::asio::placeholders::error));
    }

    void handle_read_header(int bytes_transferred, const boost::system::error_code& err)
    {
        COLOG_DEBUG(bytes_transferred, pretty_ec(err));
        COLOG_DEBUG(streambuf_view(m_response));

        if (err) {
            set_error_code(err);
            return;
        }
        // TODO FIXME
        // 利用状态机，或者分行处理header；
        // 假设header每个有效行，不会超过1024byte。
        // 超过的，按错误处理。
        // Process the response headers.
        std::istream response_stream(&m_response);
        std::string header;
        bool redirect = false;

        // NOTE 在这里处理3xx(301,302)跳转！
        while (std::getline(response_stream, header) && header != "\r") {
            size_t colon_pos = header.find(':');
            if (colon_pos == std::string::npos) {
                continue;
            }
            size_t value_beg =
                header.find_first_not_of("\t ", colon_pos + 1);
            if (value_beg == std::string::npos) {
                value_beg = header.length();
            }
            // NOTE descard the last '\r'
            size_t len = header.back() == '\r'
                ? header.length() - value_beg - 1
                : header.length() - value_beg;
            std::string key = header.substr(0, colon_pos);
            std::string value = header.substr(value_beg, len);
            switch (this->header().status_code) {
                case 301:
                case 302:
                    if (key == "Location" && !value.empty() && value != this->get_url()) {
                        redirect = true;
                        this->m_redirect_urls.push_back(value);
                    }
                    break;

                default:
                    break;
            }
            m_headers[key] = value;

            // NOTE TODO 或许，应该用vector来保存header；
            COLOG_DEBUG(sss::raw_string(header));
        }

        // Write whatever content we already have to output.
        if (m_response.size() > 0 && this->header().status_code == 200) {
            COLOG_DEBUG(SSS_VALUE_MSG(m_response.size()));
            consume_content(m_response);
        }
        else {
            discard(m_response);
        }

        if (redirect) {
            if (is_exceed_max_redirect()) {
                set_error_code(ss1x::errc::exceed_max_redirect);
                return;
            }

            if (!m_proxy_hostname.empty()) {
                ssl_tunnel_get_impl();
            }
            else {
                http_get_impl();
            }
        }

        // NOTE 如果正文过短的话，可能到这里，已经读完socket缓存了。
        // Start reading remaining data until EOF.
        boost::asio::async_read(
            m_socket, m_response, boost::asio::transfer_at_least(1),
            boost::bind(&proxy_tunnel_client::handle_read_content, this,
                        boost::asio::placeholders::bytes_transferred,
                        boost::asio::placeholders::error));
    }

    void http_post(const std::string& server, int port, const std::string& path,
                   const std::string& postParams)
    {
        COLOG_DEBUG(sss::raw_string(server), port, sss::raw_string(path));
        std::ostream request_stream(&m_request);
        // NOTE 可以从 m_headers 中，构造http-head。
        request_stream << "POST " << path << " HTTP/1.0\r\n";
        request_stream << "Host: " << server << "\r\n";

        request_stream << "Content-Type: application/x-www-form-urlencoded; "
                          "charset=utf-8\r\n";
        request_stream << "Content-Length: " +
                              std::to_string(postParams.length()) + "\r\n\r\n";
        request_stream << postParams << "\r\n\r\n";

        char port_buf[10];
        std::sprintf(port_buf, "%d", port <= 0 ? 80 : port);

        boost::asio::ip::tcp::resolver::query query(server, port_buf);
        m_resolver.async_resolve(
            query, boost::bind(&proxy_tunnel_client::handle_resolve, this,
                               boost::asio::placeholders::error,
                               boost::asio::placeholders::iterator));
    }

    void handle_resolve(
        const boost::system::error_code& err,
        boost::asio::ip::tcp::resolver::iterator endpoint_iterator)
    {
        COLOG_DEBUG(pretty_ec(err));
        if (err) {
            set_error_code(err);
            return;
        }
        if (m_socket.using_ssl()) {
            m_socket.get_ssl_socket().set_verify_mode(
                boost::asio::ssl::verify_peer);

            m_socket.get_ssl_socket().set_verify_callback(boost::bind(
                    &proxy_tunnel_client::verify_certificate, this, _1, _2));
            // TODO
            // if (is_certificate()) {
            //     ...
            // }
            // else {
            //     m_ssl_stream->set_verify_mode(boost::asio::ssl::context::verify_none);
            // }
        }
        boost::asio::async_connect(
            m_socket.lowest_layer(), endpoint_iterator,
            boost::bind(&proxy_tunnel_client::handle_connect, this,
                        boost::asio::placeholders::error));
    }

    bool verify_certificate(bool preverified,
                            boost::asio::ssl::verify_context& ctx)
    {
        COLOG_DEBUG(SSS_VALUE_MSG(preverified));
        // The verify callback can be used to check whether the certificate that
        // is being presented is valid for the peer. For example, RFC 2818
        // describes the steps involved in doing this for HTTPS. Consult the
        // OpenSSL documentation for more details. Note that the callback is
        // called once for each certificate in the certificate chain, starting
        // from the root certificate authority.

        // In this example we will simply print the certificate's subject name.
        char subject_name[256];
        X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
        X509_NAME_oneline(X509_get_subject_name(cert), subject_name,
                          sizeof(subject_name));

        COLOG_DEBUG("Verifying ", subject_name);

        return preverified;
    }

    void handle_connect(const boost::system::error_code& err)
    {
        COLOG_DEBUG(pretty_ec(err));
        if (err) {
            set_error_code(err);
            return;
        }
        if (m_socket.using_ssl()) {
            m_socket.get_ssl_socket().async_handshake(
                boost::asio::ssl::stream_base::client,
                boost::bind(&proxy_tunnel_client::handle_handshake, this,
                            boost::asio::placeholders::error));
        }
        else {
            boost::asio::async_write(
                m_socket, m_request,
                boost::bind(&proxy_tunnel_client::handle_write_request,
                            this, boost::asio::placeholders::error));
        }
    }

    void handle_handshake(const boost::system::error_code& err)
    {
        COLOG_DEBUG(pretty_ec(err));
        if (err) {
            set_error_code(err);
            return;
        }
        const char* header =
            boost::asio::buffer_cast<const char*>(m_request.data());
        COLOG_DEBUG(SSS_VALUE_MSG(header));

        // The handshake was successful. Send the request.
        boost::asio::async_write(
            m_socket, m_request,
            boost::bind(&proxy_tunnel_client::handle_write_request, this,
                        boost::asio::placeholders::error));
    }

    void handle_write_request(const boost::system::error_code& err)
    {
        COLOG_DEBUG(pretty_ec(err));
        if (err) {
            set_error_code(err);
            return;
        }
        // Read the response status line. The m_response streambuf will
        // automatically grow to accommodate the entire line. The growth
        // may be limited by passing a maximum size to the streambuf
        // constructor.
        boost::asio::async_read_until(
            m_socket, m_response, "\r\n",
            boost::bind(&proxy_tunnel_client::handle_read_status, this,
                        boost::asio::placeholders::bytes_transferred,
                        boost::asio::placeholders::error));
    }

    void handle_read_content(int bytes_transferred, const boost::system::error_code& err)
    {
        COLOG_DEBUG(bytes_transferred, pretty_ec(err), m_response.size());
        if (bytes_transferred <= 0) {
            return;
        }

        if (err) {
            // NOTE boost::asio::error::eof 打印输出 asio.misc:2
            m_has_eof = (err == boost::asio::error::eof);
            set_error_code(err);
            return;
        }

        // Write all of the data that has been read so far.
        COLOG_DEBUG(SSS_VALUE_MSG(m_response.size()));

        bool is_end = this->is_end_chunk(m_response);
        if (m_onContent && this->header().status_code == 200) {
            consume_content(m_response, bytes_transferred);
        }
        else {
            discard(m_response, bytes_transferred);
        }
        COLOG_DEBUG(SSS_VALUE_MSG(m_response.size()));

        if (is_end) {
            return;
        }

        // NOTE
        // async_read_until，在当前buffer大小范围，如果都没有读取到终止标记串，
        // 则会返回 {asio.misc,3,"Element not found"} 这个错误号。
        // 注意，此时bytes_transferred的值是0；但是 m_response.size()是有大小的，
        // 而且，此时m_response是被塞满了的！
        // 那么，如何安全地处理这种 async_read_until 动作呢？
        //
        // 首先，我需要空出buffer，才能继续读，以便获得足够用来判断的数据。
        // 此时，需要面对的另外一个问题是，如果标记字符串，恰好处于分割位置呢？
        // 这样的话，安全的做法是，保留 【标记串长度-1】byte的数据，其他的转移出去。
        // 再用同样的规则，继续读。
        //
        // 不过，这对于正则表达式的结束规则，就不好用了。此时最好的办法是，扩展
        // 缓冲区，直到正则表达式可以匹配。——扩展缓冲区，就留下了一个用来攻击
        // 的可能了。
        //
        // boost::asio::async_read_until(
        //     m_socket, m_response, "</html>",
        //     boost::bind(&proxy_tunnel_client::handle_read_content, this,
        //                 boost::asio::placeholders::bytes_transferred,
        //                 boost::asio::placeholders::error));
        //
        // Continue reading remaining data until EOF.
        boost::asio::async_read(
            m_socket, m_response, boost::asio::transfer_at_least(1),
            boost::bind(&proxy_tunnel_client::handle_read_content, this,
                        boost::asio::placeholders::bytes_transferred,
                        boost::asio::placeholders::error));
    }

    // void async_connect(const std::string& address, const std::string& port)
    // {
    //     boost::asio::ip::tcp::resolver::query query(address, port);
    //     m_resolver.async_resolve(
    //         query, boost::bind(&proxy_tunnel_client::handle_resolve, this,
    //                            boost::asio::placeholders::error,
    //                            boost::asio::placeholders::iterator));
    // }

    // void async_write(const void* data, size_t size, bool in_place = false)
    // {
    //     if (!in_place) {
    //         // do something
    //         boost::asio::async_write(
    //             m_socket, m_request,
    //             boost::bind(&proxy_tunnel_client::handle_write, this,
    //                         boost::asio::placeholders::error));
    //     }
    //     else
    //         boost::asio::async_write(
    //             m_socket, boost::asio::buffer(data, size),
    //             boost::bind(&proxy_tunnel_client::handle_write, this,
    //                         boost::asio::placeholders::error));
    // }

    // void handle_write(const boost::system::error_code& e)
    // {
    //     if (!e) {
    //         boost::asio::async_read_until(
    //             m_socket, m_response, "\r\n\r\n",
    //             boost::bind(&proxy_tunnel_client::handle_read_header, this,
    //                         boost::asio::placeholders::error));
    //     }
    //     else {
    //         onIoError(e);
    //     }
    // }

    // void onIoError(const boost::system::error_code& err)
    // {
    //     std::cerr << "Error resolve: " << err.message() << "\n";
    // }

protected:
    void set_error_code(const boost::system::error_code& ec) { m_ec = ec; }
    void discard(boost::asio::streambuf& responce, int bytes_transferred = 0)
    {
        if (bytes_transferred > 0) {
            responce.consume(std::min(responce.size(), std::size_t(bytes_transferred)));
        }
        else {
            responce.consume(responce.size());
        }
    }
    void consume_content(boost::asio::streambuf& responce, int bytes_transferred = 0)
    {
        COLOG_DEBUG(SSS_VALUE_MSG(responce.size()), SSS_VALUE_MSG(bytes_transferred));
        if (m_onContent) {
            std::size_t size = responce.size();
            if (bytes_transferred > 0 && size > std::size_t(bytes_transferred)) {
                size = bytes_transferred;
            }
            boost::asio::streambuf::const_buffers_type::const_iterator begin(
                responce.data().begin());
            const char* ptr = boost::asio::buffer_cast<const char*>(*begin);
            sss::string_view sv(ptr, size);
            m_onContent(sv);
            responce.consume(size);
        }
        else {
            discard(responce, bytes_transferred);
        }
    }
    bool is_end_chunk(const boost::asio::streambuf& buf) const
    {
        if (!m_onEndCheck) {
            return false;
        }
        int buf_size = buf.size();
        boost::asio::streambuf::const_buffers_type::const_iterator begin(
            buf.data().begin());
        const char* ptr = boost::asio::buffer_cast<const char*>(*begin);
        sss::string_view sv(ptr, buf_size);
        // return sv.find("</html>") != sss::string_view::npos;
        return m_onEndCheck(sv);
    }

private:
    boost::asio::ip::tcp::resolver m_resolver;
    ss1x::detail::socket_t         m_socket;
    bool                           m_has_eof;
    // 最大跳转次数
    // 0 表示，不允许跳转；
    // 1 表示可以跳转一次；以此类推
    size_t                         m_max_redirect;

    boost::asio::streambuf         m_request;
    boost::asio::streambuf         m_response;

    // TODO 也许，应该将header分开,request,responce
    // 不过，对于header()函数来说，用户一般只关心responce的header。
    ss1x::http::Headers            m_headers;
    boost::system::error_code      m_ec;
    std::string                    m_proxy_hostname;
    int                            m_proxy_port;
    std::vector<std::string>       m_redirect_urls;
    onResponce_t                   m_onContent;
    onEndCheck_t                   m_onEndCheck;
};
