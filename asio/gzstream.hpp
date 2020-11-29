#pragma once

#include <ss1x/asio/stream.hpp>

#include <zlib.h>

#include <sss/colorlog.hpp>

namespace ss1x {

class gzstream : public ss1x::stream
{
    typedef ss1x::stream base_type;

public:
    enum method_t
    {
        mt_none = 0,
        mt_gzip = 1,
        mt_zlib = 2,
        mt_deflate = 3
    };

private:
    on_avial_out_func_type m_on_avail_out;

public:
    gzstream()
        : m_method(mt_none)
    {
        std::memset(&m_stream, 0, sizeof(m_stream));
    }

    explicit gzstream(method_t m)
        : m_method(m)
    {
        std::memset(&m_stream, 0, sizeof(m_stream));
        if (m_method) {
            init(m);
        }
    }

    ~gzstream()
    {
        this->close();
        m_method = mt_none;
    }

private:
    method_t m_method;
    // zlib支持.
    z_stream m_stream;

    // 解压缓冲.
    char m_zlib_buffer[1024];

protected:
    // 输入的字节数.
    // std::size_t m_zlib_buffer_size;
    void init(method_t m);

    void close();

public:
    int  inflate(const char * data, size_t size, error_code_type* p_ec = nullptr);
}; // gzstream

} // namespace ss1x

