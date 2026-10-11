void declarativePopups() {
    (void)ui::syntax::css("Popup { background:#24262b; color:#ffffff; border-color:#404550; border-width:1px; border-radius:6px; padding:0px; gap:4px; box-shadow:0px 2px 8px #00000040; }");
    ui::Mount mount({},std::make_shared<ui::StyleRegistry>(),{});
    auto body=mount.make("Button");
    mount.set(body,"text",L"Popup action");
    auto element=mount.make("Popup",{body});
    auto popup=element.as<Popup>();
    View root;root.setFrame({0,0,320,240});root.add(popup);
    popup->setFrame({10,10,300,220});
    mount.set(element,"anchor-x",300.0f);mount.set(element,"anchor-y",220.0f);
    mount.set(element,"anchor-width",10.0f);mount.set(element,"anchor-height",10.0f);
    mount.set(element,"content-width",180.0f);mount.set(element,"content-height",100.0f);
    mount.set(element,"placement",L"bottom-start");
    int closed=0;popup->setOnClosed([&]{++closed;});
    mount.set(element,"open",true);
    const auto rect=popup->resolvedContentRect();
    check(rect.y<220 && rect.x+rect.width<=320,"popup flips and clamps at viewport edge");
    KeyEvent escape;escape.key=Key::Escape;
    check(popup->onKeyDown(escape)&&!popup->isOpen()&&closed==1,"Escape closes once");
    mount.set(element,"open",true);
    MouseEvent outside;outside.position={1,1};
    popup->onMouseDown(outside);
    check(!popup->isOpen()&&closed==2,"outside pointer closes");
    mount.set(element,"open",false);
    check(closed==2,"setting already closed is idempotent");
    bool rejected=false;
    try{mount.set(element,"placement",L"unknown");}catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"invalid placement rejected");
}
