#ifndef __EXCEPTION_HPP_1452918036__
#define __EXCEPTION_HPP_1452918036__

#include <iostream>
#include <stdexcept>

namespace ss1x {
namespace parser {
class rule;
class exception : public std::runtime_error {
public:
    explicit exception(const std::string& msg) : std::runtime_error(msg) {}
    ~exception() = default;
};

class ErrorPosition : public exception {
public:
    ErrorPosition(const std::string& msg, const rule* r,
                  std::string::const_iterator pos, std::pair<int, int> coord)
        : exception(msg), m_rule(r), m_pos(pos), m_coord(coord)
    {
    }

    ~ErrorPosition() = default;

public:
    const rule* getRulePtr() const { return this->m_rule; }
    std::string::const_iterator getPosition() const { return this->m_pos; }
    std::pair<int, int> getCoord() const { return this->m_coord; }
private:
    const rule* m_rule;
    std::string::const_iterator m_pos;
    std::pair<int, int> m_coord;
};
}  // namespace parser
}  // namespace ss1x

#endif /* __EXCEPTION_HPP_1452918036__ */
