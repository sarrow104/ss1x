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

// Ҫʹ���ƶ����캯���Ļ�������ͬʱ�ṩ�������캯��
//
// ������ֵ��� std::unique_ptr �Ŀ������죬�͸�ֵ����������delete״̬
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

// FIXME �군��
// ����Ӧ����������ȣ����ǹ�����ȣ�����ɶ��
//
// Ӧ����ģ����ʽƥ�䣬�������Ⱥ�˳�򡭡�
// ���ԣ���������ȡ���
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

// ����delayeval��ʹ�ó���
// �û��ȶ�����һ��rule����������������û�а󶨶����ģ�
// ������������ӹ����������а󶨸���action��
// ���ţ��û�������һ��delayeval����
// �û�ͨ����������match����(����������һ����װ����)��������ƥ��Ĺ��̣�
//
// ���裬��һ�ν����do_match������Ϊ���㣻
// �ö�����Ȼ�������κμ�¼��
// ���ţ�����Ҫ����������������ӹ����ˣ�
// �������Ƿ�֧�ʹ��ӣ� | �� >>����
// �����Ƿ�֧������ÿ����֧���а��Լ��Ķ�����
//
// ����1��
//   ÿ��rule����ֻҪ�� delayeval �����룬������һ���Լ��ģ�Ȼ�󸽼������
//   ����������棻
//
// ����2��
//   ֻҪ�� delayeval �����룬ÿ����һ���ӹ��򣬱������
//   rule::m_subs[i].do_match(...)�����ȴ���һ�� delayeval ����Ȼ�󽫸ö����
//   ָ�룬��Ϊ�õ��õ� delayeval������
//
//   �������ֵ�ǳɹ��ģ��ͽ������ʱ delayeval ���󣬲��뵽��ǰ��delayeval����
//   ��
//
// �ҵ�xml3����δ�����أ�
//
// �ҵ�xml3 ʵ�������¹��ܣ�
//
// ���ö�������������Զ�����·�����䣬�����ظ�������
//
// �ؼ����� һ�� helper�� �� ���������Լ�Rewinder�ࣻ
// /home/sarrow/extra/sss/include/sss/xml3/xml_parser.cpp|111
// /home/sarrow/extra/sss/include/sss/xml3/xml_parser.cpp|160
//
// ���а������Ĺ��캯������������Ƿ����·��(λ��+ƥ�亯��)���ã�������ԣ���
// ֱ�Ӹ�����ǰ�Ľ���������½�һ��·����
// �����������򿴵�ǰ·���Ƿ�ƥ��ɹ������ʧ�ܣ��򽫵�ǰ·�����롰��·�б���
// ͬʱ�����ڲ�����ƥ�����������Ѿ��ɹ��Ĳ��֣�����ɹ�·���б�
//
// �����û��� helper �꣬��ע���ǰ����û���дһ���������ص���䡪��ֻҪ����·��
// ���Ѿ��߹�����ô��֮ǰ�Ľ���������ء��������ǳɹ�����ʧ�ܡ�
//
// ��Rewinder�࣬��֤��ƥ����ָ���ߵ���ȷ��λ�á�������������¡�
//
// ��Ҫע����ǣ�·�����ã�������÷�Χ���������������������ڽ�����״̬�Ļ���
// ��ô����һ��·���Ļ������뱣���������״̬����������Ӧ�Ĵ�С�ȽϺ�����
//
// �����������������Ҫ���ѽϴ��ڴ�������Ļ�����ô������·�������㷨�������С�
//
// Ҳ����˵����������Ҫʹ�ü򵥵�·�������㷨��Ҳ��Ҫ�ṩһ�����ز��У�
//
// ��Ȼ������������������Ƕ��Դ���롪����Ҫ·������ĵط�������Ҫʹ�øúꡭ��
//
// ��ô����ʱ�������Ӧ��ϵ�أ�
//
// ����xml3��˵��segment-tree��ʵ����һ��ָ����������
// ָ��Ϊ�գ����ǿ�����
// �ڵ��һ���Ļ����ʹ����һ���ݹ��½�������ʹ���˰��������ɹ�ƥ�䣻
// ��������ǵ�һ�ģ�Ҳ�ܱ�֤����������ȷ�ԣ�
// Ȼ�󣬸����Ľṹ��ϵ�����������ö�����ȷ��·����һһ��Ӧ��
//
// �����Ҳ���ֵ�����
//
// ��������ʱ��������ָ�룬���Ƕ��󡪡����ص�ʱ��Ҳ�Ƕ���
//
// ��һ�����������Կ�����������
//
// �����������N���ӽڵ㡪�������Ǵ��ӹ�ϵ�������ǣ���������������ʲô��ϵ����
// �����������������Ҫ��
//
// ����Ҫ����Ҫ��ֻ�����߼��ϵġ���������ϵ������ĳ��������½磬�����ֵİ�����
// ϵ��Ҳ�����߶�����
//
// ����ע�⣬����ɢ��֮�������򡪡���Ϊ��ȫ������ϵ����ʵ�������½���ȣ�����
// ���߶Σ������޷������˭�����桢˭�����棻���Ծ��Բ��ܴ���֮��������
//
// ���뱣�ֽ�����ʱ��İ�����ϵ��
//
// Ҳ����˵do_match�еõ���ָ�룬�൱�ڸ��ڵ�ָ�룻
// Ȼ���ڲ������helper���ͻ����ʵ������������ָ��������ĸ������У�����һ��
// �ڵ㡪���Ȳ����룬�ȴ�������helper���ڲ��ȴ���һ�� delayeval ����
//
// Ȼ���ӹ���Ҫ�õ�delayeval����ָ��Ļ����͸�������������helper�л�ȡ��
//
// ������������ʱ���پ����Ƿ���������Ĳ��붯����
//
// ���ڵݹ���õ��ص㣬���Ա�֤������©�Ӻ������ɵĶ��󣻡�����Ϊ�Ӻ������ȷ���
// ��
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
