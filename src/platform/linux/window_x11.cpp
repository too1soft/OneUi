#include "internal/unicode.h"
#include "linux_runtime.h"
#include "x11_error_scope.h"
#include "platform/shared/desktop_window.h"
#include "platform/shared/key_mapping.h"
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xrandr.h>
#include <X11/keysym.h>
#include "window_properties_x11.h"
#undef None
#undef Status
#include "drag_drop_x11.h"
#include <algorithm>
#include <cerrno>
#include <clocale>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <poll.h>
#include <set>
#include <stdexcept>
#include <unistd.h>

namespace oneui::linux_platform {
namespace {
class X11Window;
class X11Connection final : public Connection {
  public:
    X11Connection() {
        display = XOpenDisplay(nullptr);
        if (!display)
            throw std::runtime_error("OneUI cannot connect to X11 DISPLAY");
        if (pipe2(wakeup, O_NONBLOCK | O_CLOEXEC) < 0) {
            XCloseDisplay(display);
            throw std::runtime_error("OneUI cannot create X11 wake pipe");
        }
        std::setlocale(LC_CTYPE, "");
        XSetLocaleModifiers("");
        im = XOpenIM(display, nullptr, nullptr, nullptr);
        Bool detectable = False;
        XkbSetDetectableAutoRepeat(display, True, &detectable);
        owner = XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0, 1, 1, 0, 0, 0);
        XSelectInput(display, owner, PropertyChangeMask);
        clipboardAtom = atom("CLIPBOARD");
        utf8Atom = atom("UTF8_STRING");
        targetsAtom = atom("TARGETS");
        transferAtom = atom("ONEUI_CLIPBOARD");
        incrAtom = atom("INCR");
    }
    ~X11Connection() override {
        if (im)
            XCloseIM(im);
        XDestroyWindow(display, owner);
        XCloseDisplay(display);
        ::close(wakeup[0]);
        ::close(wakeup[1]);
    }
    Atom atom(const char *name) { return XInternAtom(display, name, False); }
    std::unique_ptr<Window> create(WindowOptions options) override;
    void pump(int timeout);
    std::shared_ptr<platform::NativeDockDrag> dockDrag(Window &window, platform::DockDragCallbacks callbacks) override {
        return makeX11DockDrag(window, display, std::move(callbacks));
    }
    void wake() {
        const char c = 1;
        const auto ignored = ::write(wakeup[1], &c, 1);
        (void)ignored;
    }
    float scale() const {
        if (const char *resources = XResourceManagerString(display)) {
            if (const char *dpi = std::strstr(resources, "Xft.dpi:")) {
                const double value = std::strtod(dpi + 8, nullptr);
                if (std::isfinite(value) && value >= 48 && value <= 768)
                    return value / 96;
            }
        }
        return 1;
    }
    void setClipboard(const std::wstring &value) override {
        auto bytes = unicode::toUtf8(value);
        if (bytes.size() > 16u * 1024u * 1024u)
            throw std::runtime_error("X11 clipboard exceeds 16 MiB");
        ownedText = std::move(bytes);
        XSetSelectionOwner(display, clipboardAtom, owner, CurrentTime);
        XFlush(display);
        if (XGetSelectionOwner(display, clipboardAtom) != owner)
            throw std::runtime_error("X11 clipboard ownership failed");
    }
    std::wstring clipboard() override {
        const auto selectionOwner = XGetSelectionOwner(display, clipboardAtom);
        if (!selectionOwner)
            return {};
        if (selectionOwner == owner)
            return unicode::fromUtf8(ownedText);
        received.clear();
        receiving = true;
        incremental = false;
        XDeleteProperty(display, owner, transferAtom);
        XConvertSelection(display, clipboardAtom, utf8Atom, transferAtom, owner, CurrentTime);
        const double deadline = platform::DesktopWindow::nowMs() + 1500;
        while (receiving && platform::DesktopWindow::nowMs() < deadline)
            pump(20);
        if (receiving) {
            receiving = false;
            throw std::runtime_error("X11 clipboard transfer timed out");
        }
        return unicode::fromUtf8(received);
    }
    std::vector<MonitorInfo> monitors() override {
        std::vector<MonitorInfo> result;
        int count = 0;
        auto *info = XRRGetMonitors(display, DefaultRootWindow(display), True, &count);
        for (int i = 0; info && i < count; ++i) {
            const auto &m = info[i];
            char *name = XGetAtomName(display, m.name);
            Rect bounds{static_cast<float>(m.x), static_cast<float>(m.y), static_cast<float>(m.width),
                        static_cast<float>(m.height)};
            result.push_back(
                {i, bounds, bounds, scale(), bool(m.primary), unicode::fromUtf8(name ? name : "X11")});
            if (name)
                XFree(name);
        }
        if (info)
            XRRFreeMonitors(info);
        if (result.empty()) {
            Rect bounds{0, 0, static_cast<float>(DisplayWidth(display, DefaultScreen(display))),
                        static_cast<float>(DisplayHeight(display, DefaultScreen(display)))};
            result.push_back({0, bounds, bounds, scale(), true, L"X11"});
        }
        if (!result.empty() &&
            std::none_of(result.begin(), result.end(), [](const auto &m) { return m.primary; }))
            result.front().primary = true;
        return result;
    }
    Display *display = nullptr;
    XIM im = nullptr;
    int wakeup[2]{-1, -1};
    std::map<::Window, X11Window *> windows;

