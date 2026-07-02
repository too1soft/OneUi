#pragma once

#include "oneui/export.h"

#include <string>

namespace oneui {

class ONEUI_API Clipboard {
public:
    virtual ~Clipboard() = default;

    virtual void setText(std::wstring text) = 0;
    virtual std::wstring text() const = 0;
};

class ONEUI_API MemoryClipboard final : public Clipboard {
public:
    void setText(std::wstring text) override;
    std::wstring text() const override;

private:
    std::wstring text_;
};

} // namespace oneui
