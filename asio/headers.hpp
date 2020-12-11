#pragma once

#include <iostream>
#include <map>
#include <string>

#include <sss/utlstring.hpp>

//! https://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html
// NOTE
// 关于 HTTP header 域，以及值的详细定义，见上面的链接

namespace ss1x {
namespace http {
class Headers : public std::map<std::string, std::string, sss::stricmp_t> {
    typedef std::map<std::string, std::string, sss::stricmp_t> BaseT;

public:
    Headers() : status_code(0) {}
    Headers(std::initializer_list<BaseT::value_type> il) : BaseT(il), status_code(0) {}
    ~Headers() = default;

public:
    Headers(Headers&&) = default;
    Headers& operator=(Headers&&) = default;

public:
    Headers(const Headers&) = default;
    Headers& operator=(const Headers&) = default;

public:
    void print(std::ostream& o) const;

    std::string get(const std::string& key) const
    {
        BaseT::const_iterator it = this->BaseT::find(key);
        if (it != this->BaseT::end()) {
            return it->second;
        }
        else {
            return "";
        }
    }

    bool has(const std::string& key) const
    {
        return this->BaseT::find(key) != this->BaseT::end();
    }

    size_t unset(const std::string& key)
    {
        return this->BaseT::erase(key);
    }

    bool has_kv(const std::string& key, const std::string& value) const
    {
        auto it = this->BaseT::find(key);
        
        return it != this->BaseT::end() && it->second == value;
    }

    std::string get(const std::string& key, const std::string& stem) const;

    unsigned int status_code;
    std::string  http_version;
};

inline std::ostream& operator<<(std::ostream& o, const Headers& h)
{
    h.print(o);
    return o;
}
}  // namespace asio
}  // namespace ss1x
