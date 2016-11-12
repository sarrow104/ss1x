#include "oparser.hpp"
#include "delayeval.hpp"
#include "dictionary.hpp"
#include "exception.hpp"
#include "status.hpp"
#include "util.hpp"
#include "visitor.hpp"

#include <sss/util/PostionThrow.hpp>
#include <stdexcept>

#include <cctype>

namespace ss1x {
namespace parser {

// 如果只是想获取匹配位置的值，可以使用这个空action
// 当然，还可以用delayeval
const rule::ActionT rule::null_action = [](StrIterator, StrIterator,
                                           rule::matched_value_t) -> void {};

rule::matched_action_t rule::slice2string_action =
    std::function<std::string(StrIterator, StrIterator)>(&util::slice2string);

rule::matched_action_t rule::slice2double_action =
    std::function<double(StrIterator, StrIterator)>(&util::slice2double);

void rule::accept(visitor& v) const { v.visit(this); }
void rule::print(std::ostream& o) const
{
    visitor v(o);
    this->accept(v);
    o << std::endl;
}

void rule::print_current(std::ostream& o) const
{
    visitor v(o, true);
    this->accept(v);
    o << std::endl;
}

const char* rule::get_type_name(type_t t)
{
    typedef std::map<type_t, const char*> name_table_t;
    static name_table_t name_table;
    if (name_table.empty()) {
#define DO_ADD(a)           \
    case a:                 \
        name_table[a] = #a; \
        break;
        for (int i = 0; i != RULE_MAX_ITEM_COUNT; ++i) {
            switch (type_t(i)) {
                DO_ADD(RULE_TYPE_NONE);
                DO_ADD(RULE_TYPE_ANYCHAR);
                DO_ADD(RULE_TYPE_CHAR);
                DO_ADD(RULE_TYPE_RANGE);
                DO_ADD(RULE_TYPE_CHARSET);

                DO_ADD(RULE_TYPE_SEQUENCE);
                DO_ADD(RULE_TYPE_DICTIONARY);

                DO_ADD(RULE_OPERATOR_ASSERTNOT);
                DO_ADD(RULE_OPERATOR_ASSERTTRUE);
                DO_ADD(RULE_OPERATOR_MOSTONETIME);
                DO_ADD(RULE_OPERATOR_KLEENE_STAR);
                DO_ADD(RULE_OPERATOR_PLUSTIMES);
                DO_ADD(RULE_OPERATOR_RANGETIMES);

                DO_ADD(RULE_OPERATOR_BRANCH);
                DO_ADD(RULE_OPERATOR_CATENATE);
                DO_ADD(RULE_OPERATOR_MUST_FOLLOWED);

                DO_ADD(RULE_OPERATOR_DIFFERENCE);
                DO_ADD(RULE_OPERATOR_SEQUENTIAL);

                DO_ADD(RULE_OPERATOR_SEP_SUGAR);
                DO_ADD(RULE_OPERATOR_PERMUTATION);

                DO_ADD(RULE_OPERATOR_REFER);
                DO_ADD(RULE_DOMATCH_ACTION);

                DO_ADD(RULE_ANCHOR_LINE_BEGIN);
                DO_ADD(RULE_ANCHOR_LINE_END);
                DO_ADD(RULE_ANCHOR_ROW_POS);
                DO_ADD(RULE_ANCHOR_COL_POS);
                DO_ADD(RULE_ANCHOR_COL_IF_TIMES);

                DO_ADD(RULE_CTYPE_WRAPPER);

                DO_ADD(RULE_MAX_ITEM_COUNT);
            }
        }
#undef DO_ADD
    }
    std::map<type_t, const char*>::const_iterator it = name_table.find(t);
    if (it != name_table.end()) {
        return it->second;
    }
    else {
        return "";
    }
}

bool rule::match(StrIterator& it_beg, StrIterator it_end, Status* p_st) const
{
    if (p_st) {
        p_st->init(it_beg, it_end);
    }
    return this->do_match(it_beg, it_end, false, 0, p_st);
}

// NOTE 禁用动作的同时，再绑定一个delayeval对象，是没有意义的！
// 起码这种入口函数是没用的——当然，内部比如否定预判可能会，但那绝对是代码写错了！
// 如果未禁用action，也就是no_action == false；
// 同时，delayeval 没有传入，当然也是正确的；
// 因为这本来就是默认行为；
bool rule::match(StrIterator& it_beg, StrIterator it_end, delayeval& d,
                 Status* p_st) const
{
    if (p_st) {
        p_st->init(it_beg, it_end);
    }
    return this->do_match(it_beg, it_end, false, &d, p_st);
}

bool rule::match_delayeval(StrIterator& it_beg, StrIterator it_end) const
{
    ss1x::parser::delayeval d;
    bool ret = this->match(it_beg, it_end, d);
    if (ret) {
        d.eval();
    }
    return ret;
}

bool rule::check(StrIterator it_beg, StrIterator it_end, Status* p_st) const
{
    // std::cout << "@" << this << " m_anchor_used = " << this->m_anchor_used <<
    // std::endl;
    Status st;
    // NOTE
    // this->m_anchor_used
    // 这个量，其实是不准的！
    // 最安全的办法，是每次运行，检查一次；
    // 因为规则可能被重新组合；
    //
    // 当然，只从规则集合中的某一个开始，不一定能修改到所有规则的m_anchor_used的树形；
    // 因为我这个"规则树"，看起来是树，但实际上，分支节点是没法回溯到分
    // 叉的节点；
    //
    // 而且，还有引用这个规则进行跳转；
    //
    // 甚至，最近，还添加了 DoMatchActionT，用来替换某个规则的匹配算法；
    if (this->m_anchor_used) {
        if (!p_st) {
            p_st = &st;
        }
        p_st->init(it_beg, it_end);
    }
    return this->do_match(it_beg, it_end, true, 0, p_st);
}

bool rule::check_full(StrIterator it_beg, StrIterator it_end,
                      Status* p_st) const
{
    // std::cout << "@" << this << " m_anchor_used = " << this->m_anchor_used <<
    // std::endl;
    Status st;
    if (this->m_anchor_used) {
        if (!p_st) {
            p_st = &st;
        }
        p_st->init(it_beg, it_end);
    }
    return this->do_match(it_beg, it_end, true, 0, p_st) && it_beg == it_end;
}

bool rule::do_match(StrIterator& it_beg, StrIterator it_end, bool no_action,
                    delayeval* p_delay, Status* p_st) const
{
    SSS_LOG_FUNC_TRACE(sss::log::log_DEBUG);
    SSS_LOG_EXPRESSION(sss::log::log_DEBUG, this);
    SSS_LOG_EXPRESSION(sss::log::log_DEBUG, *it_beg);
    SSS_LOG_EXPRESSION(sss::log::log_DEBUG, rule::get_type_name(this->m_type));

    assert(no_action || bool(p_delay) || (!no_action && !p_delay));

    rule::matched_value_t user_data;
    bool ret = false;

    util::delayeval_helper dh(this, it_beg, this->m_action, user_data,
                              no_action, p_delay, p_st, ret);

    StrIterator it_beg_sv = it_beg;

    switch (this->m_type) {
        case RULE_TYPE_NONE:  // {{{1
            ret = true;
            break;

        case RULE_TYPE_ANYCHAR:  // {{{1
        {
            uint32_t ch;
            ret = Parser::parseUtf8Any(it_beg, it_end, ch);

            if (ret && m_action) {
                if (m_matched_action.which()) {
                    util::assign_result(this, user_data, m_action,
                                        m_matched_action, it_beg_sv, it_beg);
                }
                else {
                    user_data = uint64_t(ch);
                }
            }
        } break;

        case RULE_TYPE_CHAR:  // {{{1
        {
            // StrIterator it_beg_tmp = it_beg;
            ret = Parser::parseUtf8(it_beg, it_end, this->m_data.ch);

            if (ret && m_action) {
                if (m_matched_action.which()) {
                    util::assign_result(this, user_data, m_action,
                                        m_matched_action, it_beg_sv, it_beg);
                }
                else {
                    user_data = uint64_t(this->m_data.ch);
                }
            }
        } break;

        case RULE_TYPE_RANGE:  // {{{1
        {
            uint32_t ch = 0;
            ret =
                Parser::parseRangeUtf8(it_beg, it_end, this->m_data.ch_range[0],
                                       this->m_data.ch_range[1], ch);
            if (ret && m_action) {
                if (m_matched_action.which()) {
                    util::assign_result(this, user_data, m_action,
                                        m_matched_action, it_beg_sv, it_beg);
                }
                else {
                    user_data = uint64_t(ch);
                }
            }
        } break;

        case RULE_TYPE_CHARSET:  // {{{1
        {
            // uint32_t ch;
            std::pair<uint32_t, int> st = sss::util::utf8::peek(it_beg, it_end);
            if (st.second) {
                const auto set_it = std::lower_bound(
                    this->m_setdata.cbegin(), this->m_setdata.cend(), st.first);

                if (set_it != this->m_setdata.cend() && *set_it == st.first) {
                    std::advance(it_beg, st.second);
                    ret = true;
                    if (ret && m_action) {
                        if (m_matched_action.which()) {
                            util::assign_result(this, user_data, m_action,
                                                m_matched_action, it_beg_sv,
                                                it_beg);
                        }
                        else {
                            user_data = uint64_t(st.first);
                        }
                    }
                }
            }
        } break;

        case RULE_TYPE_SEQUENCE:  // {{{1
        {
            ret = Parser::parseSequence(it_beg, it_end, this->m_strdata);
            if (ret && m_action) {
                if (m_matched_action.which()) {
                    util::assign_result(this, user_data, m_action,
                                        m_matched_action, it_beg_sv, it_beg);
                }
                else {
                    user_data = this->m_strdata;
                }
            }
        } break;

        case RULE_TYPE_DICTIONARY:  // {{{1
        {
            int value = 0;
            ret = this->m_data.dict->match(it_beg, it_end, value);
            if (ret && m_action) {
                if (m_matched_action.which()) {
                    util::assign_result(this, user_data, m_action,
                                        m_matched_action, it_beg_sv, it_beg);
                }
                else {
                    user_data = uint64_t(value);
                }
            }
        } break;

        case RULE_ANCHOR_LINE_BEGIN:  // {{{1
            // NOTE LINE_BEGIN和LINE_END这个两种锚点；其特点和eps类似，就是不消
            // 耗任何字符！
            // eps很简单，任何情况下，它都返回true；
            //
            // LINE_BEGIN的话，当前，迭代器代表刚刚过了一个换行符，或者是流的开始；
            // LINE_END的话，这意味着，下一个，还未读取的是就是换行符，或者是流
            // 的结束——没有后续字节了；
            if (!p_st) {
                SSS_POSTION_THROW(
                    std::runtime_error,
                    "To use RULE_ANCHOR_XXX, U must supply Stauts");
            }
            ret = p_st->is_begin(it_beg);
            if (ret) {
                util::assign_result(this, user_data, m_action, m_matched_action,
                                    it_beg_sv, it_beg);
            }
            break;

        case RULE_ANCHOR_LINE_END:  // {{{1
            if (!p_st) {
                SSS_POSTION_THROW(
                    std::runtime_error,
                    "To use RULE_ANCHOR_XXX, U must supply Stauts");
            }
            ret = p_st->is_end(it_beg);
            if (ret) {
                util::assign_result(this, user_data, m_action, m_matched_action,
                                    it_beg_sv, it_beg);
            }
            break;

        case RULE_ANCHOR_ROW_POS:
            if (!p_st) {
                SSS_POSTION_THROW(
                    std::runtime_error,
                    "To use RULE_ANCHOR_XXX, U must supply Stauts");
            }
            ret = (p_st->calc_coord(it_beg).first == int(this->m_data.ch));
            if (ret) {
                util::assign_result(this, user_data, m_action, m_matched_action,
                                    it_beg_sv, it_beg);
            }
            break;

        case RULE_ANCHOR_COL_POS:
            if (!p_st) {
                SSS_POSTION_THROW(
                    std::runtime_error,
                    "To use RULE_ANCHOR_XXX, U must supply Stauts");
            }
            ret = (p_st->calc_coord(it_beg).second == int(this->m_data.ch));
            if (ret) {
                util::assign_result(this, user_data, m_action, m_matched_action,
                                    it_beg_sv, it_beg);
            }
            break;

        case RULE_ANCHOR_COL_IF_TIMES:
            if (!p_st) {
                SSS_POSTION_THROW(
                    std::runtime_error,
                    "To use RULE_ANCHOR_XXX, U must supply Stauts");
            }
            {
                assert(this->m_subs.size() == 1);

                Parser::Rewinder r(it_beg);
                while (this->m_subs[0].do_match(it_beg, it_end, no_action,
                                                dh.get_delay_ptr(), p_st)) {
                    if (this->m_col_fun(p_st->calc_coord(it_beg).second)) {
                        ret = true;
                        break;
                    }
                }
                r.commit(ret);
                if (ret) {
                    util::assign_result(this, user_data, m_action,
                                        m_matched_action, it_beg_sv, it_beg);
                }
            }
            break;

        case RULE_OPERATOR_ASSERTNOT:  // {{{1
        {
            // 求假预判；
            // 并且不消耗
            assert(this->m_subs.size() == 1);
            StrIterator it_beg_sv = it_beg;
            ret = !this->m_subs[0].do_match(it_beg_sv, it_end, true, 0, p_st);
            if (ret) {
                util::assign_result(this, user_data, m_action, m_matched_action,
                                    it_beg_sv, it_beg);
            }
        } break;

        case RULE_OPERATOR_ASSERTTRUE:  // {{{1
        {
            // 求真预判
            // 并且不消耗
            assert(this->m_subs.size() == 1);
            StrIterator it_beg_sv = it_beg;
            ret = this->m_subs[0].do_match(it_beg_sv, it_end, true, 0, p_st);
            if (ret) {
                util::assign_result(this, user_data, m_action, m_matched_action,
                                    it_beg_sv, it_beg);
            }
        } break;

        case RULE_OPERATOR_MOSTONETIME:  // {{{1
        {
            Parser::Rewinder r(it_beg);
            assert(this->m_subs.size() == 1);
            r.commit(this->m_subs[0].do_match(it_beg, it_end, no_action,
                                              dh.get_delay_ptr(), p_st));

            // always succeed.
            ret = true;
            if (ret) {
                util::assign_result(this, user_data, m_action, m_matched_action,
                                    it_beg_sv, it_beg);
            }
        } break;

        case RULE_OPERATOR_DIFFERENCE:  // A not B
        {
            Parser::Rewinder r(it_beg);
            assert(this->m_subs.size() >= 2);
            StrIterator it_beg_togo = it_beg;

            if (this->m_subs[0].do_match(it_beg_togo, it_end, no_action,
                                         dh.get_delay_ptr(), p_st)) {
                bool has_failed = false;
                for (size_t i = 1; i != this->m_subs.size(); ++i) {
                    StrIterator it_beg_fail = it_beg;
                    if (this->m_subs[i].do_match(it_beg_fail, it_end, true, 0,
                                                 p_st)) {
                        has_failed = true;
                        break;
                    }
                }
                if (!has_failed) {
                    it_beg = it_beg_togo;
                    ret = true;
                }
                if (ret) {
                    util::assign_result(this, user_data, m_action,
                                        m_matched_action, it_beg_sv, it_beg);
                }
            }
        } break;

        case RULE_OPERATOR_KLEENE_STAR:  // {{{1
        {
            Parser::Rewinder r(it_beg);
            assert(this->m_subs.size() == 1);
            while (true) {
                r.begin();
                r.commit(this->m_subs[0].do_match(it_beg, it_end, no_action,
                                                  dh.get_delay_ptr(), p_st));
                if (!r.is_commited()) {
                    break;
                }
            }

            // always succeed.
            ret = true;
            if (ret) {
                util::assign_result(this, user_data, m_action, m_matched_action,
                                    it_beg_sv, it_beg);
            }
        } break;

        case RULE_OPERATOR_PLUSTIMES:  // {{{1
        {
            Parser::Rewinder r(it_beg);
            assert(this->m_subs.size() == 1);
            if (r.commit(this->m_subs[0].do_match(it_beg, it_end, no_action,
                                                  dh.get_delay_ptr(), p_st))) {
                while (true) {
                    r.begin();
                    r.commit(this->m_subs[0].do_match(
                        it_beg, it_end, no_action, dh.get_delay_ptr(), p_st));
                    if (!r.is_commited()) {
                        break;
                    }
                }
                ret = true;
            }
            if (ret) {
                util::assign_result(this, user_data, m_action, m_matched_action,
                                    it_beg_sv, it_beg);
            }
        } break;

        case RULE_OPERATOR_RANGETIMES:  // {{{1
        {
            Parser::Rewinder r(it_beg);
            assert(this->m_subs.size() == 1);
            int low = this->m_data.ch_range[0];
            int high = this->m_data.ch_range[1];
            assert(low >= 0 && (low <= high || high == -1));
            if (low) {
                StrIterator it_beg_sv = it_beg;
                while (low &&
                       this->m_subs[0].do_match(it_beg_sv, it_end, no_action,
                                                dh.get_delay_ptr(), p_st)) {
                    low--;
                    if (high > 0) {
                        high--;
                    }
                }
                if (!low) {
                    it_beg = it_beg_sv;
                }
            }
            if (low == 0) {
                while (high != 0) {
                    r.begin();
                    r.commit(this->m_subs[0].do_match(
                        it_beg, it_end, no_action, dh.get_delay_ptr(), p_st));
                    if (!r.is_commited()) {
                        break;
                    }
                    if (high != -1) {
                        --high;
                    }
                }
                ret = true;
            }
            if (ret) {
                util::assign_result(this, user_data, m_action, m_matched_action,
                                    it_beg_sv, it_beg);
            }
        } break;

        case RULE_OPERATOR_BRANCH:  // {{{1
        {
            Parser::Rewinder r(it_beg);
            bool match_succeed = false;
            for (size_t i = 0; i != this->m_subs.size(); ++i) {
                r.commit(this->m_subs[i].do_match(it_beg, it_end, no_action,
                                                  dh.get_delay_ptr(), p_st));
                if (r.is_commited()) {
                    match_succeed = true;
                    break;
                }
            }
            ret = match_succeed;

            if (ret) {
                util::assign_result(this, user_data, m_action, m_matched_action,
                                    it_beg_sv, it_beg);
            }
        } break;

        case RULE_OPERATOR_CATENATE:  // {{{1
        {
            Parser::Rewinder r(it_beg);
            bool match_failed = false;
            for (size_t i = 0; i != this->m_subs.size(); ++i) {
                if (!this->m_subs[i].do_match(it_beg, it_end, no_action,
                                              dh.get_delay_ptr(), p_st)) {
                    match_failed = true;
                    break;
                }
            }
            r.commit(!match_failed);
            ret = !match_failed;
            if (ret) {
                util::assign_result(this, user_data, m_action, m_matched_action,
                                    it_beg_sv, it_beg);
            }
        } break;

        case RULE_OPERATOR_MUST_FOLLOWED:  // {{{1
        {
            Parser::Rewinder r(it_beg);
            assert(this->m_subs.size() >= 2u);
            if (this->m_subs[0].do_match(it_beg, it_end, no_action,
                                         dh.get_delay_ptr(), p_st)) {
                for (size_t i = 1; i != this->m_subs.size(); ++i) {
                    if (!this->m_subs[i].do_match(it_beg, it_end, no_action,
                                                  dh.get_delay_ptr(), p_st)) {
                        if (!no_action) {
                            StrIterator sample_end =
                                std::find(it_beg, it_end, '\n');
                            std::pair<int, int> coord = {0, 0};
                            if (p_st) {
                                coord = p_st->calc_coord(it_beg);
                            }
                            // FIXME
                            // 虽然只是输出出问题位置的解析器；
                            // 但是，由于解析器，可能是一堆具名解析器的组合(通
                            // 过引用)；所以完全有可能从一个点，也得到一大堆的输出——
                            // 我的建议：
                            // 1.
                            // 做一个输出规则用对象包裹器，即，在输出的时候，不跟踪引用；
                            // 2. 一个通用的裁剪工具——只输出部分，比如一行。
                            //    std::ostream 的 setw()有用没有？
                            //    好像对这里没用。
                            //    因为转嫁的是rule::print, -> visitor……
                            // 3. 做一个深度限制。
                            SSS_POSTION_ARGS_THROW(
                                ErrorPosition,
                                sss::rope_string(
                                    "@", this->m_subs[i].get_ptr(), " from: ",
                                    this->get_type_name(this->m_type), " @",
                                    this, "\nleft sample=`",
                                    sss::util::make_slice(it_beg, sample_end),
                                    "`"),
                                this->m_subs[i].get_ptr(), it_beg, coord);
                        }
                        break;
                    }
                }
                ret = true;
                r.commit(true);
            }
            if (ret) {
                util::assign_result(this, user_data, m_action, m_matched_action,
                                    it_beg_sv, it_beg);
            }
        } break;

        case RULE_OPERATOR_SEQUENTIAL:  // {{{1
            // equals to : a >> -b | b; NOTE | 优先级 ">>" 大于 |;
            // 所以，这个式子，等于： (a >> -b) | b; 另外，-b 是 匹配 b
            // 0-1次的意思……
            //! http://www.boost.org/doc/libs/1_60_0/libs/spirit/doc/html/spirit/qi/reference/operator/sequential_or.html
            // test_parser("123.456", int_ || ('.' >> int_));  // full
            // test_parser("123", int_ || ('.' >> int_));      // just the whole
            // number
            // test_parser(".456", int_ || ('.' >> int_));     // just the
            // fraction
            //
            // 连续：
            // a || b || c；
            //
            // 根据从左到右的结合性
            // -> (a || b) || c
            // -> (a || b) >> -c | c
            // -> (a >> -b | b) >> -c | c
            {
                Parser::Rewinder r(it_beg);
                assert(this->m_subs.size() >= 2);
                bool is_prev_ok = r.commit(this->m_subs[0].do_match(
                    it_beg, it_end, no_action, dh.get_delay_ptr(), p_st));
                for (size_t i = 1; i != this->m_subs.size(); ++i) {
                    Parser::Rewinder r2(it_beg);
                    Parser::Rewinder r3(it_beg);
                    is_prev_ok = r2.commit(is_prev_ok &&
                                           ((r3.begin() &&
                                             r3.commit(this->m_subs[i].do_match(
                                                 it_beg, it_end, no_action,
                                                 dh.get_delay_ptr(), p_st))) ||
                                            true)) ||
                                 r2.commit(this->m_subs[i].do_match(
                                     it_beg, it_end, no_action,
                                     dh.get_delay_ptr(), p_st));
                }
                ret = is_prev_ok;
                if (ret) {
                    util::assign_result(this, user_data, m_action,
                                        m_matched_action, it_beg_sv, it_beg);
                }
            }
            break;

        case RULE_OPERATOR_SEP_SUGAR:  // a % b {{{1
        {
            Parser::Rewinder r(it_beg);
            Parser::Rewinder r2(it_beg);
            assert(this->m_subs.size() == 2);
            if (r.commit(this->m_subs[0].do_match(it_beg, it_end, no_action,
                                                  dh.get_delay_ptr(), p_st))) {
                while (true) {
                    r2.begin();
                    r2.commit(
                        this->m_subs[1].do_match(it_beg, it_end, true, 0,
                                                 p_st) &&
                        this->m_subs[0].do_match(it_beg, it_end, no_action,
                                                 dh.get_delay_ptr(), p_st));
                    if (!r2.is_commited()) {
                        break;
                    }
                }
                ret = true;
            }
            if (ret) {
                util::assign_result(this, user_data, m_action, m_matched_action,
                                    it_beg_sv, it_beg);
            }
        } break;

        case RULE_OPERATOR_PERMUTATION:  // {{{1
            // the permutation operator, a ^ b, matches one or more operands
            // (a, b, ... etc.) in any order:
            //
            // a ^ b ^ ...
            //
            // The operands are the elements in the permutation set. Each
            // element in the permutation set may occur at most once, but not
            // all elements of the given set need to be present. Note that by
            // this definition, the permutation operator is not limited to
            // strict permutations.
            //
            // For example:
            //
            // char_('a') ^ 'b' ^ 'c'
            //
            // matches:
            //
            // "a", "ab", "abc", "cba", "bca" ... etc.
            //
            // 相当于组合数
            {
                Parser::Rewinder r(it_beg);
                Parser::Rewinder r2(it_beg);
                assert(this->m_subs.size() >= 2);
                // 找出重复元素？
                // 异或法，找出重复数，见
                //! http://www.cnblogs.com/Ivony/archive/2009/07/23/1529254.html
                // 令，1^2^...^1000（序列中不包含n）的结果为T
                // 则1^2^...^1000（序列中包含n）的结果就是T^n。
                // T^(T^n)=n。
                // 所以，将所有的数全部异或，得到的结果与1^2^3^...^1000的结果进
                // 行异或，得到的结果就是重复数。
                std::set<size_t> used_sub_idx;
                bool has_new_found = false;
                do {
                    for (size_t i = 0; i != this->m_subs.size(); ++i) {
                        if (used_sub_idx.find(i) != used_sub_idx.end()) {
                            continue;
                        }
                        if (this->m_subs[i].do_match(it_beg, it_end, no_action,
                                                     dh.get_delay_ptr(),
                                                     p_st)) {
                            used_sub_idx.insert(i);
                            has_new_found = true;
                        }
                    }
                } while (has_new_found);
                ret = !used_sub_idx.empty();
                if (ret) {
                    util::assign_result(this, user_data, m_action,
                                        m_matched_action, it_beg_sv, it_beg);
                }
            }
            break;

        case RULE_CTYPE_WRAPPER:  // {{{1
        {
            assert(this->m_data.ctypeFunc);
#if 1
            // NOTE
            // RULE_CTYPE_WRAPPER 是对 cctype 中 isxxxx函数的封装；
            // 这些函数，看似接受int类型参数，实际的值域是：
            //  unsigned char or EOF —— 根据 glibc-man
            if (it_beg != it_end && this->m_data.ctypeFunc(*it_beg) != 0) {
                it_beg++;
                ret = true;
                if (ret && m_action) {
                    if (m_matched_action.which()) {
                        util::assign_result(this, user_data, m_action,
                                            m_matched_action, it_beg_sv,
                                            it_beg);
                    }
                    else {
                        user_data = uint64_t(*it_beg_sv);
                    }
                }
            }
#else
            if (it_beg != it_end) {
                std::pair<uint32_t, int> st =
                    sss::util::utf8::peek(it_beg, it_end);
                if (st.second && this->m_data.ctypeFunc(st.first) != 0) {
                    user_data = reinterpret_cast<void*>(st.first);
                    std::advance(it_beg, st.second);
                    ret = true;
                }
            }
#endif
        } break;

        case RULE_OPERATOR_REFER:  // {{{1
            // 虽然貌似"引用"的话，直接传递p_delay指针就好了；
            // 但实际上不行；
            //
            // 因为，引用本身，也是对象，也可以包裹上action
            assert(this->m_data.ref);
            ret = this->m_data.ref->do_match(it_beg, it_end, no_action,
                                             dh.get_delay_ptr(), p_st);
            if (ret) {
                util::assign_result(this, user_data, m_action, m_matched_action,
                                    it_beg_sv, it_beg);
            }
            break;

        case RULE_DOMATCH_ACTION:  // {{{1
        {
            ret = m_domatch_action(it_beg, it_end, &dh);
            if (ret) {
                util::assign_result(this, user_data, m_action, m_matched_action,
                                    it_beg_sv, it_beg);
            }
        } break;

        case RULE_MAX_ITEM_COUNT:  // {{{1
        {
            // NOTE something error here!
        } break;
            // }}}
    }
    std::string msg = ret ? "`" + std::string(it_beg_sv, it_beg) + "`"
                          : std::string("failed");
    SSS_LOG_DEBUG("@%p, (%d) %s\n", this, ret, msg.c_str());
    if (this->has_action() && ret) {
        if (!no_action) {
            if (!p_delay) {
                this->m_action(it_beg_sv, it_beg, user_data);
            }
        }
    }
    return ret;
}

rule make_ctype_wapper(rule::CTypeFuncT f)
{
    rule ret(rule::RULE_CTYPE_WRAPPER);
    ret.m_data.ctypeFunc = f;
    return ret;
}

rule make_by_type(rule::type_t t) { return rule(t); }
// NOTE
// 注意以下全局变量的构造时机，与代码的先后关系相关！
const rule char_;
const rule eof_p = !parser::char_;
const rule utf8_;  // TODO

const rule line_begin_p = make_by_type(rule::RULE_ANCHOR_LINE_BEGIN);
const rule line_end_p = make_by_type(rule::RULE_ANCHOR_LINE_END);

const rule alnum_p = make_ctype_wapper(&::isalnum);
const rule alpha_p = make_ctype_wapper(&::isalpha);
const rule blank_p = make_ctype_wapper(&::isblank);
const rule cntrl_p = make_ctype_wapper(&::iscntrl);
const rule digit_p = make_ctype_wapper(&::isdigit);
const rule punct_p = make_ctype_wapper(&::ispunct);
const rule space_p = make_ctype_wapper(&::isspace);  // blank_p | \r,\n,\v
const rule xdigit_p = make_ctype_wapper(&::isxdigit);
const rule lower_p = make_ctype_wapper(&::islower);
const rule upper_p = make_ctype_wapper(&::isupper);

const rule cidentifier_p =
    ((parser::alpha_p | parser::char_p('_')) >>
     *(parser::alnum_p | parser::char_p('_')))
        .result(std::function<std::string(StrIterator, StrIterator)>(
            &util::slice2string));

const rule double_p =
    (-char_set_p("+-") >>
     (+digit_p || (char_p('.') >> +digit_p || (char_set_p("eE") >> +digit_p))))
        .result(std::function<double(StrIterator, StrIterator)>(
            &util::slice2double));

// 如何自动避开某些规则，skip动作？
// 将避开的规则，当做参数，设置给某上层；
//
// 然后，该层(以及以下)比较的时候，会传递这个skip规则，以便自动应用该规则；
//
// 现在的问题是，有些规则，需要看做为atom，不受这个skip逻辑影响；
//
// 比如，对于一个一般性的，解析方式的计算器，匹配数字、标识符的时候，显然不
// 能避开空格；而处理数字、标识符和运算符之间，就需要过滤空格；
//
// 所以，boost::spirit一开始定义"具名"规则的时候，就设置了skip；而规则定义
// 式内部，根据情况，还设置了lexeme——该directive 的名称，叫
// -- Inhibiting Skipping
//
// 范例：
// integer = lexeme[ -(lit('+') | '-') >> +digit ];
//
// 这样的话，rule类，需要添加两个成员参数；
// 一个是skip子对象；
// 一个是是否继承skip；
//
// 并且，do_match()需要传递这个skip的指针；
//
// 最后，operator=还需要根据情况，决定skip何时拷贝复制！

}  // namespace parser
}  // namespace ss1x
