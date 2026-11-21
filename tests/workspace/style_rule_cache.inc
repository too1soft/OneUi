void styleRuleCache() {
    StyleSheet original;
    std::string error;
    original.setCustomProperty("--ink","#112233");
    original.setCustomProperty("--surface","#334455");
    check(original.addRulesFromCss(".base {color:var(--ink);background:var(--surface);font-size:14px;} .base:hover {font-weight:650;}",&error),"cached declaration CSS");
    for(int i=0;i<30;++i) {
        StyleNode node{"text",{"base","instance-"+std::to_string(i)},i%2?StyleStateHover:StyleStateNone};
        auto box=original.resolve(node);
        check(box.foreground&&box.foreground->r==0x11&&box.background.color->b==0x55,"shared declarations preserve distinct selector/state results");
        check(box.fontWeight.has_value()==bool(i%2),"pseudo-state stays outside declaration cache");
    }
    auto transient=std::make_shared<StyleSheet>();original.setTransientStyleSheet(transient);
    auto copy=original;
    check(copy.version()==original.version()&&copy.transientStyleSheet()==transient,"copies preserve version and transient palette ownership");
    StyleSheet assigned;assigned=original;assigned=assigned;
    check(assigned.resolve({"text",{"base"},StyleStateNone}).foreground->r==0x11,"copy assignment and self-assignment preserve styles");
    original.setCustomProperty("--ink","#abcdef");
    StyleNode node{"text",{"base"},StyleStateNone};
    check(original.resolve(node).foreground->r==0xab&&copy.resolve(node).foreground->r==0x11,"token invalidation and copies stay independent");
    StyleRule late;late.selector=".base";late.box.foreground=Color{5,6,7,255};original.addRule(late);
    check(original.resolve(node).foreground->r==5,"new rules invalidate declaration and node results together");
    check(copy.resolve(node).foreground->r==0x11,"source mutations never alter copied rules");
    const std::string palette="color:var(--ink);background-color:var(--surface);";
    auto first=copy.resolveTokens(palette);
    const std::string branded="color:var(--brand);background-color:var(--surface);";
    auto alias=copy;alias.setCustomProperty("--brand","#fedcba");
    check(alias.resolveTokens(branded).foreground->r==0xfe,"token snapshots include arbitrary custom names");
    alias.setCustomProperty("--brand","#010203");
    check(alias.resolveTokens(branded).foreground->r==1,"changed custom tokens invalidate shared palettes");
    check(copy.resolveTokens(palette).foreground->r==first.foreground->r,"copies never reuse another palette's colors");
    for(int i=0;i<24;++i){alias.setCustomProperty("--unused",std::to_string(i));alias.resolveTokens(palette);}
    check(copy.resolveTokens(palette).foreground->r==0x11,"evicted palettes recompute correctly");

    const std::string theme = ":root {--preview-ink:#203040;}\nlabel.preview {color:var(--preview-ink);}";
    const std::string scoped = "\n/* Scope comment */\nlabel.preview {font-size:12px;}"
                               "\n:root {--preview-ink:#405060;}";
    const StyleNode preview{"label", {"preview"}, StyleStateNone};
    StyleSheet base, combined, cold;
    check(base.addRulesFromCss(theme, &error), "base theme enters CSS cache");
    combined.setTransientStyleSheet(transient);
    check(combined.addRulesFromCss(theme + scoped, &error), "scoped CSS builds on a cached theme");
    check(cold.addRulesFromCss("/* cold stylesheet */\n" + theme + scoped, &error),
          "same cascade can be parsed in one pass");
    const auto resolved = combined.resolve(preview);
    const auto expected = cold.resolve(preview);
    check(resolved.foreground->r == 0x40 && resolved.foreground->r == expected.foreground->r &&
          resolved.fontSize == 12 && resolved.fontSize == expected.fontSize &&
          combined.version() == cold.version() && combined.transientStyleSheet() == transient,
          "cached prefix preserves cascade, token references, version and transient stylesheet");
    check(base.resolve(preview).foreground->r == 0x20 &&
          !base.resolve(preview).fontSize, "scoped styles leave the cached theme intact");
    StyleSheet invalid;
    check(!invalid.addRulesFromCss(theme + "\nlabel.preview {font-size:invalid;}", &error) &&
          error.find("CSS:3:1:") == 0 && invalid.rules().empty() && invalid.customProperties().empty(),
          "failed scoped CSS keeps original source coordinates and publishes no rules");
}
