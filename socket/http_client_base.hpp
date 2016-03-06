#ifndef __HTTP_CLIENT_BASE_HPP_1455794403__
#define __HTTP_CLIENT_BASE_HPP_1455794403__

#include <boost/bind.hpp>
#include <boost/asio.hpp>

#include "boost_ssl_socket.hpp"
#include "boost_socket.hpp"

class http_client
{
public:
    http_client(boost::asio::io_service& io_service, boost::asio::ssl::context* ctx = NULL)
        : socket_(0), resolver_(io_service)
    {
        if(ctx)
            socket_ = new boost_ssl_socket<boost_tcp_socket>(io_service, *ctx);
        else
            socket_ = new boost_socket<boost_tcp_socket>(io_service);
    }

    void async_connect(const std::string& address, const std::string& port)
    {
        boost::asio::ip::tcp::resolver::query query(address, port);
        resolver_.async_resolve(query,
                                boost::bind(&http_client::handle_resolve,
                                            this,
                                            boost::asio::placeholders::error,
                                            boost::asio::placeholders::iterator));
    }

    void async_write(const void* data,size_t size,bool in_place=false)
    {
        if(!in_place){
            //do something
            boost::asio::async_write(socket_, request_,
                                     boost::bind(&http_client::handle_write,
                                                 this,
                                                 boost::asio::placeholders::error));
        }else
            boost::asio::async_write(socket_, boost::asio::buffer(data, size),
                                     boost::bind(&http_client::handle_write,
                                                 this,
                                                 boost::asio::placeholders::error));
    }

private:

    void handle_connect(const boost::system::error_code& e)
    {
        if(!e){
            boost_socket_base<boost_tcp_socket>::ssl_socket_base_t* p = socket_->get_ssl_socket();
            if(p)
                p->async_handshake(boost::asio::ssl::stream_base::client,
                                   boost::bind(&http_client::handle_handshake,
                                               this,
                                               boost::asio::placeholders::error));
            else
                onConnect();
        }else
            onIoError(e);
    }

    void handle_handshake(const boost::system::error_code& e)
    {
        if(!e)
            onConnect();
        else
            onIoError(e);
    }

    void handle_resolve(const boost::system::error_code& e, boost::asio::ip::tcp::resolver::iterator endpoint_iterator)
    {
        if (!e)
            boost::asio::async_connect(socket_->lowest_layer(),
                                       endpoint_iterator,
                                       boost::bind(&http_client::handle_connect,
                                                   this,
                                                   boost::asio::placeholders::error));
        else
            onIoError(e);
    }

    void handle_write(const boost::system::error_code& e)
    {
        if(!e) {
            boost::asio::async_read_until(*socket_, response_, "\r\n\r\n",
                                          boost::bind(&http_client::handle_read_header,
                                                      this,
                                                      boost::asio::placeholders::error,
                                                      boost::asio::placeholders::bytes_transferred));
        }
        else {
            onIoError(e);
        }
    }

    void handle_read_header(const boost::system::error_code& e, boost::asio::ip::tcp::resolver::iterator endpoint_iterator)
    {
        if (!e) {
        }
        else {
            onIoError(e);
        }
    }

protected:
    virtual void onConnect(){}
    virtual void onWrite(){}
    virtual void onIoError(const boost::system::error_code& e){}

private:
    // boost::asio::ip::tcp::socket    socket_;
    typedef boost::asio::ip::tcp::socket boost_tcp_socket;
    boost_socket_base<boost_tcp_socket>*    socket_;

    boost::asio::ip::tcp::resolver  resolver_;
    boost::asio::streambuf          request_;
    boost::asio::streambuf          response_;
};


#endif /* __HTTP_CLIENT_BASE_HPP_1455794403__ */
