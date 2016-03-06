#ifndef __STATUS_HPP_1453197942__
#define __STATUS_HPP_1453197942__

#include "oparser.hpp"

#include <iterator>
#include <vector>

namespace ss1x {
    namespace parser {
        // 当前，主要是用来帮助解析器，知道某一个迭代器，是否指向行首、行尾等信
        // 息；
        class Status
        {
            friend class iterator;

        public:
            enum { DEFAULT_TABSTOP = 8 };

            typedef  std::pair<StrIterator, StrIterator> value_type;

        public:
            explicit Status(int tabstop = DEFAULT_TABSTOP);
            Status(StrIterator beg, StrIterator end, int tabstop = DEFAULT_TABSTOP);

            ~Status() = default;
        
        public:
            Status(Status&& ) = default;
            Status& operator = (Status&& ) = default;
        
        public:
            Status(const Status& ) = default;
            Status& operator = (const Status& ) = default;
        
        public:

            class iterator : std::iterator<std::random_access_iterator_tag, iterator>
            {
                friend class Status;

            public:
                typedef std::random_access_iterator_tag iterator_category;
                typedef Status::value_type value_type;
                typedef std::ptrdiff_t difference_type;
                typedef value_type * pointer;
                typedef value_type & reference;

            private:
                Status * m_p_st;
                std::vector<StrIterator>::iterator m_beg_it;
                std::vector<StrIterator>::iterator m_end_it;

            protected:
                iterator()
                    : m_p_st(nullptr)
                {
                }

                explicit iterator(Status & st, bool is_end = false)
                    : m_p_st(&st)
                {
                    if (is_end) {
                        m_beg_it = m_p_st->m_begins.end();
                        m_end_it = m_p_st->m_ends.end();
                    }
                    else {
                        m_beg_it = m_p_st->m_begins.begin();
                        m_end_it = m_p_st->m_ends.begin();
                    }
                }

            public:
                iterator(const iterator& rhs) = default;
                iterator& operator = (const iterator& rhs) = default;

            public:
                iterator operator++(int)
                {
                    iterator ret(*this);
                    if (m_p_st && std::distance(this->m_beg_it, m_p_st->m_begins.end()) > 0) {
                        m_beg_it++;
                        m_end_it++;
                    }
                    return ret;
                }

                iterator& operator++()
                {
                    (*this)++;
                    return *this;
                }

                iterator operator--(int)
                {
                    iterator ret(*this);
                    if (m_p_st && std::distance(m_p_st->m_begins.begin(), this->m_beg_it) > 0) {
                        m_beg_it--;
                        m_end_it--;
                    }
                    return ret;
                }

                iterator& operator--()
                {
                    (*this)--;
                    return *this;
                }

                bool operator==(const iterator& rhs) const
                {
                    return this->m_p_st == rhs.m_p_st && m_beg_it == rhs.m_beg_it;
                }

                bool operator!=(const iterator& rhs) const
                {
                    return !(*this == rhs);
                }

                iterator operator + (int offset) const
                {
                    iterator ret(*this);
                    if (m_p_st) {
                        std::advance(ret.m_beg_it, offset);
                        std::advance(ret.m_end_it, offset);
                    }
                    return ret;
                }

                iterator operator - (int offset) const
                {
                    return this->operator+(-offset);
                }

                int operator - (const iterator& rhs) const
                {
                    if (this->m_p_st == rhs.m_p_st && m_p_st) {
                        return std::distance(rhs.m_beg_it, m_beg_it);
                    }
                    else {
                        return 0;
                    }
                }

                value_type operator*()
                {
                    if (m_p_st) {
                        return std::make_pair(*m_beg_it, *m_end_it);
                    }
                    else {
                        return std::make_pair(StrIterator(),StrIterator());
                    }
                }
            };

            iterator begin()
            {
                return iterator(*this);
            }

            iterator end()
            {
                return iterator(*this, true);
            }

            int tabstop() const {
                return this->m_tabstop;
            }

            int tabstop(int ts) {
                std::swap(m_tabstop, ts);
                this->safe_tabtop();
                return ts;
            }

            bool is_init() const {
                return !this->m_begins.empty();
            }

            size_t size() const {
                return this->m_begins.size();
            }

            value_type operator[](int i) const
            {
                if (i < 0) {
                    i = this->size() + i;
                }
                if (i < 0 || i >= int(this->size())) {
                    return std::make_pair(StrIterator(), StrIterator());
                }
                else {
                    return std::make_pair(this->m_begins[i], this->m_ends[i]);
                }
            }

            value_type front() const
            {
                if (this->is_init()) {
                    return std::make_pair(this->m_begins.front(), this->m_ends.front());
                }
                else {
                    return std::make_pair(StrIterator(), StrIterator());
                }
            }

            value_type back() const
            {
                if (this->is_init()) {
                    return std::make_pair(this->m_begins.back(), this->m_ends.back());
                }
                else {
                    return std::make_pair(StrIterator(), StrIterator());
                }
            }

            bool is_inner_match(StrIterator it_beg, StrIterator it_end) const {
                return this->is_init() && this->m_begins.front() <= it_beg && this->m_ends.back() >= it_end;
            }

            void clear() {
                this->m_begins.clear();
                this->m_ends.clear();
            }

            std::pair<int, int> calc_coord(StrIterator it) const;

            bool init(StrIterator, StrIterator);

            // bool add_begin(StrIterator);
            // bool add_end(StrIterator);

            bool is_begin(StrIterator) const;
            bool is_end(StrIterator) const;

        protected:
            void safe_tabtop() {
                if (this->m_tabstop <= 0) {
                    this->m_tabstop = DEFAULT_TABSTOP;
                }
            }

        private:
            int                         m_tabstop;
            std::vector<StrIterator>    m_begins;
            std::vector<StrIterator>    m_ends;
        };
    }
}


#endif /* __STATUS_HPP_1453197942__ */
