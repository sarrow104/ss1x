#pragma once

#include "socket_t.hpp"
#include "user_agent.hpp"

#include <iostream>
#include <istream>
#include <ostream>
#include <string>

#include <functional>
#include <stdexcept>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>

#include <ss1x/asio/headers.hpp>
#include <sss/colorlog.hpp>
#include <sss/debug/value_msg.hpp>
#include <sss/util/PostionThrow.hpp>

//! http://www.cnblogs.com/lzjsky/archive/2011/05/12/2044460.html
// 异步调用专用。
// boost::asio::placeholders::error
// boost::asio::placeholders::bytes_transferred
class http_client {
public:
    http_client(boost::asio::io_service& io_service,
                boost::asio::ssl::context* p_ctx = nullptr)
        : m_resolver(io_service)
    {
        if (p_ctx) {
            m_socket.init(io_service, *p_ctx);
        }
        else {
            m_socket.init(io_service);
        }
    }

    typedef std::function<void(boost::asio::streambuf& response)> onResponce_t;
    void setOnResponce(onResponce_t&& func) { m_onResponse = std::move(func); }
    void http_get(const std::string& server, int port, const std::string& path)
    {
        std::ostream request_stream(&m_request);
        request_stream << "GET " << path << " HTTP/1.0\r\n";
        request_stream << "Host: " << server << "\r\n";
        request_stream << "Accept: */*\r\n";
        request_stream << "User-Agent: " << USER_AGENT_DEFAULT;
        request_stream << "Connection: close\r\n\r\n";

        char port_buf[10];
        std::sprintf(port_buf, "%d", port <= 0 ? 80 : port);

        boost::asio::ip::tcp::resolver::query query(server, port_buf);
        m_resolver.async_resolve(
            query, boost::bind(&http_client::handle_resolve, this,
                               boost::asio::placeholders::error,
                               boost::asio::placeholders::iterator));
    }

