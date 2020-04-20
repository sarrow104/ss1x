#include <gq/Document.h>
#include <gq/Node.h>
#include <gq/Selection.h>

#include <ss1x/parser/dictionary.hpp>
#include <ss1x/parser/oparser.hpp>

#include <sss/path.hpp>
#include <sss/colorlog.hpp>

#include "utility.hpp"

namespace ss1x {
namespace util {
namespace url {

bool decode(std::string& url)
{
    // COLOG_DEBUG(url);
    bool modified = false;
    size_t i = 0;
    for (; i != url.size(); ++i) {
        if (url[i] == '%') {
            break;
        }
    }
    for (size_t j = i; j != url.size(); ++j) {
        modified = true;
        if (url[j] == '%' && j + 2 < url.size() && std::isxdigit(url[j + 1]) && std::isxdigit(url[j + 2])) {
            url[i] = (sss::hex2int(url[j + 1]) << 4u) | sss::hex2int(url[j + 2]);
            j += 2;
            ++i;
        }
        else {
            url[i++] = url[j];
        }
    }
    url.resize(i);
    // COLOG_DEBUG(url);
    return modified;
}

// 1. +    URL中+号表示空格             %2B
// 2. 空格 URL中的空格可以用+号或者编码 %20
// 3. /    分隔目录和子目录             %2F
// 4. ?    分隔实际的URL和参数          %3F
// 5. %    指定特殊字符                 %25
// 6. #    表示书签                     %23
// 7. &    URL中指定的参数间的分隔符    %26
// 8. =    URL中指定参数的值            %3D

static bool is_special_url_char(char c)
{
    switch (c)
    {
        case '+':
        case ' ':
        //case '/':
        case '?':
        case '%':
        case '#':
        case '&':
        case '=':
        case '\\':
            return true;
        default:
            if (0x80 & c)
            {
                return true;
            }
            else if (std::isspace(c))
            {
                return true;
            }
            else 
            {
                return false;
            }
    }
}

bool encode(std::string& path)
{
    // COLOG_DEBUG(path);
    bool modified = false;
    std::string url;
    size_t i = 0;
    // find the first char which need to be replaced
    for (; i != path.size(); ++i) {
        if (is_special_url_char(path[i])) {
            if (i && i < path.size() - 1) {
                url = path.substr(0, i);
                modified = true;
            }
            break;
        }
    }
    for (; i != path.size(); ++i) {
        if (path[i] == '\\') { // replace '\\' with '/'
            url += '/';
        }
        else if(is_special_url_char(path[i])) {
            url += '%';
            url += sss::lower_hex2char(path[i] >> 4u);
            url += sss::lower_hex2char(path[i]);
        }
        else {
            url += path[i];
        }
    }
    // COLOG_DEBUG(url);
    if (modified) {
        url.swap(path);
    }
    return modified;
}

class protocal_words : public ss1x::parser::KeywordsList {
public:
    protocal_words()
    {
        this->add("http");
        this->add("https");
        this->add("thunder");
        this->add("ftp");
    }
    ~protocal_words() = default;
};

const ss1x::parser::rule& get_protocal_p()
{
    static protocal_words dt;

    static ss1x::parser::rule protocal_p =
        ss1x::parser::dict_p(dt) > ss1x::parser::char_p(':') > &ss1x::parser::sequence("//");

    return protocal_p;
}

const ss1x::parser::rule& get_domain_p()
{
    static ss1x::parser::rule domain_p = ss1x::parser::sequence("//") >>
        (+(ss1x::parser::alnum_p | ss1x::parser::char_p('=') | ss1x::parser::char_p('-')) %
         ss1x::parser::char_p('.')) >>
        &(ss1x::parser::char_p('/') | ss1x::parser::char_p(':') | ss1x::parser::eof_p);
    return domain_p;
}

const ss1x::parser::rule& get_port_p()
{
    static ss1x::parser::rule port_p = ss1x::parser::char_p(':') >>
                                       +ss1x::parser::digit_p >>
                                       &ss1x::parser::char_p('/');
    return port_p;
}

// split(`http://192.168.1.7/`)
//   protocl = `http`
//   domain = `192.168.1.7`
//   port = `0`
//   command = `/`
std::tuple<std::string, std::string, int, std::string> split(
    const std::string& url)
{
    std::tuple<std::string, std::string, int, std::string> ret;

    const ss1x::parser::rule& protocal_p = get_protocal_p();
    const ss1x::parser::rule& domain_p = get_domain_p();
    const ss1x::parser::rule& port_p = get_port_p();

    typedef std::string::const_iterator StrIterator;
    StrIterator domain_beg = url.begin();
    StrIterator it_beg = url.begin();
    StrIterator it_end = url.end();

    // parse util :, if had;
    if (protocal_p.match(it_beg, it_end)) {
        domain_beg = it_beg;
        std::get<0>(ret).assign(url.cbegin(), domain_beg - 1);
    }
    // parse domain-name
    if (domain_p.match(it_beg, it_end)) {
        // skip "//"
        std::get<1>(ret).assign(domain_beg + 2, it_beg);
        StrIterator port_beg = it_beg;
        if (port_p.match(it_beg, it_end)) {
            int port = 0;
            for (StrIterator it = port_beg + 1; it != it_beg; ++it) {
                port *= 10;
                port += (*it - '0');
            }
            std::get<2>(ret) = port;
        }
        else {
            // 2016-11-17 add default port number
            // 或许，对于常见协议，还是默认0？
            if (std::get<1>(ret)=="http") {
                std::get<2>(ret) = 80;
            }
            else if (std::get<1>(ret)=="https") {
                std::get<2>(ret) = 443;
            }
        }
        std::get<3>(ret).assign(it_beg, it_end);
    }
    else {
        std::get<2>(ret) = 0;
        std::get<3>(ret).assign(url);
    }

    return ret;
}

std::string join(
    const std::tuple<std::string, std::string, int, std::string>& url)
{
    return join(std::get<0>(url), std::get<1>(url), std::get<2>(url),
                std::get<3>(url));
}

std::string join(const std::string& protocal, const std::string& domain,
                 int port, const std::string& command)
{
    std::ostringstream oss;
    if (!protocal.empty()) {
        oss << protocal << "://";
    }
    oss << domain;
    if (port > 0) {
        if ((protocal == "http" && port != 80) ||
            (protocal == "https" && port != 443))
        {
            oss << ":" << port;
        }
    }
    if (command.empty() || command.front() != '/') {
        oss << "/";
    }
    oss << command;
    return oss.str();
}

bool is_absolute(const std::string& url)
{
    typedef std::string::const_iterator StrIterator;
    StrIterator it_beg = url.begin();
    StrIterator it_end = url.end();
    const ss1x::parser::rule& protocal_p = get_protocal_p();

    return protocal_p.match(it_beg, it_end);
}

std::string full_of_copy(const std::string& url, const std::string& referer)
{
    if (!is_absolute(url)) {
        auto target_set = ss1x::util::url::split(url);
        auto refer_set = ss1x::util::url::split(referer);
        if (std::get<0>(target_set).empty()) {
            std::get<0>(target_set) = std::get<0>(refer_set);
        }

        if (std::get<1>(target_set).empty()) {
            std::get<1>(target_set) = std::get<1>(refer_set);
        }

        if (std::get<2>(target_set) == 0) {
            std::get<2>(target_set) = std::get<2>(refer_set);
        }

        if (!sss::is_begin_with(std::get<3>(target_set), "/")) {
            std::string out_path = sss::path::dirname(std::get<3>(refer_set));
            sss::path::append(out_path, std::get<3>(target_set));
            std::get<3>(target_set) = out_path;
        }
        return ss1x::util::url::join(target_set);
    }
    return url;
}

}  // namespace url

namespace html {

void queryText(std::ostream& o, const std::string& utf8html,
               const std::string& css_path)
{
    CDocument doc;
    doc.parse(utf8html);
    if (!doc.isOK()) {
        return;
    }

    CSelection s = doc.find(css_path);

    for (size_t i = 0; i != s.nodeNum(); ++i) {
        CNode n = s.nodeAt(i);
        o << n.textNeat();
    }
}

}  // namespace html
}  // namespace util
}  // namespace ss1x