  private:
    void selection(XEvent &event);
    void readSelection();
    ::Window owner = 0;
    Atom clipboardAtom{}, utf8Atom{}, targetsAtom{}, transferAtom{}, incrAtom{};
    std::string ownedText, received;
    bool receiving = false, incremental = false;
    struct Transfer {
        std::string text;
        std::size_t offset = 0;
        Atom target{};
        double deadline = 0;
    };
    std::map<std::pair<::Window, Atom>, Transfer> sends;
};
unsigned int virtualKey(KeySym symbol) {
    if (symbol >= XK_a && symbol <= XK_z)
        return symbol - XK_a + 'A';
    if (symbol >= XK_A && symbol <= XK_Z)
        return symbol;
    if (symbol >= XK_0 && symbol <= XK_9)
        return symbol;
    if (symbol >= XK_F1 && symbol <= XK_F24)
        return 112 + symbol - XK_F1;
    switch (symbol) {
    case XK_Return:
    case XK_KP_Enter:
        return 13;
    case XK_Tab:
    case XK_ISO_Left_Tab:
        return 9;
    case XK_space:
        return 32;
    case XK_BackSpace:
        return 8;
    case XK_Escape:
        return 27;
    case XK_Left:
        return 37;
    case XK_Right:
        return 39;
    case XK_Up:
        return 38;
    case XK_Down:
        return 40;
    case XK_Home:
        return 36;
    case XK_End:
        return 35;
    case XK_Page_Up:
        return 33;
    case XK_Page_Down:
        return 34;
    case XK_Delete:
        return 46;
    case XK_Insert:
        return 45;
    case XK_Shift_L:
    case XK_Shift_R:
        return 16;
    case XK_Control_L:
    case XK_Control_R:
        return 17;
    case XK_Alt_L:
    case XK_Alt_R:
        return 18;
    case XK_Super_L:
        return 91;
    case XK_Super_R:
        return 92;
    default:
        return symbol < 128 ? platform::asciiVirtualKey(symbol) : 0;
    }
}
class X11Window final : public platform::DesktopWindow {
  public:
    X11Window(X11Connection &c, WindowOptions o) : DesktopWindow(std::move(o)), c_(c) {}
    ~X11Window() override { setCloseToTray(false); close(); }
    WindowBackend backend() const override { return WindowBackend::X11; }
    unsigned int capabilities() const override {
        return WindowCapabilityClipboard | WindowCapabilityPlacement | WindowCapabilityActivation |
               WindowCapabilityTopmost | WindowCapabilityFileDrop | (ic_ ? WindowCapabilityIme : 0) | serviceCapabilities();
    }
    void initialize() override {
        assertUiThread();
        if (id_ || isClosed())
            return;
        Display *d = c_.display;
        resize(logicalSize_, c_.scale());
        const auto size = clientPixelSize();
        id_ = XCreateSimpleWindow(d, DefaultRootWindow(d), 0, 0, size.width, size.height, 0, 0, 0);
        if (!id_)
            throw std::runtime_error("OneUI XCreateWindow failed");
        c_.windows[id_] = this;
        gc_ = XCreateGC(d, id_, 0, nullptr);
        XSelectInput(d, id_,
                     ExposureMask | StructureNotifyMask | KeyPressMask | KeyReleaseMask | ButtonPressMask |
                         ButtonReleaseMask | PointerMotionMask | FocusChangeMask | LeaveWindowMask | PropertyChangeMask);
        deleteAtom_ = c_.atom("WM_DELETE_WINDOW");
        XSetWMProtocols(d, id_, &deleteAtom_, 1);
        setTitle(options_.title);
        setBorderless(options_.borderless);
        setMinimumClientSize(minimum_);
        initializeIme();
        drop_ = std::make_unique<X11DropTarget>(d, id_, *this);
        fileDropChanged(acceptsFileDrop());
        initializeGpu(d, nativeHandle());
        initializeAccessibility();
        if (options_.visible)
            show();
    }
    void show() override {
        initialize();
        if (!id_)
            return;
        shown_ = true;
        XMapWindow(c_.display, id_);
        if (options_.fullscreen)
            state("_NET_WM_STATE_FULLSCREEN", true);
        if (options_.topmost)
            state("_NET_WM_STATE_ABOVE", true);
        requestRedraw();
    }
    void activate() override {
        show();
        XEvent e{};
        e.xclient.type = ClientMessage;
        e.xclient.window = id_;
        e.xclient.message_type = c_.atom("_NET_ACTIVE_WINDOW");
        e.xclient.format = 32;
        e.xclient.data.l[0] = 1;
        e.xclient.data.l[1] = CurrentTime;
        sendRoot(e);
    }
    bool centerOnActiveMonitor() override {
        assertUiThread(); initialize();
        if (isClosed() || options_.fullscreen) return false;
        ::Window root = 0, child = 0; int rootX = 0, rootY = 0, x = 0, y = 0; unsigned int mask = 0;
        XQueryPointer(c_.display, DefaultRootWindow(c_.display), &root, &child, &rootX, &rootY, &x, &y, &mask);
        auto monitors = c_.monitors();
        if (monitors.empty()) return false;
        auto monitor = std::find_if(monitors.begin(), monitors.end(), [&](const auto& m) {
            return rootX >= m.bounds.x && rootX < m.bounds.x + m.bounds.width && rootY >= m.bounds.y && rootY < m.bounds.y + m.bounds.height;
        });
        if (monitor == monitors.end()) monitor = monitors.begin();
        WindowPlacement placement;
        if (!getWindowPlacement(placement)) return false;
        placement.x = monitor->workArea.x + (monitor->workArea.width - placement.width) / 2;
        placement.y = monitor->workArea.y + (monitor->workArea.height - placement.height) / 2;
        return setWindowPlacement(placement);
    }
    void showWithFade(unsigned int duration) override {
        initialize();
        const auto compositor = c_.atom(("_NET_WM_CM_S" + std::to_string(DefaultScreen(c_.display))).c_str());
        if (!duration || !clientAreaAnimationsEnabled() || !XGetSelectionOwner(c_.display, compositor)) { show(); return; }
        opacity(0); show();
        fadeStart_ = nowMs(); fadeDuration_ = std::min(duration, 10000u);
        fadeStep();
    }
    int run() override {
        initialize();
        auto& connection = c_;
        while (!connection.windows.empty())
            connection.pump(1000);
        return 0;
    }
    void pumpNativeEvents(int timeout) override { c_.pump(timeout); }
    void hideNative() override {
        assertUiThread();
        if (id_) XUnmapWindow(c_.display, id_);
        shown_ = false;queueStateChange();
    }
    void close() override {
        if (isClosed())
            return;
        assertUiThread();
        if (hideForTray()) return;
        drop_.reset();
        finishClose();
        if (ic_) {
            XDestroyIC(ic_);
            ic_ = nullptr;
        }
        if (id_) {
            c_.windows.erase(id_);
            XFreeGC(c_.display, gc_);
            XDestroyWindow(c_.display, id_);
            id_ = 0;
        }
        for (auto item : cursors_)
            XFreeCursor(c_.display, item.second);
        cursors_.clear();
        wake();
        publishClosed();
    }
    void minimize() override {
        initialize();
        XIconifyWindow(c_.display, id_, DefaultScreen(c_.display));
    }
    NativeWindowHandle nativeHandle() const override {
        return reinterpret_cast<void *>(static_cast<std::uintptr_t>(id_));
    }
    std::optional<Rect> accessibilityScreenBounds(Rect rect) const override {
        if (!id_ || isClosed()) return std::nullopt;
        int x = 0, y = 0;
        ::Window child = 0;
        if (!XTranslateCoordinates(c_.display, id_, DefaultRootWindow(c_.display), 0, 0, &x, &y, &child))
            return std::nullopt;
        return Rect{x + rect.x * dpiScale(), y + (rect.y * contentScale_ + chromeHeight()) * scale_,
                    rect.width * dpiScale(), rect.height * dpiScale()};
    }
    void setTitle(std::wstring title) override {
        options_.title = std::move(title);
        if (!id_)
            return;
        const auto utf8 = unicode::toUtf8(options_.title);
        XChangeProperty(c_.display, id_, c_.atom("_NET_WM_NAME"), c_.atom("UTF8_STRING"), 8, PropModeReplace,
                        reinterpret_cast<const unsigned char *>(utf8.data()), utf8.size());
        XStoreName(c_.display, id_, utf8.c_str());
    }
    void setFullscreen(bool v) override {
        options_.fullscreen = v;
        if (id_)
            state("_NET_WM_STATE_FULLSCREEN", v);
    }
    void setBorderless(bool value) override {
        options_.borderless = value;
        if (!id_)
            return;
        struct Hints {
            unsigned long flags, functions, decorations;
            long inputMode;
            unsigned long status;
        };
        Hints hints{2, 0, value ? 0ul : 1ul, 0, 0};
        auto atom = c_.atom("_MOTIF_WM_HINTS");
        XChangeProperty(c_.display, id_, atom, atom, 32, PropModeReplace,
                        reinterpret_cast<unsigned char *>(&hints), 5);
    }
    void toggleMaximize() override {
        initialize();
        maximized_ = !maximized_;
        state("_NET_WM_STATE_MAXIMIZED_VERT", maximized_, "_NET_WM_STATE_MAXIMIZED_HORZ");
    }
    WindowState state() const override { assertUiThread();return queryX11State(c_.display,id_,Window::state()); }
    bool restore() override { initialize();if(!id_)return false;state("_NET_WM_STATE_FULLSCREEN",false);state("_NET_WM_STATE_MAXIMIZED_VERT",false,"_NET_WM_STATE_MAXIMIZED_HORZ");show();return true; }
    bool maximize() override { initialize();if(!id_)return false;state("_NET_WM_STATE_MAXIMIZED_VERT",true,"_NET_WM_STATE_MAXIMIZED_HORZ");return true; }
    bool setTopmost(bool enabled) override { initialize();if(!id_)return false;options_.topmost=enabled;state("_NET_WM_STATE_ABOVE",enabled);return true; }
    bool setResizable(bool enabled) override { assertUiThread();if(isClosed())return false;options_.resizable=enabled;setMinimumClientSize(minimum_);queueStateChange();return true; }
    bool setClientSize(Size size) override {
        if(!std::isfinite(size.width)||!std::isfinite(size.height)||size.width<=0||size.height<=0||size.width>32767||size.height>32767)return false;
        initialize();if(!id_)return false;XResizeWindow(c_.display,id_,unsigned(size.width*dpiScale()),unsigned(size.height*dpiScale()));return true;
    }
    bool setMaximumClientSize(Size size) override { assertUiThread();if(!std::isfinite(size.width)||!std::isfinite(size.height))return false;maximum_=size;setMinimumClientSize(minimum_);return true; }
    bool setOwner(Window* owner) override {
        if(owner==this||(owner&&(owner->backend()!=backend()||owner->closed())))return false;
        initialize();if(!id_)return false;
        if(owner){owner->initialize();XSetTransientForHint(c_.display,id_,reinterpret_cast<std::uintptr_t>(owner->nativeHandle()));}
        else XDeleteProperty(c_.display,id_,XA_WM_TRANSIENT_FOR);
        return true;
    }
    void setMinimumClientSize(Size size) override {
        minimum_=size;setX11SizeConstraints(c_.display,id_,minimum_,maximum_,clientPixelSize(),dpiScale(),options_.resizable);
    }
    bool prepareContentTransfer(Widget* subtree) override {
        assertUiThread();
        if (subtree && subtree->focused() && ic_) {
            if (auto pending = Xutf8ResetIC(ic_)) XFree(pending);
            composition({}, 0);
        }
        return Window::prepareContentTransfer(subtree);
    }
    bool getWindowPlacement(WindowPlacement &p) const override {
        if ((!shown_ || maximized_ || options_.fullscreen) && restoredPlacement_) {
            p = *restoredPlacement_;
            p.maximized = maximized_;
            return true;
        }
        return queryX11Placement(c_.display,id_,maximized_,p);
    }

