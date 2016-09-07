#include <iostream>
#include <map>
#include <string>

//! https://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html
// NOTE
// 关于 HTTP header 域，以及值的详细定义，见上面的链接

namespace ss1x {
namespace http {
class Headers : public std::map<std::string, std::string> {
    typedef std::map<std::string, std::string> BaseT;

public:
    Headers() {}
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

    std::string get(const std::string& key, const std::string& stem) const;

private:
};

inline std::ostream& operator<<(std::ostream& o, const Headers& h)
{
    h.print(o);
    return o;
}
}  // namespace asio
}  // namespace ss1x
