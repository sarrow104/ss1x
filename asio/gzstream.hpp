#pragma once

#include <zlib.h>

#include <boost/asio/error.hpp>

#include <sss/colorlog.hpp>

namespace ss1x {
struct gzstream
{
    enum method_t
    {
        mt_none = 0,
        mt_gzip = 1,
        mt_zlib = 2,
        mt_deflate = 3
    };

    std::function<void(sss::string_view s)> m_on_avail_out;

    void set_on_avail_out(const std::function<void(sss::string_view s)>& func)
    {
        m_on_avail_out = func;
    }

    void set_on_avail_out(std::function<void(sss::string_view s)>&& func)
    {
        m_on_avail_out = std::move(func);
    }

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
        if (m_method) {
            inflateEnd(&m_stream);
            m_method = mt_none;
        }
    }

    method_t m_method;
    // zlib支持.
    z_stream m_stream;

    // 解压缓冲.
    char m_zlib_buffer[1024];

    // 输入的字节数.
    // std::size_t m_zlib_buffer_size;

    void init(method_t m);
    void init()
    {
        this->init(m_method);
    }

    int  inflate(const char * data, size_t size);
    int  inflate(sss::string_view sv)
    {
        return inflate(sv.data(), sv.size());
    }
};
} // namespace ss1x

