#ifndef __DICTIONARY_HPP_1452241847__
#define __DICTIONARY_HPP_1452241847__

#include "oparser.hpp"

#include <sss/algorithm.hpp>
#include <sss/utlstring.hpp>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace ss1x {
namespace parser {

typedef std::string::const_iterator StrIterator;

// TODO
// 再增加一个 dict类型，创建的时候，需要接受一个rule对象
// 以便在添加条目的时候，就进行检查；
// 然后，匹配的时候，也需要符合这个rule；
//
// 目的？
//
// 即，为了匹配整单词，而不是部分匹配；
//
// 这样的话，相当于要匹配两次；
//
// 即，先用这个规则，截取一段字符；然后，再用这个字符，去完全匹配字典——无
// 剩余；

// NOTE
//
// 词典，用来包装关键字；
// 可动态修改；所以，rule将引用这个类型——指针形式；
// 内部，应该使用vector，还是set？
// 这样的，大部分脚本，都是贪婪匹配；特别是dictionary……
// boost::spirit的symbol table，每个标识符，还可以绑定一个整数作为标记返回值；
// 它是如何实现的呢？
//
// tens_()
// {
//     add
//       ("X"    , 10)
//       ("XX"   , 20)
//       ("XXX"  , 30)
//       ("XL"   , 40)
//       ("L"    , 50)
//       ("LX"   , 60)
//       ("LXX"  , 70)
//       ("LXXX" , 80)
//       ("XC"   , 90)
//     ;
// }
//
// 像这个，不同标签，甚至是其他项的前缀！"X"是"XX"、"XXX"、"XL"、"XC"的前缀；
// "XX"是"XXX"的前缀……
//
// 因为boost::spirit确实会最长匹配……
//
// tutorial中说：
//
// “Each entry in a symbol table has an associated mutable data slot. ”
//
// 符号表中的每个条目，都关联着，一个可变的数据槽。
//
// 在创建的时候，就应该传入？

class dictionary {
public:
    dictionary();
    virtual ~dictionary();

public:
    virtual bool match(StrIterator& it_beg, StrIterator it_end, int& value) = 0;
    virtual dictionary& add(const std::string& slot, int value) = 0;
    virtual dictionary& clear() = 0;
    virtual dictionary& remove(const std::string& slot) = 0;
    virtual void print(std::ostream&) const = 0;
};

#if 0
    class DictionaryMap : public dictionary
    {
    public:
        explicit DictionaryMap(bool is_ic = false)
            : m_is_ic(is_ic)
        {
        }
        ~DictionaryMap()
        {
        }

    public:

        // NOTE
        // 考虑前缀的话，这本质上是一个递归的匹配
        // 相当于我以前做的关键字森林；
        //
        // 有路径，有出口；
        //
        // 至于我这里，可以做成"差"；
        //
        // 还有一种办法，是从最长的开始匹配；
        // 或者说保留最长的；
        bool match(StrIterator& it_beg, StrIterator it_end, int & value);

        DictionaryMap& add(const std::string& slot, int value);

        DictionaryMap& remove(const std::string& slot);

        DictionaryMap& clear();

        void print(std::ostream& o ) const;

    protected:
        // void add_prefix(const std::string& slot)
        // {
        //     for (BaseT::const_iterator it = this->BaseT::begin();
        //          it != this->BaseT::end();
        //          ++it)
        //     {
        //         // slot is prefix of it->first
        //         if (slot.length() < it->first.length() &&
        //             sss::is_begin_with(it->first, slot, m_is_ic))
        //         {
        //             m_prefixmap[slot].insert(it->first);
        //         }
        //         // it->first is prefix of slot
        //         else if (slot.length() > it->first.length() &&
        //                  sss::is_begin_with(slot, it->first, m_is_ic))
        //         {
        //             m_prefixmap[it->first].insert(slot);
        //         }
        //     }
        // }

        // void remove_prefix(const std::string& slot)
        // {
        //     this->m_prefixmap.erase(slot);
        //     for (BaseT::const_iterator it = this->BaseT::begin();
        //          it != this->BaseT::end();
        //          ++it)
        //     {
        //         if (slot.length() > it->first.length() &&
        //             sss::is_begin_with(slot, it->first, m_is_ic))
        //         {
        //             m_prefixmap[it->first].erase(slot);
        //         }
        //     }
        // }

