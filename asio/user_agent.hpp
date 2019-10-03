#pragma once
namespace ss1x {
namespace http {
static const char * user_agent_firefox = "Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:67.0) Gecko/20100101 Firefox/67.0";
// "Mozilla/5.0 (Windows; U; Windows NT 5.1; zh-CN; rv:1.9.1.6) Gecko/20091201 Firefox/3.5.6"

#define USER_AGENT_DEFAULT  ss1x::http::user_agent_firefox

} // namespace http
} // namespace ss1x
// static const char * user_agent_firefox = "Mozilla/5.0 (Windows; U; Windows NT 5.1; zh-CN; rv:1.9.1.6) Gecko/20091201 Firefox/3.5.6";

// curl 'https://zhuanlan.zhihu.com/p/44425997' -H 'User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:67.0) Gecko/20100101 Firefox/67.0' -H 'Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8' -H 'Accept-Language: zh-CN,zh;q=0.8,zh-TW;q=0.7,zh-HK;q=0.5,en-US;q=0.3,en;q=0.2' --compressed -H 'Connection: keep-alive' -H 'Cookie: _zap=3b57cafc-7deb-4c72-8e85-ca58bad3a4d7; d_c0="AMDguBPhHw-PTuphq2H426Rx_VHkYOPoJms=|1552617415"; q_c1=422513ed4a964d22a8243ffdcb21565d|1552617416000|1552617416000; capsion_ticket="2|1:0|10:1559802968|14:capsion_ticket|44:MTQ0YjdiZTQ3MTdkNDExYmE0MDE2NWFhMjBiMWVjMTc=|251a8fbb52374d184243ec3e2c1c93e3f653c068ab4de30baaf1cf9bd0533fa5"; _xsrf=af98151e-35c9-42e3-945f-872e7296e880; tgw_l7_route=4860b599c6644634a0abcd4d10d37251' -H 'Upgrade-Insecure-Requests: 1' -H 'Cache-Control: max-age=0' -H 'TE: Trailers'
