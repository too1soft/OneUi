#include "oneui/layout/theme_scope.h"
#include <set>
#include <sstream>
#include <iomanip>
#include <stdexcept>
namespace oneui {
void ThemeScope::validate(const std::vector<ThemeColorToken>& tokens) {
    std::set<std::string> seen;
    for(auto& token:tokens)if(token.rgba<0||uint64_t(token.rgba)>0xffffffffull||
        std::find(semanticColorTokens.begin(),semanticColorTokens.end(),token.id)==semanticColorTokens.end()||!seen.insert(token.id).second)
        throw std::invalid_argument("Duplicate, unknown or out-of-range theme color token");
}
void ThemeScope::setTokens(std::vector<ThemeColorToken> tokens) {
    validate(tokens);bool same=tokens.size()==tokens_.size();
    for(size_t i=0;same&&i<tokens.size();++i)same=tokens[i].id==tokens_[i].id&&tokens[i].rgba==tokens_[i].rgba;
    if(same)return;tokens_=std::move(tokens);++revision_;
    if(changed_)changed_();invalidate();
}
StyleSheet ThemeScope::merge(const StyleSheet& parent) const {
    auto sheet=parent;
    for(auto& token:tokens_) {
        std::ostringstream value;value<<'#'<<std::hex<<std::setw(8)<<std::setfill('0')<<uint32_t(token.rgba);
        sheet.setCustomProperty("--"+token.id,value.str());
    }
    return sheet;
}
} // namespace oneui
