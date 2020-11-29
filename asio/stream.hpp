// ss1x/asio/stream.hpp
#pragma once

#include <sss/string_view.hpp>
#include <sss/colorlog.hpp>

#include <cstdlib>
#include <functional>

#include <boost/asio/error.hpp>

namespace ss1x {

class stream
{
public:
    typedef std::function<void(sss::string_view s)> on_avial_out_func_type;
    typedef boost::system::error_code               error_code_type;

    stream() {}
    virtual ~stream() {};

    void set_on_avail_out(const on_avial_out_func_type& func)
    {
        m_on_avail_out = func;
    }

    void set_on_avail_out(on_avial_out_func_type&& func)
    {
        m_on_avail_out = std::move(func);
    }

    void on_avail_out(sss::string_view s)
    {
        if (m_on_avail_out)
        {
            m_on_avail_out(s);
        }
    }

    void on_avail_out(const char* data, size_t size)
    {
        if (m_on_avail_out)
        {
            m_on_avail_out({data, size});
        }
    }

    virtual int inflate(const char * data, size_t size, error_code_type* p_ec = nullptr) = 0;
    int inflate(sss::string_view sv, error_code_type * p_ec = nullptr)
    {
        return this->inflate(sv.data(), sv.size(), p_ec);
    }

protected:

    int on_err(
        int bytes_transferred,
        const error_code_type& err,
        error_code_type * p_ec)
    {
        if (err.value())
        {
            COLOG_ERROR(err.message());
        }
        if (p_ec) {
            *p_ec = err;
        }

        // 解压发生错误, 通知用户并放弃处理.
        if (!p_ec && err.value()) {
            throw err;
        }
        return bytes_transferred;
    }

protected:
    on_avial_out_func_type m_on_avail_out;
};

} // namespace ss1x