    private:
        std::map<std::string, int>    m_map;
        // std::map<std::string, std::set<std::string> > m_prefixmap;
        bool m_is_ic; // is ignore case
    };
#endif

// NOTE
// 用 std::vector<std::string> 来模拟树
class DictionaryTree : public dictionary {
protected:
    friend class slot_less_ic;
    friend class slot_less_cs;

    friend class slot_equal_ic;
    friend class slot_equal_cs;

protected:
    struct slot_t {
        slot_t() : m_value(0), m_prefixed_cnt(0) {}
        explicit slot_t(const std::string& word, int value = 0)
            : m_value(value), m_prefixed_cnt(0), m_word(word)
        {
        }
        int m_value;
        int m_prefixed_cnt;  // 是多少个的前缀？
        std::string m_word;
    };

public:
    explicit DictionaryTree(bool is_ic = false)
        : m_is_ic(is_ic), m_min_length(0)
    {
    }
    ~DictionaryTree() {}
public:
    virtual bool match(StrIterator& it_beg, StrIterator it_end, int& value);
    virtual dictionary& add(const std::string& slot, int value);
    virtual dictionary& remove(const std::string& slot);
    virtual dictionary& clear();
    virtual void print(std::ostream& o) const;

protected:
    typedef std::vector<slot_t>::iterator DataIterator;

    template <typename CompT>
    bool match(StrIterator it_beg, StrIterator it_end,
               DataIterator& max_match_iter, CompT func)
    {
        if (size_t(std::distance(it_beg, it_end)) < this->m_min_length) {
            return false;
        }
        size_t max_match_cnt = 0;

        // 函数lower_bound()在first和last中的前闭后开区间进行二分查找，返回
        // 大于或等于val的第一个元素位置。如果所有元素都小于val，则返回last
        // 的位置
        //
        // 函数upper_bound()返回的在前闭后开区间查找的关键字的上界，返回大
        // 于val的第一个元素位置，如一个数组number序列
        // 1,2,2,4.upper_bound(2)后，返回的位置是3（下标）也就是4所在的位置
        // ,同样，如果插入元素大于数组中全部元素，返回的是last。(注意：数组
        // 下标越界)
        //

        std::string first_try(it_beg, it_beg + this->m_min_length);

        DataIterator first =
            std::lower_bound(this->m_list.begin(), this->m_list.end(),
                             slot_t(first_try, 0), func);
        if (first == this->m_list.end()) {
            return false;
        }

        first_try += '\xff';
        DataIterator last =
            std::upper_bound(this->m_list.begin(), this->m_list.end(),
                             slot_t(first_try, 0), func);

        // if (first != this->m_list.cend()) { // {{{1
        //     std::cout << "first = " << first->m_word << std::endl;
        // }
        // else {
        //     std::cout << "first = " << "end()" << std::endl;
        // }
        //
        // if (last != this->m_list.cend()) {
        //     std::cout << "last = " << last->m_word << std::endl;
        // }
        // else {
        //     std::cout << "last = " << "end()" << std::endl;
        // } // }}}
        size_t current_match_cnt = 0;
        while (first < last) {
            {
                // std::cout << first->m_word << " : from " <<
                // current_match_cnt;

                current_match_cnt +=
                    sss::max_match(it_beg + current_match_cnt, it_end,
                                   first->m_word.begin() + current_match_cnt,
                                   first->m_word.end());

                // std::cout << ", to " << current_match_cnt << std::endl;
            }

            // 未匹配成功的情况：
            // 1. 待匹配流，长度不足；——直接返回false即可；
            // 2. 在前面就mis_match了；
            //    说明，需要调整范围；
            //    首先，以当前slot为前缀的条目，都需要抛弃——这修改了first;
            //    ——不过，这好像不能直接用 upper_bound 解决
            //    一个奇葩的办法是，末尾增加一个字节；
            //
            //    last没必要调整——只要匹配树下降，就可以放弃了
            if (current_match_cnt != first->m_word.length()) {
                if (it_beg + current_match_cnt == it_end) {
                    break;
                }

// std::cout << "try " << first->m_word << " failed" << std::endl;
#if 0
                    // 直接定位到大于等于目标串的范围下界
                    first = std::lower_bound(first + 1, last,
                                             slot_t(std::string(it_beg, it_beg + current_match_cnt + 1)),
                                             func);
                    if (first == last) {
                        break;
                    }
#else
                first += std::max(1, first->m_prefixed_cnt);
#endif

                // if (first != this->m_list.end()) { // {{{1
                //     std::cout << "first to " << first->m_word << std::endl;
                // }
                // else {
                //     std::cout << "first = " << "end()" << std::endl;
                // }

                // if (last != this->m_list.cend()) {
                //     std::cout << "last = " << last->m_word << std::endl;
                // }
                // else {
                //     std::cout << "last = " << "end()" << std::endl;
                // } // }}}

                continue;
            }
            // std::cout << "match succeed with " << first->m_word << std::endl;
            // 此时，完全匹配了一次；
            // 需要记录，然后继续匹配——保留最大位置，对应的迭代器即可；
            if (max_match_cnt < current_match_cnt) {
                // std::cout << " extent max_match_cnt from " << max_match_cnt
                // << " to " << current_match_cnt << std::endl;
                max_match_cnt = current_match_cnt;
                max_match_iter = first;

                // 没有后继了……
                if (!first->m_prefixed_cnt) {
                    break;
                }
                last = first + first->m_prefixed_cnt;
                // std::cout << "last to " << last->m_word << std::endl;
                if (last != this->m_list.end()) {
                    last++;
                }
                first++;
            }
        }
        return max_match_cnt;
    }

