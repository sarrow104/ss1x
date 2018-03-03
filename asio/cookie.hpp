#pragma once

#include <map>
#include <set>
#include <string>
#include <memory>
#include <cctype>
#include <cstring>
#include <ctime>
#include <tuple>
#include <algorithm>
#include <unordered_map>

#include <boost/date_time.hpp>

#include <sss/debug/value_msg.hpp>
#include <sss/time.hpp>
#include <sss/string_view.hpp>
#include <sss/spliter.hpp>

#include <ss1x/asio/utility.hpp>

namespace ss1x {
namespace cookie {
namespace detail {
typedef std::tuple<std::string, std::string, std::string> Key_t; // domain_reverse；path; key 是节点本身
typedef std::tuple<std::string, std::shared_ptr<sss::time::Date>, bool, bool> Value_t; // value, Expires, secure, httponly
typedef std::map<Key_t, Value_t> CookieMap_t; // 由路径节点，组成的域名路径
inline CookieMap_t& getCookieMap()
{
    static detail::CookieMap_t cookieMap;
    return cookieMap;
}
inline bool icase_equal(sss::string_view s1, sss::string_view s2)
{
    //std::cout << std::distance(std::begin(s1),std::end(s1)) << '=' << std::distance(std::begin(s2),std::end(s2)) << std::endl;
    return s1.size() == s2.size()
        && std::equal(s1.begin(), s1.end(),
                      s2.begin(),
                      [](char l, char r)->bool
                      {
                          return std::toupper(l) == std::toupper(r);
                      });
}
} // namespace detail

class Cookie_t
{
public:
    Cookie_t();
    explicit Cookie_t(const std::string& cookie)
        : secure_(false), httponly_(false)
    {
        sss::ViewSpliter<char> sp(cookie, ';');
        sss::string_view sv;
        while (sp.fetch(sv)) {
            while (std::isspace(sv.front())) { sv.pop_front(); }
            sss::ViewSpliter<char> sp2(sv, '=');
            sss::string_view key;
            sss::string_view value;
            int s = int(sp2.fetch(key)) + int(sp2.fetch(value));
            // std::cout << SSS_VALUE_MSG(s) << "; " << SSS_VALUE_MSG(key) << "; " << SSS_VALUE_MSG(value) << std::endl;
            switch (s) {
                case 2:
                    if (this->name_.empty()) {
                        this->name_ = key.to_string();
                        this->value_ = value.to_string();
                    }
                    else {
                        if (detail::icase_equal(key, "domain")) {
                            domain_ =  value.to_string();
                        }
                        else if (detail::icase_equal(key, "path"))
                        {
                            this->path_ = value.to_string();
                        }
                        else if (detail::icase_equal(key, "expires"))
                        {
                            this->p_expire_.reset(new sss::time::Date(value.to_string(), "%a, %d %b %Y %X GMT"));
                        }
                        else {
                            this->ext_keyval_[key.to_string()] = value.to_string();
                        }
                    }
                    break;

                case 1:
                        if (detail::icase_equal(key, "secure")) {
                            this->secure_ = true;
                        }
                        else if (detail::icase_equal(key, "httponly"))
                        {
                            this->httponly_ = true;
                        }
                        else {
                            this->ext_option_.insert(key.to_string());
                        }
                    break;

                default:
                    break;
            }
        }
    }
    ~Cookie_t() = default;

public:
    Cookie_t(Cookie_t&& ) = default;
    Cookie_t& operator = (Cookie_t&& ) = default;

public:
    Cookie_t(const Cookie_t& ) = default;
    Cookie_t& operator = (const Cookie_t& ) = default;

public:
    void server_print(std::ostream& out) const
    {
        if (name_.empty()) {
            return;
        }
        out << "Set-Cookie: "
            << name_ << "=" << value_ << ";";
        if (p_expire_) {
            out << " Expires=" << this->p_expire_->format("%a, %d %b %Y %X GMT") << ";";
        }
        if (!path_.empty()) {
            out << " Path=" << path_ << ";";
        }
        if (!domain_.empty()) {
            out << " Domain=" << domain_ << ";";
        }
        for (auto pair: ext_keyval_) {
            out << ' ' << std::get<0>(pair) << "=" << std::get<1>(pair) << ";";
        }
        if (secure_) {
            out << " Secure" << ";";
        }
        if (httponly_) {
            out << " Httponly" << ";";
        }
        for (auto option: ext_option_) {
            out << ' ' << option << ";";
        }
    }

