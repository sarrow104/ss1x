#pragma once

// client + deadline_timer, see
// http://www.boost.org/doc/libs/1_45_0/doc/html/boost_asio/example/timeouts/async_tcp_client.cpp

#include "socket_t.hpp"
#include "user_agent.hpp"
#include "stream.hpp"
#include "gzstream.hpp"
#include "brstream.hpp"
#include "echostream.hpp"

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
#include <sss/algorithm.hpp>

#include <ss1x/asio/headers.hpp>
#include <ss1x/asio/error_codec.hpp>
#include <ss1x/asio/utility.hpp>

inline sss::string_view cast_string_view(const boost::asio::streambuf& streambuf)
{
    return sss::string_view(boost::asio::buffer_cast<const char*>(streambuf.data()), streambuf.size());
}

//this->m_deadline.cancel();
//m_deadline.expires_from_now(boost::posix_time::seconds(5));
#define RET_ON_STOP  do { \
    if (m_stoped) {\
        return; \
    }\
    else {\
        this->resetTimer(); \
    }\
} while(false);

namespace ss1x {
namespace asio {

enum resource_type {
    res_type_any,
    res_type_json,
    res_type_xml,
    res_type_text,
    res_type_html,
    res_type_app,   // for binary data; eg.
    res_type_image, // for all image
};

} // namespace asio
} // namespace ss1x

namespace detail {

// NOTE error_code 模式下，用不着异常！
// class ExceptDeadlineTimer : public std::runtime_error {...};

inline bool &ss1x_asio_ptc_colog_status()
{
    static bool m_is_colog_on = false;
    return m_is_colog_on;
}

inline int &ss1x_asio_ptc_deadline_wait_secends()
{
    static int m_wait_seconds = 5 * 60;
    return m_wait_seconds;
}

} // namespace detail

static const sss::string_view CRLF{"\r\n"};
static const sss::string_view CRLF2{"\r\n" "\r\n"};

// NOTE 不定参数宏传递
// #define func(args...) inner_call(##args)
// 是gnu写法，过时了。
// 标准写法是... -> __VA_ARGS__
#define COLOG_TRIGER_INFO(...) \
    do {\
        if (detail::ss1x_asio_ptc_colog_status()) { \
            COLOG_INFO(__VA_ARGS__); \
        } \
    } while(false);

#define COLOG_TRIGER_ERROR(...) \
    do {\
        if (detail::ss1x_asio_ptc_colog_status()) { \
            COLOG_ERROR(__VA_ARGS__); \
        } \
    } while(false);

#define COLOG_TRIGER_DEBUG(...) \
    do {\
        if (detail::ss1x_asio_ptc_colog_status()) { \
            COLOG_DEBUG(__VA_ARGS__); \
        } \
    } while(false);

struct method_t
{
    enum E {
        E_NULL = 0,
        E_GET,
        E_POST,
        E_HEAD,
        E_PUT,
        E_TRACE,
        E_OPTIONS,
        E_DELETE,
        E_LOCK,
        E_MKCOL,
        E_COPY,
        E_MOVE
    };
    method_t (E e = E_NULL)
        : value(e)
    {}
    int value;
    const char * name() const {
        switch (this->value) {
            case E_GET:     return "GET";     break;
            case E_POST:    return "POST";    break;
            case E_HEAD:    return "HEAD";    break;
            case E_PUT:     return "PUT";     break;
            case E_TRACE:   return "TRACE";   break;
            case E_OPTIONS: return "OPTIONS"; break;
            case E_DELETE:  return "DELETE";  break;
            case E_LOCK:    return "LOCK";    break;
            case E_MKCOL:   return "MKCOL";   break;
            case E_COPY:    return "COPY";    break;
            case E_MOVE:    return "MOVE";    break;
            default: return "";
        }
    }
    operator bool() const {
        return value != E_NULL;
    }
    bool is(method_t::E v) const {
        return this->value == v;
    }
};

struct streambuf_view_t
{
    typedef boost::asio::streambuf value_type;
    explicit streambuf_view_t(const value_type& streambuf) : m_streambuf(streambuf)
    {
    }
    void print(std::ostream& o) const
    {
        sss::string_view buf = cast_string_view(m_streambuf);
        o << '[' << buf.size() << "; "
          << sss::raw_string(buf) << ']';
    }
    const value_type& m_streambuf;
};

inline std::ostream& operator << (std::ostream& o, const streambuf_view_t& b)
{
    b.print(o);
    return o;
}

inline streambuf_view_t streambuf_view(const boost::asio::streambuf& stream)
{
    return streambuf_view_t{stream};
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
size_t parse_http_status_line(Iterator begin, Iterator end,
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

    size_t consume_cnt = 0u;
    Iterator iter = begin;
    std::string reason;
    while (iter != end && state != fail)
    {
        char c = *iter++;
        ++consume_cnt;
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
            if (c == '\n')
            {
                // NOTE `iter` now points to one bytes after '\n'
                end = iter;
            }
            else
            {
                state = fail;
            }
            break;

        default:
            state = fail;
            break;
        }
    }

    if (state == fail)
    {
        status = 0;
    }
    COLOG_TRIGER_DEBUG(sss::raw_string(std::string(begin, end)), SSS_VALUE_MSG(consume_cnt), SSS_VALUE_MSG(state == fail));
    return consume_cnt;
}
} // namespace detail

//! http://www.cnblogs.com/lzjsky/archive/2011/05/12/2044460.html
// 异步调用专用。
// boost::asio::placeholders::error
// boost::asio::placeholders::bytes_transferred
class proxy_tunnel_client {
public:
    typedef int64_t               bytes_size_t;
    typedef std::function<void()> onFinished_t;
    typedef std::function<void(sss::string_view response)> onResponce_t;
    typedef std::function<bool(sss::string_view mark)> onEndCheck_t;
    typedef std::function<std::vector<std::string>(const std::string& url)>
             CookieFunc_t;
    typedef std::function<bool(const std::string& domain, const std::string& sever_cookie)>
             SetCookieFunc_t;