    void http_post(const std::string& server, int port, const std::string& path,
                   const std::string& postParams)
    {
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
            query, boost::bind(&http_client::handle_resolve, this,
                               boost::asio::placeholders::error,
                               boost::asio::placeholders::iterator));
    }

    void handle_resolve(
        const boost::system::error_code& e,
        boost::asio::ip::tcp::resolver::iterator endpoint_iterator)
    {
        if (!e) {
            if (m_socket.is_ssl()) {
                m_socket.get_ssl_socket().set_verify_mode(
                    boost::asio::ssl::verify_peer);

                m_socket.get_ssl_socket().set_verify_callback(boost::bind(
                    &http_client::verify_certificate, this, _1, _2));
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
                boost::bind(&http_client::handle_connect, this,
                            boost::asio::placeholders::error));
        }
        else {
            set_error_code(e);
        }
    }

    void onIoError(const boost::system::error_code& err)
    {
        std::cerr << "Error resolve: " << err.message() << "\n";
    }

    bool verify_certificate(bool preverified,
                            boost::asio::ssl::verify_context& ctx)
    {
        // The verify callback can be used to check whether the certificate that
        // is
        // being presented is valid for the peer. For example, RFC 2818
        // describes
        // the steps involved in doing this for HTTPS. Consult the OpenSSL
        // documentation for more details. Note that the callback is called once
        // for each certificate in the certificate chain, starting from the root
        // certificate authority.

        // In this example we will simply print the certificate's subject name.
        char subject_name[256];
        X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
        X509_NAME_oneline(X509_get_subject_name(cert), subject_name, 256);

        COLOG_DEBUG("Verifying ", subject_name);

        return preverified;
    }

    void handle_connect(const boost::system::error_code& e)
    {
        if (!e) {
            if (m_socket.is_ssl()) {
                m_socket.get_ssl_socket().async_handshake(
                    boost::asio::ssl::stream_base::client,
                    boost::bind(&http_client::handle_handshake, this,
                                boost::asio::placeholders::error));
            }
            else {
                boost::asio::async_write(
                    m_socket, m_request,
                    boost::bind(&http_client::handle_write_request, this,
                                boost::asio::placeholders::error));
            }
        }
        else {
            set_error_code(e);
        }
    }

    void handle_handshake(const boost::system::error_code& e)
    {
        if (!e) {
            const char* header =
                boost::asio::buffer_cast<const char*>(m_request.data());
            COLOG_DEBUG(SSS_VALUE_MSG(header));

            // The handshake was successful. Send the request.
            boost::asio::async_write(
                m_socket, m_request,
                boost::bind(&http_client::handle_write_request, this,
                            boost::asio::placeholders::error));
        }
        else {
            set_error_code(e);
        }
    }

    void handle_write_request(const boost::system::error_code& err)
    {
        if (!err) {
            // Read the response status line. The m_response streambuf will
            // automatically grow to accommodate the entire line. The growth may
            // be
            // limited by passing a maximum size to the streambuf constructor.
            boost::asio::async_read_until(
                m_socket, m_response, "\r\n",
                boost::bind(&http_client::handle_read_status_line, this,
                            boost::asio::placeholders::error));
        }
        else {
            set_error_code(err);
        }
    }

    void handle_read_status_line(const boost::system::error_code& err)
    {
        if (!err) {
            // Check that response is OK.
            std::istream response_stream(&m_response);
            response_stream >> m_headers.http_version;
            response_stream >> m_headers.status_code;
            std::string status_message;
            std::getline(response_stream, status_message);
            if (!response_stream ||
                m_headers.http_version.substr(0, 5) != "HTTP/") {
                std::cerr << "Invalid response\n";
                return;
            }
            switch (m_headers.status_code) {
                case 200:
                    break;

                case 301:
                case 302:
                    break;

                default:
                    COLOG_DEBUG("Response returned with status code ",
                                m_headers.status_code);
                    return;
            }
            COLOG_DEBUG(SSS_VALUE_MSG(m_headers.status_code));

            // Read the response headers, which are terminated by a blank line.
            boost::asio::async_read_until(
                m_socket, m_response, "\r\n\r\n",
                boost::bind(&http_client::handle_read_headers, this,
                            boost::asio::placeholders::error));
        }
        else {
            set_error_code(err);
        }
    }

    void handle_read_headers(const boost::system::error_code& err)
    {
        if (!err) {
            // Process the response headers.
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
            if (m_response.size() > 0) {
                COLOG_DEBUG(SSS_VALUE_MSG(m_response.size()));
                if (m_onResponse) {
                    m_onResponse(m_response);
                }
            }

            // Start reading remaining data until EOF.
            boost::asio::async_read(
                m_socket, m_response, boost::asio::transfer_at_least(1),
                boost::bind(&http_client::handle_read_content, this,
                            boost::asio::placeholders::error));
        }
        else {
            set_error_code(err);
        }
    }

    void handle_read_content(const boost::system::error_code& err)
    {
        if (!err) {
            // Write all of the data that has been read so far.
            COLOG_DEBUG(SSS_VALUE_MSG(m_response.size()));

            if (m_onResponse) {
                m_onResponse(m_response);
            }

            // Continue reading remaining data until EOF.
            boost::asio::async_read(
                m_socket, m_response, boost::asio::transfer_at_least(1),
                boost::bind(&http_client::handle_read_content, this,
                            boost::asio::placeholders::error));
        }
        else if (err != boost::asio::error::eof) {
            set_error_code(err);
        }
    }

    // void async_connect(const std::string& address, const std::string& port)
    // {
    //     boost::asio::ip::tcp::resolver::query query(address, port);
    //     m_resolver.async_resolve(
    //         query, boost::bind(&http_client::handle_resolve, this,
    //                            boost::asio::placeholders::error,
    //                            boost::asio::placeholders::iterator));
    // }

    // void async_write(const void* data, size_t size, bool in_place = false)
    // {
    //     if (!in_place) {
    //         // do something
    //         boost::asio::async_write(
    //             m_socket, m_request,
    //             boost::bind(&http_client::handle_write, this,
    //                         boost::asio::placeholders::error));
    //     }
    //     else
    //         boost::asio::async_write(
    //             m_socket, boost::asio::buffer(data, size),
    //             boost::bind(&http_client::handle_write, this,
    //                         boost::asio::placeholders::error));
    // }

    // void handle_write(const boost::system::error_code& e)
    // {
    //     if (!e) {
    //         boost::asio::async_read_until(
    //             m_socket, m_response, "\r\n\r\n",
    //             boost::bind(&http_client::handle_read_header, this,
    //                         boost::asio::placeholders::error));
    //     }
    //     else {
    //         onIoError(e);
    //     }
    // }

    // void handle_read_header(const boost::system::error_code& e)
    // {
    //     if (!e) {
    //         // Process the response headers.
    //         std::istream response_stream(&m_response);
    //         std::string header;
    //         while (std::getline(response_stream, header) && header != "\r")
    //             std::cout << header << "\n";
    //         std::cout << "\n";

    //         // Write whatever content we already have to output.
    //         if (m_response.size() > 0) std::cout << &m_response;

    //         // Start reading remaining data until EOF.
    //         boost::asio::async_read(
    //             m_socket, m_response, boost::asio::transfer_at_least(1),
    //             boost::bind(&http_client::handle_read_content, this,
    //                         boost::asio::placeholders::error));
    //     }
    //     else {
    //         onIoError(e);
    //     }
    // }

    ss1x::http::Headers& header() { return m_headers; }
    const boost::system::error_code& error_code() const { return m_ec; }
protected:
    void set_error_code(const boost::system::error_code& ec) { m_ec = ec; }
private:
    boost::asio::ip::tcp::resolver m_resolver;
    ss1x::detail::socket_t m_socket;

    boost::asio::streambuf m_request;
    boost::asio::streambuf m_response;

    ss1x::http::Headers m_headers;
    boost::system::error_code m_ec;
    onResponce_t m_onResponse;
};