    void client_print(std::ostream& out, const std::string& url) const
    {
        auto url_info = ss1x::util::url::split_port_auto(url);
        auto domain = std::get<1>(url_info);
        auto path = std::get<3>(url_info);
        if (!name_.empty() &&
            (path_.empty() || sss::string_view(path).is_begin_with(path_)) &&
            (domain_.empty() || sss::string_view(domain).is_end_with(domain_)) &&
            (!secure_ || std::get<0>(url_info) == "https"))
        {
            out << " Cookie: " << name_ << "=" << value_ << ";";
        }
    }

    bool has_key(sss::string_view key) const
    {
        if (detail::icase_equal(key, "domain")) {
            return !domain_.empty();
        }
        else if (detail::icase_equal(key, "path"))
        {
            return !this->path_.empty();
        }
        else if (detail::icase_equal(key, "expires"))
        {
            return bool(this->p_expire_);
        }
        else {
            return this->ext_keyval_.find(key.to_string()) != ext_keyval_.end();
        }
    }

    std::string get_value(sss::string_view key) const
    {
        if (detail::icase_equal(key, "domain")) {
            return domain_;
        }
        else if (detail::icase_equal(key, "path"))
        {
            return this->path_;
        }
        else if (detail::icase_equal(key, "expires"))
        {
            return p_expire_->format("%a, %d %b %Y %X GMT");
        }
        else {
            auto it = ext_keyval_.find(key.to_string());
            if (it != ext_keyval_.end()) {
                return it->second;
            }
            else {
                return "";
            }
        }
    }

    bool has_option(sss::string_view key) const
    {
        if (detail::icase_equal(key, "secure")) {
            return this->secure_;
        }
        else if (detail::icase_equal(key, "httponly"))
        {
            return this->httponly_;
        }
        else {
            return this->ext_option_.find(key.to_string()) != ext_option_.end();
        }
    }

    const std::string& name() const
    {
        return this->name_;
    }
    const std::string& value() const
    {
        return this->value_;
    }
    const std::string& domain() const
    {
        return this->domain_;
    }
    const std::string& path() const
    {
        return this->path_;
    }

    std::shared_ptr<sss::time::Date> expires() const
    {
        return this->p_expire_;
    }

    bool secure() const
    {
        return this->secure_;
    }

