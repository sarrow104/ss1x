#include "gzstream.hpp"

#include <sss/debug/value_msg.hpp>
#include <ss1x/asio/error_codec.hpp>

#include <boost/asio/error.hpp>

namespace ss1x {

void gzstream::init(method_t m)
{
    int windowBits = 0;
    switch (m)
    {
        case mt_none:
            return;

        case mt_gzip:
            windowBits = 16 + MAX_WBITS;
            break;

        case mt_zlib:
            windowBits = MAX_WBITS;
            break;

        case mt_deflate:
            windowBits = -MAX_WBITS;
            break;

        default:
            {
                SSS_POSITION_THROW(std::runtime_error, "wrong ",
                                   __PRETTY_FUNCTION__, " method ", m);
                m_method = mt_none;
            }

    }

    // NOTE 通过 !m_stream.zalloc 可以判断z_stream是否初始化了。
    if (Z_OK !=
        inflateInit2(&m_stream, windowBits))  // 初始化ZLIB库,
                                              // 每次解压每个chunked的时候,
                                              // 不需要重新初始化.
    {
        boost::system::error_code ec =
            boost::asio::error::operation_not_supported;
        COLOG_ERROR("Init zlib invalid, error message: \'", ec.message(), "\'");
        m_stream.zalloc = 0;
        return;
    }
}

void gzstream::close()
{
    if (m_stream.zalloc) {
        inflateEnd(&m_stream);
    }
    std::memset(&m_stream, 0, sizeof(m_stream));
}

int gzstream::inflate(const char * data, size_t size, error_code_type* p_ec)
{
    //std::cout << sss::raw_string(data, size) << std::endl;
    COLOG_DEBUG(SSS_VALUE_MSG(size));
    if (m_stream.avail_in == 0) {
        m_stream.avail_in = size;
        m_stream.next_in = (z_const Bytef *)data;
    }
    int bytes_transferred = 0;
    int ec = Z_OK;
    // http://www.zlib.net/zlib_how.html
    while (m_stream.avail_in > 0 && ec == Z_OK) {
        COLOG_DEBUG(SSS_VALUE_MSG(m_stream.avail_in));
        m_stream.avail_out =
            sizeof(m_zlib_buffer);  // 输出位置，连续空闲内存区域长度
        m_stream.next_out = (Bytef *)m_zlib_buffer;  // 下一个输出位置地址；
        ec = ::inflate(&m_stream, Z_SYNC_FLUSH);
        if (ec < 0) {
            if (ec == Z_BUF_ERROR) // extra input needed
            {
                return 0;
            }
            return base_type::on_err(
                bytes_transferred,
                ss1x::errc::errc_t(ss1x::errc::stream_decoder_gzip_start + abs(ec)),
                p_ec);
        }
        auto current_cnt = sizeof(m_zlib_buffer) - m_stream.avail_out;
        base_type::on_avail_out(m_zlib_buffer, current_cnt);
        bytes_transferred += current_cnt;
    }
    return bytes_transferred;
}

} // namespace ss1x