    static bool s_is_status_code_ok(int status_code)
    {
        return status_code / 100 == 2;
    }

public:
    proxy_tunnel_client(boost::asio::io_service& io_service,
                        boost::asio::ssl::context* p_ctx = nullptr)
        : m_resolver(io_service),
          m_socket(io_service),
          m_has_eof(false),
          m_is_chunked(false),
          m_expect_res_type(ss1x::asio::res_type_any),
          m_content_to_read(0),
          m_max_redirect(5),
          m_stoped(false),
          m_response(2048),
          m_deadline(io_service)
    {
        COLOG_TRIGER_DEBUG(SSS_VALUE_MSG(m_request.max_size()), SSS_VALUE_MSG(m_response.max_size()));
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
    void                    setSetCookieFunc(SetCookieFunc_t&& func) {
        m_onResponseSetCookie = std::move(func);
    }
    void                    setOnFinished(onFinished_t&& func) {
        m_onFinished = std::move(func);
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


    void http_get(
        const std::string& url,
        ss1x::asio::resource_type expect_type = ss1x::asio::res_type_any)
    {
        COLOG_TRIGER_DEBUG(SSS_VALUE_MSG(url));
        m_method = method_t(method_t::E_GET);
        m_expect_res_type = expect_type;
        this->initUrl(url);
        // m_redirect_urls.resize(0);
        // m_redirect_urls.push_back(url);
        http_get_impl();
    }

    void ssl_tunnel_get(
        const std::string& proxy_domain, int proxy_port,
        const std::string& url,
        ss1x::asio::resource_type expect_type = ss1x::asio::res_type_any)
    {
        COLOG_TRIGER_DEBUG(SSS_VALUE_MSG(proxy_domain), SSS_VALUE_MSG(proxy_port), SSS_VALUE_MSG(url), SSS_VALUE_MSG(m_request_headers));

        m_method = method_t(method_t::E_GET);
        m_expect_res_type = expect_type;
        this->initUrl(url);
        // m_redirect_urls.resize(0);
        // m_redirect_urls.push_back(url);
        m_proxy_hostname = proxy_domain;
        m_proxy_port     = proxy_port;
        ssl_tunnel_get_impl();
    }

    void http_post(
        const std::string& url,
        const std::string& content)
    {
        m_method = method_t(method_t::E_POST);
        this->initUrl(url);
        // m_redirect_urls.resize(0);
        // m_redirect_urls.push_back(url);
        m_post_content = content;
        http_get_impl();
    }

    void ssl_tunnel_post(
        const std::string& proxy_domain, int proxy_port,
        const std::string& url,
        const std::string& content,
        ss1x::asio::resource_type expect_type = ss1x::asio::res_type_any)
    {
        m_method = method_t(method_t::E_POST);
        m_expect_res_type = expect_type;
        this->initUrl(url);
        // m_redirect_urls.resize(0);
        // m_redirect_urls.push_back(url);
        m_proxy_hostname = proxy_domain;
        m_proxy_port     = proxy_port;
        m_post_content   = content;
        ssl_tunnel_get_impl();
    }

private:
    bool is_need_ssl(const decltype(ss1x::util::url::split_port_auto("")) & url_info)
    {
        return std::get<2>(url_info) == 443;
    }

    void http_get_impl()
    {
        // auto url_info = ss1x::util::url::split_port_auto(get_url());
        // auto& url_info = this->m_u;
        COLOG_TRIGER_INFO(
            SSS_VALUE_MSG(is_need_ssl(m_url_info)),
            SSS_VALUE_MSG(m_socket.is_ssl_enabled()),
            SSS_VALUE_MSG(m_socket.using_ssl()),
            SSS_VALUE_MSG(m_socket.has_ssl()));

        if (is_need_ssl(m_url_info)) {
            m_socket.upgrade_to_ssl();
        }
        else {
            m_socket.disable_ssl();
        }

        COLOG_TRIGER_INFO(
            SSS_VALUE_MSG(is_need_ssl(m_url_info)),
            SSS_VALUE_MSG(m_socket.is_ssl_enabled()),
            SSS_VALUE_MSG(m_socket.using_ssl()),
            SSS_VALUE_MSG(m_socket.has_ssl()));

        http_get_impl(std::get<1>(m_url_info), std::get<2>(m_url_info), std::get<3>(m_url_info));
    }

    // const boost::system::error_code& err
    // deadline_time 被调用的时候，error_code 就两种情况，
    // 要么非0值，boost::asio::error::operation_aborted
    //
    // 要么0值，success
    void check_deadline(const boost::system::error_code& err)
    {
        COLOG_TRIGER_INFO(SSS_VALUE_MSG(pretty_ec(err)));
        COLOG_TRIGER_INFO(SSS_VALUE_MSG(err == boost::asio::error::operation_aborted));
        // if (!err) {
        //     set_error_code(boost::asio::error::eof);
        //     return;
        // }
        if (err == boost::asio::error::operation_aborted) {
            return;
        }

        // Check whether the deadline has passed. We compare the deadline against
        // the current time since a new asynchronous operation may have moved the
        // deadline before this actor had a chance to run.
        if (m_deadline.expires_at() <= boost::asio::deadline_timer::traits_type::now())
        {
            // The deadline has passed. The socket is closed so that any outstanding
            // asynchronous operations are cancelled.
            // this->m_socket.get_socket().close();
            this->m_socket.get_socket().close();
            this->m_stoped = true;
            // this->m_deadline.cancel();
            set_error_code(ss1x::errc::deadline_timer_error);

            // There is no longer an active deadline. The expiry is set to positive
            // infinity so that the actor takes no action until a new deadline is set.
            // m_deadline.expires_at(boost::posix_time::pos_infin);
        }

        // Put the actor back to sleep.
        //m_deadline.async_wait(boost::bind(&proxy_tunnel_client::check_deadline,
        //                                  this,
        //                                  boost::asio::placeholders::error));
    }

    void http_get_impl(const std::string& server, int port, const std::string& path)
    {
        COLOG_TRIGER_INFO(sss::raw_string(server), port, sss::raw_string(path));

        this->startTimer();

        char port_buf[24];
        std::sprintf(port_buf, "%d", port <= 0 ? 80 : port);

        boost::asio::ip::tcp::resolver::query query(server, port_buf);
        COLOG_TRIGER_DEBUG(SSS_VALUE_MSG(server), SSS_VALUE_MSG(port));
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
        // auto url_info = ss1x::util::url::split_port_auto(get_url());
        if (is_need_ssl(m_url_info)) {
            m_socket.upgrade_to_ssl();
        }
        else {
            m_socket.disable_ssl();
        }
        COLOG_TRIGER_DEBUG(sss::raw_string(m_proxy_hostname), m_proxy_port);

        // int32_t -> 11 + 1bytes
        // int64_t -> 21 bytes

        // ceil(log_10(2^64)) + 1 = 21 bytes
        // ceil(log_10(2^32)) + 1) = 11 bytes
        char port_buf[24];
        std::sprintf(port_buf, "%d", m_proxy_port <= 0 ? 80 : m_proxy_port);

        this->startTimer();

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
        RET_ON_STOP;
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
        RET_ON_STOP;
        COLOG_TRIGER_DEBUG("");
        if (err)
        {
            COLOG_TRIGER_ERROR("Connect to http proxy \'", m_proxy_hostname, ":",
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
        RET_ON_STOP;
        COLOG_TRIGER_DEBUG(pretty_ec(err));
        if (err)
        {
            endpoint_iterator++;
            boost::asio::ip::tcp::resolver::iterator end;
            if (endpoint_iterator == end)
            {
                COLOG_TRIGER_ERROR("Connect to http proxy \'" , m_proxy_hostname , ":" , m_proxy_port ,
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
        discard(m_response); // clean response before receive data
        std::ostream request_stream(&m_request);
        // auto url_info = ss1x::util::url::split_port_auto(get_url());
        if (std::get<0>(m_url_info) == "https") {
            request_stream << "CONNECT " << std::get<1>(m_url_info) << ":" << std::get<2>(m_url_info) << " HTTP/1.1\r\n";
            request_stream << "Host: " << std::get<1>(m_url_info) << ":" << std::get<2>(m_url_info) << CRLF;
            request_stream << "User-Agent: " << USER_AGENT_DEFAULT << CRLF;
#if 1
            // NOTE 貌似 是否close，对于结果没啥影响
            // request_stream << "Connection: close\r\n\r\n";
            // NOTE 服务端可以主动发出 close动作，来提示，中断链接
            request_stream << "Proxy-Connection: ""keep-alive" << CRLF;
#endif
            request_stream << CRLF;

            COLOG_TRIGER_DEBUG(streambuf_view(m_request));

            // NOTE
            // copied from:
            // https://github.com/boostorg/beast/blob/bfd4378c133b2eb35277be8b635adb3f1fdaf09d/example/http/client/sync-ssl/http_client_sync_ssl.cpp#L67
            if(!::SSL_set_tlsext_host_name(m_socket.get_ssl_socket().native_handle(), std::get<1>(m_url_info).c_str()))
            {
                boost::system::error_code ec{static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
                throw boost::system::system_error{ec};
            }

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
        RET_ON_STOP;
        COLOG_TRIGER_DEBUG(pretty_ec(err));
        if (err)
        {
            COLOG_TRIGER_ERROR("Connect to http proxy ", sss::raw_string(m_proxy_hostname),
                        ":", m_proxy_port, "error message ", err.message());
            set_error_code(err);
            return;
        }
        boost::asio::async_read_until(sock, m_response, "\r\n\r\n",
          boost::bind(&proxy_tunnel_client::handle_https_proxy_status<Stream>,
                      this,
                      boost::ref(sock), boost::asio::placeholders::error));
    }

    template<typename Stream>
    void handle_https_proxy_status(Stream& sock, const boost::system::error_code& err)
    {
        RET_ON_STOP;
        COLOG_TRIGER_DEBUG(pretty_ec(err));
        if (err)
        {
            COLOG_TRIGER_ERROR("Connect to http proxy ", sss::raw_string(m_proxy_hostname),
                        ":", m_proxy_port, "error message ", err.message());
            set_error_code(err);
            return;
        }

        const char* line_beg =
            boost::asio::buffer_cast<const char*>(m_response.data());
        const char* line_end = line_beg + m_response.size();

        // 解析状态行，
        // 检查http状态码；
        int version_major = 0;
        int version_minor = 0;
        int status_code   = 0;

        size_t status_line_size
            = detail::parse_http_status_line(
                line_beg, line_end,
                version_major, version_minor, status_code);
        if (!status_line_size) {
            boost::system::error_code ec{ss1x::errc::malformed_status_line};
            this->set_error_code(ec);
            COLOG_TRIGER_ERROR("Connect to http proxy ", sss::raw_string(m_proxy_hostname),
                        ":", m_proxy_port, "error message ", err.message());
            return;
        }
        COLOG_TRIGER_DEBUG("proxy_status:http version: ", version_major, '.', version_minor);
        m_response.consume(status_line_size);

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
        RET_ON_STOP;
        (void)sock;
        COLOG_TRIGER_DEBUG(SSS_VALUE_MSG(bytes_transferred), pretty_ec(err));
        if (err)
        {
            COLOG_TRIGER_ERROR("Connect to http proxy ", sss::raw_string(m_proxy_hostname),
                        ":", m_proxy_port, "error message ", err.message());
            set_error_code(err);
            return;
        }

        // Process the response headers from proxy server.
        // std::istream response_stream(&m_response);
        m_response_headers.clear();
        std::size_t raw_header_length{0};
        processHeader(m_response_headers, cast_string_view(m_response), raw_header_length);
        // processHeader(m_response_headers, response_stream);

        m_response.consume(raw_header_length);

        // Write whatever content we already have to output.
        if (m_response.size() > 0 && s_is_status_code_ok(this->header().status_code)) { // 200
            COLOG_TRIGER_DEBUG(SSS_VALUE_MSG(m_response.size()));
            consume_content(m_response);
        }

        COLOG_TRIGER_DEBUG("Connect to http proxy \'", m_proxy_hostname, ":", m_proxy_port, "\'.");

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

    // NOTE if one filed value is empty, this will deprecated
    void requestStreamHelper(std::set<std::string>& used_field,
                             ss1x::http::Headers& request_header,
                             std::ostream& request_stream,
                             const std::string& field,
                             const std::string& default_value = "")
    {
        RET_ON_STOP;
        if (used_field.find(field) != used_field.end()) {
            return;
        }
        auto it = request_header.find(field);
        if (it != request_header.end())
        {
            if (!it->second.empty()) {
                sss::string_view fd_value = it->second;
                while(!fd_value.empty()) {
                    auto pos = fd_value.find(CRLF);
                    request_stream << field << ": " << fd_value.substr(0, pos) << CRLF;
                    if (pos == sss::string_view::npos)
                    {
                        break;
                    }
                    fd_value = fd_value.substr(pos + 2);
                }
            }
        }
        else if (!default_value.empty()) {
            request_stream << field << ": " << default_value << CRLF;
        }
        used_field.insert(field);
    }

    void requestStreamDumpRest(std::set<std::string>& used_field,
                               ss1x::http::Headers& request_header,
                               std::ostream& request_stream)
    {
        RET_ON_STOP;
        for (const auto & kv : request_header) {
            if (used_field.find(kv.first) != used_field.end()) {
                continue;
            }
            request_stream << kv.first << ": " << kv.second << CRLF;
        }
    }

    void handle_https_proxy_handshake(const boost::system::error_code& err)
    {
        RET_ON_STOP;
        COLOG_TRIGER_DEBUG(pretty_ec(err));
        if (err)
        {
            COLOG_TRIGER_ERROR("Connect to http proxy ", sss::raw_string(m_proxy_hostname),
                        ":", m_proxy_port, "error message ", err.message());
            set_error_code(err);
            return;
        }

        COLOG_TRIGER_DEBUG("Handshake to ", sss::raw_string(get_url()));

        COLOG_TRIGER_DEBUG("m_response.consume(", m_response.size(), ")");
        // 清空接收缓冲区.
        discard(m_response);

        // 发起异步请求.
        async_request();
    }

    void async_request()
    {
        RET_ON_STOP;
        COLOG_TRIGER_DEBUG("");
        // GET / HTTP/1.1
        // Host: www.google.co.jp
        // Accept: text/html, application/xhtml+xml, */*
        // User-Agent: avhttp/2.9.9
        // Connection: close

        discard(m_response);
        discard(m_request);
        // NOTE 逻辑上，POST，GET，这两种方法，应该公用一个request-generator
        std::ostream request_stream(&m_request);
        // auto url_info = ss1x::util::url::split_port_auto(get_url());

        if (m_proxy_hostname.empty()) {
            request_stream << m_method.name() << " " << std::get<3>(m_url_info) << " HTTP/";
        }
        else {
            request_stream << m_method.name() << " " << this->get_url() << " HTTP/";
        }

        if (!m_request_headers.http_version.empty()) {
            int version_major = -1;
            int version_minor = -1;
            int offset        = -1;
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
            COLOG_TRIGER_DEBUG("http_version", m_request_headers.http_version);
            request_stream << m_request_headers.http_version;
        }
        else {
            // NOTE HTTP/1.1 可能需要支持
            // Transfer-Encoding "chunked"
            // 特性，这需要特殊处理！先读取16进制的长度数字，然后读取该长度的数据。
            // 不过，如果中间发生了截断，怎么办？即，被m_response本身的buffer阶段了……
            // 还有，如果chunk数，太大了怎么办？另外，读取chunk数，本身的时候，
            // 被截断了，怎么办？
            // 只能是状态机了！
            const char * http_version = "1.1";
            COLOG_TRIGER_DEBUG("http_version", http_version);
            request_stream << http_version;
        }
        request_stream << CRLF;

        std::set<std::string> used_field;

        // NOTE Host 需要手动拼凑
        if (m_proxy_hostname.empty()) {
            request_stream << "Host: " << std::get<1>(m_url_info) << CRLF;
        }
        else {
            request_stream << "Host: " << m_proxy_hostname << CRLF;
        }
        used_field.insert("Host");

        requestStreamHelper(used_field, m_request_headers, request_stream, "User-Agent", USER_AGENT_DEFAULT);
        // 另外，可能还需要根据html标签类型的不同，来限制可以获取的类型；
        // 比如img的话，就获取
        // Accept: image/webp,image/*,*/*;q=0.8
        // 当然，禁用，也是一个选择。
        const char * accept_type = "text/html, application/xhtml+xml, */*";
        switch (m_expect_res_type)
        {
            case ss1x::asio::res_type_any:   accept_type = "*/*";                        break;
            case ss1x::asio::res_type_json:  accept_type = "application/json, */*";      break;
            case ss1x::asio::res_type_app:
            case ss1x::asio::res_type_xml:   accept_type = "application/xhtml+xml, */*"; break;
            case ss1x::asio::res_type_html:  accept_type = "text/html, */*";             break;
            case ss1x::asio::res_type_text:  accept_type = "text/plain, */*";            break;
            case ss1x::asio::res_type_image: accept_type = "image/webp, */*";            break;
        }
        requestStreamHelper(used_field, m_request_headers, request_stream, "Accept", accept_type);

        // NOTE 部分网站，比如 http://i.imgur.com/lYQgi0R.gif 必须要提供 (Accept-Encoding "gzip, deflate, sdch") 参数；
        // 不然，无法正常从图床获取图片，而是给你一个frame，再显示图片。
        // requestStreamHelper(used_field, m_request_headers, request_stream, "Accept-Encoding", "gzip, deflate, sdch");
        // 参考： https://imququ.com/post/vary-header-in-http.html
        // NOTE 2019-09-19 add `br`
        // NOTE 2019-10-04 `sdch' is for developed by google, and supported by chrome only; so ...
        // https://www.cnblogs.com/xingzc/p/9082035.html
        // https://cloud.tencent.com/developer/section/1189886
        requestStreamHelper(used_field, m_request_headers, request_stream, "Accept-Encoding", "gzip, deflate, br");

        if (m_request_headers.has("Referer")) {
            requestStreamHelper(used_field, m_request_headers, request_stream, "Referer", "");
        }

        if (m_method.is(method_t::E_POST)) {
            std::string CL_str = sss::cast_string(m_post_content.size());
            requestStreamHelper(used_field, m_request_headers, request_stream, "Content-Type", "application/x-www-form-urlencoded");
            requestStreamHelper(used_field, m_request_headers, request_stream, "Content-Length", CL_str);
        }

        if (m_request_headers.has("Cookie")) {
            requestStreamHelper(used_field, m_request_headers, request_stream, "Cookie", "");
        }
        else if (m_onRequestCookie) {
            auto cookies = m_onRequestCookie(this->get_url());
            int cookie_cnt = 0;
            for (auto& cookie : cookies) {
                COLOG_TRIGER_INFO(std::get<1>(m_url_info), std::get<3>(m_url_info), cookie);
                if (cookie.empty()) {
                    continue;
                }
                request_stream << "Cookie" << ": " << cookie << CRLF;
                ++cookie_cnt;
            }
            if (cookie_cnt) {
                used_field.insert("Cookie");
            }
        }

        // NOTE Connection 选项 等于 close和keep-alive的区别在于，keep-alive的时候，服务器端，不会主动关闭通信，也就是没有eof传来。
        // 这需要客户端，自动分析包大小，进行消息拆分。
        // 比如，分析：Content-Length: 4376 字段
        requestStreamHelper(used_field, m_request_headers, request_stream, "Connection", "close");
        requestStreamDumpRest(used_field, m_request_headers, request_stream);
        request_stream << CRLF;

        // 2017-12-25
        if (m_method.is(method_t::E_POST)) {
            request_stream << m_post_content << CRLF;
        }

        COLOG_TRIGER_DEBUG(streambuf_view(m_request));
        boost::asio::async_write(
            m_socket, m_request,
            boost::asio::transfer_exactly(m_request.size()),
            boost::bind(&proxy_tunnel_client::handle_request, this,
                        boost::asio::placeholders::error));
    }

    void handle_request(const boost::system::error_code& err)
    {
        RET_ON_STOP;
        COLOG_TRIGER_DEBUG(pretty_ec(err));
        if (err)
        {
            COLOG_TRIGER_ERROR("Send request, error message: ", err.message());
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
        RET_ON_STOP;
        COLOG_TRIGER_DEBUG(bytes_transferred, pretty_ec(err));
        if (err)
        {
            COLOG_TRIGER_ERROR("Read status line, error message: ", err.message());
            set_error_code(err);
            return;
        }

        // 非标准http服务器直接向客户端发送文件的需要, 但是依然需要以malformed_status_line通知用户,
        // malformed_status_line并不意味着连接关闭, 关于m_response中的数据如何
        // 处理, 由用户自己决定是否读取, 这时, 用户可以使用
        // read_some/async_read_some来读取这个链接上的所有数据.

        COLOG_TRIGER_DEBUG(streambuf_view(m_response));

        const char* line_beg =
            boost::asio::buffer_cast<const char*>(m_response.data());
        const char* line_end = line_beg + m_response.size();

        // 检查http状态码, version_major和version_minor是http协议的版本号.
        int version_major = 0;
        int version_minor = 0;
        int status_code   = 0;
        size_t status_line_size =
            detail::parse_http_status_line(
                line_beg, line_end, // in
                version_major, version_minor, status_code); // out

        if (status_code == 0)
        {
            COLOG_TRIGER_ERROR("Malformed status line");
            set_error_code(ss1x::errc::malformed_status_line);
            return;
        }
        this->header().status_code = status_code;

        COLOG_TRIGER_DEBUG(status_line_size, version_major, '.', version_minor, status_code);
        discard(m_response, status_line_size);

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

    void processResponseSetCookie(ss1x::http::Headers& headers)
    {
        if (headers.has("Set-Cookie") && m_onResponseSetCookie) {
            m_onResponseSetCookie(std::get<1>(m_url_info), headers.get("Set-Cookie"));
        }
    }

    void processResponseSetCookie2(const std::string& domain, std::string& cookie)
    {
        if (m_onResponseSetCookie) {
            m_onResponseSetCookie(domain, cookie);
        }
    }

    size_t processHeaderOnce(ss1x::http::Headers& headers, sss::string_view head_line_view)
    {
        auto pos = 0;
        size_t raw_header_length = 0;
        do {
            auto end_pos = head_line_view.find(CRLF, pos);
            if (end_pos == sss::string_view::npos) {
                this->set_error_code(ss1x::errc::malformed_status_line);
                break;
            }

            auto line = head_line_view.substr(0, end_pos);
            raw_header_length += line.size() + CRLF.size();
            if (line.empty()) {
                // this means the last "\r\n" encountered
                raw_header_length += CRLF.size();
                break;
            }

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
            auto key   = line.substr(0, colon_pos).to_string();
            auto value = line.substr(value_beg, len).to_string();
            // NOTE 多值的key，通过"\r\n"为间隔，串接在一起。
            // TODO 或许，应该用vector来保存header；
            if (key == "Set-Cookie") {
                this->processResponseSetCookie2(std::get<1>(m_url_info), value);
            }
            if (!headers[key].empty()) {
                headers[key].append("\r\n");
            }
            headers[key].append(value);

            if (key == "Content-Length") {
                m_content_to_read = sss::string_cast<uint32_t>(value);
            }

            COLOG_TRIGER_DEBUG(sss::raw_string(line));
        } while(false);

        return raw_header_length;
    }

    void processHeader(ss1x::http::Headers& headers, sss::string_view head_line_view, size_t& raw_header_length)
    {
        RET_ON_STOP;
        while (!head_line_view.empty()) {
            size_t cur_len = processHeaderOnce(headers, head_line_view);
            raw_header_length += cur_len;
            head_line_view = head_line_view.substr(cur_len);
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
    //         COLOG_TRIGER_DEBUG(sss::raw_string(header));
    //     }
    // }

    void handle_read_header(int bytes_transferred, const boost::system::error_code& err)
    {
        RET_ON_STOP;
        COLOG_TRIGER_DEBUG(bytes_transferred, pretty_ec(err));
        COLOG_TRIGER_DEBUG(streambuf_view(m_response));

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
        //  boost::bind(&http_stream::handle_header<Handler>,
        //      this, handler, std::string(""),
        //      boost::asio::placeholders::bytes_transferred,
        //      boost::asio::placeholders::error
        //  )
        // );

        auto line = cast_string_view(m_response);
        if (!line.is_begin_with(CRLF)) {
            auto header_len = processHeaderOnce(m_response_headers, line);
            m_response.consume(header_len);

            boost::asio::async_read_until(
                m_socket, m_response, "\r\n",
                boost::bind(&proxy_tunnel_client::handle_read_header, this,
                            boost::asio::placeholders::bytes_transferred,
                            boost::asio::placeholders::error));
            return;
            // NOTE 如果header的单行，都会"爆栈"，那么说明有"攻击"行为。
        }

        m_response.consume(CRLF.size());

        COLOG_TRIGER_DEBUG(SSS_VALUE_MSG(m_response_headers));

        // processResponseSetCookie(m_response_headers);

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
                            COLOG_TRIGER_INFO(SSS_VALUE_MSG(newLocation));
                            // this->m_redirect_urls.push_back(newLocation);
                            this->addRedirectUrl(newLocation);
                        }
                        else {
                            COLOG_TRIGER_ERROR(m_response_headers);
                            COLOG_TRIGER_ERROR(it->second, " invalid");
                            set_error_code(ss1x::errc::invalid_redirect);
                            return;
                        }
                    }
                    else {
                        COLOG_TRIGER_ERROR("new Location can not be found.");
                        set_error_code(ss1x::errc::redirect_not_found);
                        return;
                    }
                }
                break;

            default:
                // for 200,...206,400,403...
                if (m_response_headers.has_kv("Transfer-Encoding", "chunked")) { // 将chunked处理，移动到跳转的后面
                    this->m_is_chunked = true;
                    COLOG_TRIGER_DEBUG(SSS_VALUE_MSG(m_is_chunked));
                }
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

        if (m_onContent) {
            if (m_response_headers.has("Content-Encoding")) {
                const auto& content_encoding = m_response_headers.get("Content-Encoding");
                if (content_encoding == "gzip") {
                    m_stream.reset(new ss1x::gzstream(ss1x::gzstream::mt_gzip));
                }
                else if (content_encoding == "zlib") {
                    m_stream.reset(new ss1x::gzstream(ss1x::gzstream::mt_zlib));
                }
                else if (content_encoding == "deflate") {
                    m_stream.reset(new ss1x::gzstream(ss1x::gzstream::mt_deflate));
                }
                else if (content_encoding == "br")
                {
                    m_stream.reset(new ss1x::brstream);
                }
                else {
                    COLOG_ERROR("not support Content-Encoding ", content_encoding);
                }
            }
            else {
                m_stream.reset(new ss1x::echostream);
            }

            if (m_stream) {
                m_stream->set_on_avail_out(m_onContent);
            }
        }

        COLOG_TRIGER_DEBUG(SSS_VALUE_MSG(m_is_chunked));

        if (m_is_chunked) {
            boost::asio::async_read_until(
                m_socket, m_response, "\r\n",
                boost::bind(&proxy_tunnel_client::handle_read_chunk_head, this,
                            boost::asio::placeholders::bytes_transferred,
                            boost::asio::placeholders::error));
        }
        else {
            COLOG_TRIGER_DEBUG(SSS_VALUE_MSG(m_response.size()));
            // Write whatever content we already have to output.
            if (m_response.size() > 0) {
                consume_content(m_response);
            }

            if (!m_content_to_read) {
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

    // NOTE chunk head pattern: '\x'+ '\s'* '\r\n'
    void handle_read_chunk_head(int bytes_transferred, const boost::system::error_code& err)
    {
        RET_ON_STOP;
        COLOG_TRIGER_DEBUG(pretty_ec(err), bytes_transferred, "out of", m_response.size());
        if (bytes_transferred <= 0) {
            auto sv = cast_string_view(m_response);
            COLOG_TRIGER_DEBUG(SSS_VALUE_MSG(sv));
            // NOTE hand-made eof mark
            set_error_code(boost::asio::error::eof);
            return;
        }

        int chunk_size = -1;
        int offset     = -1;
        sss::string_view sv = cast_string_view(m_response);
        if (std::isxdigit(sv.front()))
        {
            int ec = std::sscanf(sv.data(), "%x%n", &chunk_size, &offset);
            if (ec != 1) {
                SSS_POSITION_THROW(std::runtime_error,
                                   "parse x-digit-number error: ",
                                   sss::raw_string(sv));
            }
            // NOTE 标准允许chunk_size的16进制字符后面，跟若干padding用的空格字符！
            while (sv[offset] == ' ')
            {
                ++offset;
            }
            sv = sv.substr(offset);
            if (!sv.is_begin_with(CRLF))
            {
                SSS_POSITION_THROW(std::runtime_error,
                                   "unexpect none-crlf trailing bytes: ",
                                   sss::raw_string(sv));
            }
            sv.remove_prefix(CRLF.size());
            offset += CRLF.size();
        }
        else
        {
            SSS_POSITION_THROW(std::runtime_error,
                               "parse x-digit-number error: ",
                               sss::raw_string(sv));
        }

        m_content_to_read = chunk_size + CRLF.size();
        COLOG_TRIGER_DEBUG(SSS_VALUE_MSG(chunk_size), " <- ", sss::raw_string(cast_string_view(m_response).substr(0, offset)));
        m_response.consume(offset);
        if (!chunk_size) {
            consume_content(m_response, CRLF.size());
            COLOG_TRIGER_DEBUG(SSS_VALUE_MSG(chunk_size));
            set_error_code(boost::asio::error::eof);
            return;
        }

        if (err) {
            // NOTE boost::asio::error::eof 打印输出 asio.misc:2
            m_has_eof = (err == boost::asio::error::eof);
            set_error_code(err);
            return;
        }

        boost::asio::async_read(
            m_socket, m_response, boost::asio::transfer_exactly(m_content_to_read),
            boost::bind(&proxy_tunnel_client::handle_read_content, this,
                        boost::asio::placeholders::bytes_transferred,
                        boost::asio::placeholders::error));
    }

    void http_post(const std::string& server, int port, const std::string& path,
                   const std::string& postParams)
    {
        RET_ON_STOP;
        COLOG_TRIGER_DEBUG(sss::raw_string(server), port, sss::raw_string(path));
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
        RET_ON_STOP;
        COLOG_TRIGER_DEBUG(pretty_ec(err));
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

            // boost::asio::ssl::context::no_tlsv1;
            // ssl::context ssl_context_{ssl::context::tls};
            // ssl_context_.set_default_verify_paths();
            // ssl_context_.add_verify_path("/opt/misc/certificates");
            //
            //! https://github.com/encode/httpx/issues/646
            //context = ssl.SSLContext(ssl.PROTOCOL_TLS)
            // context.options |= ssl.OP_NO_SSLv2
            // context.options |= ssl.OP_NO_SSLv3
            // context.options |= ssl.OP_NO_TLSv1
            // context.options |= ssl.OP_NO_TLSv1_1
            // context.options |= ssl.OP_NO_COMPRESSION
            // context.set_ciphers(DEFAULT_CIPHERS)
            //
            //! trouble-shooting wrong version number
            // https://github.com/openssl/openssl/issues/6289
            // curl -v https://testnet.binance.vision/api/v3/exchangeInfo
            //
            // NOTE this site use tls version 1.3!

            // http://stackoverflow.com/questions/35387482/security-consequences-due-to-setting-set-verify-modeboostasiosslverify-n
            m_socket.get_ssl_socket().set_verify_mode(boost::asio::ssl::verify_none);
            m_socket.get_ssl_socket().set_verify_callback(
                boost::asio::ssl::rfc2818_verification(host), ec);

            if (ec)
            {
                COLOG_TRIGER_ERROR("Set verify callback \'" , host, "\', error message \'" , ec.message() , "\'");
                return;
            }
#endif

        }
        boost::asio::async_connect(
            m_socket.lowest_layer(), endpoint_iterator,
            boost::bind(&proxy_tunnel_client::handle_connect, this,
                        boost::asio::placeholders::error));
    }

    void verify_certificate(bool preverified,
                            boost::asio::ssl::verify_context& ctx)
    {
        RET_ON_STOP;
        COLOG_TRIGER_DEBUG(SSS_VALUE_MSG(preverified));
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

        COLOG_TRIGER_DEBUG("Verifying ", subject_name);

        // return preverified;
    }

    void handle_connect(const boost::system::error_code& err)
    {
        RET_ON_STOP;
        COLOG_TRIGER_DEBUG(pretty_ec(err));
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
        RET_ON_STOP;
        COLOG_TRIGER_DEBUG(pretty_ec(err));
        if (err) {
            set_error_code(err);
            return;
        }
        // const char* header =
        //     boost::asio::buffer_cast<const char*>(m_request.data());
        // COLOG_TRIGER_DEBUG(SSS_VALUE_MSG(header));

        async_request();
    }

    void handle_read_content(int bytes_transferred, const boost::system::error_code& err)
    {
        RET_ON_STOP;
        (void)bytes_transferred;

        assert(bytes_transferred >= 0);

        COLOG_TRIGER_DEBUG(SSS_VALUE_MSG(m_response.size()), pretty_ec(err), SSS_VALUE_MSG(m_is_chunked), SSS_VALUE_MSG(m_content_to_read));

        int bytes_available
            = std::min<bytes_size_t>(m_response.size(), m_content_to_read);

        // FIXME NOTE the ending CRLF!
        if (m_onContent && s_is_status_code_ok(this->header().status_code)) { // 200
            consume_content(m_response, bytes_available);
        }
        else {
            // NOTE this will discard data from 404 response
            discard(m_response, bytes_available);
        }

        if (err && err != boost::asio::error::eof) {
            // NOTE boost::asio::error::eof 打印输出 asio.misc:2
            set_error_code(err);
        }

        if (m_content_to_read > 0) {
            boost::asio::async_read(
                m_socket, m_response, boost::asio::transfer_at_least(1),
                boost::bind(&proxy_tunnel_client::handle_read_content, this,
                            boost::asio::placeholders::bytes_transferred,
                            boost::asio::placeholders::error));
            return;
        }

        if (m_is_chunked) {
            boost::asio::async_read_until(
                m_socket, m_response, "\r\n",
                boost::bind(&proxy_tunnel_client::handle_read_chunk_head, this,
                            boost::asio::placeholders::bytes_transferred,
                            boost::asio::placeholders::error));
            return;
        }

        // NOTE here means noting to do
        set_error_code(boost::system::error_code{});

        // NOTE
        // async_read_until()，在当前buffer大小范围，如果都没有读取到终止标记串，
        // 则会返回 {asio.misc,3,"Element not found"} 这个错误号。
        //
        // 注意，此时 m_response.size() == m_response.max_size()
        // 即，m_response 被塞满了！
        //
        // 那么，如何安全地处理这种 async_read_until 动作呢？
        //
        // 首先，
        // 1. 需要空出buffer，才能继续读，以便获得足够用来判断的数据。
        // 2. 避免标记字符串，恰好处于分割位置；因为这样的话，将很难处理标记内存段的查找
        //
        // boost::asio::async_read_until(
        //     m_socket, m_response, "</html>",
        //     boost::bind(&proxy_tunnel_client::handle_read_content, this,
        //                 boost::asio::placeholders::bytes_transferred,
        //                 boost::asio::placeholders::error));
        //
        // 此时，看结尾部分，是否含有标记字符串的前缀即可——如果，标记字符串的前缀，
        // 就是当前 m_response 的结束部分，则有一定概率标记字符串被截断了；
        //
        // 此时，应该让consume掉，除了前缀部分的数据，然后继续async_read_until()查找标记即可
        //
        // 这样，就可以避免，在任意未知，切割标记字符串，问题得到了简化；
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
    void startTimer()
    {
        // NOTE 这里，用一个全局函数来修改默认值。
        COLOG_TRIGER_INFO(SSS_VALUE_MSG(detail::ss1x_asio_ptc_deadline_wait_secends()));
        // m_deadline.expires_from_now(boost::posix_time::seconds(detail::ss1x_asio_ptc_deadline_wait_secends()));
        m_deadline.expires_from_now(boost::posix_time::seconds(5));

        m_deadline.async_wait(boost::bind(&proxy_tunnel_client::check_deadline,
                                          this,
                                          boost::asio::placeholders::error));
    }

    void resetTimer()
    {
        m_deadline.cancel();
        this->startTimer();
    }

    void initUrl(const std::string& url)
    {
        m_redirect_urls.resize(0);
        this->addRedirectUrl(url);
    }

    void addRedirectUrl(const std::string& url)
    {
        m_redirect_urls.push_back(url);
        m_url_info = ss1x::util::url::split_port_auto(url);
        COLOG_TRIGER_DEBUG(SSS_VALUE_MSG(m_url_info));
    }

    void set_error_code(const boost::system::error_code& ec) {
        this->m_ec = ec;
        this->m_deadline.cancel();
        this->m_stoped = true;
        if (m_onFinished) {
            COLOG_DEBUG(ec);
            m_onFinished();
        }
    }
    void discard(boost::asio::streambuf& response, int bytes_transferred = 0)
    {
        if (bytes_transferred > 0) {
            // NOTE boost::asio::streambuf{} will consume at most .size() count bytes from streambuf
            m_content_to_read -= bytes_transferred;
            response.consume(std::size_t(bytes_transferred));
        }
        else {
            m_content_to_read -= response.size();
            response.consume(response.size());
        }
    }

    //
    void consume_content(boost::asio::streambuf& response, int bytes_transferred = 0)
    {
        // NOTE m_content_to_read 包括了结尾的"\r\n"!
        // 如果 bytes_transferred 正好等于 m_content_to_read，说明，这次刚好读到chunk结束
        // 如果小于m_content_to_read，则都说明，还有数据。
        // 并且，最后两个字节，需要忽略！
        auto old_to_read = m_content_to_read;

        std::size_t size = response.size();
        if (bytes_transferred <= 0)
        {
            bytes_transferred = size;
        }
        else
        {
            bytes_transferred = std::min<size_t>(bytes_transferred, size);
        }

        bytes_transferred = std::min<size_t>(m_content_to_read, bytes_transferred);

        if (m_onContent) {
            sss::string_view sv = cast_string_view(response).substr(0, bytes_transferred);
            if (m_is_chunked) {
                if (m_content_to_read >= bytes_transferred + bytes_size_t(CRLF.size()))
                {
                    // NOTE nothing
                }
                else if (m_content_to_read == bytes_transferred + 1)
                {
                    sv.remove_suffix(1);
                }
                else if (m_content_to_read == bytes_transferred + 0)
                {
                    sv.remove_suffix(2);
                }
                else if (m_content_to_read < bytes_transferred)
                {
                    SSS_POSITION_THROW(
                        std::runtime_error,
                        "to many bytes_transferred");
                }
            }

            assert(m_content_to_read >= bytes_transferred);
            m_content_to_read -= bytes_transferred;

            if (sv.size())
            {
                if (m_stream) {
                    boost::system::error_code ec;
                    int covert_cnt = m_stream->inflate(sv, &ec);
                    if (ec && ec.value() != Z_BUF_ERROR && covert_cnt <= 0) {
                        COLOG_TRIGER_ERROR(SSS_VALUE_MSG(ec));
                        SSS_POSITION_THROW(std::runtime_error, "inflate error!");
                        set_error_code(ec);
                    }
                }
                else {
                    m_onContent(sv);
                }
            }
        }

        response.consume(bytes_transferred);
        COLOG_TRIGER_DEBUG(SSS_VALUE_MSG(m_content_to_read), "<=", old_to_read, '-', bytes_transferred);
        if (old_to_read == m_content_to_read)
        {
            SSS_POSITION_THROW(std::runtime_error, "");
        }
    }

    bool is_end_chunk(const boost::asio::streambuf& buf) const
    {
        if (!m_onEndCheck) {
            return false;
        }
        sss::string_view sv = cast_string_view(buf);
        // return sv.find("</html>") != sss::string_view::npos;
        return m_onEndCheck(sv);
    }

private:
    boost::asio::ip::tcp::resolver m_resolver;
    ss1x::detail::socket_t         m_socket;
    // NOTE the other endpoint close the socket
    // the response stream may still has byte to read
    bool                           m_has_eof;
    //! https://en.wikipedia.org/wiki/Chunked_transfer_encoding
    //! http://blog.csdn.net/whatday/article/details/7571451
    bool                           m_is_chunked;
    ss1x::asio::resource_type      m_expect_res_type;

    // NOTE this means also chunked body size when `m_is_chunked == true`
    bytes_size_t                   m_content_to_read;
    // 最大跳转次数
    // 0 表示，不允许跳转；
    // 1 表示可以跳转一次；以此类推
    size_t                         m_max_redirect;

    bool                           m_stoped;
    std::unique_ptr<ss1x::stream>  m_stream;

    boost::asio::streambuf         m_request;
    boost::asio::streambuf         m_response;

    boost::asio::deadline_timer    m_deadline;

    // 不过，对于header()函数来说，用户一般只关心response的header。
    ss1x::http::Headers            m_response_headers;
    ss1x::http::Headers            m_request_headers;
    boost::system::error_code      m_ec;
    std::string                    m_proxy_hostname;
    int                            m_proxy_port;
    std::vector<std::string>       m_redirect_urls;
    decltype(ss1x::util::url::split_port_auto("")) m_url_info;

    method_t                       m_method;
    std::string                    m_post_content;
    onFinished_t                   m_onFinished;
    onResponce_t                   m_onContent;
    onEndCheck_t                   m_onEndCheck;
    CookieFunc_t                   m_onRequestCookie; // Cookie: ...
    SetCookieFunc_t                m_onResponseSetCookie; // Set-Cookie: ...
};
