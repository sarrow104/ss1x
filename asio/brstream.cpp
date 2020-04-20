// ss1x/asio/brstream.cpp
#include "brstream.hpp"

#include <sss/debug/value_msg.hpp>
#include <ss1x/asio/error_codec.hpp>

#include <boost/asio/error.hpp>

static const size_t kFileBufferSize = 1 << 19;

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

    while (size != 0 && m_state == BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT)
    {
        m_state
            = ::BrotliDecoderDecompressStream(
                m_ptr_dec,
                &size, reinterpret_cast<const uint8_t**>(&data),
                &available_out, reinterpret_cast<uint8_t**>(&buffer),
                0);

        if (m_state == BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT ||
            m_state == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT)
        {
            auto current_cnt = m_buffer.size() - available_out;
            base_type::on_avail_out(sss::string_view(m_buffer.data(), current_cnt));
            available_out = m_buffer.size();
            buffer = &m_buffer[0];
            m_state = BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT;
            bytes_transferred += current_cnt;

        }
        else if (m_state == BROTLI_DECODER_RESULT_SUCCESS)
        {
            base_type::on_avail_out(sss::string_view(m_buffer.data(), m_buffer.size() - available_out));

            bytes_transferred += m_buffer.size() - available_out;
            break;

        }
        else
        {
            std::cout << SSS_VALUE_MSG(m_state) << std::endl;
            return base_type::on_err(
                bytes_transferred,
                ss1x::errc::stream_decoder_corrupt_input,
                p_ec);
        }
    } // while

    return bytes_transferred;
}

} // namespace ss1x

