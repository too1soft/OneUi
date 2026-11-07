#include "oneui/mobile/frame_mailbox.h"
#include "oneui/mobile/remote_interaction.h"
#include <cassert>
#include <limits>
#include <thread>
#include <vector>
using namespace oneui::mobile;

static void zoomedClickTargetsTheDrawnPixel() {
    RemoteViewport viewport;
    std::vector<RemoteMouseAction> events;
    RemoteInteraction remote([&](const auto &event) { events.push_back(event); });
    remote.setViewport({400, 500}, {1600, 1000});
    remote.setZoomViewport(&viewport, {});
    remote.setMode(RemoteTouchMode::Direct);

    remote.pointer({1, RemoteTouchPhase::Down, {120, 250}, 10});
    remote.pointer({2, RemoteTouchPhase::Down, {280, 250}, 11});
    remote.pointer({1, RemoteTouchPhase::Move, {40, 250}, 20});
    remote.pointer({2, RemoteTouchPhase::Move, {360, 250}, 20});
    remote.pointer({1, RemoteTouchPhase::Up, {40, 250}, 30});
    remote.pointer({2, RemoteTouchPhase::Up, {360, 250}, 31});
    assert(std::abs(viewport.zoom() - 2.f) < .01f && events.empty());

    const auto tap = [&](RemotePoint point, uint64_t time) {
        remote.pointer({1, RemoteTouchPhase::Down, point, time});
        remote.pointer({1, RemoteTouchPhase::Up, point, time + 30});
    };
    tap({300, 300}, 500);
    assert(events.size() == 2 && events.front().pressed && !events.back().pressed);
    assert(std::abs(events.front().position.x - 1000) < .1f);
    assert(std::abs(events.front().position.y - 600) < .1f);

    events.clear();
    remote.pointer({1, RemoteTouchPhase::Down, {120, 250}, 1000});
    remote.pointer({2, RemoteTouchPhase::Down, {280, 250}, 1001});
    remote.pointer({1, RemoteTouchPhase::Move, {60, 250}, 1010});
    remote.pointer({2, RemoteTouchPhase::Move, {220, 250}, 1010});
    remote.pointer({1, RemoteTouchPhase::Up, {60, 250}, 1020});
    remote.pointer({2, RemoteTouchPhase::Up, {220, 250}, 1021});
    tap({240, 300}, 1500);
    assert(events.size() == 2 && std::abs(events.front().position.x - 1000) < .1f);

    events.clear();
    viewport.reset();
    tap({200, 40}, 2000);
    assert(events.empty());
}

static void trackpadScrollsWhileZoomed() {
    RemoteViewport viewport;
    std::vector<RemoteMouseAction> events;
    RemoteInteraction remote([&](const auto &event) { events.push_back(event); });
    remote.setViewport({400, 500}, {1600, 1000});
    remote.setZoomViewport(&viewport, {});
    viewport.transform({200, 250}, {200, 250}, 2);
    const auto origin = viewport.origin();
    remote.setCursorPosition({800, 500});
    remote.pointer({1, RemoteTouchPhase::Down, {150, 250}, 10});
    remote.pointer({2, RemoteTouchPhase::Down, {250, 250}, 11});
    remote.pointer({1, RemoteTouchPhase::Move, {150, 230}, 20});
    remote.pointer({2, RemoteTouchPhase::Move, {250, 230}, 21});
    remote.pointer({1, RemoteTouchPhase::Up, {150, 230}, 30});
    remote.pointer({2, RemoteTouchPhase::Up, {250, 230}, 31});
    assert(events.size() == 2 && events[0].scrollY + events[1].scrollY == 40);
    assert(viewport.zoom() == 2 && viewport.origin().x == origin.x && viewport.origin().y == origin.y);
    const auto point = viewport.project(viewport.toLocal({800, 500}));
    assert(std::abs(point.x - 800) < .01f && std::abs(point.y - 500) < .01f);
    events.clear();
    remote.pointer({1, RemoteTouchPhase::Down, {150, 250}, 100});
    remote.setCursorPosition({0, 0});
    remote.pointer({1, RemoteTouchPhase::Up, {150, 250}, 700});
    assert(events.size() == 2 && events[0].button == 3 && events[0].position.x == 800);

    remote.cancel();
    viewport.reset();
    events.clear();
    remote.pointer({1, RemoteTouchPhase::Down, {150, 250}, 1000});
    remote.pointer({2, RemoteTouchPhase::Down, {250, 250}, 1001});
    for (int step = 1; step <= 8; ++step) {
        remote.pointer({1, RemoteTouchPhase::Move, {150.f - step * 2.5f, 250}, uint64_t(1010 + step)});
        remote.pointer({2, RemoteTouchPhase::Move, {250.f + step * 2.5f, 250}, uint64_t(1020 + step)});
    }
    remote.pointer({1, RemoteTouchPhase::Up, {130, 250}, 1100});
    remote.pointer({2, RemoteTouchPhase::Up, {270, 250}, 1101});
    assert(events.empty() && std::abs(viewport.zoom() - 1.4f) < .01f);
}

