#ifndef __GETFILE_HPP_1456798559__
#define __GETFILE_HPP_1456798559__

#include <iosfwd>

namespace ss1x {
namespace http {
class Headers;
}  // namespace http
namespace asio {

void getFile(std::ostream& outFile, const std::string& serverName,
             const std::string& getCommand, int port = 80);

void getFile(std::ostream& outFile, ss1x::http::Headers& header,
             const std::string& serverName, const std::string& getCommand,
             int port = 80);

void getFile(std::ostream& outFile, ss1x::http::Headers& header,
             const std::string& url);

void getFile(std::ostream& outFile, const std::string& url);

void proxyGetFile(std::ostream& outFile, const std::string& proxy_domain,
                  int proxy_port, const std::string& serverName,
                  const std::string& getCommand, int port = 80);

void proxyGetFile(std::ostream& outFile, const std::string& proxy_domain,
                  int proxy_port, const std::string& url);

void proxyGetFile(std::ostream& outFile, ss1x::http::Headers& header,
                  const std::string& proxy_domain, int proxy_port,
                  const std::string& serverName, const std::string& getCommand,
                  int port = 80);

void proxyGetFile(std::ostream& outFile, ss1x::http::Headers& header,
                  const std::string& proxy_domain, int proxy_port,
                  const std::string& url);

}  // namespace asio
}  // namespace ss1x

#endif /* __GETFILE_HPP_1456798559__ */
