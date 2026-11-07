#include "oneui/vpn/json_api.h"
#include "oneui/json.h"
#include <string>
#include <vector>
#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

namespace {
bool prefix(const oneui::json::Value &value, OneUiVpnPrefixV1 &out, bool dns) {
    const auto text = dns ? value.string() : value["address"].string();
    if (text.empty() || text.find('\0') != std::string::npos) return false;
    out.family = text.find(':') == std::string::npos ? 4 : 6;
    const auto bits = dns ? (out.family == 4 ? 32 : 128) : value["prefix"].integer(-1);
    if (bits < 0 || bits > (out.family == 4 ? 32 : 128)) return false;
    out.prefix_length = static_cast<uint32_t>(bits);
#if defined(_WIN32)
    return InetPtonA(out.family == 4 ? AF_INET : AF_INET6, text.c_str(), out.address) == 1;
#else
    return inet_pton(out.family == 4 ? AF_INET : AF_INET6, text.c_str(), out.address) == 1;
#endif
}
bool prefixes(const oneui::json::Value &array, std::vector<OneUiVpnPrefixV1> &values, size_t maximum, bool dns = false) {
    if (!array.isArray() || array.size() > maximum) return false;
    values.resize(array.size());
    for (size_t i = 0; i < values.size(); ++i) if (!prefix(array.at(i), values[i], dns)) return false;
    return true;
}
}
OneUiVpnResult oneui_vpn_connect_json_v1(OneUiVpnSession *session, const char *json, size_t length) {
    if (!session || !json || !length || length > 65536) return ONEUI_VPN_INVALID_ARGUMENT;
    try {
        const auto value = oneui::json::parse(std::string_view(json, length));
        const auto label = value["label"].string();
        const auto mtu = value["mtu"].integer(-1);
        if (value["version"].integer() != 1 || label.find('\0') != std::string::npos || mtu < 1280 || mtu > 9000)
            return ONEUI_VPN_INVALID_ARGUMENT;
        std::vector<OneUiVpnPrefixV1> addresses, routes, dns;
        if (!prefixes(value["addresses"], addresses, 16) || !prefixes(value["routes"], routes, 256) ||
            !prefixes(value["dnsServers"], dns, 8)) return ONEUI_VPN_INVALID_ARGUMENT;
        const auto domains = value["dnsDomains"];
        if (!domains.isArray() || domains.size() > 16) return ONEUI_VPN_INVALID_ARGUMENT;
        std::vector<std::string> names;
        std::vector<const char *> pointers;
        names.reserve(domains.size()); pointers.reserve(domains.size());
        for (size_t i = 0; i < domains.size(); ++i) {
            names.push_back(domains.at(i).string());
            if (names.back().find('\0') != std::string::npos) return ONEUI_VPN_INVALID_ARGUMENT;
            pointers.push_back(names.back().c_str());
        }
        OneUiVpnConfigV1 config{sizeof(config), label.c_str(), static_cast<uint32_t>(mtu), value["allowDefaultRoute"].boolean() ? 1u : 0u,
            addresses.data(), addresses.size(), routes.data(), routes.size(), dns.data(), dns.size(), pointers.data(), pointers.size()};
        return oneui_vpn_connect_v1(session, &config);
    } catch (...) { return ONEUI_VPN_INVALID_ARGUMENT; }
}
