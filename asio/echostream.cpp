// ss1x/asio/echostream.cpp
#include "echostream.hpp"

#include <sss/debug/value_msg.hpp>
#include <ss1x/asio/error_codec.hpp>

#include <boost/asio/error.hpp>

namespace ss1x {

int echostream::inflate(const char * data, size_t size, int * p_ec)
{
    COLOG_DEBUG(SSS_VALUE_MSG(size));
    (void)p_ec;
    base_type::on_avail_out(data, size);
    return size;
}

} // namespace ss1x

