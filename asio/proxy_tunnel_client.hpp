#pragma once

#include "socket_t.hpp"
#include "user_agent.hpp"
#include "gzstream.hpp"

#include <cctype>

#include <vector>
#include <string>

#include <iostream>
#include <istream>
#include <ostream>

#include <stdexcept>

#include <memory>
#include <functional>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>

#include <sss/colorlog.hpp>
#include <sss/debug/value_msg.hpp>
#include <sss/util/PostionThrow.hpp>
#include <sss/string_view.hpp>
#include <sss/utlstring.hpp>
#include <sss/hex_print.hpp>

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

inline streambuf_t streambuf_view(const boost::asio::streambuf& stream)
{
    return streambuf_t{stream};
}

inline sss::string_view cast_string_view(const boost::asio::streambuf& streambuf)
{
    int buf_size = streambuf.size();
    boost::asio::streambuf::const_buffers_type::const_iterator begin(
        streambuf.data().begin());
    const char* ptr = boost::asio::buffer_cast<const char*>(*begin);
    sss::string_view buf(ptr, buf_size);
    return sss::string_view(ptr, buf_size);
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

inline pretty_ec_t pretty_ec(const boost::system::error_code& ec)
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
			else {
                state = fail;
            }
			break;

        case http_version_major:
            if (c == '.') {
                state = http_version_minor_start;
            }
            else if (std::isdigit(c)) {
                version_major = version_major * 10 + c - '0';
            }
            else {
                state = fail;
            }
            break;

		case http_version_minor_start:
			if (std::isdigit(c))
			{
				version_minor = version_minor * 10 + c - '0';
				state = http_version_minor;
			}
			else {
                state = fail;
            }
			break;

		case http_version_minor:
			if (c == ' ') {
				state = status_code_start;
            }
			else if (std::isdigit(c)) {
                version_minor = version_minor * 10 + c - '0';
            }
			else {
                state = fail;
            }
			break;

		case status_code_start:
			if (std::isdigit(c))
			{
				status = status * 10 + c - '0';
				state = status_code;
			}
			else {
                state = fail;
            }
			break;

		case status_code:
            if (c == ' ') {
                state = reason_phrase;
            }
            else if (std::isdigit(c)) {
                status = status * 10 + c - '0';
            }
            else {
                state = fail;
            }
            break;

        case reason_phrase:
            if (c == '\r') {
                state = linefeed;
            }
            else if (std::iscntrl(c)) {
                state = fail;
            }
            else {
                reason.push_back(c);
            }
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
    typedef std::function<std::string(const std::string& domain, const std::string& path)>
             CookieFunc_t;

public:
    proxy_tunnel_client(boost::asio::io_service& io_service,
                        boost::asio::ssl::context* p_ctx = nullptr)
        : m_resolver(io_service),
          m_socket(io_service),
          m_has_eof(false),
          m_chunked_transfer(false),
          m_chunk_size(0),
          m_response_content_length(-1),
          m_max_redirect(5),
          m_is_gzip(false),
          m_request(2048),
          m_response(2048),
          m_deadline(io_service)
    {
        
        COLOG_DEBUG(m_request.max_size());
        COLOG_DEBUG(m_response.max_size());
        if (p_ctx) {
            m_socket.upgrade_to_ssl(*p_ctx);
        }
    }

    void upgrade_to_ssl(boost::asio::ssl::context& ctx)
    {
        m_socket.upgrade_to_ssl(ctx);
    }

    void                    setCookieFunc(CookieFunc_t&& func) {
        m_onRequestCookie = std::move(func);
    }
    void                    setOnContent(const onResponce_t& func)  { m_onContent  = func;       }
    void                    setOnEndCheck(const onEndCheck_t& func) { m_onEndCheck = func;       }
    void                    setOnContent(onResponce_t&& func)  { m_onContent  = std::move(func); }
    void                    setOnEndCheck(onEndCheck_t&& func) { m_onEndCheck = std::move(func); }

    ss1x::http::Headers&    header()                           { return m_response_headers;            }
    ss1x::http::Headers&    request_header()                   { return m_request_headers;             }
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

    // const boost::system::error_code& err
    void check_deadline()
    {
        // if (err && err !=boost::asio::error::eof) {
        //     this->m_socket.get_socket().cancel(); 
        //     return;
        // }
        // Check whether the deadline has passed. We compare the deadline against 
        // the current time since a new asynchronous operation may have moved the 
        // deadline before this actor had a chance to run. 

        if (m_deadline.expires_at() <= boost::asio::deadline_timer::traits_type::now()) 
        { 
            // The deadline has passed. The socket is closed so that any outstanding 
            // asynchronous operations are cancelled. 

            this->m_socket.get_socket().close(); 

            // There is no longer an active deadline. The expiry is set to positive 
            // infinity so that the actor takes no action until a new deadline is set. 
            m_deadline.expires_at(boost::posix_time::pos_infin); 

            throw "Time out : deadline on client!"; 
        } 

        // Put the actor back to sleep. 
        m_deadline.async_wait(boost::bind(&proxy_tunnel_client::check_deadline, this)); 
    }

    void http_get_impl(const std::string& server, int port, const std::string& path)
    {
        COLOG_DEBUG(sss::raw_string(server), port, sss::raw_string(path));

        // m_deadline.async_wait(boost::bind(&proxy_tunnel_client::check_deadline, this)); 

        // //TODO magic number
        // m_deadline.expires_from_now(boost::posix_time::seconds(5 * 60)); 

        // std::cout << __PRETTY_FUNCTION__ << std::endl;

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

    bool had_exceed_max_redirect() const
    {
        return m_redirect_urls.size() > m_max_redirect + 1;
    }
    void ssl_tunnel_get_impl()
    {
        auto url_info = ss1x::util::url::split_port_auto(get_url());
        if (is_need_ssl(url_info)) {
            m_socket.upgrade_to_ssl();
        }
        else {
            m_socket.disable_ssl();
        }
        COLOG_DEBUG(sss::raw_string(m_proxy_hostname), m_proxy_port);

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
        if (std::get<0>(url_info) == "https") {
            request_stream << "CONNECT " << std::get<1>(url_info);
            if (std::get<2>(url_info) != 80) {
                request_stream << ":" << std::get<2>(url_info);
            }
            request_stream << " HTTP/1.1\r\n";
            request_stream << "Host: " << std::get<1>(url_info) << "\r\n";
            request_stream << "Accept: text/html, application/xhtml+xml, */*\r\n";
            request_stream << "User-Agent: " << USER_AGENT_DEFAULT << "\r\n";
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
        else {
            async_request();
            return;
        }
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

        // TODO 按行拆分 proxy header 的处理
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
        // std::istream response_stream(&m_response);
        m_response_headers.clear();
        processHeader(m_response_headers, cast_string_view(m_response));
        // processHeader(m_response_headers, response_stream);

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

    void requestStreamHelper(std::set<std::string>& used_field,
                             ss1x::http::Headers& request_header,
                             std::ostream& request_stream,
                             const std::string& field,
                             const std::string& default_value = "")
    {
        if (used_field.find(field) != used_field.end()) {
            return;
        }
        auto it = request_header.find(field);
        if (it != request_header.end())
        {
            if (!it->second.empty()) {
                request_stream << field << ": " << it->second << "\r\n";
            }
        }
        else if (!default_value.empty()) {
            request_stream << field << ": " << default_value << "\r\n";
        }
        used_field.insert(field);
    }

    void requestStreamDumpRest(std::set<std::string>& used_field,
                               ss1x::http::Headers& request_header,
                               std::ostream& request_stream)
    {
        for (const auto & kv : request_header) {
            if (used_field.find(kv.first) != used_field.end()) {
                continue;
            }
            request_stream << kv.first << ": " << kv.second << "\r\n";
        }
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
        discard(m_request);
        // NOTE 逻辑上，POST，GET，这两种方法，应该公用一个request-generator
        std::ostream request_stream(&m_request);
        auto url_info = ss1x::util::url::split_port_auto(get_url());

        if (m_proxy_hostname.empty()) {
            request_stream << "GET " << std::get<3>(url_info) << " HTTP/";
        }
        else {
            request_stream << "GET " << this->get_url() << " HTTP/";
        }

        if (!m_request_headers.http_version.empty()) {
            int version_major = -1;
            int version_minor = -1;
            int offset = -1;
            if (std::sscanf(m_request_headers.http_version.c_str(), "%d.%d%n",
                            &version_major, &version_minor, &offset) != 2 ||
                version_major <= 0 || version_minor < 0 ||
                size_t(offset) != m_request_headers.http_version.length())
            {
                SSS_POSITION_THROW(
                    std::runtime_error,
                    "error parssing user supliment http-version with ",
                    sss::raw_string(m_request_headers.http_version));
            }
            COLOG_DEBUG("http_version", m_request_headers.http_version);
            request_stream << m_request_headers.http_version;
        }
        else {
            // NOTE HTTP/1.1 可能需要支持
            // Transfer-Encoding "chunked"
            // 特性，这需要特殊处理！先读取16进制的长度数字，然后读取该长度的数据。
            // 不过，如果中间发生了截断，怎么办？即，被m_responce本身的buffer阶段了……
            // 还有，如果chunk数，太大了怎么办？另外，读取chunk数，本身的时候，
            // 被截断了，怎么办？
            // 只能是状态机了！
            COLOG_DEBUG("http_version", "1.0");
            request_stream << "1.0";
        }
        request_stream << "\r\n";

        std::set<std::string> used_field;

        if (m_proxy_hostname.empty()) {
            request_stream << "Host: " << std::get<1>(url_info) << "\r\n";
        }
        else {
            request_stream << "Host: " << m_proxy_hostname << "\r\n";
        }
        used_field.insert("Host");
        // 另外，可能还需要根据html标签类型的不同，来限制可以获取的类型；
        // 比如img的话，就获取
        // Accept: image/webp,image/*,*/*;q=0.8
        // 当然，禁用，也是一个选择。
        requestStreamHelper(used_field, m_request_headers, request_stream, "Accept", "text/html, application/xhtml+xml, */*");
        // NOTE 部分网站，比如 http://i.imgur.com/lYQgi0R.gif 必须要提供 (Accept-Encoding "gzip, deflate, sdch") 参数；
        // 不然，无法正常从图床获取图片，而是给你一个frame，再显示图片。
        // requestStreamHelper(used_field, m_request_headers, request_stream, "Accept-Encoding", "gzip, deflate, sdch");
        // 参考： https://imququ.com/post/vary-header-in-http.html
        requestStreamHelper(used_field, m_request_headers, request_stream, "Accept-Encoding", "gzip, deflate, sdch");
        processRequestCookie(request_stream, std::get<1>(url_info), std::get<3>(url_info));
        used_field.insert("Cookie");
        requestStreamHelper(used_field, m_request_headers, request_stream, "User-Agent", USER_AGENT_DEFAULT);
        // NOTE Connection 选项 等于 close和keep-alive的区别在于，keep-alive的时候，服务器端，不会主动关闭通信，也就是没有eof传来。
        // 这需要客户端，自动分析包大小，进行消息拆分。
        // 比如，分析：Content-Length: 4376 字段
        requestStreamHelper(used_field, m_request_headers, request_stream, "Connection", "close");
        requestStreamDumpRest(used_field, m_request_headers, request_stream);
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

        // 复制到新的streambuf中处理首行http状态, 如果不是http状态行, 那么将保
        // 持m_response中的内容,这主要是为了兼容非标准http服务器直接向客户端发
        // 送文件的需要, 但是依然需要以malformed_status_line通知用户,
        // malformed_status_line并不意味着连接关闭, 关于m_response中的数据如何
        // 处理, 由用户自己决定是否读取, 这时, 用户可以使用
        // read_some/async_read_some来读取这个链接上的所有数据.
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
        //
        // 另外，大部分浏览器，处理header的时候，都是定义一个最大空间。比如2048；
        // 这里的话，可以一个kv，限制最大长度是1024；然后可以有256个kv值对；
        // 等等；

        m_response_headers.clear();

        boost::asio::async_read_until(
            m_socket, m_response, "\r\n",
            boost::bind(&proxy_tunnel_client::handle_read_header, this,
                        boost::asio::placeholders::bytes_transferred,
                        boost::asio::placeholders::error));
    }

    void processHeader(ss1x::http::Headers& headers, sss::string_view head_line_view)
    {
        if (!head_line_view.empty() && !head_line_view.is_end_with("\r\n")) {
            SSS_POSITION_THROW(std::runtime_error, "not end with \"\\r\\n\"");
        }
        auto pos = 0;

        while (!head_line_view.empty()) {
            auto end_pos = head_line_view.find("\r\n", pos);
            if (end_pos == sss::string_view::npos) {
                // NOTE someting wrong
                break;
            }
            auto line = head_line_view.substr(0, end_pos);
            if (line.empty()) {
                // 说明，传入的是空行
                break;
            }
            head_line_view = head_line_view.substr(end_pos + 2);
            size_t colon_pos = line.find(':');

            if (colon_pos == std::string::npos) {
                // NOTE someting wrong
                continue;
            }
            size_t value_beg =
                line.find_first_not_of("\t ", colon_pos + 1);
            if (value_beg == std::string::npos) {
                value_beg = line.length();
            }
            // NOTE descard the last '\r'
            size_t len = line.length() - value_beg;
            auto key = line.substr(0, colon_pos).to_string();
            auto value = line.substr(value_beg, len).to_string();
            // NOTE 多值的key，通过"\r\n"为间隔，串接在一起。
            // TODO 或许，应该用vector来保存header；
            if (!headers[key].empty()) {
                headers[key].append("\r\n");
            }
            headers[key].append(value);

            if (key == "Content-Length") {
                m_response_content_length = sss::string_cast<uint32_t>(value);
            }

            COLOG_DEBUG(sss::raw_string(line));
        }
    }

    // void processHeader(ss1x::http::Headers& headers, std::istream& response_stream)
    // {
    //     std::string header;

    //     while (std::getline(response_stream, header) && header != "\r") {
    //         size_t colon_pos = header.find(':');
    //         if (header.back() == '\r') {
    //             header.pop_back();
    //         }
    //         if (colon_pos == std::string::npos) {
    //             continue;
    //         }
    //         size_t value_beg =
    //             header.find_first_not_of("\t ", colon_pos + 1);
    //         if (value_beg == std::string::npos) {
    //             value_beg = header.length();
    //         }
    //         // NOTE descard the last '\r'
    //         size_t len = header.length() - value_beg;
    //         std::string key = header.substr(0, colon_pos);
    //         std::string value = header.substr(value_beg, len);
    //         headers[key] = value;

    //         // NOTE TODO 或许，应该用vector来保存header；
    //         COLOG_DEBUG(sss::raw_string(header));
    //     }
    // }

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

        // avhttp中，是用一个std::string，作为临时buffer，不断读取字符，直到读取到仅"\r\n"
        // 的header结束标记为止；
        // 之后，才一股脑，完成header的解析。
        // 这确实是一种，逃避"\r\n"状态拆分的方法，虽然看起来好像会不断分配内存……
        // 而且，内存没有复用。
        //
        // /home/sarrow/Sources/avhttp/include/avhttp/impl/http_stream.ipp:2065
        //
        // template <typename Handler>
        // void http_stream::handle_header(Handler handler,
        //  std::string header_string, int bytes_transferred, const boost::system::error_code& err)
        // 
        // 注意，函数接口中，header_string是值传递！
        //
        // 初始调用形如：
        //
		// // 异步读取所有Http header部分.
		// boost::asio::async_read_until(m_sock, m_response, "\r\n",
		// 	boost::bind(&http_stream::handle_header<Handler>,
		// 		this, handler, std::string(""),
		// 		boost::asio::placeholders::bytes_transferred,
		// 		boost::asio::placeholders::error
		// 	)
		// );
        if (bytes_transferred > 2) {
            auto line = cast_string_view(m_response).substr(0, bytes_transferred);
            if (line.is_end_with("\r\n")) {
                processHeader(m_response_headers, line);
                m_response.consume(line.size());
                // fall through
            }
            else if (line.is_end_with("\r")) {
                boost::asio::async_read_until(
                    m_socket, m_response, "\n",
                    boost::bind(&proxy_tunnel_client::handle_read_header, this,
                                boost::asio::placeholders::bytes_transferred,
                                boost::asio::placeholders::error));
                return;
            }

            boost::asio::async_read_until(
                m_socket, m_response, "\r\n",
                boost::bind(&proxy_tunnel_client::handle_read_header, this,
                            boost::asio::placeholders::bytes_transferred,
                            boost::asio::placeholders::error));
            return;
            // NOTE 如果header的单行，都会"爆栈"，那么说明有"攻击"行为。
        }
        else if (bytes_transferred < 2) {
            COLOG_ERROR("left byts count :", bytes_transferred);
            return;
        }

        // NOTE 此时 bytes_transferred == 2并且，responce的buf中，必须为"\r\n"。
        m_response.consume(2);

        COLOG_DEBUG(m_response_headers);

        bool redirect = false;

        switch (this->header().status_code) {
            // NOTE FIXME 301,302这里有种情况没有考虑到——就是明明是跳转，
            // 但是没有提供Location(或者因为种种原因，未获取到该值)
            case 301:
            case 302:
                {
                    const auto it = m_response_headers.find("Location");
                    if (it != m_response_headers.end()) {
                        if (!it->second.empty() && it->second != this->get_url()) {
                            redirect = true;
                            auto newLocation = ss1x::util::url::full_of_copy(it->second, this->get_url());
                            COLOG_INFO(SSS_VALUE_MSG(newLocation));
                            this->m_redirect_urls.push_back(newLocation);
                        }
                        else {
                            COLOG_ERROR(m_response_headers);
                            COLOG_ERROR(it->second, " invalid");
                            set_error_code(ss1x::errc::invalid_redirect);
                            return;
                        }
                    }
                    else {
                        COLOG_ERROR("new Location can not be found.");
                        set_error_code(ss1x::errc::redirect_not_found);
                        return;
                    }
                }
                break;

            case 200:
                if (m_response_headers.has_kv("Transfer-Encoding", "chunked")) { // 将chunked处理，移动到跳转的后面
                    this->m_chunked_transfer = true;
                    COLOG_DEBUG(SSS_VALUE_MSG(m_chunked_transfer));
                }
                break;

            default:
                break;
        }

        if (redirect) {
            if (had_exceed_max_redirect()) {
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

        if (m_onContent && m_response_headers.has("Content-Encoding")) {
            m_is_gzip = true;
            const auto& content_encoding = m_response_headers.get("Content-Encoding");
            if (content_encoding == "gzip") {
                m_gzstream.reset(new ss1x::gzstream(ss1x::gzstream::mt_gzip));
            }
            else if (content_encoding == "zlib") {
                m_gzstream.reset(new ss1x::gzstream(ss1x::gzstream::mt_zlib));
            }
            else if (content_encoding == "deflate") {
                m_gzstream.reset(new ss1x::gzstream(ss1x::gzstream::mt_deflate));
            }
            else {
                m_is_gzip = false;
            }

            if (m_gzstream) {
                m_gzstream->set_on_avail_out(m_onContent);
            }
        }

        if (m_chunked_transfer) {
            boost::asio::async_read_until(
                m_socket, m_response, "\r\n",
                boost::bind(&proxy_tunnel_client::handle_read_chunk_head, this,
                            boost::asio::placeholders::bytes_transferred,
                            boost::asio::placeholders::error));
        }
        else {
            // Write whatever content we already have to output.
            if (m_response.size() && m_response_content_length != -1) {
                m_response_content_length -= m_response.size();
            }
            if (m_response.size() > 0 && this->header().status_code == 200) {
                COLOG_DEBUG(SSS_VALUE_MSG(m_response.size()));
                consume_content(m_response);
            }
            else {
                discard(m_response);
            }

            if (!m_response_content_length) {
                return;
            }
            // NOTE 如果正文过短的话，可能到这里，已经读完socket缓存了。
            // Start reading remaining data until EOF.
            boost::asio::async_read(
                m_socket, m_response, boost::asio::transfer_at_least(1),
                boost::bind(&proxy_tunnel_client::handle_read_content, this,
                            boost::asio::placeholders::bytes_transferred,
                            boost::asio::placeholders::error));
        }
    }

    void handle_read_chunk_head(int bytes_transferred, const boost::system::error_code& err)
    {
        COLOG_DEBUG(pretty_ec(err), bytes_transferred, "out of", m_response.size());
        if (bytes_transferred <= 0) {
            // NOTE hand-made eof mark
            set_error_code(boost::asio::error::eof);
            return;
        }

        int chunk_size = -1;
        boost::asio::streambuf::const_buffers_type::const_iterator begin(
            m_response.data().begin());
        const char* ptr = boost::asio::buffer_cast<const char*>(*begin);
        sss::string_view sv(ptr, bytes_transferred);
        int offset = -1;
        int ec = std::sscanf(ptr, "%x%n", &chunk_size, &offset);
        if (ec != 1) {
            SSS_POSITION_THROW(std::runtime_error,
                               "parse x-digit-number error: ",
                               sss::raw_string(sv));
        }
        // NOTE 标准允许chunk_size的16进制字符后面，跟若干padding用的空格字符！
        bool is_all_space =
            sss::is_all(ptr + offset, ptr + bytes_transferred - 2,
                        [](char ch) -> bool { return ch == ' '; });
        if (!is_all_space) {
            SSS_POSITION_THROW(std::runtime_error,
                               "unexpect none-blankspace trailing byte: ",
                               sss::raw_string(sss::string_view(
                                   ptr + offset,
                                   bytes_transferred - 2 - offset)));
        }

        bool is_end_with_crlf = sv.is_end_with("\r\n");
        if (!is_end_with_crlf) {
            SSS_POSITION_THROW(std::runtime_error,
                               "unexpect none-crlf trailing bytes: ",
                               sss::raw_string(sv));
        }

        m_chunk_size = chunk_size + 2;
        COLOG_DEBUG(SSS_VALUE_MSG(chunk_size), " <- ", sss::raw_string(sv));
        discard(m_response, bytes_transferred);
        if (!chunk_size) {
            COLOG_DEBUG(SSS_VALUE_MSG(chunk_size));
            set_error_code(boost::asio::error::eof);
            return;
        }

        if (err) {
            // NOTE boost::asio::error::eof 打印输出 asio.misc:2
            m_has_eof = (err == boost::asio::error::eof);
            set_error_code(err);
            return;
        }

        if (m_is_gzip && m_gzstream) {
            // NOTE 这样做，是否有必要？
            m_gzstream->init();
        }

        boost::asio::async_read(
            m_socket, m_response, boost::asio::transfer_exactly(m_chunk_size),
            boost::bind(&proxy_tunnel_client::handle_read_content, this,
                        boost::asio::placeholders::bytes_transferred,
                        boost::asio::placeholders::error));
    }

    void http_post(const std::string& server, int port, const std::string& path,
                   const std::string& postParams)
    {
        COLOG_DEBUG(sss::raw_string(server), port, sss::raw_string(path));
        std::ostream request_stream(&m_request);
        // NOTE 可以从 m_response_headers 中，构造http-head。
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

#if USE_509
            m_socket.get_ssl_socket().set_verify_callback(boost::bind(
                    &proxy_tunnel_client::verify_certificate, this, _1, _2));
            // TODO
            // if (is_certificate()) {
            //     ...
            // }
            // else {
            //     m_ssl_stream->set_verify_mode(boost::asio::ssl::context::verify_none);
            // }
#else
            boost::system::error_code ec;
            std::string host = std::get<1>(ss1x::util::url::split_port_auto(get_url()));

            // http://stackoverflow.com/questions/35387482/security-consequences-due-to-setting-set-verify-modeboostasiosslverify-n
            m_socket.get_ssl_socket().set_verify_mode(boost::asio::ssl::verify_none);
			m_socket.get_ssl_socket().set_verify_callback(
				boost::asio::ssl::rfc2818_verification(host), ec);

			if (ec)
			{
                COLOG_ERROR("Set verify callback \'" , host, "\', error message \'" , ec.message() , "\'");
				return;
			}
#endif

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
            async_request();
        }
    }

    void handle_handshake(const boost::system::error_code& err)
    {
        COLOG_DEBUG(pretty_ec(err));
        if (err) {
            set_error_code(err);
            return;
        }
        // const char* header =
        //     boost::asio::buffer_cast<const char*>(m_request.data());
        // COLOG_DEBUG(SSS_VALUE_MSG(header));

        async_request();
    }

    void handle_read_content(int bytes_transferred, const boost::system::error_code& err)
    {
        COLOG_DEBUG(pretty_ec(err), bytes_transferred, "out of", m_response.size());
        if (bytes_transferred <= 0) {
            // NOTE hand-made eof mark
            set_error_code(boost::asio::error::eof);
            return;
        }

        // Write all of the data that has been read so far.
        bool is_end = (!m_chunked_transfer &&
                       (m_response_content_length == bytes_transferred ||
                        this->is_end_chunk(m_response))) ||
                      (m_chunked_transfer && !m_chunk_size);
        if (m_onContent && this->header().status_code == 200) {
            consume_content(m_response, bytes_transferred);
        }
        else {
            discard(m_response, bytes_transferred);
        }
        COLOG_DEBUG(SSS_VALUE_MSG(m_response.size()));

        if (is_end) {
            COLOG_DEBUG("is_end_chunk");
            set_error_code(boost::asio::error::eof);
            return;
        }

        if (err) {
            // NOTE boost::asio::error::eof 打印输出 asio.misc:2
            m_has_eof = (err == boost::asio::error::eof);
            set_error_code(err);
            if (!m_chunk_size) {
                return;
            }
            if (m_has_eof && m_chunk_size) {
                if (m_onContent && this->header().status_code == 200) {
                    consume_content(m_response, m_chunk_size);
                }
                else {
                    discard(m_response);
                }
                // m_response.commit(m_chunk_size + 5);
            }
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
        //
        // NOTE
        // 
        // async_read(sock, buffer(size), handler)是读满size字节再调用handdler；
        // asycn_read_some() 是读读一点就调用；
        // 那么async_read + boost::asio::transfer_at_least(1) 呢？
        //
        // 当然，遇到错误(包括eof)，都会调用……
        if (m_chunked_transfer) {
            if (m_chunk_size) {
                boost::asio::async_read(
                    m_socket, m_response, boost::asio::transfer_exactly(m_chunk_size),
                    boost::bind(&proxy_tunnel_client::handle_read_content, this,
                                boost::asio::placeholders::bytes_transferred,
                                boost::asio::placeholders::error));
            }
            else {
                boost::asio::async_read_until(
                    m_socket, m_response, "\r\n",
                    boost::bind(&proxy_tunnel_client::handle_read_chunk_head, this,
                                boost::asio::placeholders::bytes_transferred,
                                boost::asio::placeholders::error));
            }
        }
        else {
            if (!m_response_content_length) {
                return;
            }
            boost::asio::async_read(
                m_socket, m_response, boost::asio::transfer_at_least(1),
                boost::bind(&proxy_tunnel_client::handle_read_content, this,
                            boost::asio::placeholders::bytes_transferred,
                            boost::asio::placeholders::error));
        }
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
    void processRequestCookie(std::ostream& request_stream, const std::string& server, const std::string& path)
    {
        if (m_onRequestCookie) {
            std::string cookie = m_onRequestCookie(server, path);
            if (!cookie.empty()) {
                COLOG_INFO(server, path, cookie);
                request_stream << "Cookie: " << cookie << "\r\n";
            }
        }
    }

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
        // NOTE m_chunk_size 包括了结尾的"\r\n"!
        // 如果 bytes_transferred 正好等于 m_chunk_size，说明，这次刚好读到chunk结束
        // 如果小于m_chunk_size，则都说明，还有数据。
        // 并且，最后两个字节，需要忽略！
        COLOG_DEBUG(bytes_transferred, "out of", responce.size());
        if (m_onContent) {
            std::size_t size = responce.size();
            if (bytes_transferred > 0 && size > std::size_t(bytes_transferred)) {
                size = bytes_transferred;
            }
            boost::asio::streambuf::const_buffers_type::const_iterator begin(
                responce.data().begin());
            const char* ptr = boost::asio::buffer_cast<const char*>(*begin);
            sss::string_view sv(ptr, size);
            if (m_chunked_transfer) {
                switch (m_chunk_size - bytes_transferred) {
                    case 0:
                        {
                            int trail_len = std::min(2, bytes_transferred);
                            sss::string_view trail(
                                sss::string_view("\r\n").substr(2 - trail_len));
                            if (!sv.is_end_with(trail)) {
                                SSS_POSITION_THROW(std::runtime_error,
                                                   "chunked data not end with ",
                                                   sss::raw_string(trail), ", but ",
                                                   sss::raw_string(sv));
                            }
                            sv.remove_suffix(trail_len);
                            m_chunk_size = 0;
                        }
                        break;

                    case 1:
                        {
                            int trail_len = std::min(1, bytes_transferred);
                            sss::string_view trail(
                                sss::string_view("\r\n").substr(2 - trail_len));
                            if (!sv.is_end_with(trail)) {
                                SSS_POSITION_THROW(std::runtime_error,
                                                   "chunked data not end with ",
                                                   sss::raw_string(trail), ", but ",
                                                   sss::raw_string(sv));
                            }
                            sv.remove_suffix(trail_len);
                            m_chunk_size = 1;
                        }
                        break;

                    default:
                        if (bytes_transferred > m_chunk_size) {
                            SSS_POSITION_THROW(
                                std::runtime_error,
                                size_t(bytes_transferred), ">", m_chunk_size);
                        }
                        // bytes_transferred <= m_chunk_size - 2
                        m_chunk_size -= bytes_transferred;
                        COLOG_DEBUG("left ", m_chunk_size);
                }
            }
            else {
                if (m_response_content_length != -1) {
                    m_response_content_length -= bytes_transferred;
                }
            }
            if (m_is_gzip) {
                if (!m_gzstream) {
                    SSS_POSITION_THROW(std::runtime_error, "m_is_gzip, m_gzstream not match!");
                }
                int covert_cnt = m_gzstream->inflate(sv);
                if (covert_cnt <= 0) {
                    SSS_POSITION_THROW(std::runtime_error, "inflate error!");
                }
            }
            else {
                m_onContent(sv);
            }
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
    //! https://en.wikipedia.org/wiki/Chunked_transfer_encoding
    //! http://blog.csdn.net/whatday/article/details/7571451
    bool                           m_chunked_transfer;
    int32_t                        m_chunk_size;
    int32_t                        m_response_content_length;
    // 最大跳转次数
    // 0 表示，不允许跳转；
    // 1 表示可以跳转一次；以此类推
    size_t                         m_max_redirect;

    bool                            m_is_gzip;
    std::unique_ptr<ss1x::gzstream> m_gzstream;

    boost::asio::streambuf         m_request;
    boost::asio::streambuf         m_response;

    boost::asio::deadline_timer    m_deadline;

    // TODO 也许，应该将header分开,request,responce
    // 不过，对于header()函数来说，用户一般只关心responce的header。
    ss1x::http::Headers            m_response_headers;
    ss1x::http::Headers            m_request_headers;
    boost::system::error_code      m_ec;
    std::string                    m_proxy_hostname;
    int                            m_proxy_port;
    std::vector<std::string>       m_redirect_urls;
    onResponce_t                   m_onContent;
    onEndCheck_t                   m_onEndCheck;
    CookieFunc_t                   m_onRequestCookie;
};
