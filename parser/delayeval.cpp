#include "delayeval.hpp"
#include "util.hpp"

#include <sss/log.hpp>
#include <sss/util/PostionThrow.hpp>

#include <stdexcept>

namespace ss1x {
namespace parser {

delayeval::delayeval() : m_rul_ptr(0) {}
delayeval::~delayeval() {}
delayeval::delayeval(const delayeval& rhs)
    : m_rul_ptr(rhs.m_rul_ptr),
      m_it_beg(rhs.m_it_beg),
      m_it_end(rhs.m_it_end),
      m_action(rhs.m_action),
      m_usr_data(rhs.m_usr_data),
      m_subs(rhs.m_subs)
{
}

// 要使用移动构造函数的话，必须同时提供拷贝构造函数
//
// 不过奇怪的是 std::unique_ptr 的拷贝构造，和赋值函数，都是delete状态
//! /usr/include/c++/4.8/bits/unique_ptr.h|464
delayeval::delayeval(delayeval&& rhs)
    : m_rul_ptr(rhs.m_rul_ptr),
      m_it_beg(rhs.m_it_beg),
      m_it_end(rhs.m_it_end),
      m_action(rhs.m_action),
      m_usr_data(rhs.m_usr_data),
      m_subs(std::move(rhs.m_subs))
{
}

delayeval& delayeval::operator=(delayeval&& rhs)
{
    if (this != &rhs) {
        this->m_rul_ptr = rhs.m_rul_ptr;
        this->m_it_beg = rhs.m_it_beg;
        this->m_it_end = rhs.m_it_end;
        this->m_action = rhs.m_action;
        this->m_usr_data = rhs.m_usr_data;
        this->m_subs = std::move(rhs.m_subs);
    }
    return *this;
}

void delayeval::eval() const
{
    if (this->is_init()) {
        SSS_POSTION_THROW(std::runtime_error,
                          "must eval at the root delayeval node");
    }
    for (SubsT::const_iterator it = this->m_subs.cbegin();
         it != this->m_subs.cend(); ++it) {
        it->eval_inner();
    }
}

// FIXME 完蛋！
// 到底应该是深度优先，还是广度优先，还是啥？
//
// 应该是模拟正式匹配，发生的先后顺序……
// 所以，是深度优先……
void delayeval::eval_inner() const
{
    if (!this->is_init()) {
        SSS_POSTION_THROW(std::runtime_error, "current sub node not init!");
    }

    if (this->m_action == 0) {
        SSS_POSTION_THROW(std::runtime_error,
                          "current sub delayeval has no action!");
    }
    for (SubsT::const_iterator it = this->m_subs.cbegin();
         it != this->m_subs.cend(); ++it) {
        it->eval_inner();
    }
    this->m_action(this->m_it_beg, this->m_it_end, this->m_usr_data);
}

delayeval& delayeval::assign(const rule* p_rule, StrIterator it_beg,
                             StrIterator it_end, rule::ActionT action,
                             rule::matched_value_t usr_data)
{
    this->m_rul_ptr = p_rule;
    this->m_it_beg = it_beg;
    this->m_it_end = it_end;
    this->m_action = action;
    this->m_usr_data = usr_data;

    return *this;
}

// 考虑delayeval的使用场景
// 用户先定义了一个rule——它本身往往是没有绑定动作的；
// 组成这个对象的子规则，往往是有绑定各种action；
// 接着，用户定义了一个delayeval对象；
// 用户通过这个规则的match函数(或者其他的一个包装函数)，来启动匹配的过程；
//
// 假设，第一次进入的do_match函数，为顶层；
// 该顶层显然不用做任何记录；
// 接着，就需要进入它下面包含的子规则了；
// 常见的是分支和串接（ | 和 >>）；
// 假设是分支，并且每个分支都有绑定自己的动作；
//
// 策略1：
//   每个rule对象，只要有 delayeval 对象传入，都创建一个自己的；然后附加在这个
//   传入对象下面；
//
// 策略2：
//   只要有 delayeval 对象传入，每进入一次子规则，比如调用
//   rule::m_subs[i].do_match(...)，就先创建一个 delayeval 对象，然后将该对象的
//   指针，作为该调用的 delayeval参数。
//
//   如果返回值是成功的，就将这个临时 delayeval 对象，插入到当前的delayeval下面
//   ；
//
// 我的xml3是如何处理的呢？
//
// 我的xml3 实现了如下功能：
//
// 利用对象构造和析构，自动处理路径记忆，避免重复搜索；
//
// 关键在于 一个 helper宏 和 包裹器，以及Rewinder类；
// /home/sarrow/extra/sss/include/sss/xml3/xml_parser.cpp|111
// /home/sarrow/extra/sss/include/sss/xml3/xml_parser.cpp|160
//
// 其中包裹器的构造函数，用来检查是否可以路径(位置+匹配函数)复用？如果可以，则
// 直接复用以前的结果；否则新建一个路径；
// 析构函数，则看当前路径是否匹配成功？如果失败，则将当前路径插入“死路列表”；
// 同时，将内部的子匹配结果，当做已经成功的部分，插入成功路径列表；
//
// 至于用户的 helper 宏，则注意是帮助用户简写一个立即返回的语句——只要记忆路径
// 中已经走过，那么把之前的结果立即返回——不管是成功还是失败。
//
// 而Rewinder类，则保证了匹配流指针走到正确的位置——在上述情况下。
//
// 需要注意的是，路径复用，有其可用范围——如果解析结果，依赖于解释器状态的话，
// 那么记忆一个路径的话，必须保存解析器的状态，并制作对应的大小比较函数。
//
// 如果，解析器本身需要花费较大内存来保存的话，那么，上述路径记忆算法将不可行。
//
// 也就是说，如果我真的要使用简单的路劲记忆算法，也需要提供一个开关才行；
//
// 当然，我上述作法，必须嵌入源代码——需要路径处理的地方，都需要使用该宏……
//
// 那么处理时机，与对应关系呢？
//
// 对于xml3来说，segment-tree其实就是一个指针代表的树；
// 指针为空，就是空树；
// 节点仅一个的话，就代表仅一个递归下降函数，使用了包裹器并成功匹配；
// 由于入口是单一的；也能保证单根树的正确性；
// 然后，该树的结构关系，与解析最后敲定的正确的路径，一一对应；
//
// 我最好也保持单根；
//
// 不过，此时，不再是指针，而是对象——返回的时候，也是对象；
//
// 第一个单根，可以看做是容器；
//
// 它下面可以有N个子节点——比如是串接关系，或者是；其他——至于是什么关系，对
// 于这个解析树，还重要吗？
//
// 不重要，重要的只是其逻辑上的“包含”关系——即某规则的上下界，所体现的包含关
// 系，也就是线段树；
//
// ——注意，不能散列之后，再排序——因为完全包含关系（其实就是上下界相等）的两
// 个线段，可能无法分清楚谁在外面、谁在里面；所以绝对不能打乱之后再排序；
//
// 必须保持解析的时候的包含关系！
//
// 也就是说do_match中得到的指针，相当于父节点指针；
// 然后内部的这个helper，就会根据实际情况，往这个指针所代表的父对象中，插入一个
// 节点——先不插入，先创建——helper类内部先创建一个 delayeval 对象；
//
// 然后子规则，要用到delayeval对象指针的话，就根据情况，从这个helper中获取；
//
// 当析构函数的时候，再决定是否完成真正的插入动作；
//
// 由于递归调用的特点，可以保证不会遗漏子函数生成的对象；——因为子函数，先返回
// ；
delayeval& delayeval::push_back(const rule* p_rule, StrIterator it_beg,
                                StrIterator it_end, rule::ActionT action,
                                rule::matched_value_t usr_data)
{
    SSS_LOG_FUNC_TRACE(sss::log::log_DEBUG);
    delayeval tmp;
    tmp.assign(p_rule, it_beg, it_end, action, usr_data);
    SSS_LOG_EXPRESSION(sss::log::log_DEBUG, action != 0);
    this->m_subs.push_back(tmp);
    return *this;
}

delayeval& delayeval::push_back(const delayeval& sub_delay)
{
    SSS_LOG_FUNC_TRACE(sss::log::log_DEBUG);
    SSS_LOG_EXPRESSION(sss::log::log_DEBUG, sub_delay.m_action != 0);
    if (sub_delay.m_action == 0) {
        // std::cout << *sub_delay.m_rul_ptr << std::endl;
        std::cout << *sub_delay.m_it_beg << std::endl;
        std::cout << std::distance(sub_delay.m_it_beg, sub_delay.m_it_end)
                  << std::endl;
        exit(0);
    }
    this->m_subs.push_back(sub_delay);
    return *this;
}

delayeval& delayeval::push_back(delayeval&& sub_delay)
{
    SSS_LOG_FUNC_TRACE(sss::log::log_DEBUG);
    SSS_LOG_EXPRESSION(sss::log::log_DEBUG, sub_delay.m_action != 0);
    if (sub_delay.m_action == 0) {
        // std::cout << *sub_delay.m_rul_ptr << std::endl;
        std::cout << *sub_delay.m_it_beg << std::endl;
        std::cout << std::distance(sub_delay.m_it_beg, sub_delay.m_it_end)
                  << std::endl;
        exit(0);
    }
    this->m_subs.push_back(std::move(sub_delay));
    return *this;
}

void delayeval::print_userdata(std::ostream& o) const
{
    o << "boost::variant::which() = " << this->m_usr_data.which() << std::endl;
    ss1x::parser::util::print_var p(o);
    boost::apply_visitor(p, this->m_usr_data);
    o << std::endl;
}

}  // namespace parser
}  // namespace ss1x
