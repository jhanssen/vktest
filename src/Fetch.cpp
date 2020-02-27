#include "Fetch.h"
#include <httplib.h>
#include <LUrlParser.h>

Buffer Fetch::fetch(const std::string& uri)
{
    const auto url = LUrlParser::ParseURL::parseURL(uri);
    if (!url.isValid())
        return Buffer();
    if (url.scheme_ == "https") {
        int port;
        if (!url.getPort(&port))
            port = 443;
        httplib::SSLClient cli(url.host_, port);
        auto res = cli.Get(("/" + url.path_).c_str());
        if (res && res->status == 200 && !res->body.empty()) {
            Buffer buf;
            buf.assign(reinterpret_cast<uint8_t*>(&res->body[0]), res->body.size());
            return buf;
        }
    } else if (url.scheme_ == "http") {
        int port;
        if (!url.getPort(&port))
            port = 80;
        httplib::Client cli(url.host_, port);
        auto res = cli.Get(("/" + url.path_).c_str());
        if (res && res->status == 200 && !res->body.empty()) {
            Buffer buf;
            buf.assign(reinterpret_cast<uint8_t*>(&res->body[0]), res->body.size());
            return buf;
        }
    }
    return Buffer();
}
