#ifndef __OPARSER_HPP_1452140160__
#define __OPARSER_HPP_1452140160__

// 偏向对象的解析器组合
// 所有的解析器，都是对象；可以组合在一起；
//
// 对于highlighter来说，如何实现部分更新？
//
// 对于嵌套类型的语法，肯定有层次结构——层次结构没变化，然后syntax元素，和修改
// 之前一致的话，就说明后续内容，不用再进行语法解析了。
//
// 首先对象可以组合
//
// 可以赋值，即，拥有值语义；
//
//

#if __cplusplus < 201103
#error you must switch on -std=c++11
#endif

#include <boost/variant.hpp>
#include <sss/util/Parser.hpp>
#include <sss/util/PostionThrow.hpp>

#include <sss/log.hpp>
#include <sss/util/utf8.hpp>

#include <functional>
#include <utility>

#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <type_traits>

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <tuple>

#define SSS_ZeroMem(a) std::memset(&a, 0, sizeof(a))

#define CHAR_T char32_t

namespace ss1x {
namespace parser {

namespace util {
struct delayeval_helper;
}

typedef std::string::const_iterator StrIterator;
typedef sss::util::Parser<StrIterator> Parser;

class visitor;
class Status;
class delayeval;

class dictionary;

class rule {
public:
    typedef int (*CTypeFuncT)(int);

    struct empty {
    };  // empty value for boost::variant

#ifndef MATCHED_VALUE_T_LIST
#define MATCHED_VALUE_T_LIST empty, uint64_t, double, std::string
#endif

    typedef boost::variant<MATCHED_VALUE_T_LIST> matched_value_t;

    typedef boost::variant<MATCHED_VALUE_T_LIST,
                           std::function<uint64_t(StrIterator, StrIterator)>,
                           std::function<std::string(StrIterator, StrIterator)>,
                           std::function<double(StrIterator, StrIterator)>>
        matched_action_t;
#undef MATCHED_VALUE_T_LIST

    typedef std::function<void(StrIterator, StrIterator, matched_value_t)>
        ActionT;

    typedef std::function<bool(StrIterator&, StrIterator,
                               util::delayeval_helper*)>
        DoMatchActionT;

    static const ActionT null_action;

    static matched_action_t slice2string_action;
    static matched_action_t slice2double_action;

    friend class visitor;
    friend class delayeval;
    friend struct util::delayeval_helper;

    static uint64_t toUint64_t(const matched_value_t& var)
    {
        if (const uint64_t* p = boost::get<uint64_t>(&var)) {
            return *p;
        }
        return 0;
    }

    static double toDouble(const matched_value_t& var)
    {
        if (const double* p = boost::get<double>(&var)) {
            return *p;
        }
        return 0.0;
    }

    static std::string toString(const matched_value_t& var)
    {
        if (const std::string* p = boost::get<std::string>(&var)) {
            return *p;
        }
        return "";
    }

public:
    friend rule eps();
    friend rule eof();

    friend rule range(const rule& r, int m, int n);
    friend rule refer(rule& rhs);

    // r匹配失败时，才执行动作func
    // NOTE
    // 这其实就是利用分支的短路行为；
    // 让最后一个分支是eps，然后将该动作绑定到这个eps即可；
    friend rule on_fail(const rule& r, ActionT func);

    friend rule sequence(const std::string& seq);
    friend rule dict_p(dictionary& d);

    // 当前匹配位置，行、列；从0开始；
    friend rule col_p(int);
    friend rule row_p(int);

    friend rule col_if_times_p(const rule& r, std::function<bool(int)>);

    // maybe more:
    // friend rule col_range_p(int );
    // friend rule row_range_p(int );
    //
    // friend rule col_if_p(std::function<bool(int)>);
    // friend rule row_if_p(std::function<bool(int)>);

    friend rule user_domtch(DoMatchActionT domatch_action);

    friend rule make_ctype_wapper(CTypeFuncT f);

    friend rule char_p(char ch);
    friend rule char_set_p(const char* s);
    friend rule char_range_p(char ch_low, char ch_high);