    bool httponly() const
    {
        return this->httponly_;
    }
private:
    std::string name_;
    std::string value_;
    std::string domain_;
    std::string path_;
    std::shared_ptr<sss::time::Date> p_expire_;
    bool secure_;
    bool httponly_;
    std::set<std::string> ext_option_;
    std::map<std::string, std::string> ext_keyval_;
};

// curl 是Netscape Cookie的模式来管理cookie的。本质上还是一个树形结构。
// 不过，需要注意的是，Set-Cookie 可以在同一个response里面出现多次。
// 这是什么意思呢？
// 就是说，这东西，本质上是一个由生存周期的环境变量。
//
// domain,path,key构成了主键。值是Value，以及Expires,secure,HttpOnly等树形——如果理解为一张表的话。
// 当然，其查找方式呢，又有些区别。相当于windows中，regedit处理打开方式的时候，还提供了"*"，用来匹配所有的文件类型。
// domain中，如果子路径，没有提供，就说明，可以匹配下属所有的子路径。

inline bool set(const std::string& domain, const std::string& cookies){
    Cookie_t cookie(cookies);
    if (cookie.value().empty()) {
        return false;
    }
    else {
        std::string domain_final = cookie.domain();
        if (domain_final.empty() && !domain.empty()) {
            domain_final = domain;
        }
        std::reverse(domain_final.begin(), domain_final.end()); // 逆序
        detail::Key_t key = std::make_tuple(domain_final, cookie.path(), cookie.name());
        detail::Value_t value = std::make_tuple(cookie.value(), cookie.expires(), cookie.secure(), cookie.httponly());
        detail::getCookieMap()[key] = value;
        return true;
    }
}

// NOTE
// Cookie本身的保存，应该是倒树形来处理的。
// 即，www.baidu.com 这样的domain，应该是 com,baidu,www 这样的顺序。
// 用一个多级别的存储逻辑。
// 当然，这样就太繁琐了。因为，对于简单的爬虫来说，域名没这么多。
// 最好是，先重置域名。
//
// 然后，用map(或者deque，并随时排序)保存
// 对于爬虫来说，set动作，比get动作，少见多了。
// 所以，要清理过期的cookie的话，最好是在set的时候，进行清理。
// 为了清理的时候方便，在set数据的时候，又需要将数据，按照过期时间先后排序。
// 同时，考虑到，相同的domain，不同的路径，也可能使用不同的cookie。
// 那么，最好的存储模式是；
// std::map<date,id> ;; 按照过期时间排序
// std::map<domain,id>
// std::map<path,id>
// std::hash<id>
//
// 如果，是按照domain(split+desc)，排序之后，那么用lower_bound，找出边界(或者说，最大前缀)；

inline std::vector<std::string> get(const std::string& url)
{
    auto url_info = ss1x::util::url::split_port_auto(url);
    std::string & domain = std::get<1>(url_info);
    const std::string& path = std::get<3>(url_info);
    std::vector<std::string> rv;
    sss::time::Date cur;
    detail::CookieMap_t::iterator it = detail::getCookieMap().begin();
    std::reverse(domain.begin(), domain.end());
    while (it != detail::getCookieMap().end()) {
        const detail::Key_t & key = it->first;
        const detail::Value_t & value = it->second;
        if (std::get<1>(value) && *std::get<1>(value) < cur) {
            it = detail::getCookieMap().erase(it);
            continue;
        }
        if ((std::get<0>(key).empty() || sss::string_view(domain).is_begin_with(std::get<0>(key))) &&
            (std::get<1>(key).empty() || sss::string_view(path).is_begin_with(std::get<1>(key))) &&
            (!std::get<2>(value) || std::get<0>(url_info) == "https"))
        {
            rv.push_back(std::get<2>(key) + "=" + std::get<0>(value));
        }
        ++it;
    }
    return rv;
}
} // namespace ss1x

// std::vector<std::string> get(std::string url)
// {
//     auto url_info = ss1x::util::url::split_port_auto(url);
//     return get(std::get<1>(url_info), std::get<3>(url_info));
// }

} // end-of namespace cookie
// 1.cookie的属性
// 
// 一般cookie所具有的属性，包括：
// 
// Domain：域，表示当前cookie所属于哪个域或子域下面。
// 
// 此处需要额外注意的是，在C#中，如果一个cookie不设置对应的Domain，那么在CookieContainer.Add(cookies)的时候，会死掉。对于服务器返回的Set-Cookie中，如果没有指定Domain的值，那么其Domain的值是默认为当前所提交的http的请求所对应的主域名的。比如访问 http://www.example.com，返回一个cookie，没有指名domain值，那么其为值为默认的www.example.com。
// 
// Path：表示cookie的所属路径。
// 
// Expire time/Max-age：表示了cookie的有效期。expire的值，是一个时间，过了这个时间，该cookie就失效了。或者是用max-age指定当前cookie是在多长时间之后而失效。如果服务器返回的一个cookie，没有指定其expire time，那么表明此cookie有效期只是当前的session，即是session cookie，当前session会话结束后，就过期了。对应的，当关闭（浏览器中）该页面的时候，此cookie就应该被浏览器所删除了。
// 
// secure：表示该cookie只能用https传输。一般用于包含认证信息的cookie，要求传输此cookie的时候，必须用https传输。
// 
// httponly：表示此cookie必须用于http或https传输。这意味着，浏览器脚本，比如javascript中，是不允许访问操作此cookie的。
// 
// 2.服务器发送cookie给客户端
// 
//  从服务器端，发送cookie给客户端，是对应的Set-Cookie。包括了对应的cookie的名称，值，以及各个属性。
// 
//  例如：
// 
//  
// 
// Set-Cookie: lu=Rg3vHJZnehYLjVg7qi3bZjzg; Expires=Tue, 15 Jan 2013 21:47:38 GMT; Path=/; Domain=.169it.com; HttpOnly
// 
// Set-Cookie: made_write_conn=1295214458; Path=/; Domain=.169it.com
// 
// Set-Cookie: reg_fb_gate=deleted; Expires=Thu, 01 Jan 1970 00:00:01 GMT; Path=/; Domain=.169it.com; HttpOnly



