#pragma once

#include <stdexcept>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>

#include <sss/util/PostionThrow.hpp>

namespace ss1x {
namespace detail {
template <typename T>
constexpr T const& constexpr_max(T const& a, T const& b)
{
    return a > b ? a : b;
}

struct socket_t {
private:
    typedef boost::asio::ip::tcp::socket basic_socket_t;
    typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket_t;
    char buf[detail::constexpr_max(sizeof(basic_socket_t),
                                   sizeof(ssl_socket_t))];
    enum status_t { st_NONE, st_NORMAL, st_SSL };
    status_t m_status;

public:
    socket_t() : m_status(st_NONE) {}
    explicit socket_t(boost::asio::io_service& io_service) : m_status(st_NONE)
    {
        this->init(io_service);
    }
    socket_t(boost::asio::io_service& io_service,
             boost::asio::ssl::context& ctx)
        : m_status(st_NONE)
    {
        this->init(io_service, ctx);
    }

    ~socket_t()
    {
        switch (m_status) {
            case st_SSL:
                reinterpret_cast<ssl_socket_t*>(buf)->~ssl_socket_t();
                break;

            case st_NORMAL:
                reinterpret_cast<basic_socket_t*>(buf)->~basic_socket_t();
                break;

            case st_NONE:
                break;
        }
    }
    void init(boost::asio::io_service& io_service)
    {
        if (m_status != st_NONE) {
            SSS_POSITION_THROW(std::runtime_error, "not st_NONE");
        }
        m_status = st_NORMAL;
        new (buf) basic_socket_t(io_service);
    }

    void init(boost::asio::io_service& io_service,
              boost::asio::ssl::context& ctx)
    {
        if (m_status != st_NONE) {
            SSS_POSITION_THROW(std::runtime_error, "not st_NONE");
        }
        m_status = st_SSL;
        new (buf) ssl_socket_t(io_service, ctx);
    }
    bool is_ssl() const { return m_status == st_SSL; }
    operator void*() const { return reinterpret_cast<void*>(m_status); }
    basic_socket_t& get_socket()
    {
        if (m_status != st_NORMAL) {
            SSS_POSITION_THROW(std::runtime_error, "need normal socket here");
        }
        return *reinterpret_cast<basic_socket_t*>(buf);
    }
    ssl_socket_t& get_ssl_socket()
    {
        if (m_status != st_SSL) {
            SSS_POSITION_THROW(std::runtime_error, "need ssl socket here");
        }
        return *reinterpret_cast<ssl_socket_t*>(buf);
    }

    boost::asio::ip::tcp::socket::lowest_layer_type& lowest_layer()
    {
        switch (m_status) {
            case st_NONE:
                SSS_POSITION_THROW(std::runtime_error, "st_NONE");
                break;
            case st_NORMAL:
                return get_socket().lowest_layer();

            case st_SSL:
                return get_ssl_socket().lowest_layer();
        }
    }

    template <typename MutableBufferSequence>
    std::size_t read_some(const MutableBufferSequence& buffers,
                          boost::system::error_code& ec)
    {
        switch (m_status) {
            case st_NONE:
                SSS_POSITION_THROW(std::runtime_error, "st_NONE");
                break;
            case st_NORMAL:
                return get_socket().read_some(buffers, ec);

            case st_SSL:
                return get_ssl_socket().read_some(buffers);
        }
    }

    template <typename MutableBufferSequence>
    std::size_t read_some(const MutableBufferSequence& buffers)
    {
        switch (m_status) {
            case st_NONE:
                SSS_POSITION_THROW(std::runtime_error, "st_NONE");
                break;
            case st_NORMAL:
                return get_socket().read_some(buffers);

            case st_SSL:
                return get_ssl_socket().read_some(buffers);
        }
    }

    template <typename MutableBufferSequence, typename ReadHandler>
    void async_read_some(const MutableBufferSequence& buffers,
                         ReadHandler&& handler)
    {
        switch (m_status) {
            case st_NONE:
                SSS_POSITION_THROW(std::runtime_error, "st_NONE");
                break;
            case st_NORMAL:
                get_socket().async_read_some(buffers, handler);
                break;

            case st_SSL:
                get_ssl_socket().async_read_some(buffers, handler);
                break;
        }
    }

    template <typename MutableBufferSequence, typename ReadHandler>
    void async_read_until(const MutableBufferSequence& buffers,
                          const std::string &delim,
                          ReadHandler&& handler)
    {
        switch (m_status) {
            case st_NONE:
                SSS_POSITION_THROW(std::runtime_error, "st_NONE");
                break;
            case st_NORMAL:
                boost::asio::async_read_until(get_socket(), buffers, delim, handler);
                break;

            case st_SSL:
                boost::asio::async_read_until(get_ssl_socket(), buffers, delim, handler);
                break;
        }
    }

    template <typename ConstBufferSequence>
    std::size_t write_some(const ConstBufferSequence& buffers,
                           boost::system::error_code& ec)
    {
        switch (m_status) {
            case st_NONE:
                SSS_POSITION_THROW(std::runtime_error, "st_NONE");
                break;
            case st_NORMAL:
                return get_socket().write_some(buffers, ec);

            case st_SSL:
                return get_ssl_socket().write_some(buffers, ec);
        }
    }

    template <typename ConstBufferSequence>
    std::size_t write_some(const ConstBufferSequence& buffers)
    {
        switch (m_status) {
            case st_NONE:
                SSS_POSITION_THROW(std::runtime_error, "st_NONE");
                break;
            case st_NORMAL:
                return get_socket().write_some(buffers);

            case st_SSL:
                return get_ssl_socket().write_some(buffers);
        }
    }

    template <typename MutableBufferSequence, typename ReadHandler>
    void async_write_some(const MutableBufferSequence& buffers,
                          ReadHandler&& handler)
    {
        switch (m_status) {
            case st_NONE:
                SSS_POSITION_THROW(std::runtime_error, "st_NONE");
                break;
            case st_NORMAL:
                get_socket().async_write_some(buffers, handler);
                break;

            case st_SSL:
                get_ssl_socket().async_write_some(buffers, handler);
                break;
        }
    }
};
}  // namespace detail

} // namespace ss1x