    // utf8
    friend rule utf8_p(const char* utf8ch);
    friend rule utf8_set_p(const char* utf8ch);
    // 关于中文
    // 张勇范围：U+4E00..U+9FA5
    //! http://blog.csdn.net/shuilan0066/article/details/7839189
    friend rule utf8_range_p(const char* utf8_low, const char* utf8_high);

public:
    enum type_t {
        RULE_TYPE_NONE = 0,
        RULE_TYPE_ANYCHAR,      // .
        RULE_TYPE_CHAR,         // 'x'
        RULE_TYPE_RANGE,        // 'z'-'z'
        RULE_TYPE_CHARSET,      // "abcxyz"

        RULE_TYPE_SEQUENCE,     // literal sequence; 序列
        RULE_TYPE_DICTIONARY,   // 词典

        // 操作符重载
        RULE_OPERATOR_ASSERTNOT,   // !a; negtive predicate 否定预查
        RULE_OPERATOR_ASSERTTRUE,  // &a; positive predicate 肯定预查

        RULE_OPERATOR_MOSTONETIME,  // -a; 0-1次 operator -()
        RULE_OPERATOR_KLEENE_STAR,  // *a; 0-n次 operator *()
        RULE_OPERATOR_PLUSTIMES,    // +a; 1-n次 operator +()
        RULE_OPERATOR_RANGETIMES,   // ...

        RULE_OPERATOR_BRANCH,         // a | b; Parse a or b
        RULE_OPERATOR_CATENATE,       // a >> b; Parse a followed by b
        RULE_OPERATOR_MUST_FOLLOWED,  // a > b; Expect. must followed;
        // if a pass but b failed, then throw exception

        RULE_OPERATOR_DIFFERENCE,  // a - b; Parse a but not b
        RULE_OPERATOR_SEQUENTIAL,  // a || b; Parse a or b or a followed by b

        RULE_OPERATOR_SEP_SUGAR,    // a % b; equals to `a >> *(b >> a)`
        RULE_OPERATOR_PERMUTATION,  // a ^ b; Permutation.
        // Parse a or b or a followed by b or b followed by a.
        // 好比set；并且分支可以组合；

        RULE_OPERATOR_REFER,  // 可以反复使用的，必须用REFER包裹一下；
                              // 这样；上层的可以保存其指针；

        RULE_DOMATCH_ACTION,  // 用户自定义的action函数；用来执行用户设置的do_match动作；
        RULE_CTYPE_WRAPPER,

        RULE_ANCHOR = (1 << 8),

        RULE_ANCHOR_LINE_BEGIN = RULE_ANCHOR,  // 锚点，行首
        RULE_ANCHOR_LINE_END,                  // 锚点，行尾
        RULE_ANCHOR_ROW_POS,                   // 锚点，文本行位置；
        RULE_ANCHOR_COL_POS,                   // 锚点，文本列位置；

        RULE_ANCHOR_COL_IF_TIMES,  // 锚点；当位置符合，并且内部可以继续匹
                                   // 配的时候，重复多次；

        RULE_MAX_ITEM_COUNT
    };

    friend rule make_by_type(rule::type_t t);

    static const char* get_type_name(type_t t);

public:
    rule() { this->init_before(RULE_TYPE_ANYCHAR); }
    explicit rule(char data)
    {
        this->init_before(RULE_TYPE_CHAR);
        this->m_data.ch = data;
    }

    explicit rule(char range_beg, char range_end)
    {
        this->init_before(RULE_TYPE_RANGE);
        this->m_data.ch_range[0] = range_beg;
        this->m_data.ch_range[1] = range_end;
    }

    explicit rule(const char* charset)
    {
        this->init_before(RULE_TYPE_CHARSET);
        this->m_strdata.assign(charset);
    }

    explicit rule(dictionary& d)
    {
        this->init_before(RULE_TYPE_DICTIONARY);
        this->m_data.dict = &d;
    }

    ~rule() {}
public:
    // NOTE
    // 如果一掉要对默认的拷贝构造函数做输出log，
    // 最好将log封装为一个成员；
    //
    // 然后默认构造函数就可以接管构造，而不用小心地每个成员做拷贝……
    rule(const rule& r) = default;
    // {
    //     SSS_LOG_DEBUG("%p from %p\n", this, r.get_ptr());
    //     m_type = r.m_type;
    //     std::memmove(&m_data, &r.m_data, sizeof(match_t));
    //     this->m_name = r.m_name;
    //     this->m_subs = r.m_subs;
    //     this->m_strdata = r.m_strdata;
    //     this->m_action = r.m_action;
    // }

