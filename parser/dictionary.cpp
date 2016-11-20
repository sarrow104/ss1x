#include "dictionary.hpp"

#include "oparser.hpp"

#include <sss/algorithm.hpp>
#include <sss/log.hpp>
#include <sss/utlstring.hpp>

#include <algorithm>

namespace ss1x {
namespace parser {

dictionary::dictionary() {}
dictionary::~dictionary() {}
#if 0
    bool DictionaryMap::match(StrIterator& it_beg, StrIterator it_end, int & value)
    {
        typedef std::map<std::string, int> MapT;
        size_t length = 0;
        MapT::const_iterator matched_it = this->m_map.cend();
        for (MapT::const_iterator it = this->m_map.begin();
             it != this->m_map.end();
             ++it)
        {
            Parser::Rewinder r(it_beg);
            if (r.commit(Parser::parseSequence(it_beg, it_end, it->first)) &&
                it->first.length() > length)
            {
                length = it->first.length();
                matched_it = it;
            }
            // NOTE
            // std::map中，是有序的排列；
            //
            // 那么，是不是如果匹配长度下降，就可以退出循环？
        }
        if (length) {
            value = matched_it->second;
        }
        return length;
    }

    DictionaryMap& DictionaryMap::add(const std::string& slot, int value)
    {
        this->m_map[slot] = value;
        // add_prefix(slot);
        return *this;
    }

    DictionaryMap& DictionaryMap::remove(const std::string& slot)
    {
        this->m_map.erase(slot);
        // remove_prefix(slot);
        return *this;
    }

    DictionaryMap& DictionaryMap::clear()
    {
        this->m_map.clear();
        return *this;
    }

