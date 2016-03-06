#ifndef __VISITOR_HPP_1452310363__
#define __VISITOR_HPP_1452310363__

#include "oparser.hpp"

#include <set>

namespace ss1x {
namespace parser {

    class rule;

    class visitor
    {
    public:
        // visitor(std::ostream& o, StrIterator beg, StrIterator end)
        //     : m_o(o), m_indent(0), m_beg(beg), m_end(end), m_first(true)
        // {
        // }

        explicit visitor(std::ostream& o, bool no_refer = false)
            : m_o(o), m_no_refer(no_refer),
              m_indent(0), m_first(true)
        {
        }

        ~visitor()
        {
        }

    protected:
        std::ostream& o()
        {
            return this->m_o;
        }

        void printRuleName(const rule* r)
        {
            if (!r->m_name.empty()) {
                this->o() << " [" << r->m_name << "]";
            }
        }

    public:
        void visit(const rule* r);

    public:
        std::ostream&                   m_o;
        bool                            m_no_refer;

        int                             m_indent;
        bool                            m_first;
        std::set<const ss1x::parser::rule*>   m_has_entered;
        std::set<const ss1x::parser::rule*>   m_tobe_entered;
    };

    // 如何绑定特定的writer ?
    // 很简单，使用预设的action；然后绑定一个指向int的void*指针；
    //
    // 这样，前两个参数表示传入，第三个表示传出；

    // inline StrIterator invoke_parser(StrIterator beg, StrIterator end, rule& r)
    // {
    //     visitor v(beg, end);
    //     r.accept(v);
    //     return beg;
    // }

}
}


#endif /* __VISITOR_HPP_1452310363__ */
