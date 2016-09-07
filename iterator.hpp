#ifndef __ITERATOR_HPP_1449323147__
#define __ITERATOR_HPP_1449323147__

#include <type_traits>
#include <utility>

namespace ss1x {
template <typename T>
using iterator_value_type =
    typename std::remove_reference<decltype(*std::declval<T>())>::type;
}  // namespace ss1x

#endif /* __ITERATOR_HPP_1449323147__ */
