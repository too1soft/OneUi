// Real IR direction binding and exact preceding serialized layouts.
#include "views/format/legacy_layout_contract.h"
void layoutDirectionContract(const char* file) {
        for (const auto& historical : oneview::legacyLayoutContracts()) {
            const auto bytes = oneview::encodeComponents({historical});
            expect(oneview::encodeComponents(oneview::decodeComponents(bytes)) == bytes,
                   "exact pre-direction layout descriptor is retained");
            auto forged = historical; forged.properties.back().flags ^= 2;
            bool rejected = false;
            try { oneview::decodeComponents(oneview::encodeComponents({forged})); }
            catch (const oneview::Error&) { rejected = true; }
            expect(rejected, "pre-direction version cannot admit forged property flags");
        }
        {
            Case c(file); c.start();
            const auto direction = id(c.image, "Root", "direction");
            auto stack = c.runtime->root->element.as<oneui::Stack>();
            const auto first = stack->children().front();
            expect(stack->direction() == oneui::StackDirection::Column, "bound Column default direction");
            c.vm.text(direction, "row");
            c.runtime->notify(c.runtime->rootScope, direction); c.runtime->flush();
            expect(stack->direction() == oneui::StackDirection::Row && stack->children().front() == first,
                   "direction reflows the same child widgets without attaching a second VM");
            c.vm.text(direction, "invalid"); c.runtime->notify(c.runtime->rootScope, direction);
            try { c.runtime->flush(); expect(false, "invalid direction accepted"); }
            catch (const Failure&) {}
            expect(stack->direction() == oneui::StackDirection::Row, "invalid direction retains the layout");
            c.vm.text(direction, "column"); c.runtime->notify(c.runtime->rootScope, direction); c.runtime->flush();
            expect(stack->direction() == oneui::StackDirection::Column && stack->children().front() == first,
                   "valid direction recovers without rebuilding controls");
        }
}
