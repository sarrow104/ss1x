#include "util.hpp"

namespace ss1x {
    namespace parser {
        namespace util {

            void assign_result(const ss1x::parser::rule * p_r,
                               ss1x::parser::rule::matched_value_t& mv,
                               const ss1x::parser::rule::ActionT& a,
                               const ss1x::parser::rule::matched_action_t& ma,
                               const ss1x::parser::StrIterator& it_beg,
                               const ss1x::parser::StrIterator& it_end)
            {
                (void) p_r;
                if (a) {
                    if (ma.which()) {
                        mv = boost::apply_visitor(calc_result(it_beg, it_end), ma);
                    }

#if 0
                    std::cout << __func__ << " assign mv = ";
                    boost::apply_visitor(print_var(std::cout), mv);
                    std::cout << "; type_name = " << mv.type().name();
                    std::cout << "; type_id = " << mv.which();
                    std::cout << "; from " << std::string(it_beg, it_end);
                    std::cout << std::endl;
                    std::cout << "@" << p_r << std::endl;
#endif
                }
            }

            uint32_t parseUint32_t(ss1x::parser::StrIterator& it_beg, ss1x::parser::StrIterator it_end)
            {
                uint32_t ret = 0;
                while (it_beg != it_end && std::isdigit(*it_beg)) {
                    ret *= 10;
                    ret += (*it_beg - '0');
                    ++it_beg;
                }

                return ret;
            }

            double slice2double(ss1x::parser::StrIterator it_beg, ss1x::parser::StrIterator it_end)
            {
                double ret = 0.0;
                bool   is_negtive = false;
                // sign part
                if (it_beg != it_end) {
                    switch (*it_beg) {
                    case '-':
                        is_negtive = true;
                        ++it_beg;
                        break;

                    case '+':
                        ++it_beg;
                        break;

                    default:
                        break;
                    }
                }
                // int part
                ret = parseUint32_t(it_beg, it_end);

                // frac part
                if (it_beg != it_end && *it_beg == '.') {
                    ++it_beg;
                    double frac_base = 1;
                    while (it_beg != it_end && std::isdigit(*it_beg)) {
                        frac_base *= 10;
                        ret += (*it_beg - '0') / frac_base;
                        ++it_beg;
                    }
                }

                if (is_negtive) {
                    ret = - ret;
                }

                // e-part
                if (it_beg != it_end && std::toupper(*it_beg) == 'E') {
                    ++it_beg;
                    bool is_e_negtive = false;
                    switch (*it_beg) {
                    case '-':
                        is_e_negtive = true;
                        ++it_beg;
                        break;

                    case '+':
                        ++it_beg;
                        break;

                    default:
                        break;
                    }
                    int e = parseUint32_t(it_beg, it_end);

                    if (e) {
                        if (is_e_negtive) {
                            while (e) {
                                ret /= 10;
                                e--;
                            }
                        }
                        else {
                            while (e) {
                                ret *= 10;
                                e--;
                            }
                        }
                    }
                }

                return ret;
            }

            std::string slice2string(ss1x::parser::StrIterator it_beg, ss1x::parser::StrIterator it_end)
            {
                return std::string(it_beg, it_end);
            }
            
        } // end of namespace util
    } // end of namespace parser
} // end of namespace ss1x
