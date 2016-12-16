#include "sha1.hpp"

#include <fstream>

namespace ss1x {
namespace uuid {
namespace detail {
std::string sha1_to_string(const uint32_t (&hash)[5])
{
    std::string ret;
    ret.reserve(20);
    for (const auto& i : hash) {
        ret.push_back((i >> 24) & 0xFFu);
        ret.push_back((i >> 16) & 0xFFu);
        ret.push_back((i >> 8 ) & 0xFFu);
        ret.push_back((i >> 0 ) & 0xFFu);
    }
    return ret;
}
} // namespace detail

std::string sha1::fromFile(const std::string& fname, size_t buffsize)
{
    std::vector<char> v(buffsize);
    std::ifstream fd{fname.c_str(), std::ios_base::in | std::ios_base::binary};
    fd.exceptions(std::ifstream::eofbit | std::ifstream::failbit |
                  std::ifstream::badbit);

    fd.seekg(0, std::ios::end);
    size_t left_bytes = fd.tellg();
    fd.seekg(0, std::ios::beg);

    boost::uuids::detail::sha1 sha1;
    while (left_bytes > 0u) {
        if (buffsize > left_bytes) {
            buffsize = left_bytes;
        }
        fd.read(v.data(), buffsize);
        if (!fd.good()) {
            std::cout << "bad" << std::endl;
            break;
        }
        left_bytes -= buffsize;
        sha1.process_bytes(v.data(), buffsize);
    }
    uint32_t hash[5] = {0};
    sha1.get_digest(hash);

    return detail::sha1_to_string(hash);
}

std::string sha1::fromBytes(const char* buf, size_t buffsize)
{
    size_t local_bufsize = 1024u * 128u;
    std::vector<char> v(local_bufsize);

    boost::uuids::detail::sha1 sha1;
    sha1.process_bytes(buf, buffsize);

    uint32_t hash[5] = {0};
    sha1.get_digest(hash);

    return detail::sha1_to_string(hash);
}
}  // namespace uuid
}  // namespace ss1x
