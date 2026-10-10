#pragma once
#include "oneui/view.h"
#include "oneui/style_sheet.h"
#include <array>
namespace oneui {
// Public semantic color vocabulary. No metrics, URLs or executable CSS.
inline constexpr std::array<const char*,14> semanticColorTokens={
    "chrome","canvas","surface","ink","muted","line","control","hover",
    "accent","accent-ink","selection","error","warning","success"};
struct ThemeColorToken {std::string id;int64_t rgba=0;};
class ONEUI_API ThemeScope final : public View {
public:
    static void validate(const std::vector<ThemeColorToken>&);
    void setTokens(std::vector<ThemeColorToken>);
    const std::vector<ThemeColorToken>& tokens() const{return tokens_;}
    uint64_t revision() const{return revision_;}
    void setOnTokensChanged(std::function<void()> callback){changed_=std::move(callback);}
    StyleSheet merge(const StyleSheet&) const;
    Size naturalSize() const override{return children().empty()?Size{}:children()[0]->naturalSize();}
protected:
    void layoutChildren() override{if(!children().empty())children()[0]->setFrame(frame());}
private:
    std::vector<ThemeColorToken> tokens_;
    uint64_t revision_=0;
    std::function<void()> changed_;
};
} // namespace oneui