    bool setWindowPlacement(const WindowPlacement &p) override {
        if (p.width <= 0 || p.height <= 0)
            return false;
        auto monitors = c_.monitors();
        if (!std::any_of(monitors.begin(), monitors.end(), [&](const auto &m) {
                return p.x < m.bounds.x + m.bounds.width &&
                       p.y<m.bounds.y + m.bounds.height &&static_cast<double>(p.x) + p.width> m.bounds.x &&
                       static_cast<double>(p.y) + p.height > m.bounds.y;
            }))
            return false;
        initialize();
        restoredPlacement_ = p;
        XMoveResizeWindow(c_.display, id_, p.x, p.y, p.width, p.height);
        if (!shown_)
            resize({p.width / scale_, p.height / scale_}, scale_);
        if (maximized_ != p.maximized)
            toggleMaximize();
        return true;
    }
    void paint() {
        if (!shown_ || !needsPaint() || !id_)
            return;
        if (paintGpu()) return;
        const auto& pixels=render();paintX11Pixels(c_.display,id_,gc_,pixels,clientPixelSize());
    }

    void event(XEvent &event) {
        if(event.type==PropertyNotify||event.type==MapNotify||event.type==UnmapNotify||event.type==ConfigureNotify)queueStateChange();
        const auto alive = lifetime_;
        if (drop_ && drop_->event(event)) return;
        if (XFilterEvent(&event, id_))
            return;
        switch (event.type) {
        case ClientMessage:
            if (static_cast<Atom>(event.xclient.data.l[0]) == deleteAtom_)
                requestClose();
            break;
        case Expose:
            requestRedraw();
            break;
        case ConfigureNotify: {
            const float newScale = c_.scale();
            resize({event.xconfigure.width / newScale, event.xconfigure.height / newScale}, newScale);
            break;
        }
        case FocusIn:
            if (ic_)
                XSetICFocus(ic_);
            focus(true);
            break;
        case FocusOut:
            pressedKeys_.clear();
            if (ic_)
                XUnsetICFocus(ic_);
            focus(false);
            break;
        case KeyPress:
        case KeyRelease:
            keyboard(event.xkey);
            break;
        case MotionNotify:
            pointer(event.xmotion.x, event.xmotion.y, 0, event.xmotion.state, 0, event.xmotion.time);
            break;
        case ButtonPress:
        case ButtonRelease:
            pointer(event.xbutton.x, event.xbutton.y, event.type == ButtonPress ? 1 : 2, event.xbutton.state,
                    event.xbutton.button, event.xbutton.time);
            break;
        case LeaveNotify:
            mouse({{-1, -1}, MouseButton::None}, 0);
            break;
        default:
            break;
        }
        if (!*alive || isClosed())
            return;
        if (ic_) {
            Rect caret = caretRect();
            XPoint spot{static_cast<short>(caret.x * scale_),
                        static_cast<short>((caret.y + caret.height) * scale_)};
            auto attributes = XVaCreateNestedList(0, XNSpotLocation, &spot, nullptr);
            XSetICValues(ic_, XNPreeditAttributes, attributes, nullptr);
            XFree(attributes);
        }
    }

