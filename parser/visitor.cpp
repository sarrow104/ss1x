#include "visitor.hpp"
#include "dictionary.hpp"

#include <map>
#include <cctype>

namespace {
    void printable_char(std::ostream& o, char c)
    {
        if (::isprint(c)) {
            o << c;
        }
        else {
            o << "\\x" << std::hex << std::setfill('0') << std::setw(2) << uint32_t(c);
        }
    }

    void printable_string(std::ostream& o, const std::string& s)
    {
        typedef std::string::const_iterator StrIterator;
        for (StrIterator it = s.begin(); it != s.end(); ++it) {
            printable_char(o, *it);
        }
    }
}
namespace ss1x {
namespace parser {

    void visitor::visit(const rule* r) {
        if (!r) {
            return;
        }
        if (this->m_first) {
            this->m_has_entered.insert(r);
            this->m_first = false;
        }
        typedef std::map<rule::CTypeFuncT, const char *> ctypefunc_nametable_t;
        static ctypefunc_nametable_t ctypefunc_nametable;
        if (ctypefunc_nametable.empty()) {
#define DO_ADD(a) ctypefunc_nametable[&a] = #a;
            DO_ADD(::isalnum);
            DO_ADD(::isalpha);
            DO_ADD(::isblank);
            DO_ADD(::iscntrl);
            DO_ADD(::ispunct);
            DO_ADD(::isdigit);
            DO_ADD(::isspace);
            DO_ADD(::isxdigit);
            DO_ADD(::islower);
            DO_ADD(::isupper);
#undef DO_ADD
        }

        this->o() << rule::get_type_name(r->m_type) << " @" << r;
        this->printRuleName(r);


        this->o()
            << " [" << (bool(r->m_action) ? 'A' : ' ') << "]"
            << "[" << r->m_matched_action.which() << "]";

        switch (r->m_type) {
        case rule::RULE_TYPE_NONE:
            break;

        case rule::RULE_TYPE_ANYCHAR:
            break;

        case rule::RULE_TYPE_CHAR:
            this->o() << " `";
            printable_char(this->o(), r->m_data.ch);
            this->o() << "`";
            break;

        case rule::RULE_TYPE_RANGE:
            this->o() << " `";
            printable_char(this->o(), r->m_data.ch_range[0]);
            this->o() << "-";
            printable_char(this->o(), r->m_data.ch_range[1]);
            this->o() << "`";
            break;

        case rule::RULE_TYPE_CHARSET:
            this->o() << " `";
            {
                std::string s;
                sss::util::utf8::dumpout2utf8(r->m_setdata.cbegin(), r->m_setdata.cend(),
                                              std::back_inserter(s));
                printable_string(this->o(), s);
            }
            this->o() << "`";
            break;

        case rule::RULE_TYPE_SEQUENCE:
            this->o() << " `";
            printable_string(this->o(), r->m_strdata);
            this->o() << "`";
            break;

        case rule::RULE_TYPE_DICTIONARY:
            this->o() << *r->m_data.dict << std::endl;
            break;

        case rule::RULE_ANCHOR_LINE_BEGIN:
            break;

        case rule::RULE_ANCHOR_LINE_END:
            break;

        case rule::RULE_OPERATOR_KLEENE_STAR:
            break;

        case rule::RULE_OPERATOR_PLUSTIMES:
            break;

        case rule::RULE_OPERATOR_RANGETIMES:
            {
                this->o() << " {" << int(r->m_data.ch_range[0]) << ", ";
                if (int(r->m_data.ch_range[1]) == -1) {
                    this->o() << "INF";
                }
                else {
                    this->o() << int(r->m_data.ch_range[1]);
                }
                this->o() << "}";
            }
            break;

        case rule::RULE_OPERATOR_DIFFERENCE:
            break;

        case rule::RULE_OPERATOR_CATENATE:
            break;

        case rule::RULE_OPERATOR_BRANCH:
            break;

        case rule::RULE_CTYPE_WRAPPER:
            {
                ctypefunc_nametable_t::const_iterator it =
                    ctypefunc_nametable.find(r->m_data.ctypeFunc);
                if (it != ctypefunc_nametable.end()) {
                    this->o() << " `" << it->second << "()`";
                }
                else {
                    this->o() << " `not registered function`";
                }
            }
            break;

        case rule::RULE_OPERATOR_REFER:
            {
                this->o() << ": " << r->m_data.ref;
                this->printRuleName(r->m_data.ref);
                if (!this->m_no_refer && this->m_has_entered.find(r->m_data.ref) == this->m_has_entered.end()) {
                    this->m_tobe_entered.insert(r->m_data.ref);
                }
            }
            break;

        default:
            break;
        }

        this->o() << std::endl;

        bool is_first = true;
        for (size_t i = 0; i != r->m_subs.size(); ++i) {
            if (is_first) {
                this->o() << std::string(m_indent, '\t') << '{' << std::endl;
                m_indent ++;
                is_first = false;
            }
            this->o() << std::string(m_indent, '\t') << i << ": ";
            r->m_subs[i].accept(*this);
        }
        if (!is_first) {
            --m_indent;
            this->o() << std::string(m_indent, '\t') << '}' << std::endl;
        }

        if (!this->m_no_refer && this->m_indent == 0 && !this->m_tobe_entered.empty()) {
            this->o() << std::endl;
            this->m_first = true;
            const ss1x::parser::rule * next = *this->m_tobe_entered.begin();
            this->m_tobe_entered.erase(this->m_tobe_entered.begin());
            next->accept(*this);
        }
    }
}
}
