#pragma once

#include "oneui/controls/code_editor.h"

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace oneui::code {

class Document;
struct LineChange;

// Incremental tokenizer. Only the state at the end of every line is kept for
// the whole document; tokens are produced for the lines being painted. After
// an edit, lexing resumes at the first changed line and stops as soon as a
// line ends in the same state as before.
class Lexer {
public:
    void setLanguage(CodeLanguage language);
    const CodeLanguage& language() const { return language_; }
    void reset(const Document& document);
    void apply(const Document& document, const LineChange& change);
    std::vector<CodeToken> tokens(const Document& document, std::size_t line) const;
    std::size_t relexed() const { return relexed_; }
    // True when the column lies in a comment or string (bracket matching).
    bool inert(const Document& document, std::size_t line, std::size_t column) const;

private:
    std::uint32_t lex(const std::wstring& text, std::uint32_t state, std::vector<CodeToken>* out) const;
    std::uint32_t startState(std::size_t line) const { return line == 0 ? 0 : endStates_[line - 1]; }
    CodeTokenKind classify(const std::wstring& word, bool call) const;
    std::uint32_t dollarTag(const std::wstring& tag) const;

    CodeLanguage language_;
    std::unordered_set<std::wstring> keywords_, types_, functions_, constants_;
    std::vector<std::uint32_t> endStates_;
    mutable std::vector<std::wstring> dollarTags_;
    std::size_t relexed_ = 0;
};

} // namespace oneui::code
