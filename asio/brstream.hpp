// ss1x/asio/brstream.hpp
// brotli stream library
#pragma once

#include <ss1x/asio/stream.hpp>

#include <sss/colorlog.hpp>

extern "C" {
#include <brotli/decode.h>
#include <brotli/encode.h>
}

#include <array>

namespace ss1x {

class brstream : public ss1x::stream
{
    typedef ss1x::stream base_type;
    typedef base_type::on_avial_out_func_type on_avial_out_func_type;

public:
    // brotli default buffer size | input / output
    static const size_t kBufferSize = 1 << 19;

    brstream()
        :
            m_ptr_dec (0),
            m_state   (BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT)
    {
        m_ptr_dec = BrotliDecoderCreateInstance(NULL, NULL, NULL);
    }

    bool good() const
    {
        return m_ptr_dec;
    }

    ~brstream()
    {
        if (m_ptr_dec)
        {
            BrotliDecoderDestroyInstance(m_ptr_dec);
            m_ptr_dec = 0;
        }
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
    // brotli-decode支持.
    BrotliDecoderState* m_ptr_dec; //  = BrotliDecoderCreateInstance(NULL, NULL, NULL);

    // 解压缓冲.
    std::array<char, kBufferSize> m_buffer;

    BrotliDecoderResult m_state;
};

} // namespace ss1x

