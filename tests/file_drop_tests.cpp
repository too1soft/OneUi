#include "platform/shared/file_drop.h"
#include <iostream>
int main() {
    using oneui::platform::filePathsFromUriList;
    int failures = 0;
    auto check = [&](bool value) { if (!value) ++failures; };
    const auto paths = filePathsFromUriList("# comment\r\nfile:///tmp/a%20b.txt\r\nfile://localhost/tmp/%E4%B8%AD%F0%9F%98%80\nfile:/tmp/a+b\n");
    check(paths == std::vector<std::wstring>{L"/tmp/a b.txt", L"/tmp/\u4e2d\U0001f600", L"/tmp/a+b"});
    for (const auto* value : {"https://example.com/a", "file://remote/a", "file:relative", "file:///tmp/%00x", "file:///tmp/%", "file:///tmp/%GG", "file:///tmp/%FF", "file:///tmp/x#fragment", "file:///tmp/x?query"})
        check(filePathsFromUriList(value).empty());
    check(filePathsFromUriList(std::string(4u * 1024u * 1024u + 1, 'x')).empty());
    if (failures) std::cerr << failures << " URI-list parsing checks failed\n";
    return failures ? 1 : 0;
}
