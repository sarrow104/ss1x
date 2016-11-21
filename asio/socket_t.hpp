#pragma once

#include <stdexcept>
#include <memory>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>

#include <sss/util/PostionThrow.hpp>
#include <sss/colorlog.hpp>
#include <sss/debug/value_msg.hpp>

namespace ss1x {
namespace detail {
template <typename T>
constexpr T const& constexpr_max(T const& a, T const& b)
{
    return a > b ? a : b;
}

struct socket_t {
private:
    enum status_t { st_NONE, st_NORMAL, st_SSL };

    status_t m_status;

    // typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket_t;
    typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket&> ssl_socket_t;
    typedef boost::asio::ip::tcp::socket basic_socket_t;
    basic_socket_t m_socket;
    std::unique_ptr<ssl_socket_t> m_ssl_stream;

public:
    explicit socket_t(boost::asio::io_service& io_service): m_socket(io_service)
    {
    }
    socket_t(boost::asio::io_service& io_service,
             boost::asio::ssl::context& ctx)
        : m_socket(io_service), m_ssl_stream(new ssl_socket_t(m_socket, ctx))
    {
        // m_ssl_stream = utility::details::make_unique<boost::asio::ssl::stream<boost::asio::ip::tcp::socket &>>(m_socket, ssl_context);
    }

    ~socket_t() = default;

    void upgrade_to_ssl(boost::asio::ssl::context& ctx)
    {
        COLOG_DEBUG("from ", &ctx);
        m_ssl_stream.reset(new ssl_socket_t(m_socket, ctx));
    }
    
    bool is_ssl() const {
        COLOG_DEBUG("is_ssl = ", bool(m_ssl_stream));
        return bool(m_ssl_stream);
    }
    operator const void*() const { return reinterpret_cast<const void*>(&m_socket); }
    basic_socket_t& get_socket()
    {
        return m_socket;
    }
    ssl_socket_t& get_ssl_socket()
    {
        if (!m_ssl_stream) {
            SSS_POSITION_THROW(std::runtime_error, "need ssl socket here");
        }
        return *m_ssl_stream;
    }

    boost::asio::ip::tcp::socket::lowest_layer_type& lowest_layer()
    {
        if (m_ssl_stream) {
            return get_ssl_socket().lowest_layer();
        }
        else {
            return get_socket().lowest_layer();
        }
    }

    template <typename MutableBufferSequence>
    std::size_t read_some(const MutableBufferSequence& buffers,
                          boost::system::error_code& ec)
    {
        if (m_ssl_stream) {
            return get_ssl_socket().read_some(buffers);
        }
        else {
            return get_socket().read_some(buffers, ec);
        }
    }

    template <typename MutableBufferSequence>
    std::size_t read_some(const MutableBufferSequence& buffers)
    {
        if (m_ssl_stream) {
            return get_ssl_socket().read_some(buffers);
        }
        else {
            return get_socket().read_some(buffers);
        }
    }

    template <typename MutableBufferSequence, typename ReadHandler>
    void async_read_some(const MutableBufferSequence& buffers,
                         ReadHandler&& handler)
    {
        if (m_ssl_stream) {
            get_ssl_socket().async_read_some(buffers, handler);
        }
        else {
            get_socket().async_read_some(buffers, handler);
        }
    }

    template <typename MutableBufferSequence, typename ReadHandler>
    void async_read_until(const MutableBufferSequence& buffers,
                          const std::string &delim,
                          ReadHandler&& handler)
    {
        if (m_ssl_stream) {
            boost::asio::async_read_until(get_ssl_socket(), buffers, delim, handler);
        }
        else {
            boost::asio::async_read_until(get_socket(), buffers, delim, handler);
        }
    }

    template <typename ConstBufferSequence>
    std::size_t write_some(const ConstBufferSequence& buffers,
                           boost::system::error_code& ec)
    {
        if (m_ssl_stream) {
            return get_ssl_socket().write_some(buffers, ec);
        }
        else {
            return get_socket().write_some(buffers, ec);
        }
    }

    template <typename ConstBufferSequence>
    std::size_t write_some(const ConstBufferSequence& buffers)
    {
        if (m_ssl_stream) {
            return get_ssl_socket().write_some(buffers);
        }
        else {
            return get_socket().write_some(buffers);
        }
    }

    template <typename MutableBufferSequence, typename ReadHandler>
    void async_write_some(const MutableBufferSequence& buffers,
                          ReadHandler&& handler)
    {
        if (m_ssl_stream) {
            get_ssl_socket().async_write_some(buffers, handler);
        }
        else {
            get_socket().async_write_some(buffers, handler);
        }
    }
};
}  // namespace detail

} // namespace ss1x

