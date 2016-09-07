#include "headers.hpp"

#include <sss/spliter.hpp>
#include <sss/util/Parser.hpp>
#include <sss/util/StringSlice.hpp>

namespace ss1x {
namespace http {
void Headers::print(std::ostream& o) const
{
    for (const auto item : *this) {
        o << item.first << ": " << item.second << "\r\n";
    }
}

std::string Headers::get(const std::string& key, const std::string& stem) const
{
    BaseT::const_iterator it = this->BaseT::find(key);
    if (it == this->BaseT::end()) {
        return "";
    }

    typedef std::string::const_iterator StrIterator;
    typedef sss::RangeSpliter<StrIterator> RangeSpliterT;
    typedef sss::util::StringSlice<StrIterator> SliceT;
    SliceT range;
    RangeSpliterT rs(it->second.cbegin(), it->second.cend(), ';');
    while (rs.fetch(range)) {
        range.ltrim();
        if (sss::is_begin_with(range.begin(), range.end(), stem) &&
            sss::is_begin_with(range.begin() + stem.length(), range.end(),
                               "=")) {
            return std::string(range.begin() + stem.length() + 1, range.end());
        }
    }
    return "";
}
}  // namespace asio
}  // namespace ss1x
