void headerGeometry() {
    DockItemView item;
    auto body = std::make_shared<View>();
    auto handle = std::make_shared<DragHandle>();
    handle->setPreferredSize({0, 47});
    item.add(handle);
    item.add(body);
    item.header = handle;
    item.body = body;
    item.setFrame({80, 80, 340, 540});
    test_support::RecordingCanvas canvas;
    item.paint(canvas);
    check(handle->naturalSize().height == 26 && near(handle->frame().height, 47) &&
              near(handle->frame().y, 80) && near(body->frame().y, 127) &&
              near(body->frame().height, 493),
          "explicit header height overrides its smaller natural height without body overlap");

    auto header = std::make_shared<Stack>(StackDirection::Row);
    item.reconcileChildren({header, body});
    header->add(handle);
    header->setPreferredSize({0, 47});
    header->setSizeContainment(true);
    header->setAlign(StackAlign::Center);
    item.header = header;
    item.paint(canvas);
    check(header->sizeContained() && header->naturalSize().height == 0 &&
              near(header->frame().y, 80) && near(header->frame().height, 47) &&
              near(handle->frame().y, 80) && near(handle->frame().height, 47) &&
              near(body->frame().y, handle->frame().y + handle->frame().height) &&
              near(body->frame().y + body->frame().height, 620),
          "size-contained header keeps its explicit height and centered handle above the body");

    header->setSizeContainment(false);
    header->setPreferredSize({});
    handle->setPreferredSize({0, 61});
    item.paint(canvas);
    check(near(header->naturalSize().height, 61) && near(header->frame().height, 61) &&
              near(body->frame().y, 141),
          "unspecified header height still falls back to natural content height");
    header->setPreferredSize({0, 15});
    item.paint(canvas);
    check(near(header->frame().height, 29) && near(body->frame().y, 109),
          "positive header preference retains the 29 pixel minimum");
    header->setPreferredSize({0, 47});
    item.setFrame({80, 80, 340, 20});
    item.paint(canvas);
    check(near(header->frame().height, 20) && near(body->frame().y, 100) &&
              near(body->frame().height, 0),
          "header is capped by the pane extent and body never receives negative height");
    header->setVisible(false);
    item.paint(canvas);
    check(near(body->frame().y, 80) && near(body->frame().height, 20),
          "hidden header consumes no body space");
}
