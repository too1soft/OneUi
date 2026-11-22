#include "oneui/export.h"
ONEUI_API int oneui_used();
int main() { return oneui_used() == 42 ? 0 : 1; }
