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

    // NOTE ͨ�� !m_stream.zalloc �����ж�z_stream�Ƿ��ʼ���ˡ�
    if (Z_OK !=
        inflateInit2(&m_stream, windowBits))  // ��ʼ��ZLIB��,
                                              // ÿ�ν�ѹÿ��chunked��ʱ��,
                                              // ����Ҫ���³�ʼ��.
    {
        boost::system::error_code ec =
            boost::asio::error::operation_not_supported;
        COLOG_ERROR("Init zlib invalid, error message: \'", ec.message(), "\'");
        // handler(ec);
        return;
    }
}

int gzstream::inflate(const char * data, size_t size)
{
    COLOG_DEBUG(SSS_VALUE_MSG(size));
    if (m_stream.avail_in == 0) {
        m_stream.avail_in = size;
        m_stream.next_in = (z_const Bytef *)data;
    }
    int bytes_transferred = 0;
    while (m_stream.avail_in > 0) {
        m_stream.avail_out =
            sizeof(m_zlib_buffer);  // ���λ�ã����������ڴ����򳤶�
        m_stream.next_out = (Bytef *)m_zlib_buffer;  // ��һ�����λ�õ�ַ��
        int ret = ::inflate(&m_stream, Z_SYNC_FLUSH);
        if (ret < 0) {
            boost::system::error_code err =
                boost::asio::error::operation_not_supported;
            // ��ѹ��������, ֪ͨ�û�����������.
            COLOG_ERROR(err.message());
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

