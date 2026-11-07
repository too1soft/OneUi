#pragma once
#include <string>

namespace oneui::mobile {
// Open/create a 0700 child of an application-owned, non-writable parent.
// Reject symlink children. The caller owns the returned close-on-exec fd.
int openPrivateDirectory(const std::string &path);
}
