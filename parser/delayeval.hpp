#ifndef __DELAYEVAL_HPP_1452659613__
#define __DELAYEVAL_HPP_1452659613__

/**
 * 延迟计算类.
 *
 * NOTE
 * 这个类，其实并不是禁用action(预判部分，默认会禁用的)；而是将action动作的调用
 * ，修改为动作序列的push；
 * 因此真正的match动作，需要额外两个控制参数，一个是delayeval指针，一个是bool
 * no_action
 *
 * public:
 *   bool rule::match(Iterator& it_beg, Iterator it_end);
 *   bool rule::match(Iterator& it_beg, Iterator it_end, delayeval& delay);
 *   // 检查；不触发动作，也不延迟
 *   bool rule::check(Iterator it_beg, Iterator it_end);
 *   // 检查是否完全匹配——无剩余
 *   bool rule::check_full(Iterator it_beg, Iterator it_end);
 *
 * protected:
 *   rule::do_match(Iterator& it_beg, Iterator it_end, bool no_action,
 * delayeval* p_delay);
 *
 */

#include "oparser.hpp"

#include <sss/util/StringSlice.hpp>

#include <functional>
#include <list>
#include <utility>
#include <vector>

namespace ss1x {
namespace parser {

class rule;

class delayeval {
    typedef std::string::const_iterator StrIterator;

public:
    delayeval();

    ~delayeval();

    delayeval(const delayeval& rhs);

    // 要使用移动构造函数的话，必须同时提供拷贝构造函数
    // 要让编译器选择使用移动构造的话，
    // 必须要显式使用std::move()
    delayeval(delayeval&& rhs);

public:
    delayeval& operator=(delayeval&& rhs);

    bool is_init() const { return bool(this->m_rul_ptr); }
    void set_end_pos(StrIterator it_end) { this->m_it_end = it_end; }
    void set_user_data(const rule::matched_value_t& ud)
    {
        this->m_usr_data = ud;
    }

    void eval() const;

    delayeval& assign(const rule* p_rule, StrIterator it_beg,
                      StrIterator it_end, rule::ActionT action,
                      rule::matched_value_t usr_data);

    delayeval& push_back(const rule* p_rule, StrIterator it_beg,
                         StrIterator it_end, rule::ActionT action,
                         rule::matched_value_t usr_data);

    delayeval& push_back(const delayeval& sub_delay);
    delayeval& push_back(delayeval&& sub_delay);

    size_t size() const { return this->m_subs.size(); }
    delayeval& operator[](int i) { return this->m_subs[i]; }
    void print_userdata(std::ostream& o) const;

    template <typename T>
    T getUserData() const
    {
        // std::cout << __func__ << " " << this->m_usr_data.which() <<
        // std::endl;
        // std::cout << sss::util::make_slice(m_it_beg, m_it_end) << std::endl;
        return boost::get<T>(this->m_usr_data);
    }

    rule::matched_value_t get_userdata() const { return this->m_usr_data; }
    sss::util::StringSlice<StrIterator> get_slice() const
    {
        return sss::util::make_slice(this->m_it_beg, this->m_it_end);
    }

protected:
    void eval_inner() const;

private:
    const rule* m_rul_ptr;
    StrIterator m_it_beg;
    StrIterator m_it_end;
    rule::ActionT m_action;
    rule::matched_value_t m_usr_data;

    typedef std::vector<delayeval> SubsT;
    SubsT m_subs;
};
}  // namespace parser
}  // namespace ss1x

#endif /* __DELAYEVAL_HPP_1452659613__ */