  protected:
    void wake() override { c_.wake(); }
    void contentScaleChanged() override { setMinimumClientSize(minimum_); }
    void fileDropChanged(bool enabled) override { if (drop_) drop_->enableFiles(enabled); }

  public:
    bool expireDrop() { return drop_ && drop_->expire(); }

  private:
    double fadeStart_ = 0;
    unsigned int fadeDuration_ = 0;
    void opacity(float value) {
        unsigned long opacity = static_cast<unsigned long>(std::clamp(double(value), 0., 1.) * 0xfffffffful);
        XChangeProperty(c_.display, id_, c_.atom("_NET_WM_WINDOW_OPACITY"), XA_CARDINAL, 32, PropModeReplace,
            reinterpret_cast<const unsigned char*>(&opacity), 1);
    }
    void fadeStep() {
        requestAnimationFrame([this](double now) {
            const float progress = std::clamp((now - fadeStart_) / fadeDuration_, 0., 1.);
            opacity(progress);
            if (progress < 1) fadeStep();
            else XDeleteProperty(c_.display, id_, c_.atom("_NET_WM_WINDOW_OPACITY"));
        });
    }
    std::unique_ptr<X11DropTarget> drop_;
    void sendRoot(XEvent &e) {
        XSendEvent(c_.display, DefaultRootWindow(c_.display), False,
                   SubstructureRedirectMask | SubstructureNotifyMask, &e);
        XFlush(c_.display);
    }
    void state(const char *a, bool v, const char *b = nullptr) {
        XEvent e{};
        e.xclient.type = ClientMessage;
        e.xclient.window = id_;
        e.xclient.message_type = c_.atom("_NET_WM_STATE");
        e.xclient.format = 32;
        e.xclient.data.l[0] = v ? 1 : 0;
        e.xclient.data.l[1] = c_.atom(a);
        e.xclient.data.l[2] = b ? c_.atom(b) : 0;
        e.xclient.data.l[3] = 1;
        sendRoot(e);
    }
    static void preeditDone(XIM, XPointer p, XPointer) {
        auto *self = reinterpret_cast<X11Window *>(p);
        self->preedit_.clear();
        self->composition({}, 0);
    }
    static void preeditDraw(XIM, XPointer p, XPointer data) {
        auto *self = reinterpret_cast<X11Window *>(p);
        auto *draw = reinterpret_cast<XIMPreeditDrawCallbackStruct *>(data);
        if (!draw || draw->chg_first < 0 || draw->chg_length < 0)
            return;
        std::wstring text;
        if (draw->text) {
            if (draw->text->encoding_is_wchar)
                text.assign(draw->text->string.wide_char, draw->text->length);
            else if (draw->text->string.multi_byte)
                text = unicode::fromUtf8(draw->text->string.multi_byte);
        }
        auto start = std::min<std::size_t>(draw->chg_first, self->preedit_.size());
        self->preedit_.replace(start, std::min<std::size_t>(draw->chg_length, self->preedit_.size() - start),
                               text);
        self->composition(self->preedit_, std::max(0, draw->caret));
    }
    void initializeIme() {
        if (!c_.im)
            return;
        XIMCallback start{reinterpret_cast<XPointer>(this),
                          reinterpret_cast<XIMProc>(+[](XIM, XPointer, XPointer) -> int { return -1; })};
        XIMCallback done{reinterpret_cast<XPointer>(this), preeditDone},
            draw{reinterpret_cast<XPointer>(this), preeditDraw};
        XIMCallback caret{reinterpret_cast<XPointer>(this), +[](XIM, XPointer p, XPointer data) {
                              auto *self = reinterpret_cast<X11Window *>(p);
                              auto *c = reinterpret_cast<XIMPreeditCaretCallbackStruct *>(data);
                              if (c)
                                  self->composition(self->preedit_, std::max(0, c->position));
                          }};
        auto attributes =
            XVaCreateNestedList(0, XNPreeditStartCallback, &start, XNPreeditDoneCallback, &done,
                                XNPreeditDrawCallback, &draw, XNPreeditCaretCallback, &caret, nullptr);
        ic_ = XCreateIC(c_.im, XNInputStyle, XIMPreeditCallbacks | XIMStatusNothing, XNClientWindow, id_,
                        XNFocusWindow, id_, XNPreeditAttributes, attributes, nullptr);
        XFree(attributes);
        if (!ic_)
            ic_ = XCreateIC(c_.im, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, id_,
                            XNFocusWindow, id_, nullptr);
    }
    void keyboard(XKeyEvent &event) {
        const auto alive = lifetime_;
        KeySym symbol = XLookupKeysym(&event, 0);
        KeyEvent e;
        e.virtualKey = virtualKey(symbol);
        e.key = platform::logicalKey(e.virtualKey);
        e.scanCode = event.keycode;
        e.pressed = event.type == KeyPress;
        if (e.pressed)
            e.repeat = !pressedKeys_.insert(event.keycode).second;
        else
            pressedKeys_.erase(event.keycode);
        e.shift = event.state & ShiftMask;
        e.control = event.state & ControlMask;
        e.alt = event.state & Mod1Mask;
        e.win = event.state & Mod4Mask;
        const bool consumed = key(e);
        const bool printable = platform::printableVirtualKey(e.virtualKey);
        if (!*alive || isClosed() || rawKeyConsumed() ||
            (consumed && !(inputState().editable && printable)) || !e.pressed || e.control || e.win)
            return;
        char buffer[256];
        int status = 0;
        int length;
        if (ic_)
            length = Xutf8LookupString(ic_, &event, buffer, sizeof(buffer), &symbol, &status);
        else
            length = XLookupString(&event, buffer, sizeof(buffer), &symbol, nullptr);
        if (status == XBufferOverflow) {
            std::string large(static_cast<std::size_t>(length) + 1, '\0');
            length = Xutf8LookupString(ic_, &event, large.data(), large.size(), &symbol, &status);
            if (length > 0)
                text(unicode::fromUtf8({large.data(), static_cast<std::size_t>(length)}));
        } else if (length > 0)
            text(unicode::fromUtf8({buffer, static_cast<std::size_t>(length)}));
    }
    void pointer(int x, int y, int action, unsigned int state, unsigned int button, Time time) {
        const auto alive = lifetime_;
        Point point{x / scale_, y / scale_};
        if (button == 4 || button == 5) {
            if (action == 1)
                wheel({point, button == 4 ? 40.f : -40.f, bool(state & ShiftMask), bool(state & ControlMask),
                       bool(state & Mod1Mask), nowMs()});
            return;
        }
        if (action == 1 && button == 1 && dragRegion(point)) {
            XEvent e{};
            e.xclient.type = ClientMessage;
            e.xclient.window = id_;
            e.xclient.message_type = c_.atom("_NET_WM_MOVERESIZE");
            e.xclient.format = 32;
            ::Window root, child;
            int rx, ry, wx, wy;
            unsigned int mask;
            XQueryPointer(c_.display, id_, &root, &child, &rx, &ry, &wx, &wy, &mask);
            e.xclient.data.l[0] = rx;
            e.xclient.data.l[1] = ry;
            e.xclient.data.l[2] = 8;
            e.xclient.data.l[3] = 1;
            XUngrabPointer(c_.display, CurrentTime);
            sendRoot(e);
            return;
        }
        if (action == 1) {
            clicks_ = time - lastClick_ < 400 && std::abs(x - lastX_) < 4 && std::abs(y - lastY_) < 4
                          ? clicks_ % 3 + 1
                          : 1;
            lastClick_ = time;
            lastX_ = x;
            lastY_ = y;
        }
        mouse({point,
               button == 3   ? MouseButton::Right
               : button == 2 ? MouseButton::Middle
                             : MouseButton::Left,
               bool(state & ShiftMask), bool(state & ControlMask), bool(state & Mod1Mask), clicks_},
              action);
        if (!*alive || isClosed())
            return;
        auto kind = cursor(point);
        auto it = cursors_.find(kind);
        if (it == cursors_.end()) {
            unsigned int shape = XC_left_ptr;
            switch (kind) {
            case CursorKind::Pointer:
                shape = XC_hand2;
                break;
            case CursorKind::Text:
                shape = XC_xterm;
                break;
            case CursorKind::Crosshair:
                shape = XC_crosshair;
                break;
            case CursorKind::Grab:
                shape = XC_fleur;
                break;
            case CursorKind::ResizeHorizontal:
                shape = XC_sb_h_double_arrow;
                break;
            case CursorKind::ResizeVertical:
                shape = XC_sb_v_double_arrow;
                break;
            case CursorKind::ResizeNorthWestSouthEast: shape = XC_bottom_right_corner; break;
            case CursorKind::ResizeNorthEastSouthWest: shape = XC_bottom_left_corner; break;
            default:
                break;
            }
            it = cursors_.emplace(kind, XCreateFontCursor(c_.display, shape)).first;
        }
        XDefineCursor(c_.display, id_, it->second);
    }
    X11Connection &c_;
    ::Window id_ = 0;
    GC gc_ = nullptr;
    Atom deleteAtom_ = 0;
    XIC ic_ = nullptr;
    Size maximum_{};
    Size minimum_{};
    bool maximized_ = false;
    std::wstring preedit_;
    std::map<CursorKind, Cursor> cursors_;
    std::optional<WindowPlacement> restoredPlacement_;
    std::set<unsigned int> pressedKeys_;
    Time lastClick_ = 0;
    int clicks_ = 1, lastX_ = 0, lastY_ = 0;
};
std::unique_ptr<Window> X11Connection::create(WindowOptions options) {
    return std::make_unique<X11Window>(*this, std::move(options));
}
void X11Connection::readSelection() {
    Atom type{};
    int format{};
    unsigned long count{}, remaining{};
    unsigned char *data = nullptr;
    if (XGetWindowProperty(display, owner, transferAtom, 0, 4 * 1024 * 1024, True, AnyPropertyType, &type,
                           &format, &count, &remaining, &data) != Success) {
        receiving = false;
        return;
    }
    if (type == incrAtom)
        incremental = true;
    else if (format == 8 && data && received.size() + count <= 16 * 1024 * 1024 && remaining == 0) {
        received.append(reinterpret_cast<char *>(data), count);
        if (!incremental || count == 0)
            receiving = false;
    } else {
        if (count || remaining)
            received.clear();
        receiving = false;
    }
    if (data)
        XFree(data);
}
void X11Connection::selection(XEvent &event) {
    if (event.type == SelectionNotify && event.xselection.requestor == owner && receiving) {
        if (!event.xselection.property)
            receiving = false;
        else
            readSelection();
    } else if (event.type == PropertyNotify && event.xproperty.window == owner &&
               event.xproperty.atom == transferAtom && event.xproperty.state == PropertyNewValue &&
               receiving && incremental)
        readSelection();
    else if (event.type == SelectionRequest && event.xselectionrequest.owner == owner) {
        auto &r = event.xselectionrequest;
        ClipboardPeerErrorScope errors(display, r.requestor);
        XEvent reply{};
        reply.xselection = {SelectionNotify, 0,        False, display, r.requestor,
                            r.selection,     r.target, 0,     r.time};
        Atom property = r.property ? r.property : r.target;
        if (r.target == targetsAtom) {
            Atom targets[]{targetsAtom, utf8Atom};
            XChangeProperty(display, r.requestor, property, XA_ATOM, 32, PropModeReplace,
                            reinterpret_cast<unsigned char *>(targets), 2);
            reply.xselection.property = property;
        } else if (r.target == utf8Atom && sends.size() < 16) {
            if (ownedText.size() <= 64 * 1024)
                XChangeProperty(display, r.requestor, property, r.target, 8, PropModeReplace,
                                reinterpret_cast<const unsigned char *>(ownedText.data()), ownedText.size());
            else {
                unsigned long size = ownedText.size();
                XChangeProperty(display, r.requestor, property, incrAtom, 32, PropModeReplace,
                                reinterpret_cast<unsigned char *>(&size), 1);
                XSelectInput(display, r.requestor, PropertyChangeMask);
                sends[{r.requestor, property}] = {ownedText, 0, r.target, platform::DesktopWindow::nowMs() + 5000};
            }
            reply.xselection.property = property;
        }
        XSendEvent(display, r.requestor, False, 0, &reply);
        if (errors.failed()) sends.erase({r.requestor, property});
    } else if (event.type == PropertyNotify && event.xproperty.state == PropertyDelete) {
        auto it = sends.find({event.xproperty.window, event.xproperty.atom});
        if (it != sends.end()) {
            ClipboardPeerErrorScope errors(display, it->first.first);
            auto &t = it->second;
            auto count = std::min<std::size_t>(64 * 1024, t.text.size() - t.offset);
            XChangeProperty(display, it->first.first, it->first.second, t.target, 8, PropModeReplace,
                            reinterpret_cast<const unsigned char *>(t.text.data() + t.offset), count);
            t.offset += count;
            if (errors.failed() || !count)
                sends.erase(it);
        }
    }
}
void X11Connection::pump(int timeout) {
    platform::dispatchNativeServices();
    char bytes[128];
    while (::read(wakeup[0], bytes, sizeof(bytes)) > 0) {
    }
    while (XPending(display)) {
        XEvent e{};
        XNextEvent(display, &e);
        if (x11DockEvent(display, e)) continue;
        selection(e);
        auto it = windows.find(e.xany.window);
        if (it != windows.end())
            it->second->event(e);
    }
    if (pollX11DockDrag(display)) timeout = std::min(timeout, 20);
    auto snapshot = windows;
    for (auto pair : snapshot) {
        if (!windows.count(pair.first))
            continue;
        auto *window = pair.second;
        window->dispatchWork();
        if (!windows.count(pair.first))
            continue;
        window->paint();
        if (window->expireDrop()) timeout = std::min(timeout, 20);
        if (window->hasWork())
            timeout = std::min(timeout, 16);
    }
    XFlush(display);
    const double now = platform::DesktopWindow::nowMs();
    for (auto it = sends.begin(); it != sends.end();) {
        if (now >= it->second.deadline) it = sends.erase(it);
        else {
            timeout = std::min(timeout, std::max(1, static_cast<int>(it->second.deadline - now)));
            ++it;
        }
    }
    if (XPending(display) || windows.empty())
        timeout = 0;
    pollfd fds[]{{ConnectionNumber(display), POLLIN, 0}, {wakeup[0], POLLIN, 0}};
    if (platform::pollNativeServices(fds, 2, timeout) < 0 && errno != EINTR)
        throw std::runtime_error("OneUI X11 poll failed");
}
} // namespace
std::shared_ptr<Connection> x11Connection() { return std::make_shared<X11Connection>(); }
} // namespace oneui::linux_platform
