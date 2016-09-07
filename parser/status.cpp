#include "status.hpp"

#include <sss/util/PostionThrow.hpp>
#include <sss/util/utf8.hpp>
#include <sss/utlstring.hpp>

#include <sss/log.hpp>

#include <algorithm>
#include <stdexcept>

namespace ss1x {
namespace parser {

Status::Status(int tabstop) : m_tabstop(tabstop) {}
Status::Status(StrIterator beg, StrIterator end, int tabstop)
    : m_tabstop(tabstop)
{
    this->safe_tabtop();
    this->init(beg, end);
}

bool Status::init(StrIterator it_beg, StrIterator it_end)
{
    if (this->is_inner_match(it_beg, it_end)) {
        return true;
    }

    this->clear();

    if (std::distance(it_beg, it_end) > 0) {
        this->m_begins.push_back(it_beg);
    }
    while (it_beg != it_end) {
        if (*it_beg == '\n') {
            this->m_ends.push_back(it_beg);
            this->m_begins.push_back(it_beg + 1);
            std::advance(it_beg, 1);
        }
        else if (std::distance(it_beg, it_end) >= 2 && *it_beg == '\r' &&
                 *(it_beg + 1) == '\n') {
            this->m_ends.push_back(it_beg);
            this->m_begins.push_back(it_beg + 2);
            std::advance(it_beg, 2);
        }
        else {
            std::advance(it_beg, 1);
        }
    }
    this->m_ends.push_back(it_end);
    return true;
}

// bool Status::add_begin(StrIterator it)
// {
//     if (m_begins.empty() || m_begins.back() < it) {
//         m_begins.push_back(it);
//         return true;
//     }
//     return false;
// }
//
// bool Status::add_end(StrIterator it)
// {
//     if (m_ends.empty() || m_ends.back() < it) {
//         m_ends.push_back(it);
//         return true;
//     }
//     return false;
// }

bool Status::is_begin(StrIterator it) const
{
    return std::binary_search(m_begins.cbegin(), m_begins.cend(), it);
}

bool Status::is_end(StrIterator it) const
{
    return std::binary_search(m_ends.cbegin(), m_ends.cend(), it);
}

std::pair<int, int> Status::calc_coord(StrIterator it) const
{
    SSS_LOG_FUNC_TRACE(sss::log::log_DEBUG);
    int row = 0;
    int col = 0;

    if (this->m_begins.empty()) {
        SSS_POSTION_THROW(std::runtime_error, "empty Status::" << __func__);
    }
    if (std::distance(m_begins.front(), it) < 0 ||
        std::distance(it, m_ends.back()) < 0) {
        SSS_POSTION_THROW(std::runtime_error, "it not in Status range");
    }

    // lower_bound 和 upper_bound ，返回的是查找值 v，在递增区间[first,
    // last)之间，
    // 第一个大于等于 v 和 第一个大于 v 的指针；
    //
    // 而我需要的是，第一个小于等于v的指针！（m_begins中），或者：
    // 第一个大于等于 v 的指针；（m_ends中）；

    auto it_row = std::lower_bound(m_ends.cbegin(), m_ends.cend(), it);

    row = std::distance(m_ends.cbegin(), it_row);

    col = sss::util::utf8::text_width(m_begins[row], it, m_tabstop);

    SSS_LOG_EXPRESSION(sss::log::log_DEBUG, row);
    SSS_LOG_EXPRESSION(sss::log::log_DEBUG, col);

    return std::make_pair(row, col);
}

}  // namespace parser
}  // namespace ss1x
