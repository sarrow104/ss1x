#ifndef __UTIL_HPP_1454901077__
#define __UTIL_HPP_1454901077__

#include "oparser.hpp"
#include "delayeval.hpp"

namespace ss1x {
    namespace parser {
        namespace util {
            // NOTE
            // 只有当，显式提供了delayeval指针，并且当前rule对象，捆绑了
            // action，才有用；
            // 就是说，需要两个东西：
            // 1. 当前rule指针
            struct delayeval_helper
            {
                explicit delayeval_helper(const ss1x::parser::rule *    p_rule,
                                          ss1x::parser::StrIterator &   s_it,
                                          ss1x::parser::rule::ActionT   action,
                                          ss1x::parser::rule::matched_value_t &  user_data,
                                          bool                          no_action,
                                          ss1x::parser::delayeval *     pdelay,
                                          ss1x::parser::Status *        p_st,
                                          bool&                         ret)
                    : m_p_rule(p_rule), m_it_beg(s_it), m_it_beg_sv(s_it),
                    m_action(action), m_usr_data(user_data), m_no_action(no_action),
                    m_parent_delay(pdelay), m_status(p_st), m_is_ok(ret)
                {
                    if (!this->m_no_action && this->m_parent_delay && this->m_action != 0) {
                        this->m_current_delay.assign(p_rule, s_it, s_it, action, user_data);
                    }
                }

                // 如果将 m_parent_delay 直接传递给子规则，则说明，当前根本不需要管理delayeval树！
                // 也就是说，需要管理的，仅只有将 m_parent_delay 传递除去的情形！
                ~delayeval_helper()
                {
                    if (this->m_is_ok && this->m_current_delay.is_init()) {
                        this->m_current_delay.set_end_pos(this->m_it_beg);
                        this->m_current_delay.set_user_data(m_usr_data);
                        this->m_parent_delay->push_back(std::move(this->m_current_delay));
                    }
                }

                // 传递给子 do_match() 的指针；
                // 情况如下：
                // no_action == 1 那么返回0；
                // no_action == 0
                //     查看父 delayeval 指针状态以及当前是否有action；
                //     m_parent_delay ? m_action != 0 ? &m_current_delay : m_parent_delay : 0;
                ss1x::parser::delayeval * get_delay_ptr()
                {
                    if (this->m_current_delay.is_init()) {
                        return &this->m_current_delay;
                    }
                    else {
                        return this->m_parent_delay;
                    }
                }

                ss1x::parser::delayeval * get_current_delay_ptr()
                {
                    if (this->m_current_delay.is_init()) {
                        return &this->m_current_delay;
                    }
                    else {
                        return 0;
                    }
                }

                void assign(uint64_t vi)
                {
                    this->m_usr_data = vi;
                }

                void assign(double vd)
                {
                    this->m_usr_data = vd;
                }

                void assign(const std::string& vs)
                {
                    this->m_usr_data = vs;
                }

                // NOTE
                // call函数，只是用于 自定义的 match 函数！
                // 而自定义的match函数，理论上，匹配的数据是传入的StrIterator迭代器；
                // 子路径，如果是其他的规则，则call即可；
                // 如果内部定义了rule，那么，它也应该自己定义对应的delayeval来处理——如果需要的话；
                // ——至于 Status，肯定是公用的；
                bool call(const rule * p_rule, StrIterator & it_beg, StrIterator it_end)
                {
                    if (!p_rule) {
                        SSS_POSITION_THROW(std::runtime_error,
                                          "delayeval_helper::" , __func__ , " null rule * ptr!");

                    }
                    return p_rule->do_match(it_beg, it_end,
                                            m_no_action, this->get_delay_ptr(),
                                            m_status);
                }

                void commit(bool is_ok);

                const ss1x::parser::rule *      m_p_rule;
                ss1x::parser::StrIterator &     m_it_beg;
                ss1x::parser::StrIterator       m_it_beg_sv;
                ss1x::parser::rule::ActionT     m_action;
                ss1x::parser::rule::matched_value_t &  m_usr_data;
                bool                            m_no_action;
                ss1x::parser::delayeval *       m_parent_delay;
                ss1x::parser::Status *          m_status;
                bool &                          m_is_ok;
                ss1x::parser::delayeval         m_current_delay;
            };

            class calc_result : public boost::static_visitor<ss1x::parser::rule::matched_value_t>
            {
            public:
                typedef ss1x::parser::StrIterator StrIterator;
                typedef ss1x::parser::rule::matched_value_t matched_value_t;
                typedef ss1x::parser::rule::empty empty;
                calc_result(StrIterator it_beg, StrIterator it_end)
                    : m_it_beg(it_beg), m_it_end(it_end)
                {
                }

                matched_value_t operator() (const empty& e) const
                {
                    return matched_value_t(e);
                }

                matched_value_t operator() (const uint64_t& i) const
                {
                    return matched_value_t(i);
                }

                matched_value_t operator() (const std::string& s) const
                {
                    return matched_value_t(s);
                }

                matched_value_t operator() (std::function<uint64_t(StrIterator,StrIterator)> f) const
                {
                    return f(m_it_beg, m_it_end);
                }

                matched_value_t operator() (std::function<std::string(StrIterator,StrIterator)> f) const
                {
                    return f(m_it_beg, m_it_end);
                }

                matched_value_t operator() (std::function<double(StrIterator,StrIterator)> f) const
                {
                    return f(m_it_beg, m_it_end);
                }

                StrIterator m_it_beg;
                StrIterator m_it_end;
            };

            class print_var : public boost::static_visitor<void>
            {
            public:
                print_var(std::ostream& o)
                    : m_o(o)
                {
                }

                void operator ()(const ss1x::parser::rule::empty& ) const
                {
                    m_o << "{empty}";
                }

                void operator ()(const uint64_t & i) const
                {
                    m_o << i;
                }

                void operator ()(const double & d) const
                {
                    m_o << d;
                }

                void operator ()(const std::string & s) const
                {
                    m_o << s;
                }

                std::ostream& m_o;
            };

            void assign_result(const ss1x::parser::rule * p_r,
                               ss1x::parser::rule::matched_value_t& mv,
                               const ss1x::parser::rule::ActionT& a,
                               const ss1x::parser::rule::matched_action_t& ma,
                               const ss1x::parser::StrIterator& it_beg,
                               const ss1x::parser::StrIterator& it_end);
            

            uint32_t parseUint32_t(ss1x::parser::StrIterator& it_beg, ss1x::parser::StrIterator it_end);

            double slice2double(ss1x::parser::StrIterator it_beg, ss1x::parser::StrIterator it_end);

            std::string slice2string(ss1x::parser::StrIterator it_beg, ss1x::parser::StrIterator it_end);
            
        } // end of namespace util
    } // end of namespace parser
} // end of namespace ss1x


#endif /* __UTIL_HPP_1454901077__ */
