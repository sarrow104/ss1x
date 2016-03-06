#ifndef __BOOST_SSL_SOCKET_HPP_1455794217__
#define __BOOST_SSL_SOCKET_HPP_1455794217__

#include "boost_socket_base.hpp"

template<typename T> 
class boost_ssl_socket : public boost_socket_base<T>
                       , public boost::asio::ssl::stream<T>
{
public:
    typedef boost::asio::ssl::stream<T> base2;
    
    boost_ssl_socket(boost::asio::io_service& io_service,boost::asio::ssl::context& ctx)
        :base2(io_service,ctx)
    { }
};


#endif /* __BOOST_SSL_SOCKET_HPP_1455794217__ */
