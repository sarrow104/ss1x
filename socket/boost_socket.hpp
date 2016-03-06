#ifndef __BOOST_SOCKET_HPP_1455794300__
#define __BOOST_SOCKET_HPP_1455794300__

#include "boost_socket_base.hpp"

template<typename T>
class boost_socket : public boost_socket_base<T>
                   , public T
{
public:
    typedef T base2;
    
    boost_socket(boost::asio::io_service& io_service)
        :base2(io_service)
    { }
};


#endif /* __BOOST_SOCKET_HPP_1455794300__ */

