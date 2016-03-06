// http://www.cppblog.com/qinqing1984/archive/2013/03/20/198644.html

#ifndef __BOOST_SOCKET_BASE_HPP_1455794209__
#define __BOOST_SOCKET_BASE_HPP_1455794209__

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/logic/tribool.hpp>
// #include <boost/system/error_code.hpp>
#include <boost/system/config.hpp>

template<typename T>
class boost_socket_base
{
public:
    typedef boost::asio::ssl::stream<T> ssl_socket_base_t;
    typedef T socket_base_t;

protected:
    boost_socket_base()
        :tb_(boost::indeterminate)
    { }

public:
    virtual ~boost_socket_base()
    { }

    ssl_socket_base_t* get_ssl_socket()
    {
        if(tb_){
            BOOST_ASSERT(ss_);        
            return ss_;
        }else if(!tb_)
            return NULL;
        else{
            if((ss_ = dynamic_cast<ssl_socket_base_t*>(this)))
                tb_ = true;
            return ss_;
        } 
    }

    socket_base_t* get_socket()
    {
        if(!tb_){
            BOOST_ASSERT(s_);        
            return s_;
        }else if(tb_)
            return NULL;
        else{
            if((s_ = dynamic_cast<socket_base_t*>(this)))
                tb_ = false;
            return s_;
        }
    }
        
    typename T::lowest_layer_type& lowest_layer()
    {
        ssl_socket_base_t* p = get_ssl_socket();
        return p ? p->lowest_layer() : get_socket()->lowest_layer();
    }
    
    template <typename MutableBufferSequence>
    std::size_t read_some(const MutableBufferSequence& buffers, boost::system::error_code& ec)
    {
        ssl_socket_base_t* p = get_ssl_socket();
        return p ? p->read_some(buffers) : get_socket()->read_some(buffers, ec);
    }

    template <typename MutableBufferSequence>
    std::size_t read_some(const MutableBufferSequence& buffers)
    {
        //与上面相同，但不带ec
    }
    
    template <typename MutableBufferSequence, typename ReadHandler>
    void async_read_some(const MutableBufferSequence& buffers, BOOST_ASIO_MOVE_ARG(ReadHandler) handler)
    {
        ssl_socket_base_t* p = get_ssl_socket();
        return p ? p->async_read_some(buffers,handler) : get_socket()->async_read_some(buffers,handler);
    }

    template <typename ConstBufferSequence>
    std::size_t write_some(const ConstBufferSequence& buffers, boost::system::error_code& ec)
    {
        ssl_socket_base_t* p = get_ssl_socket();
        return p ? p->write_some(buffers,ec) : get_socket()->write_some(buffers,ec);
    }
    
    template <typename ConstBufferSequence>
    std::size_t write_some(const ConstBufferSequence& buffers)
    {
        //与上面相同，但不带ec
    }
    
    template <typename MutableBufferSequence, typename ReadHandler>
    void async_write_some(const MutableBufferSequence& buffers,BOOST_ASIO_MOVE_ARG(ReadHandler) handler)
    {    
        ssl_socket_base_t* p = get_ssl_socket();
        return p ? p->async_write_some(buffers,handler) : get_socket()->async_write_some(buffers,handler);
    }

private:
    boost::tribool tb_;
    union {
        ssl_socket_base_t* ss_;
        socket_base_t* s_;
    };
};


#endif /* __BOOST_SOCKET_BASE_HPP_1455794209__ */