int main() {
    zoomedClickTargetsTheDrawnPixel();
    trackpadScrollsWhileZoomed();
    std::vector<RemoteMouseAction> events;
    RemoteInteraction remote([&](auto action) { events.push_back(action); });
    remote.setViewport({400, 800}, {1920, 1080});
    auto center = remote.mapFit({200, 400});
    assert(std::abs(center.x - 960) < .1f && std::abs(center.y - 540) < .1f);
    auto edge = remote.mapFit({-500, 9999});
    assert(edge.x == 0 && edge.y == 1079);
    remote.pointer({5, RemoteTouchPhase::Down, {100, 100}, 1000});
    remote.pointer({5, RemoteTouchPhase::Up, {100, 100}, 1050});
    assert(events.size() == 2 && events[0].pressed && !events[1].pressed);
    remote.pointer({5, RemoteTouchPhase::Down, {100, 100}, 1100});
    assert(events.back().pressed);
    const auto before = events.size();
    remote.setViewport({400, 800}, {1920, 1080});
    assert(events.size() == before && remote.active());
    remote.setEnabled(false);
    assert(!events.back().pressed && !remote.active());
    events.clear();
    remote.pointer({1, RemoteTouchPhase::Down, {100, 100}, 2000});
    remote.pointer({1, RemoteTouchPhase::Up, {100, 100}, 2100});
    assert(events.empty());
    remote.setEnabled(true);
    remote.pointer({1, RemoteTouchPhase::Down, {100, 100}, 3000});
    remote.pointer({2, RemoteTouchPhase::Down, {120, 100}, 3010});
    remote.pointer({1, RemoteTouchPhase::Move, {100, 80}, 3030});
    remote.pointer({1, RemoteTouchPhase::Up, {100, 80}, 3040});
    remote.pointer({2, RemoteTouchPhase::Up, {120, 100}, 3050});
    assert(events.size() == 1 && events[0].scrollY == 40 && events[0].button == 0);
    remote.setViewport({std::numeric_limits<float>::infinity(), 800}, {1920, 1080});
    assert(remote.mapFit({10, 10}).x == 0);
    FrameMailbox frames;
    uint8_t bytes[32]{};
    assert(!frames.submit(bytes, sizeof(bytes), 5, 2, 16));
    assert(!frames.submit(bytes, 7, 2, 2, 8));
    assert(frames.submit(bytes, sizeof(bytes), 2, 2, 16));
    bytes[0] = 200;
    auto owned = frames.take();
    assert(owned.pixels && (*owned.pixels)[0] == 0 && owned.stride == 8);
    assert(!frames.take().pixels);
    std::thread producer([&] {
        uint8_t pixel[4] = {1, 2, 3, 4};
        for (int i = 0; i < 1000; ++i)
            assert(frames.submit(pixel, 4, 1, 1, 4));
    });
    for (int i = 0; i < 1000; ++i) {
        auto frame = frames.take();
        if (frame.pixels)
            assert(frame.pixels->size() == 4 && (*frame.pixels)[3] == 4);
    }
    producer.join();
    frames.clear();
    assert(!frames.take().pixels);
    const uint8_t gray[6] = {16, 235, 16, 235, 128, 128};
    assert(frames.submitNv12(gray, 6, 2, 2, 2));
    auto rgb = frames.take();
    assert(rgb.pixels->at(0) == 0 && rgb.pixels->at(4) == 255 && rgb.stride == 8);
    const uint8_t red[6] = {81, 81, 81, 81, 90, 240};
    assert(frames.submitNv12(red, 6, 2, 2, 2));
    rgb = frames.take();
    assert(rgb.pixels->at(0) > 250 && rgb.pixels->at(1) < 2 && rgb.pixels->at(2) < 2);
    assert(!frames.submitNv12(bytes, 6, 3, 2, 3));
}
