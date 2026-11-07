#include "oneui/vpn/c_api.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

namespace {
bool validIp(const OneUiVpnPrefixV1 &ip, bool route) {
    if ((ip.family != 4 && ip.family != 6) || ip.prefix_length > (ip.family == 4 ? 32u : 128u))
        return false;
    const size_t bytes = ip.family == 4 ? 4 : 16;
    if (ip.family == 4 && std::any_of(ip.address + 4, ip.address + 16, [](uint8_t b) { return b != 0; }))
        return false;
    if (!route) {
        if (ip.prefix_length == 0 || std::all_of(ip.address, ip.address + bytes, [](uint8_t b) { return b == 0; }))
            return false;
        if ((ip.family == 4 && ip.address[0] >= 224) || (ip.family == 6 && ip.address[0] == 255))
            return false;
    } else {
        // Routes must already be canonical, preventing adapters from silently
        // claiming a different network than the caller requested.
        for (size_t bit = ip.prefix_length; bit < bytes * 8; ++bit)
            if (ip.address[bit / 8] & (0x80u >> (bit % 8))) return false;
    }
    return true;
}
bool utf8Label(const char *s) {
    if (!s) return false;
    size_t n = 0;
    while (n <= 128 && s[n]) ++n;
    if (n == 0 || n > 128) return false;
    for (size_t i = 0; i < n;) {
        const auto c = static_cast<unsigned char>(s[i++]);
        if (c < 0x20 || c == 0x7f) return false;
        if (c < 0x80) continue;
        size_t rest = c >= 0xc2 && c <= 0xdf ? 1 : c >= 0xe0 && c <= 0xef ? 2 : c >= 0xf0 && c <= 0xf4 ? 3 : 9;
        if (rest == 9 || i + rest > n) return false;
        const auto first = static_cast<unsigned char>(s[i]);
        if ((c == 0xe0 && first < 0xa0) || (c == 0xed && first >= 0xa0) ||
            (c == 0xf0 && first < 0x90) || (c == 0xf4 && first >= 0x90)) return false;
        while (rest--) if ((static_cast<unsigned char>(s[i++]) & 0xc0) != 0x80) return false;
    }
    return true;
}
bool domain(const char *s) {
    if (!s) return false;
    size_t n = 0, label = 0;
    for (; n < 254 && s[n]; ++n) {
        const char c = s[n];
        if (c == '.') {
            if (!label || s[n - 1] == '-') return false;
            label = 0;
        } else {
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || (c == '-' && label))) return false;
            if (++label > 63) return false;
        }
    }
    return n > 0 && n <= 253 && label > 0 && s[n - 1] != '-';
}
std::string quoted(const char *s) {
    std::string out = "\"";
    for (; *s; ++s) {
        if (*s == '\\' || *s == '"') out += '\\';
        out += *s;
    }
    return out + '"';
}
std::string address(const OneUiVpnPrefixV1 &p) {
    char part[8];
    std::string out;
    const size_t groups = p.family == 4 ? 4 : 8;
    for (size_t i = 0; i < groups; ++i) {
        if (i) out += p.family == 4 ? '.' : ':';
        if (p.family == 4) std::snprintf(part, sizeof part, "%u", static_cast<unsigned>(p.address[i]));
        else std::snprintf(part, sizeof part, "%x", static_cast<unsigned>((p.address[2*i] << 8) | p.address[2*i+1]));
        out += part;
    }
    return out;
}
std::string prefixes(const OneUiVpnPrefixV1 *p, size_t count, bool dns) {
    std::string out = "[";
    for (size_t i = 0; i < count; ++i) {
        if (i) out += ',';
        if (dns) out += quoted(address(p[i]).c_str());
        else out += "{\"address\":" + quoted(address(p[i]).c_str()) +
                    ",\"prefix\":" + std::to_string(p[i].prefix_length) + '}';
    }
    return out + ']';
}
}

OneUiVpnResult oneui_vpn_validate_config_v1(const OneUiVpnConfigV1 *c) {
    if (!c || c->struct_size != sizeof(*c) || !utf8Label(c->label) ||
        c->mtu < 1280 || c->mtu > 9000 || c->allow_default_route > 1 ||
        !c->addresses || !c->address_count || c->address_count > 16 ||
        !c->routes || !c->route_count || c->route_count > 256 ||
        c->dns_server_count > 8 || (c->dns_server_count && !c->dns_servers) ||
        c->dns_domain_count > 16 || (c->dns_domain_count && !c->dns_domains) ||
        (c->dns_domain_count && !c->dns_server_count)) return ONEUI_VPN_INVALID_ARGUMENT;
    for (size_t i = 0; i < c->address_count; ++i)
        if (!validIp(c->addresses[i], false)) return ONEUI_VPN_INVALID_ARGUMENT;
    for (size_t i = 0; i < c->route_count; ++i) {
        const bool familyConfigured = std::any_of(c->addresses, c->addresses + c->address_count,
            [&](const OneUiVpnPrefixV1 &p) { return p.family == c->routes[i].family; });
        if (!familyConfigured || !validIp(c->routes[i], true) ||
            (!c->routes[i].prefix_length && !c->allow_default_route))
            return ONEUI_VPN_INVALID_ARGUMENT;
    }
    for (size_t i = 0; i < c->dns_server_count; ++i)
        if (!validIp(c->dns_servers[i], false) || c->dns_servers[i].prefix_length !=
            (c->dns_servers[i].family == 4 ? 32u : 128u)) return ONEUI_VPN_INVALID_ARGUMENT;
    for (size_t i = 0; i < c->dns_domain_count; ++i)
        if (!domain(c->dns_domains[i])) return ONEUI_VPN_INVALID_ARGUMENT;
    return ONEUI_VPN_OK;
}

OneUiVpnResult oneui_vpn_config_json_v1(const OneUiVpnConfigV1 *c, char *buffer,
                                      size_t capacity, size_t *required) {
    if (!required || (!buffer && capacity)) return ONEUI_VPN_INVALID_ARGUMENT;
    *required = 0;
    const auto result = oneui_vpn_validate_config_v1(c);
    if (result != ONEUI_VPN_OK) return result;
    try {
        std::string json = "{\"version\":1,\"label\":" + quoted(c->label) +
            ",\"mtu\":" + std::to_string(c->mtu) + ",\"allowDefaultRoute\":" +
            (c->allow_default_route ? "true" : "false") +
            ",\"addresses\":" + prefixes(c->addresses, c->address_count, false) +
            ",\"routes\":" + prefixes(c->routes, c->route_count, false) +
            ",\"dnsServers\":" + prefixes(c->dns_servers, c->dns_server_count, true) + ",\"dnsDomains\":[";
        for (size_t i = 0; i < c->dns_domain_count; ++i) {
            if (i) json += ',';
            json += quoted(c->dns_domains[i]);
        }
        json += "]}";
        *required = json.size() + 1;
        if (!buffer || capacity < *required) return ONEUI_VPN_BUFFER_TOO_SMALL;
        std::memcpy(buffer, json.c_str(), *required);
        return ONEUI_VPN_OK;
    } catch (...) { return ONEUI_VPN_FAILED; }
}
