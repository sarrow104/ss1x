// ss1x/asio/brstream.cpp
#include "brstream.hpp"

#include <sss/debug/value_msg.hpp>
#include <ss1x/asio/error_codec.hpp>

#include <boost/asio/error.hpp>

namespace ss1x {

int brstream::inflate(const char * data, size_t size, int * p_ec)
{
    COLOG_DEBUG(SSS_VALUE_MSG(size));

    if (!good())
    {
        if (p_ec)
        {
            *p_ec = ss1x::errc::stream_decoder_br_init_failed;
        }
        return 0;
    }

    int bytes_transferred = 0;
    char * buffer         = &m_buffer[0];
    size_t available_out  = m_buffer.size();
    //const char * data_bak = data;
    //size_t size_bak       = size;

    // FIXME
    base_type::on_avail_out(data, size);
    return size;

    while (size != 0 && m_state == BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT)
    {
        m_state
            = ::BrotliDecoderDecompressStream(
                m_ptr_dec,
                &size, reinterpret_cast<const uint8_t**>(&data),
                &available_out, reinterpret_cast<uint8_t**>(&buffer),
                0);

        switch (m_state)
        {
#define __BSTREAM_LOG__(m) case m: COLOG_DEBUG(#m); break;
            __BSTREAM_LOG__(BROTLI_DECODER_RESULT_ERROR);
            __BSTREAM_LOG__(BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT);
            __BSTREAM_LOG__(BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT);
            __BSTREAM_LOG__(BROTLI_DECODER_RESULT_SUCCESS);
#undef __BSTREAM_LOG__
            default: COLOG_DEBUG(SSS_VALUE_MSG(m_state));
        }

        if (m_state == BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT ||
            m_state == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT)
        {
            auto current_cnt = m_buffer.size() - available_out;
            base_type::on_avail_out(m_buffer.data(), current_cnt);
            available_out = m_buffer.size();
            buffer = &m_buffer[0];
            m_state = BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT;
            bytes_transferred += current_cnt;

        }
        else if (m_state == BROTLI_DECODER_RESULT_SUCCESS)
        {
            base_type::on_avail_out(m_buffer.data(), m_buffer.size() - available_out);

            bytes_transferred += m_buffer.size() - available_out;
            break;

        }
        else // BROTLI_DECODER_RESULT_ERROR == 0
        {
            return base_type::on_err(
                bytes_transferred,
                ss1x::errc::stream_decoder_corrupt_input,
                p_ec);
        }
    } // while

    return bytes_transferred;
}

} // namespace ss1x

