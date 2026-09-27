#pragma once
#include "oneui/ui_app.h"
#include "oneui/ui_declarative.h"

namespace oneui::ui {
// Owns the window, mount and worker mailbox in safe destruction order.
// Keep the ViewModel alive until run() returns; close() revokes queued callbacks.
class DeclarativeApp {
    App app_;
    UiMailbox mailbox_;
    std::shared_ptr<int> alive_=std::make_shared<int>(0);
    bool scheduled_=false;
    Mount mount_;
    void schedule() {
        if(scheduled_ || !alive_)return;
        scheduled_=true;auto weak=std::weak_ptr<int>(alive_);
        app_.window().requestAnimationFrame([this,weak](double){if(weak.expired())return;scheduled_=false;mount_.flush();});
    }
public:
    explicit DeclarativeApp(std::wstring title,int width=1100,int height=860)
        :app_(std::move(title),width,height),mount_([this]{schedule();}) {
        auto weak=std::weak_ptr<int>(alive_);
        mailbox_.setWake([this,weak]{app_.window().post([this,weak]{if(!weak.expired())mailbox_.drain();});});
    }
    ~DeclarativeApp(){close();}
    DeclarativeApp(const DeclarativeApp&)=delete;
    Window& window(){return app_.window();}
    Mount& mount(){return mount_;}
    UiMailbox::Sender dispatcher() const{return mailbox_.sender();}
    void close(){mailbox_.close();alive_.reset();}
    int run(Element root,std::function<void()> ready={}) {
        try {auto result=app_.run(root,[&]{mount_.flush();if(ready)ready();});close();return result;}
        catch(...){close();throw;}
    }
};
}
