#include "sha1.hpp"

#include <fstream>

namespace ss1x {
namespace uuid {
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

    std::string ret;
    for (const auto& i : hash) {
        ret.push_back((i >> 24) & 0xFFu);
        ret.push_back((i >> 16) & 0xFFu);
        ret.push_back((i >> 8) & 0xFFu);
        ret.push_back((i >> 0) & 0xFFu);
    }
    return ret;
}
}  // namespace uuid
}  // namespace ss1x
