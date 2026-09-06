#include "oneui/reactive.h"
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

static void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
int main() {
    try {
        oneui::Binding<int> binding;
        int calls = 0, fallback = 99;
        {
            oneui::State<int> state(1);
            binding.bind(state, [&] { ++calls; });
            state.set(2);
            require(calls == 1 && binding.get(fallback) == 2, "bound update");
        }
        require(!binding.bound() && binding.get(fallback) == 99, "state destruction safely expires binding");
        binding.set(7, fallback);
        require(fallback == 7, "expired binding writes fallback");
        binding.reset();
        oneui::State<int> state(0);
        std::vector<int> sequence;
        oneui::Subscription second;
        auto first = state.subscribeScoped([&](int value) {
            sequence.push_back(value * 10);
            if (value == 1) { second.reset(); state.set(2); }
        });
        second = state.subscribeScoped([&](int value) { sequence.push_back(value); });
        state.set(1);
        require(sequence == std::vector<int>({10, 20}), "unsubscribe immediate, nested updates queued");
        oneui::State<int> stable(0);
        sequence.clear();
        auto outer = stable.subscribeScoped([&](int value) { if (value == 1) stable.set(2); sequence.push_back(value); });
        auto observer = stable.subscribeScoped([&](int value) { sequence.push_back(value * 10); });
        stable.set(1);
        require(sequence == std::vector<int>({1, 10, 2, 20}), "stable notification snapshots");
        auto dying = std::make_unique<oneui::State<int>>(0);
        auto destroy = dying->subscribeScoped([&](int) { dying.reset(); });
        bool late = false;
        auto lateObserver = dying->subscribeScoped([&](int) { late = true; });
        dying->set(1);
        require(!late, "destroying state cancels remaining delivery");
        oneui::State<int> adding(0);
        oneui::Subscription added;
        int newCalls = 0;
        auto add = adding.subscribeScoped([&](int) {
            if (!added) added = adding.subscribeScoped([&](int) { ++newCalls; });
        });
        adding.set(1); require(newCalls == 0, "new listener not in current event");
        adding.set(2); require(newCalls == 1, "new listener receives next event");
        oneui::State<int> original(3);
        oneui::Binding<int> movedBinding(original);
        oneui::State<int> moved(std::move(original));
        moved.set(4);
        require(movedBinding.get(fallback) == 4, "move transfers observable identity");
        auto copied = moved;
        copied.set(5);
        require(movedBinding.get(fallback) == 4, "copy has independent identity");
        std::cout << "Reactive lifetime tests passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