    void DictionaryMap::print(std::ostream& o ) const
    {
        typedef std::map<std::string, int> MapT;
        o << "DictionaryMap at " << this << std::endl;
        int cnt = 0;
        for (MapT::const_iterator it = this->m_map.begin();
             it != this->m_map.end(); ++it)
        {
            o
                << "slot " << cnt++ << "; value: " << it->second << "\"" << it->first << "\"" << std::endl;
        }
    }
#endif

class slot_less_ic {
public:
    bool operator()(const DictionaryTree::slot_t& lhs,
                    const DictionaryTree::slot_t& rhs)
    {
        return sss::stricmp_t()(lhs.m_word, rhs.m_word);
    }
};

class slot_less_cs {
public:
    bool operator()(const DictionaryTree::slot_t& lhs,
                    const DictionaryTree::slot_t& rhs)
    {
        return std::less<std::string>()(lhs.m_word, rhs.m_word);
    }
};

class slot_equal_ic {
public:
    bool operator()(const DictionaryTree::slot_t& lhs,
                    const DictionaryTree::slot_t& rhs)
    {
        return !sss::stricmp_t()(lhs.m_word, rhs.m_word) &&
               !sss::stricmp_t()(rhs.m_word, lhs.m_word);
    }
};

class slot_equal_cs {
public:
    bool operator()(const DictionaryTree::slot_t& lhs,
                    const DictionaryTree::slot_t& rhs)
    {
        return lhs.m_word == rhs.m_word;
    }
};

bool DictionaryTree::match(StrIterator& it_beg, StrIterator it_end, int& value)
{
    typedef std::vector<slot_t>::iterator DataIterator;
    DataIterator max_match_iter = this->m_list.end();

    if (this->m_is_ic) {
        if (!this->match(it_beg, it_end, max_match_iter, slot_less_ic())) {
            return false;
        }
    }
    else {
        if (!this->match(it_beg, it_end, max_match_iter, slot_less_cs())) {
            return false;
        }
    }

    if (max_match_iter != this->m_list.end()) {
        it_beg += max_match_iter->m_word.length();
        value = max_match_iter->m_value;
    }
    return max_match_iter != this->m_list.end();
}

dictionary& DictionaryTree::add(const std::string& slot, int value)
{
    typedef std::vector<slot_t>::iterator DataIterator;
    if (this->m_is_ic) {
        this->add(slot, value, slot_less_ic());
    }
    else {
        this->add(slot, value, slot_less_cs());
    }
    return *this;
}

dictionary& DictionaryTree::remove(const std::string& slot)
{
    if (this->m_is_ic) {
        this->remove(slot, slot_less_ic());
    }
    else {
        this->remove(slot, slot_less_cs());
    }
    return *this;
}

dictionary& DictionaryTree::clear()
{
    this->m_list.clear();
    return *this;
}

void DictionaryTree::calcPrefix()
{
    this->m_min_length = this->m_list.rbegin()->m_word.length();
    for (size_t i = 0; i + 1 != this->m_list.size(); ++i) {
        if (this->m_list[i].m_word.length() < this->m_min_length) {
            this->m_min_length = this->m_list[i].m_word.length();
        }
        this->m_list[i].m_prefixed_cnt = 0;
        for (size_t next = i + 1; next != this->m_list.size(); ++next) {
            if (!sss::is_begin_with(this->m_list[next].m_word,
                                    this->m_list[i].m_word, this->m_is_ic)) {
                break;
            }
            this->m_list[i].m_prefixed_cnt++;
        }
    }
}

void DictionaryTree::print(std::ostream& o) const
{
    o << "DictionaryTree at " << this << std::endl;
    for (size_t i = 0; i != this->m_list.size(); ++i) {
        o << "slot " << i << ";"
          << "\t value: " << this->m_list[i].m_value << ";"
          << "\t prefix: " << this->m_list[i].m_prefixed_cnt << ";"
          << "\t\"" << this->m_list[i].m_word << "\"" << std::endl;
    }
}

KeywordsTree::KeywordsTree(const ss1x::parser::rule r, bool is_ic)
    : DictionaryTree(is_ic), m_rule(r)
{
}

KeywordsTree::~KeywordsTree() {}
bool KeywordsTree::match(StrIterator& it_beg, StrIterator it_end, int& value)
{
    SSS_LOG_FUNC_TRACE(sss::log::log_DEBUG);
    typedef std::vector<slot_t>::iterator DataIterator;
    StrIterator it_beg_sv = it_beg;

    if (this->m_rule.match(it_beg_sv, it_end)) {
        StrIterator t_beg = it_beg;
        StrIterator t_end = it_beg_sv;

#if 1
        std::string slot(t_beg, t_end);
        if (this->m_is_ic) {
            DataIterator it =
                std::lower_bound(this->m_list.begin(), this->m_list.end(),
                                 slot_t(slot), slot_less_ic());
            if (it != this->m_list.end() &&
                slot_equal_ic()(*it, slot_t(slot))) {
                value = it->m_value;
                it_beg = it_beg_sv;
                return true;
            }
        }
        else {
            DataIterator it =
                std::lower_bound(this->m_list.begin(), this->m_list.end(),
                                 slot_t(slot), slot_less_cs());
            if (it != this->m_list.end() &&
                slot_equal_cs()(*it, slot_t(slot))) {
                value = it->m_value;
                it_beg = it_beg_sv;
                return true;
            }
        }
#else
        int t_value = 0;
        if (this->DictionaryTree::match(t_beg, t_end, t_value) &&
            t_beg == t_end) {
            value = t_value;
            it_beg = it_beg_sv;
            return true;
        }
#endif
    }
    return false;
}

dictionary& KeywordsTree::add(const std::string& slot, int value)
{
    if (ss1x::parser::is_full_match(this->m_rule, slot)) {
        this->DictionaryTree::add(slot, value);
    }
    return *this;
}

void KeywordsTree::print(std::ostream& o) const
{
    o << "KeywordsTree at " << this << std::endl;

    o << "rule\n" << this->m_rule << std::endl;

    for (size_t i = 0; i != this->m_list.size(); ++i) {
        o << "slot " << i << ";"
          << "\t value: " << this->m_list[i].m_value << ";"
          << "\t prefix: " << this->m_list[i].m_prefixed_cnt << ";"
          << "\t\"" << this->m_list[i].m_word << "\"" << std::endl;
    }
}

KeywordsList::KeywordsList(const ss1x::parser::rule r, bool is_ic)
    : KeywordsTree(r, is_ic)
{
}

bool KeywordsList::match(StrIterator& it_beg, StrIterator it_end, int& value)
{
    return this->KeywordsTree::match(it_beg, it_end, value);
}

KeywordsList& KeywordsList::add(const std::string& slot, int /*value*/)
{
    this->KeywordsTree::add(slot, 0);
    return *this;
}

void KeywordsList::print(std::ostream& o) const
{
    o << "KeywordsList at " << this << std::endl;
    o << "rule\n" << this->m_rule << std::endl;

    for (size_t i = 0; i != this->m_list.size(); ++i) {
        o << "slot " << i << ";"
          << "\t prefix: " << this->m_list[i].m_prefixed_cnt << ";"
          << "\t\"" << this->m_list[i].m_word << "\"" << std::endl;
    }
}
}  // namespace parser
}  // namespace ss1x
