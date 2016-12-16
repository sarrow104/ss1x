#ifndef __UTILITY_HPP_1456799081__
#define __UTILITY_HPP_1456799081__

#include <string>
#include <tuple>

namespace ss1x {
namespace parser {
class rule;
}  // namespace parser
namespace util {
namespace url {
// 协议字符串、域名、显式端口号、pathcommand
std::tuple<std::string, std::string, int, std::string> split(
    const std::string& url);

// NOTE 关于地址解析；
// 除了正常的地址形式外(协议、域名、端口、请求路径，以及参数)，还有几种形式：
//
// 1. 省略协议 //domain.name/path?parameters 
// 2. 省略域名 /path?parameters
// 3. 相对路径 ../x.html img/hello.jpg
inline auto split_port_auto(const std::string& url) -> decltype(ss1x::util::url::split(url))
{
    auto url_info = ss1x::util::url::split(url);
    if (std::get<2>(url_info) <= 0) {
        if (std::get<0>(url_info) == "http") {
            std::get<2>(url_info) = 80;
        }
        else if (std::get<0>(url_info) == "https"){
            std::get<2>(url_info) = 443;
        }
        else {
            std::get<2>(url_info) = 80;
        }
    }
    if (std::get<3>(url_info).empty()) {
        std::get<3>(url_info) = "/";
    }
    return url_info;
}

std::string join(
    const std::tuple<std::string, std::string, int, std::string>& url);
std::string join(const std::string& protocal, const std::string& domain,
                 int port, const std::string& command);

const ss1x::parser::rule& get_protocal_p();
const ss1x::parser::rule& get_domain_p();
const ss1x::parser::rule& get_port_p();

bool is_absolute(const std::string& url);
std::string full_of_copy(const std::string& url, const std::string& referer);

}  // namespace url
namespace html {
void queryText(std::ostream& o, const std::string& utf8html,
               const std::string& css_path);
}  // namespace html
}  // namespace util
}  // namespace ss1x

#endif /* __UTILITY_HPP_1456799081__ */
