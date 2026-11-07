#include "napi_support.h"
#include <cerrno>
#include <cstring>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace oneui::harmony {
namespace {
napi_value info(napi_env env, napi_callback_info argsInfo) {
    return safe(env, [&] {
        auto a = args(env, argsInfo, 1); const auto name = text(env, a[0], IFNAMSIZ - 1);
        const auto index = if_nametoindex(name.c_str());
        if (!index) throw std::runtime_error("Unknown physical interface");
        const int fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
        if (fd < 0) throw std::runtime_error("Physical interface flags");
        ifreq request{}; std::memcpy(request.ifr_name, name.c_str(), name.size() + 1);
        const int status = ioctl(fd, SIOCGIFFLAGS, &request); close(fd);
        if (status != 0) throw std::runtime_error("Physical interface flags");
        napi_value result; check(napi_create_object(env, &result));
        property(env, result, "index", number(env, static_cast<int>(index)));
        property(env, result, "up", boolean(env, request.ifr_flags & IFF_UP));
        property(env, result, "loopback", boolean(env, request.ifr_flags & IFF_LOOPBACK));
        property(env, result, "pointToPoint", boolean(env, request.ifr_flags & IFF_POINTOPOINT));
        property(env, result, "multicast", boolean(env, request.ifr_flags & IFF_MULTICAST));
        return result;
    });
}
}
void registerNetwork(napi_env env, napi_value exports) {
    const napi_property_descriptor methods[]{{"interfaceInfo", nullptr, info, nullptr, nullptr, nullptr, napi_default, nullptr}};
    check(napi_define_properties(env, exports, std::size(methods), methods));
}
}
