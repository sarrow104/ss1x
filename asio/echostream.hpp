// ss1x/asio/echostream.hpp
#pragma once

#include <ss1x/asio/stream.hpp>

#include <sss/colorlog.hpp>

#include <array>

namespace ss1x {

class echostream : public ss1x::stream
{
    typedef ss1x::stream base_type;
    typedef base_type::on_avial_out_func_type on_avial_out_func_type;

public:
    echostream()
    {
    }

    ~echostream()
    {
    }

    /**
     * @brief inflate
     *
     * @param [in]data input memory buffer starting address
     * @param [in]size input memory buffer size
     * @param [out]p_ec write error code to when an error en-countered
     *
     * @return byte converted out
     */
    int  inflate(const char * data, size_t size, error_code_type* p_ec = nullptr);

private:
};

} // namespace ss1x

