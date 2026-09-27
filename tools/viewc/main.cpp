#include "oneui/ui_template_support.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <limits>

namespace fs=std::filesystem;
using namespace oneui::ui::syntax;
struct Attribute { std::string value; int line=1; };
struct Node { std::string tag,text; std::map<std::string,Attribute> attrs; std::vector<Node> children; int line=1; };
struct Parser {
    std::string source,file; std::size_t pos=0;
    int line() const { return 1+int(std::count(source.begin(),source.begin()+pos,'\n')); }
    [[noreturn]] void fail(std::string message) const { throw std::runtime_error(file+":"+std::to_string(line())+":1: "+message); }
    bool starts(std::string s) const { return source.compare(pos,s.size(),s)==0; }
    void space() { while(pos<source.size() && std::isspace((unsigned char)source[pos])) ++pos; }
    std::string name() {
        auto begin=pos; while(pos<source.size() && (std::isalnum((unsigned char)source[pos]) || std::string("_-:@.").find(source[pos])!=std::string::npos)) ++pos;
        if(begin==pos) fail("Expected name"); return source.substr(begin,pos-begin);
    }
    Node node() {
        space(); if(!starts("<")) fail("Expected opening tag"); ++pos;
        Node n; n.line=line(); n.tag=name(); space();
        while(!starts(">") && !starts("/>")) {
            auto key=name(); space(); Attribute a; a.line=line();
            if(starts("=")) {
                ++pos; space(); if(!starts("\"") && !starts("'")) fail("Attribute needs quoted value");
                char quote=source[pos++]; auto begin=pos;
                while(pos<source.size() && source[pos]!=quote) ++pos;
                if(pos==source.size()) fail("Unclosed attribute"); a.value=source.substr(begin,pos-begin); ++pos;
            } else if(key!="scoped" && key!="v-else") fail("Attribute needs a value");
            if(!n.attrs.emplace(key,a).second) fail("Duplicate attribute: "+key); space();
        }
        if(starts("/>")) { pos+=2; return n; } ++pos;
        if(n.tag=="style") {
            auto end=source.find("</style>",pos); if(end==source.npos) fail("Unclosed style");
            n.text=source.substr(pos,end-pos); pos=end+8; return n;
        }
        while(!starts("</")) {
            if(pos>=source.size()) fail("Unclosed "+n.tag);
            if(starts("<!--")) { auto end=source.find("-->",pos); if(end==source.npos) fail("Unclosed comment"); pos=end+3; }
            else if(starts("<")) n.children.push_back(node());
            else n.text+=source[pos++];
        }
        pos+=2; auto close=name(); space(); if(close!=n.tag || !starts(">")) fail("Mismatched closing tag"); ++pos;
        n.text=trim(n.text); return n;
    }
    std::vector<Node> all() { std::vector<Node> nodes; if(starts("\xEF\xBB\xBF"))pos=3;space(); while(pos<source.size()) { if(starts("<!--")) {auto end=source.find("-->",pos);if(end==source.npos)fail("Unclosed comment");pos=end+3;}else nodes.push_back(node()); space(); } return nodes; }
};
std::string read(fs::path path) { std::ifstream f(path,std::ios::binary); if(!f) throw std::runtime_error("Cannot read "+path.string()); return {std::istreambuf_iterator<char>(f),{}}; }
std::string quote(const std::string& s) { std::ostringstream out; out<<std::quoted(s); auto result=out.str(); std::string escaped; for(char c:result) { if(c=='\n') escaped+="\\n"; else if(c=='\r') escaped+="\\r"; else escaped+=c; } return escaped; }
struct Document { fs::path file; std::string name,scope,vmType; Node root; std::string style; std::map<std::string,std::string> imports; };
struct Compiler {
    std::vector<Document> docs;
    std::set<std::string> loading;
    std::ostringstream out;
    int serial=0;
    std::set<std::string> references;
    [[noreturn]] void fail(const Document& d,int line,std::string message) { throw std::runtime_error(d.file.generic_string()+":"+std::to_string(line)+":1: "+message); }
    void load(fs::path path,std::string name) {
        path=fs::absolute(path).lexically_normal();
        if(!loading.insert(path.generic_string()).second) throw std::runtime_error("Cyclic template import: "+path.string());
        Document d; d.file=path; d.name=name; d.scope="scope_"+name;
        auto nodes=Parser{read(path),path.generic_string()}.all(); bool found=false;
        for(auto& n:nodes) {
            if(n.tag=="import") {
                if(n.attrs.size()!=2 || !n.attrs.count("name") || !n.attrs.count("src")) fail(d,n.line,"import needs name and src");
                auto alias=n.attrs.at("name").value;
                if(!std::regex_match(alias,std::regex("[A-Z][A-Za-z0-9_]*")) || components().count(alias) || d.imports.count(alias)) fail(d,n.line,"Invalid or duplicate component import");
                auto childName=name+"_"+alias; d.imports[alias]=childName; load(path.parent_path()/n.attrs.at("src").value,childName);
            } else if(n.tag=="template") {
                if(found || n.children.size()!=1 || !n.text.empty()) fail(d,n.line,"Exactly one template with one root is required");
                for(auto& a:n.attrs) if(a.first!="view-model") fail(d,n.line,"Unknown template attribute");
                if(n.attrs.count("view-model")) { d.vmType=n.attrs.at("view-model").value; if(!std::regex_match(d.vmType,std::regex("[A-Za-z_][A-Za-z0-9_:]*"))) fail(d,n.line,"Invalid ViewModel type"); }
                found=true; d.root=n.children.front();
            } else if(n.tag=="style") {
                if(n.attrs.size()!=1 || !n.attrs.count("scoped")) fail(d,n.line,"Inline styles require scoped");
                d.style+=css(n.text,d.scope,path.generic_string(),n.line);
            } else fail(d,n.line,"Expected import, template or style");
        }
        if(!found) fail(d,1,"Missing template"); docs.push_back(std::move(d)); loading.erase(path.generic_string());
    }
    std::string expr(const Document& d,int line,const std::string& value,const std::string& item={}) {
        if(value=="true" || value=="false") return value;
        if(!std::regex_match(value,std::regex("[A-Za-z_][A-Za-z0-9_]*(\\.[A-Za-z_][A-Za-z0-9_]*)*"))) fail(d,line,"Only member references are supported; compute expressions in the ViewModel");
        if(!item.empty() && value.rfind(item+".",0)==0) return item+"->"+value.substr(item.size()+1);
        return "vm."+value;
    }
    void location(const Document& d,int line) { out<<"\n#line "<<line<<" "<<quote(d.file.generic_string())<<"\n"; }
    float layoutValue(const Document& d,const std::string& key,const Attribute& a) {
        if(!std::regex_match(a.value,std::regex("[0-9]+(\\.[0-9]+)?"))) fail(d,a.line,key+" expects a finite nonnegative number in logical pixels (no px suffix)");
        float value=0;
        try { value=std::stof(a.value); validateLayoutNumber(key,value); }
        catch(const std::exception&) { fail(d,a.line,key+" expects a finite "+std::string(key=="max-width" || key=="min-column-width"?"positive":"nonnegative")+" number within float range"); }
        return value;
    }
    std::string list(const std::vector<std::string>& values) { std::string s="{"; for(auto& v:values) s+=v+","; return s+"}"; }
    std::string emit(const Document& d,Node n,std::string item={}) {
        location(d,n.line);
        for(const auto& a:n.attrs) if(layoutNumber(a.first)) layoutValue(d,a.first,a.second);
        if(n.attrs.count("min") && n.attrs.count("max") && layoutValue(d,"min",n.attrs.at("min"))>layoutValue(d,"max",n.attrs.at("max")))
            fail(d,n.attrs.at("max").line,"max must be at least min; both constrain the parent's main axis, use Content max-width for a body width limit");
        if(n.attrs.count("v-for")) {
            auto value=n.attrs.at("v-for").value; std::smatch match;
            if(!std::regex_match(value,match,std::regex("([a-zA-Z_][a-zA-Z0-9_]*) in ([a-zA-Z_][a-zA-Z0-9_.]*)")) || !n.attrs.count(":key")) fail(d,n.line,"v-for requires 'item in member' and :key");
            auto variable=match[1].str(),source=expr(d,n.line,match[2].str(),item),key=expr(d,n.line,n.attrs.at(":key").value,variable);
            if(n.attrs.count("v-if") || n.attrs.count("v-else")) fail(d,n.line,"Put conditions inside v-for, not on the same node");
            n.attrs.erase("v-for"); n.attrs.erase(":key"); auto id="e"+std::to_string(++serial);
            out<<"auto "<<id<<" = ui.repeat("<<source<<", [](const auto& "<<variable<<") { return "<<key<<"; }, [&vm](oneui::ui::Mount& ui, auto "<<variable<<") {\n";
            auto child=emit(d,n,variable); out<<"return "<<child<<"; });\n"; return id;
        }
        std::vector<std::string> children;
        std::string previousCondition;
        bool custom=d.imports.count(n.tag)>0;
        if(!custom && components().count(n.tag)) {
            auto tag=components().at(n.tag);
            if(tag!="stack" && tag!="scroll-view" && !n.children.empty())fail(d,n.line,"Leaf components cannot contain child elements");
            if((n.tag=="Scroll" || n.tag=="FormRow") && n.children.size()!=1)fail(d,n.line,n.tag+" requires exactly one child");
            if(n.tag=="FormGrid")for(auto& child:n.children) {
                if(child.tag!="FormRow")fail(d,child.line,"FormGrid accepts FormRow children only");
                if(child.attrs.count("v-for"))fail(d,child.line,"FormGrid direct v-for children are unsupported in v1");
            }
            if(pagePattern(n.tag) && std::count_if(n.children.begin(),n.children.end(),[](auto& child){return child.tag=="ActionBar";})>1)fail(d,n.line,"Page patterns accept at most one ActionBar");
        }
        if(n.text.find("{{")!=n.text.npos)fail(d,n.line,"Use :text for member binding; interpolation expressions are unsupported");
        std::string slotVariable;
        if(custom) {
            slotVariable="slots"+std::to_string(++serial); out<<"oneui::ui::Slots "<<slotVariable<<";\n";
            std::map<std::string,std::vector<Node>> groups;
            for(auto child:n.children) { auto name=child.attrs.count("slot")?child.attrs.at("slot").value:"default"; child.attrs.erase("slot"); groups[name].push_back(std::move(child)); }
            for(auto& group:groups) {
                out<<slotVariable<<"["<<quote(group.first)<<"] = [&vm"<<(item.empty()?"":","+item)<<"](oneui::ui::Mount& ui) { auto old = ui.scope(); ui.setScope("<<quote(d.scope)<<");\n";
                std::vector<std::string> content; for(auto& child:group.second) content.push_back(emit(d,child,item));
                out<<"ui.setScope(old); return std::vector<oneui::ui::Element>"<<list(content)<<"; };\n";
            }
        } else for(auto child:n.children) {
            auto condition=child.attrs.find("v-if"),otherwise=child.attrs.find("v-else");
            bool inverse=otherwise!=child.attrs.end();
            if(inverse && previousCondition.empty()) fail(d,child.line,"v-else must immediately follow v-if");
            std::string current=condition!=child.attrs.end()?expr(d,child.line,condition->second.value,item):(inverse?previousCondition:"");
            child.attrs.erase("v-if"); child.attrs.erase("v-else");
            auto c=emit(d,child,item); children.push_back(c);
            if(!current.empty()) out<<"ui.condition("<<c<<","<<current<<","<<(inverse?"true":"false")<<");\n";
            previousCondition=inverse?"":current;
        }
        auto id="e"+std::to_string(++serial); location(d,n.line);
        if(n.tag=="slot") {
            for(auto& a:n.attrs) if(a.first!="name") fail(d,a.second.line,"Unknown slot attribute");
            auto name=n.attrs.count("name")?n.attrs.at("name").value:"default";
            out<<"auto "<<id<<" = ui.make(\"Column\", slots.count("<<quote(name)<<") ? slots.at("<<quote(name)<<")(ui) : std::vector<oneui::ui::Element>"<<list(children)<<");\n"; return id;
        }
        if(custom) out<<"auto "<<id<<" = build_"<<d.imports.at(n.tag)<<"(vm,ui,"<<slotVariable<<");\n";
        else {
            if(!components().count(n.tag)) fail(d,n.line,"Unknown component: "+n.tag);
            out<<"auto "<<id<<" = ui.make("<<quote(n.tag)<<","<<list(children)<<");\n";
        }
        out<<"ui.locate("<<id<<","<<quote(d.file.generic_string())<<","<<n.line<<");\n";
        if(!n.text.empty()) {
            if(!property(n.tag,"text")) fail(d,n.line,"Text content is not supported here");
            out<<"ui.set("<<id<<",\"text\",oneui::ui::wide("<<quote(n.text)<<"));\n";
        }
        for(auto& entry:n.attrs) {
            auto key=entry.first,value=entry.second.value; location(d,entry.second.line);
            if(key=="ref") {
                if(!std::regex_match(value,std::regex("[A-Za-z_][A-Za-z0-9_]*")))fail(d,entry.second.line,"ref expects a static identifier");
                if(!item.empty())fail(d,entry.second.line,"ref inside v-for is unsupported; use keyed row state");
                if(!references.insert(value).second)fail(d,entry.second.line,"Duplicate ref: "+value);
                out<<"ui.remember("<<quote(value)<<","<<id<<".widget);\n";continue;
            }
            if(key=="@activate" || key=="@delete") {
                if(n.tag!="DataTable")fail(d,entry.second.line,"activate/delete events require DataTable");
                out<<"ui.tableEvent("<<id<<","<<quote(key.substr(1))<<","<<expr(d,entry.second.line,value,item)<<");\n";continue;
            }
            if(key=="v-model") {
                if(n.tag!="Input" && n.tag!="SearchInput" && n.tag!="Switch" && n.tag!="Select" && n.tag!="DataTable") fail(d,n.line,"v-model unsupported on component");
                auto type=n.tag=="Switch"?"bool":n.tag=="Select"?"int":"std::wstring";
                out<<"ui.modelTyped<"<<type<<">("<<id<<","<<expr(d,n.line,value,item)<<");\n"; continue;
            }
            if(key=="v-if") {out<<"ui.condition("<<id<<","<<expr(d,n.line,value,item)<<");\n";continue;}
            if(key=="@click") { if(n.tag!="Button") fail(d,n.line,"click only supported on Button"); out<<"ui.click("<<id<<","<<expr(d,n.line,value,item)<<");\n";continue; }
            bool bound=key[0]==':'; if(bound) key.erase(0,1);
            if(!property(custom?"Column":n.tag,key)) fail(d,n.line,"Unknown property/event: "+entry.first+" on "+n.tag);
            if(key=="item-key") { if(value!="id" || bound) fail(d,n.line,"DataTable uses TableRow.id as its stable key"); continue; }
            if(bound) {
                if(layoutNumber(key) || key=="align" || key=="class" || key=="variant") fail(d,entry.second.line,"This property is static in v1");
                std::string type="std::wstring";
                if(key=="disabled" || key=="visible" || key=="checked") type="bool";
                else if(key=="selectedIndex") type="int";
                else if(key=="items") type=n.tag=="Select"?"std::vector<std::wstring>":"std::vector<oneui::ui::TableRow>";
                else if(key=="columns") type="std::vector<oneui::TableColumn>";
                out<<"ui.bindTyped<"<<type<<">("<<id<<","<<quote(key)<<","<<expr(d,n.line,value,item)<<");\n";
            } else {
                std::string v="oneui::ui::wide("+quote(value)+")";
                if(key=="class" || key=="variant") v="std::string("+quote(value)+")";
                else if(key=="visible" || key=="disabled" || key=="checked") { if(value!="true" && value!="false") fail(d,n.line,"Expected true or false"); v=value; }
                else if(layoutNumber(key)) {
                    std::ostringstream number; number<<std::setprecision(std::numeric_limits<float>::max_digits10)<<layoutValue(d,key,entry.second);
                    v="float("+number.str()+")";
                } else if(key=="selectedIndex") {
                    if(!std::regex_match(value,std::regex("-1|[0-9]+"))) fail(d,entry.second.line,"selectedIndex expects an integer >= -1");
                    try { v=std::to_string(std::stoi(value)); } catch(const std::exception&) { fail(d,entry.second.line,"selectedIndex is outside integer range"); }
                } else if(key=="align") {
                    if(value!="start" && value!="center" && value!="end") fail(d,entry.second.line,"Content align must be start, center or end");
                } else if(key=="tone") {
                    if(value!="neutral" && value!="success" && value!="warning" && value!="error" && value!="pending") fail(d,entry.second.line,"Status tone must be neutral, success, warning, error or pending");
                } else if(key=="items" || key=="columns") fail(d,n.line,"Use a bound ViewModel member for collection properties");
                out<<"ui.set("<<id<<","<<quote(key)<<","<<v<<");\n";
            }
        }
        return id;
    }
    std::string generate(std::string header) {
        out<<"// Generated by oneui-viewc. Edit the .one source.\n#pragma once\n#include \"oneui/ui_declarative.h\"\n";
        if(!header.empty()) out<<"#include "<<quote(header)<<"\n";
        for(auto& d:docs) {
            references.clear();
            out<<"template<class VM> oneui::ui::Element build_"<<d.name<<"(VM& vm,oneui::ui::Mount& ui,const oneui::ui::Slots& slots={}) { auto oldScope=ui.scope(); ui.setScope("<<quote(d.scope)<<");\n";
            if(!d.vmType.empty())out<<"static_assert(std::is_same_v<VM,"<<d.vmType<<">,\"Wrong template ViewModel\");\n";
            auto root=emit(d,d.root); out<<"ui.setScope(oldScope); return "<<root<<"; }\n";
        }
        auto name=docs.back().name;
        out<<"inline std::string styles_"<<name<<"() { return ";
        std::string styles; for(auto& d:docs) styles+=d.style; out<<quote(styles)<<"; }\n";
        out<<"inline std::vector<std::pair<std::string,std::string>> sources_"<<name<<"() { return {";
        for(auto& d:docs) out<<"{"<<quote(d.file.generic_string())<<","<<quote(d.scope)<<"},";
        out<<"}; }\n"; return out.str();
    }
};
int main(int argc,char** argv) {
    try {
        std::map<std::string,std::string> args;
        for(int i=1;i+1<argc;i+=2) args[argv[i]]=argv[i+1];
        if(!args.count("--input") || !args.count("--output") || !args.count("--name")) throw std::runtime_error("Usage: oneui-viewc --input page.one --output page.g.h --name Page [--vm-header vm.h]");
        if(!std::regex_match(args["--name"],std::regex("[A-Za-z_][A-Za-z0-9_]*"))) throw std::runtime_error("Invalid generated name");
        Compiler compiler; compiler.load(args["--input"],args["--name"]); auto generated=compiler.generate(args["--vm-header"]);
        fs::path output=args["--output"]; fs::create_directories(output.parent_path());
        std::ofstream file(output,std::ios::binary); file<<generated; if(!file) throw std::runtime_error("Cannot write output");
        if(args.count("--depfile")) { std::ofstream dep(args["--depfile"]); auto escape=[](std::string s) { std::string r; for(char c:s) { if(c==' ' || c=='#') r+='\\'; r+=c; } return r; }; dep<<escape(output.generic_string())<<":"; for(auto& d:compiler.docs) dep<<" "<<escape(d.file.generic_string()); dep<<"\n"; }
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<"\n"; return 1; }
}
