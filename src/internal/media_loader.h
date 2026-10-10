#pragma once
#include "oneui/controls/media_preview.h"
namespace oneui::internal {
void loadLocalMedia(std::string,std::shared_ptr<std::atomic<bool>>,MediaPreview::Completion);
}
