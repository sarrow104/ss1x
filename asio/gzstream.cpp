#include "gzstream.hpp"

#include <sss/debug/value_msg.hpp>

namespace ss1x {
void gzstream::init(method_t m)
{
    int windowBits = 0;
    switch (m)
    {
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
            SSS_POSITION_THROW(std::runtime_error, "wrong ",
                               __PRETTY_FUNCTION__, " method ", m);

    }

    if (m_stream.zalloc) {
        inflateEnd(&m_stream);
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
        // handler(ec);
        return;
    }
}

const char* zlib_errmsg(int err)
{
    switch (err)
    {
        case Z_ERRNO:           return "Z_ERRNO";
        case Z_STREAM_ERROR:    return "Z_STREAM_ERROR";
        case Z_DATA_ERROR:      return "Z_DATA_ERROR";
        case Z_MEM_ERROR:       return "Z_MEM_ERROR";
        case Z_BUF_ERROR:       return "Z_BUF_ERROR";
        case Z_VERSION_ERROR:   return "Z_VERSION_ERROR";
        default: return "";
    }
    return "";
}

int gzstream::inflate(const char * data, size_t size, int * p_ec)
{
    COLOG_DEBUG(SSS_VALUE_MSG(size));
    if (m_stream.avail_in == 0) {
        m_stream.avail_in = size;
        m_stream.next_in = (z_const Bytef *)data;
    }
    int bytes_transferred = 0;
    // http://www.zlib.net/zlib_how.html
    while (m_stream.avail_in > 0) {
        m_stream.avail_out =
            sizeof(m_zlib_buffer);  // 输出位置，连续空闲内存区域长度
        m_stream.next_out = (Bytef *)m_zlib_buffer;  // 下一个输出位置地址；
        int ec = ::inflate(&m_stream, Z_SYNC_FLUSH);
        if (p_ec) {
            *p_ec = ec;
        }
        if (ec < 0) {
            COLOG_ERROR(zlib_errmsg(ec));
            boost::system::error_code err =
                boost::asio::error::operation_not_supported;
            // 解压发生错误, 通知用户并放弃处理.
            COLOG_ERROR(err.message());
            if (!p_ec && ec != Z_BUF_ERROR) {
                throw ec;
            }
            return 0;
        }
        auto current_cnt = sizeof(m_zlib_buffer) - m_stream.avail_out;
        if (m_on_avail_out) {
            m_on_avail_out(sss::string_view(m_zlib_buffer, current_cnt));
        }
        bytes_transferred += current_cnt;
    }
    return bytes_transferred;
}
} // namespace ss1x