    template <typename CompT>
    void add(const std::string& slot, int value, CompT func)
    {
        if (!std::binary_search(this->m_list.begin(), this->m_list.end(),
                                slot_t(slot, value), func)) {
            this->m_list.push_back(slot_t(slot, value));
            std::sort(this->m_list.begin(), this->m_list.end(), func);
            this->calcPrefix();
        }
    }

    template <typename CompT>
    void remove(const std::string& slot, CompT func)
    {
        typedef std::vector<slot_t>::iterator DataIterator;
        DataIterator first = std::lower_bound(
            this->m_list.begin(), this->m_list.end(), slot_t(slot, 0), func);

        if (first != this->m_list.end() && !(func(slot_t(slot, 0), *first))) {
            this->m_list.erase(first);
            std::sort(this->m_list.begin(), this->m_list.end(), func);
            this->calcPrefix();
        }
    }

protected:
    void calcPrefix();

protected:
    std::vector<slot_t> m_list;
    bool m_is_ic;
    size_t m_min_length;
};

class KeywordsTree : public DictionaryTree {
public:
    explicit KeywordsTree(
        const ss1x::parser::rule = ss1x::parser::cidentifier_p,
        bool is_ic = false);
    ~KeywordsTree();

public:
    virtual bool match(StrIterator& it_beg, StrIterator it_end, int& value);
    virtual dictionary& add(const std::string& slot, int value);
    // dictionary& remove(const std::string& slot); -> old
    void print(std::ostream& o) const;  // -> old

protected:
    ss1x::parser::rule m_rule;
};

inline std::ostream& operator<<(std::ostream& o, const dictionary& d)
{
    d.print(o);
    return o;
}

class KeywordsList : public KeywordsTree {
public:
    using KeywordsTree::print;

public:
    explicit KeywordsList(
        const ss1x::parser::rule = ss1x::parser::cidentifier_p,
        bool is_ic = false);
    ~KeywordsList() = default;

public:
    void print(std::ostream& o) const;
    bool match(StrIterator& it_beg, StrIterator it_end, int& value);
    KeywordsList& add(const std::string& slot, int value = 0);
};

inline std::ostream& operator<<(std::ostream& o, const KeywordsList& d)
{
    d.print(o);
    return o;
}
}  // namespace parser
}  // namespace ss1x

#endif /* __DICTIONARY_HPP_1452241847__ */