    rule& operator=(const rule& r) = default;

public:
    // move
    rule(rule&& rhs) = default;
    rule& operator=(rule&& rhs) = default;

protected:
    const rule* get_ptr() const { return this; }
    rule* get_ptr() { return this; }
private:
    explicit rule(type_t type)
    {
        this->init_before(type);
        // std::memset(&this->m_data, 0, sizeof(this->m_data));
    }

    // explicit rule(rule * rhs)
    //     : m_type(RULE_OPERATOR_REFER)
    // {
    //     this->m_data.ref = rhs;
    // }

public:
    void accept(visitor& v) const;

protected:
    void init_before(type_t t)
    {
        this->m_type = t;
        this->m_anchor_used = bool(this->m_type & RULE_ANCHOR);
        //                 = this->m_type == RULE_ANCHOR_LINE_BEGIN
        //                 | this->m_type == RULE_ANCHOR_LINE_END
        //                 | this->m_type == RULE_ANCHOR_COL_POS
        //                 | this->m_type == RULE_ANCHOR_ROW_POS
        //                 | this->m_type == RULE_ANCHOR_COL_IF_TIMES;

        SSS_ZeroMem(this->m_data);
    }

    void push_back(const rule& r)
    {
        if (r.m_anchor_used) {
            this->m_anchor_used = true;
        }

        this->m_subs.push_back(r);
    }

    void push_back(rule&& r)
    {
        if (r.m_anchor_used) {
            this->m_anchor_used = true;
        }

        this->m_subs.push_back(std::move(r));
    }

public:
    size_t size() const { return this->m_subs.size(); }
    bool has_action() const
    {
#if __cplusplus >= 201103
        return this->m_action != 0;
#else
        return false;
#endif
    }

    void print(std::ostream&) const;

    void print_current(std::ostream& o) const;

protected:
    void simplify_push_back(rule::type_t t, rule& r, const rule& in) const
    {
        if (in.m_type == t && !in.has_action()) {
            if (in.m_anchor_used) {
                r.m_anchor_used = true;
            }
            std::copy(in.m_subs.begin(), in.m_subs.end(),
                      std::back_inserter(r.m_subs));
        }
        else {
            r.push_back(in);
        }
    }

public:
    // Not predicate. If the predicate a matches, fail. Otherwise, return a
    // zero length match.
    rule operator!() const
    {
        // SSS_LOG_FUNC_TRACE(sss::log::log_DEBUG);
        rule ret(RULE_OPERATOR_ASSERTNOT);
        ret.push_back(*this);
        return ret;
    }

    // And predicate. If the predicate a matches, return a zero length
    // match. Otherwise, fail.
    rule operator&() const
    {
        // SSS_LOG_FUNC_TRACE(sss::log::log_DEBUG);
        rule ret(RULE_OPERATOR_ASSERTTRUE);
        ret.push_back(*this);
        return ret;
    }

    // Optional. Parse a zero or one time
    rule operator-() const
    {
        // SSS_LOG_FUNC_TRACE(sss::log::log_DEBUG);
        rule ret(RULE_OPERATOR_MOSTONETIME);
        ret.push_back(*this);
        return ret;
    }

    // Kleene. Parse a zero or more times
    rule operator*() const
    {
        // SSS_LOG_FUNC_TRACE(sss::log::log_DEBUG);
        rule ret(RULE_OPERATOR_KLEENE_STAR);
        ret.push_back(*this);
        // SSS_LOG_EXPRESSION(sss::log::log_DEBUG, this)
        return ret;
    }

    // Plus. Parse a one or more times
    rule operator+() const
    {
        // SSS_LOG_FUNC_TRACE(sss::log::log_DEBUG);
        rule ret(RULE_OPERATOR_PLUSTIMES);
        ret.push_back(*this);
        return ret;
    }

    // Alternative. Parse a or b
    rule operator|(const rule& rhs) const
    {
        rule ret(RULE_OPERATOR_BRANCH);
        simplify_push_back(RULE_OPERATOR_BRANCH, ret, *this);
        simplify_push_back(RULE_OPERATOR_BRANCH, ret, rhs);
        return ret;
    }

