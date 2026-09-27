#pragma once
#include "oneui/ui_declarative.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <condition_variable>

namespace oneui::ui {
inline std::string readStyleFile(const std::filesystem::path& file) {
    std::ifstream input(file,std::ios::binary);
    if(!input) throw std::runtime_error("Cannot read style source: "+file.string());
    return {std::istreambuf_iterator<char>(input),{}};
}
inline std::string inlineStyles(const std::string& text,const std::string& scope,const std::string& file) {
    std::size_t pos=0; std::string output;
    while((pos=text.find("<style",pos))!=text.npos) {
        auto begin=text.find('>',pos),end=text.find("</style>",begin);
        if(begin==text.npos || end==text.npos) throw std::runtime_error(file+": unclosed style block");
        if(text.substr(pos,begin-pos).find("scoped")==std::string::npos) throw std::runtime_error(file+": inline styles require scoped");
        output+=syntax::css(text.substr(begin+1,end-begin-1),scope,file,1+int(std::count(text.begin(),text.begin()+begin+1,'\n')));
        pos=end+8;
    }
    return output;
}
// Polls files off-thread, posts only after an observed change has settled for
// 200ms. The host performs parsing/application on the UI thread transactionally.
class StyleWatcher {
    std::atomic<bool> stopped_{false}; std::thread thread_;
    std::mutex mutex_; std::condition_variable wake_;
public:
    StyleWatcher(UiMailbox::Sender sender,std::vector<std::string> files,std::function<void()> changed) {
        auto stamp=[](const std::string& file) { std::error_code ec; auto time=std::filesystem::last_write_time(file,ec); return ec?std::filesystem::file_time_type::min():time; };
        std::vector<std::filesystem::file_time_type> previous(files.size());
        for(std::size_t i=0;i<files.size();++i) previous[i]=stamp(files[i]);
        thread_=std::thread([this,sender,files=std::move(files),changed=std::move(changed),stamp,previous=std::move(previous)]() mutable {
            bool pending=false; auto last=std::chrono::steady_clock::now();
            while(!stopped_ && !sender.cancelled()) {
                { std::unique_lock<std::mutex> lock(mutex_); wake_.wait_for(lock,std::chrono::milliseconds(50),[this]{return stopped_.load();}); }
                auto now=std::chrono::steady_clock::now();
                for(std::size_t i=0;i<files.size();++i) { auto value=stamp(files[i]); if(value!=previous[i]) {previous[i]=value;pending=true;last=now;} }
                if(pending && now-last>=std::chrono::milliseconds(200)) { sender.post(changed); pending=false; }
            }
        });
    }
    ~StyleWatcher() { stopped_=true; wake_.notify_all(); if(thread_.joinable()) thread_.join(); }
};
}
