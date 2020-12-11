#pragma once

#include <memory>
#include <stdexcept>
#include <mutex>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>

#include <sss/colorlog.hpp>
#include <sss/debug/value_msg.hpp>
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
    // typedef boost::asio::ssl::stream<boost_socket_t> ssl_socket_t;
    typedef boost::asio::ssl::stream<basic_socket_t&> ssl_socket_t;

    basic_socket_t m_socket;
    std::unique_ptr<ssl_socket_t> m_ssl_stream;
    std::mutex m_socket_lock;
    bool m_endable_ssl;

public:
    // NOTE higher version of boost::asio need this typedef(eg. 1.74)
    typedef boost::asio::ip::tcp::socket::lowest_layer_type::executor_type executor_type;

    explicit socket_t(boost::asio::io_service& io_service)
        : m_socket(io_service), m_endable_ssl(true)
    {
    }
    socket_t(boost::asio::io_service& io_service,
             boost::asio::ssl::context& ctx)
        : m_socket(io_service), m_ssl_stream(new ssl_socket_t(m_socket, ctx)), m_endable_ssl(true)
    {
    }

    ~socket_t() = default;

    void upgrade_to_ssl(boost::asio::ssl::context& ctx)
    {
        std::lock_guard<std::mutex> lock(m_socket_lock);
        if (!has_ssl()) {
            COLOG_DEBUG("from ", &ctx);
            m_ssl_stream.reset(new ssl_socket_t(m_socket, ctx));
        }
        m_endable_ssl = true;
    }

    // This simply instantiates the internal state to support ssl. It does not perform the handshake.
    void upgrade_to_ssl()
    {
        // == Info:   CAfile: /etc/ssl/certs/ca-certificates.crt
        //   CApath: /etc/ssl/certs
        // => Send SSL data, 5 bytes (0x5)
        // 0000: .....
        // == Info: TLSv1.3 (OUT), TLS handshake, Client hello (1):

        std::lock_guard<std::mutex> lock(m_socket_lock);
        if (!has_ssl()) {
            // /home/sarrow/extra/boost1_67/include/boost/asio/ssl/context_base.hpp:81
            boost::asio::ssl::context ssl_context(boost::asio::ssl::context::sslv23);
            //boost::asio::ssl::context ssl_context(boost::asio::ssl::context::tlsv13);
            ssl_context.set_default_verify_paths();
            ssl_context.set_options(boost::asio::ssl::context::default_workarounds
                                    //| boost::asio::ssl::context::no_sslv2
                                    //| boost::asio::ssl::context::single_dh_use
                                    // | boost::asio::ssl::context::no_sslv3
                                    );
            m_ssl_stream.reset(new ssl_socket_t(m_socket, ssl_context));
        }
        m_endable_ssl = true;
    }

    void disable_ssl()
    {
        std::lock_guard<std::mutex> lock(m_socket_lock);
        if (m_ssl_stream && m_endable_ssl) {
            m_endable_ssl = false;
        }
    }

    void enable_ssl()
    {
        std::lock_guard<std::mutex> lock(m_socket_lock);
        if (!m_endable_ssl) {
            m_endable_ssl = true;
        }
    }

    bool is_ssl_enabled() const
    {
        return m_endable_ssl;
    }

    bool has_ssl() const
    {
        COLOG_DEBUG("has_ssl = ", bool(m_ssl_stream));
        return bool(m_ssl_stream);
    }
    operator const void*() const
    {
        return reinterpret_cast<const void*>(&m_socket);
    }
    basic_socket_t& get_socket() { return m_socket; }
    ssl_socket_t& get_ssl_socket()
    {
        if (!m_ssl_stream) {
            SSS_POSITION_THROW(std::runtime_error, "need ssl socket here");
        }
        return *m_ssl_stream;
    }

    void close()
    {
        this->get_socket().close();
    }

    void cancel()
    {
        this->get_socket().cancel();
    }

    bool using_ssl() const
    {
        return bool(m_ssl_stream) && m_endable_ssl;
    }

    boost::asio::ip::tcp::socket::lowest_layer_type& lowest_layer()
    {
        if (using_ssl()) {
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
        if (using_ssl()) {
            return get_ssl_socket().read_some(buffers);
        }
        else {
            return get_socket().read_some(buffers, ec);
        }
    }

    template <typename MutableBufferSequence>
    std::size_t read_some(const MutableBufferSequence& buffers)
    {
        if (using_ssl()) {
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
        if (using_ssl()) {
            get_ssl_socket().async_read_some(buffers, handler);
        }
        else {
            get_socket().async_read_some(buffers, handler);
        }
    }

    template <typename MutableBufferSequence, typename ReadHandler>
    void async_read_until(const MutableBufferSequence& buffers,
                          const std::string& delim, ReadHandler&& handler)
    {
        if (using_ssl()) {
            boost::asio::async_read_until(get_ssl_socket(), buffers, delim,
                                          handler);
        }
        else {
            boost::asio::async_read_until(get_socket(), buffers, delim,
                                          handler);
        }
    }

    template <typename ConstBufferSequence>
    std::size_t write_some(const ConstBufferSequence& buffers,
                           boost::system::error_code& ec)
    {
        if (using_ssl()) {
            return get_ssl_socket().write_some(buffers, ec);
        }
        else {
            return get_socket().write_some(buffers, ec);
        }
    }

    template <typename ConstBufferSequence>
    std::size_t write_some(const ConstBufferSequence& buffers)
    {
        if (using_ssl()) {
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
        if (using_ssl()) {
            get_ssl_socket().async_write_some(buffers, handler);
        }
        else {
            get_socket().async_write_some(buffers, handler);
        }
    }
};
}  // namespace detail

}  // namespace ss1x