    // Sequence. Parse a followed by b
    rule operator>>(const rule& rhs) const
    {
        // 关于ACTION的问题；
        // boost::spirit中，action可以用()来限定作用范围；
        // ( r1 >> r2 )[Action1]
        // 即，只有在(r1 >> r2)成功之后，才会调用Action1；
        //
        // ( r1 >> r2 )[Action1] >> r3
        // 此时，r1 和 r2 的组合，显然不能分开；
        rule ret(RULE_OPERATOR_CATENATE);
        simplify_push_back(RULE_OPERATOR_CATENATE, ret, *this);
        simplify_push_back(RULE_OPERATOR_CATENATE, ret, rhs);
        return ret;
    }

    // Expect. Parse a followed by b. b is expected to match when a
    // matches, otherwise, an expectation_failure is thrown.
    //
    // 可以串接！
    rule operator>(const rule& rhs) const
    {
        rule ret(RULE_OPERATOR_MUST_FOLLOWED);
        simplify_push_back(RULE_OPERATOR_MUST_FOLLOWED, ret, *this);
        simplify_push_back(RULE_OPERATOR_MUST_FOLLOWED, ret, rhs);
        return ret;
    }

    // Difference. Parse a but not b
    //
    // NOTE 可以串接！由于结合关系，只能左侧有限！因为，右侧，是不引用的！
    //
    // FIXME
    // 需要注意的是，对于 a - b，如果 b附带有action，可以忽略的！
    // 因为本质上，- 后面的，会忽略！
    rule operator-(const rule& rhs) const
    {
        // SSS_LOG_FUNC_TRACE(sss::log::log_DEBUG);
        rule ret(RULE_OPERATOR_DIFFERENCE);
        simplify_push_back(RULE_OPERATOR_DIFFERENCE, ret, *this);
        ret.push_back(rhs);  // NOTE 这里必须把右边当做一个整体处理！
        return ret;
    }

    // Sequential Or. Parse a or b or a followed by b
    rule operator||(const rule& rhs) const
    {
        // SSS_LOG_FUNC_TRACE(sss::log::log_DEBUG);
        rule ret(RULE_OPERATOR_SEQUENTIAL);
        simplify_push_back(RULE_OPERATOR_SEQUENTIAL, ret, *this);
        simplify_push_back(RULE_OPERATOR_SEQUENTIAL, ret, rhs);
        return ret;
    }

    // List. Parse a delimited b one or more times
    rule operator%(const rule& rhs) const
    {
        // SSS_LOG_FUNC_TRACE(sss::log::log_DEBUG);
        rule ret(RULE_OPERATOR_SEP_SUGAR);
        ret.push_back(*this);
        ret.push_back(rhs);
        return ret;
    }

    // Permutation. Parse a or b or a followed by b or b followed by a.
    rule operator^(const rule& rhs) const
    {
        // SSS_LOG_FUNC_TRACE(sss::log::log_DEBUG);
        rule ret(RULE_OPERATOR_PERMUTATION);
        simplify_push_back(RULE_OPERATOR_PERMUTATION, ret, *this);
        simplify_push_back(RULE_OPERATOR_PERMUTATION, ret, rhs);
        return ret;
    }

public:
    bool match(StrIterator& it_beg, StrIterator it_end, Status* p_st = 0) const;
    bool match(StrIterator& it_beg, StrIterator it_end, delayeval& d,
               Status* p_st = 0) const;

    bool match_delayeval(StrIterator& it_beg, StrIterator it_end) const;

    bool check(StrIterator it_beg, StrIterator it_end, Status* p_st = 0) const;
    bool check_full(StrIterator it_beg, StrIterator it_end,
                    Status* p_st = 0) const;

protected:
    bool do_match(StrIterator& it_beg, StrIterator it_end,
                  bool no_action = false, delayeval* p_delay = 0,
                  Status* = 0) const;

public:
    rule& name(const std::string& name)
    {
        this->m_name = name;
        return *this;
    }
    // rule& operator[](const std::string& name)
    // {
    //     this->m_name = name;
    //     return *this;
    // }

    rule operator[](ActionT func) const
    {
        rule ret(*this);
        ret.m_action = func;
        return ret;
    }
#if 0
        // matched_action_t list:
        template <typename T>
            rule result(typename std::enable_if<std::is_integral<T>::value, T>::type v) const
            {
                rule ret(*this);
                ret.m_matched_action = v;
                return ret;
            }

        rule result(const std::string& v) const
        {
            rule ret(*this);
            ret.m_matched_action = v;
            return ret;
        }

