#pragma once

#include <string>

// 算法参考
//! https://en.wikipedia.org/wiki/SHA-1
// 代码参考：
//! http://stackoverflow.com/questions/28489153/how-to-portably-compute-a-sha1-hash-in-c

// sha1 生成20字节的digest串；通常表示成40个字符的hex串；
// boost::uuids::detail::sha1::get_digest() 函数，会将digest信息，写入4字节*5 数
// 组中；
// 如果需要将结果输出为hex串，则需要将每个4字节整数，从高位到低位，依次转换；然
// 后，重复5次即可。
namespace ss1x {
namespace uuid {
class sha1 {
public:
    static const size_t default_buffsize = 1024u * 128u;
    static std::string fromFile(const std::string& fname,
                                size_t buffsize = default_buffsize);

    static std::string fromBytes(const char * buf,
                                 size_t buffsize);

    static std::string xorBytes(std::string& sha_id1, std::string& sha_id2);

    static std::string fromFile5M(const std::string& fname);
};
}  // namespace uuid
}  // namespace ss1x
