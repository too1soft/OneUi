#include "internal/media_loader.h"
#include "include/codec/SkCodec.h"
#include "include/core/SkData.h"
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>
namespace oneui::internal {
namespace {
constexpr size_t maxFile=32*1024*1024,maxPixels=16*1024*1024,cacheLimit=32*1024*1024;
std::shared_ptr<MediaPixels> failure(const char* code,const char* message) {
    auto r=std::make_shared<MediaPixels>();r->code=code;r->message=message;return r;
}
struct Request {std::string source;std::shared_ptr<std::atomic<bool>> cancelled;MediaPreview::Completion complete;};
struct Cached {std::string key;std::shared_ptr<const MediaPixels> pixels;};
class Loader {
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Request> queue_;
    std::deque<Cached> cache_;
    size_t bytes_=0;
    bool stop_=false;
    std::thread worker_;
    std::shared_ptr<const MediaPixels> decode(const Request& r) {
        if(r.source.find("://")!=std::string::npos||r.source.rfind("\\\\",0)==0||r.source.rfind("//",0)==0)
            return failure("unsupported","Only local static image files are accepted");
        auto path=std::filesystem::u8path(r.source);std::error_code error;
        if(!std::filesystem::is_regular_file(path,error))return failure("not-found","Image file is unavailable");
        auto size=std::filesystem::file_size(path,error);if(error||size>maxFile)return failure("limit","Encoded image exceeds 32 MiB");
        auto time=std::filesystem::last_write_time(path,error);if(error)return failure("io","Cannot read image revision");
        auto canonical=std::filesystem::absolute(path).lexically_normal().u8string();
        auto key=std::string(canonical.begin(),canonical.end())+":"+std::to_string(size)+":"+std::to_string(static_cast<long long>(time.time_since_epoch().count()));
        for(auto it=cache_.begin();it!=cache_.end();++it)if(it->key==key) {
            auto value=*it;cache_.erase(it);cache_.push_back(value);return value.pixels;
        }
        std::ifstream file(path,std::ios::binary);if(!file)return failure("permission","Cannot open image");
        auto data=SkData::MakeUninitialized(size);if(!data)return failure("limit","Image allocation failed");
        if(!file.read(static_cast<char*>(data->writable_data()),std::streamsize(size)))return failure("io","Cannot read image bytes");
        if(*r.cancelled)return {};
        auto codec=SkCodec::MakeFromData(data);if(!codec)return failure("format","Unsupported or malformed image");
        const auto info=codec->getInfo();int width=info.width(),height=info.height();
        if(width<=0||height<=0||size_t(width)*size_t(height)>maxPixels)return failure("limit","Decoded image exceeds 16 megapixels");
        if(codec->getFrameCount()>1)return failure("unsupported","Animated images are not enabled");
        auto result=std::make_shared<MediaPixels>();result->width=width;result->height=height;result->rgba.resize(size_t(width)*height*4);
        auto target=SkImageInfo::Make(width,height,kRGBA_8888_SkColorType,kUnpremul_SkAlphaType);
        if(codec->getPixels(target,result->rgba.data(),size_t(width)*4)!=SkCodec::kSuccess)return failure("decode","Image decoding failed");
        if(*r.cancelled)return {};
        auto cost=result->rgba.size();
        while(!cache_.empty()&&(bytes_+cost>cacheLimit||cache_.size()>=8)){bytes_-=cache_.front().pixels->rgba.size();cache_.pop_front();}
        if(cost<=cacheLimit){cache_.push_back({key,result});bytes_+=cost;}
        return result;
    }
    void run() {
        for(;;) {
            Request request;
            {std::unique_lock<std::mutex> lock(mutex_);wake_.wait(lock,[&]{return stop_||!queue_.empty();});if(stop_)return;request=std::move(queue_.front());queue_.pop_front();}
            if(*request.cancelled)continue;
            std::shared_ptr<const MediaPixels> result;
            try{result=decode(request);}catch(...){result=failure("decode","Image decoding failed");}
            if(result&&!*request.cancelled)try{request.complete(std::move(result));}catch(...){}
        }
    }
public:
    Loader():worker_([this]{run();}){}
    ~Loader(){{std::lock_guard<std::mutex> lock(mutex_);stop_=true;queue_.clear();}wake_.notify_one();worker_.join();}
    void submit(Request request) {
        bool full=false;
        {std::lock_guard<std::mutex> lock(mutex_);
            queue_.erase(std::remove_if(queue_.begin(),queue_.end(),[](const auto& r){return bool(*r.cancelled);}),queue_.end());
            full=queue_.size()>=64;if(!full)queue_.push_back(std::move(request));
        }
        if(full)request.complete(failure("busy","Image decode queue is full"));else wake_.notify_one();
    }
};
}
void loadLocalMedia(std::string source,std::shared_ptr<std::atomic<bool>> cancelled,MediaPreview::Completion complete) {
    static Loader loader;loader.submit({std::move(source),std::move(cancelled),std::move(complete)});
}
} // namespace oneui::internal