        rule result(std::function<uint64_t(StrIterator,StrIterator)> f)
        {
            rule ret(*this);
            ret.m_matched_action = f;
            return ret;
        }

        rule result(std::function<double(StrIterator,StrIterator)> f)
        {
            rule ret(*this);
            ret.m_matched_action = f;
            return ret;
        }

        rule result(std::function<std::string(StrIterator,StrIterator)> f)
        {
            rule ret(*this);
            ret.m_matched_action = f;
            return ret;
        }
#else
    template <typename T>
    rule result(T v) const
    {
        rule ret(*this);
        ret.m_matched_action = v;
        return ret;
    }
#endif

    rule& empty_action()
    {
        this->m_action = rule::null_action;
        return *this;
    }

    rule& action(ActionT func)
    {
        this->m_action = func;
        return *this;
    }

    rule action(ActionT func) const
    {
        rule ret(*this);
        ret.m_action = func;
        return ret;
    }

    template <typename T>
    rule& action(ActionT func, T res)
    {
        this->m_action = func;
        this->m_matched_action = res;
        return *this;
    }

    template <typename T>
    rule action(ActionT func, T res) const
    {
        rule ret(*this);
        ret.m_action = func;
        ret.m_matched_action = res;
        return ret;
    }

private:
    // type part
    type_t m_type;
    bool m_anchor_used;

    // data part
    union match_t {
        CHAR_T ch;
        CHAR_T ch_range[2];
        rule* ref;
        dictionary* dict;
        CTypeFuncT ctypeFunc;
    } m_data;
    std::string m_strdata;
    std::vector<uint32_t> m_setdata;

    // name optional
    std::string m_name;

    // subrules
    std::vector<rule> m_subs;

    // actions
    ActionT m_action;
    matched_action_t m_matched_action;
    std::function<bool(int)> m_col_fun;
    DoMatchActionT m_domatch_action;

