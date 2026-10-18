void surfaceDrag(const char* runtimeFile) {
    Case c(runtimeFile); c.start();
    auto surface=uniqueChild(c.runtime->root, "InteractiveSurface")->element.as<oneui::InteractiveSurface>();
    surface->setFrame({0,0,200,60});
    oneui::MouseEvent down{{10,10},oneui::MouseButton::Left};
    oneui::MouseEvent move{{40,20},oneui::MouseButton::Left}; move.alt=true;move.control=true;
    surface->onMouseDown(down); surface->onMouseMove(move);
    expect(c.vm.invokedArgs.size()==8 && c.vm.invokedArgs[0].integer==0 && c.vm.invokedArgs[1].number==10,"drag starts with original coordinates");
    surface->onMouseUp(move);
    expect(c.vm.invokedArgs[0].integer==2 && c.vm.invokedArgs[3].number==40 && c.vm.invokedArgs[5].integer==1 && c.vm.invokedArgs[6].integer==1,"drop forwards final coordinates and modifiers");
    surface->onMouseDown(down); surface->onMouseMove(move);
    oneui::KeyEvent escape;escape.key=oneui::Key::Escape; surface->onKeyDown(escape);
    expect(c.vm.invokedArgs[0].integer==3,"escape forwards cancellation");
    c.runtime->close();const auto calls=c.vm.invokes;
    surface->onMouseDown(down);surface->onMouseMove(move);surface->onMouseUp(move);
    expect(c.vm.invokes==calls,"retained surface cannot dispatch after disconnect");
}