    // TODO
    // 这样和m_action并列，太浪费空间了；
};

// FIXME 2016-01-13 全局对象的构造函数中，不能有sss::log先关函数
// 全局对象依赖关系问题；
extern const rule eof_p;

extern const rule char_;

extern const rule utf8_;

//---------------------------------------

extern const rule line_begin_p;

extern const rule line_end_p;

//---------------------------------------

extern const rule alnum_p;
extern const rule alpha_p;

extern const rule blank_p;  // 仅针对空格、制表符等空白符
extern const rule space_p;  // 而space，还包括不显示的字符，比如 \f, \r, \n

extern const rule cntrl_p;
extern const rule digit_p;
extern const rule punct_p;
extern const rule xdigit_p;
extern const rule lower_p;
extern const rule upper_p;

extern const rule cidentifier_p;

extern const rule double_p;

extern const rule chinese_normal_p;

// ----------------------------------------------------------------------

inline std::ostream& operator<<(std::ostream& o, const rule& r)
{
    r.print(o);
    return o;
}

inline rule refer(rule& rhs)
{
    parser::rule ret(rule::RULE_OPERATOR_REFER);
    if (rhs.m_type == rule::RULE_OPERATOR_REFER) {
        ret = std::move(rhs);
    }
    else {
        ret.m_data.ref = const_cast<parser::rule*>(rhs.get_ptr());
        ret.m_anchor_used = rhs.m_anchor_used;
        ret.m_matched_action = rhs.m_matched_action;
    }
    return ret;
}

//
/**
 * @brief create rule a{m, n}.
 * which match `a` times, from m to n; greedy
 *
 * @param [in] ref
 * @param [in] low
 * @param [in] high   -1 means INF
 *
 * @return the range parttern
 *
 * or change range() -> times()
 */
inline rule range(const rule& ref, int low, int high = -1)
{
    if (low < 0) {
        low = 0;
    }
    if (high < -1) {
        high = -1;
    }

    if (high > 0 && high < low) {
        SSS_POSITION_THROW(std::logic_error,
                          "range high " , high , " less than low " , low);
    }
    rule ret(rule::RULE_OPERATOR_RANGETIMES);
    ret.m_data.ch_range[0] = low & 0xFF;
    ret.m_data.ch_range[1] = high & 0xFF;
    ret.push_back(ref);
    return ret;
}

// 生成一个不消耗任何东西的匹配器 —— epsilon；
// 可以捆绑action，以完成初始化工作；
inline rule eps()
{
    rule ret(rule::RULE_TYPE_NONE);
    return ret;
}

// has reach the end of stream ?
inline rule eof() { return parser::rule().operator!(); }
inline rule sequence(const std::string& seq)
{
    rule ret(rule::RULE_TYPE_SEQUENCE);
    ret.m_strdata = seq;
    return ret;
}

inline rule char_p(char ch) { return rule(ch); }
inline rule char_range_p(char ch_low, char ch_high)
{
    return rule(ch_low, ch_high);
}

rule utf8_set_p(const char* utf8ch);

inline rule char_set_p(const char* s) { return utf8_set_p(s); }
// TODO
inline rule utf8_p(const char* u8s)
{
    rule ret(rule::RULE_TYPE_CHAR);
    int len = 0;
    std::tie(ret.m_data.ch, len) = sss::util::utf8::peek(u8s);
    if (!len) {
        ret.m_type = rule::RULE_TYPE_ANYCHAR;
    }

    return ret;
}

inline rule utf8_set_p(const char* utf8ch)
{
    const char* u8_beg = utf8ch;
    const char* u8_end = std::strchr(utf8ch, '\0');

    uint32_t uch = 0u;

    rule ret(rule::RULE_TYPE_CHARSET);
    while (sss::util::Parser<const char*>::parseUtf8Any(u8_beg, u8_end, uch)) {
        ret.m_setdata.push_back(uch);
    }
    std::sort(ret.m_setdata.begin(), ret.m_setdata.end());
    ret.m_setdata.erase(std::unique(ret.m_setdata.begin(), ret.m_setdata.end()),
                        ret.m_setdata.end());
    return ret;
}

inline rule utf8_range_p(const char* utf8_low, const char* utf8_high)
{
    int len1 = 0;
    int len2 = 0;
    rule ret(rule::RULE_TYPE_RANGE);
    std::tie(ret.m_data.ch_range[0], len1) = sss::util::utf8::peek(utf8_low);
    std::tie(ret.m_data.ch_range[1], len2) = sss::util::utf8::peek(utf8_high);

    return ret;
}

// ----------------------------------------------------------------------

inline int match(const parser::rule& r, const std::string& s,
                 ss1x::parser::delayeval& d)
{
    StrIterator it_beg = s.begin();
    StrIterator it_end = s.end();
    r.match(it_beg, it_end, d);
    return std::distance(s.cbegin(), it_beg);
}

inline int match(const parser::rule& r, const std::string& s)
{
    StrIterator it_beg = s.begin();
    StrIterator it_end = s.end();
    r.match(it_beg, it_end);
    return std::distance(s.cbegin(), it_beg);
}

inline bool is_full_match(const parser::rule& r, const std::string& s)
{
    StrIterator it_beg = s.begin();
    StrIterator it_end = s.end();
    return r.match(it_beg, it_end) && it_beg == it_end;
}

inline rule on_fail(const rule& r, rule::ActionT func)
{
    return r | parser::eps()[func];
}

inline rule dict_p(dictionary& d) { return rule(d); }
inline rule row_p(int row)
{
    if (row < 0) {
        row = 0;
    }
    rule ret(rule::RULE_ANCHOR_ROW_POS);
    ret.m_data.ch = row;
    return ret;
}

inline rule col_p(int col)
{
    if (col < 0) {
        col = 0;
    }
    rule ret(rule::RULE_ANCHOR_COL_POS);
    ret.m_data.ch = col;
    return ret;
}

inline rule col_if_times_p(const rule& r, std::function<bool(int)> fun)
{
    rule ret(rule::RULE_ANCHOR_COL_IF_TIMES);
    ret.m_col_fun = fun;
    ret.push_back(r);
    return ret;
}

inline rule user_domtch(rule::DoMatchActionT domatch_action)
{
    rule ret(rule::RULE_DOMATCH_ACTION);
    ret.m_domatch_action = domatch_action;
    return ret;
}

// TODO FIXME
// 是否制作，双元操作符重载，比如rule | '-' 这种？
//
// 关键在于，- 'a'，这种表达式，就算不考虑解析器库，它也是合法的；
//
// 万一用户的想法，真的是将小写字母'a'取相反数呢？

}  // namespace parser
}  // namespace ss1x

#endif /* __OPARSER_HPP_1452140160__ */
