#include "internal/unicode.h"
#include "linux_runtime.h"
#include "oneui/controls/window_title_bar.h"
#include "pipe_write.h"
#include "platform/shared/desktop_window.h"
#include "platform/shared/key_mapping.h"
#include "platform/shared/text_input.h"
#include "text-input-unstable-v3-client-protocol.h"
#include "viewporter-client-protocol.h"
#include "xdg-shell-client-protocol.h"
#ifdef ONEUI_HAVE_FRACTIONAL_SCALE
#include "fractional-scale-v1-client-protocol.h"
#endif
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <map>
#include <poll.h>
#include <set>
#include <stdexcept>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon.h>

namespace oneui::linux_platform {
namespace {
class WaylandWindow;
class WaylandConnection;
void releaseKeyboard(wl_keyboard* object) {
    if (wl_keyboard_get_version(object) >= WL_KEYBOARD_RELEASE_SINCE_VERSION)
        wl_keyboard_release(object);
    else wl_keyboard_destroy(object);
}
void releasePointer(wl_pointer* object) {
    if (wl_pointer_get_version(object) >= WL_POINTER_RELEASE_SINCE_VERSION)
        wl_pointer_release(object);
    else wl_pointer_destroy(object);
}
struct Output {
    WaylandConnection* connection = nullptr;
    wl_output *object = nullptr;
    MonitorInfo info;
    int scale = 1;
};
struct Offer {
    wl_data_offer *object = nullptr;
    bool utf8 = false, plain = false;
};
struct ClipboardSend {
    int fd;
    std::string text;
    std::size_t offset = 0;
    double deadline;
};
class WaylandConnection final : public Connection {
  public:
    WaylandConnection();
    ~WaylandConnection() override;
    std::unique_ptr<Window> create(WindowOptions options) override;
    void setClipboard(const std::wstring &value) override;
    std::wstring clipboard() override;
    std::vector<MonitorInfo> monitors() override;
    void pump(int timeout);
    void wake() {
        char b = 1;
        const auto ignored = ::write(wakeup[1], &b, 1);
        (void)ignored;
    }
    WaylandWindow *find(wl_surface *surface) const;
    void syncInput(bool force = false);
    void keyboard(uint32_t key, bool pressed, bool repeat = false);
    void pointerCursor();
    wl_display *display = nullptr;
    wl_registry *registry = nullptr;
    wl_compositor *compositor = nullptr;
    wl_shm *shm = nullptr;
    xdg_wm_base *shell = nullptr;
    wl_seat *seat = nullptr;
    wl_keyboard *keyboardObject = nullptr;
    wl_pointer *pointerObject = nullptr;
    wl_data_device_manager *dataManager = nullptr;
    wl_data_device *dataDevice = nullptr;
    zwp_text_input_manager_v3 *textManager = nullptr;
    zwp_text_input_v3 *textInput = nullptr;
    wp_viewporter *viewporter = nullptr;
#ifdef ONEUI_HAVE_FRACTIONAL_SCALE
    wp_fractional_scale_manager_v1 *fractionalManager = nullptr;
#endif
    std::map<wl_surface *, WaylandWindow *> windows;
    std::map<uint32_t, std::unique_ptr<Output>> outputs;
    std::map<wl_data_offer *, std::unique_ptr<Offer>> offers;
    WaylandWindow *keyFocus = nullptr;
    WaylandWindow *pointerFocus = nullptr;
    WaylandWindow *textFocus = nullptr;
    wl_data_offer *selection = nullptr;
    wl_data_source *source = nullptr;
    std::string ownedText;
    Point pointerPosition{};
    uint32_t serial = 0, pointerSerial = 0;
    xkb_context *xkb = nullptr;
    xkb_keymap *keymap = nullptr;
    xkb_state *keyState = nullptr;
    int wakeup[2]{-1, -1};
    int repeatRate = 25, repeatDelay = 600;
    uint32_t repeatedKey = 0;
    double repeatAt = 0;
    bool shift = false, control = false, alt = false, meta = false, inputEnabled = false;
    std::wstring pendingPreedit, pendingCommit;
    int pendingCaret = 0;
    uint32_t deleteBefore = 0, deleteAfter = 0;
    bool hasPreedit = false, hasCommit = false;
    wl_cursor_theme *cursorTheme = nullptr;
    wl_surface *cursorSurface = nullptr;
    std::string inputSignature;
    std::vector<ClipboardSend> sends;
    uint32_t inputSerial = 0;
    uint32_t inputSessionSerial = 0;
    std::uint64_t inputSession = 0;
    const void *inputIdentity = nullptr;
    bool inputMethodChange = false;
    void resetPendingInput() {
        hasCommit = hasPreedit = false;
        deleteBefore = deleteAfter = 0;
        pendingCommit.clear();
        pendingPreedit.clear();
    }

  private:
    void release() noexcept;
    void flushClipboardSends();
    static void global(void *, wl_registry *, uint32_t, const char *, uint32_t);
    static void globalRemove(void *, wl_registry *, uint32_t);
    static void seatCapabilities(void *, wl_seat *, uint32_t);
    void installTextInput();
};
struct Buffer {
    wl_buffer *object = nullptr;
    void *data = MAP_FAILED;
    std::size_t bytes = 0;
    int width = 0, height = 0;
    bool busy = false;
    ~Buffer() {
        if (object)
            wl_buffer_destroy(object);
        if (data != MAP_FAILED)
            munmap(data, bytes);
    }
};
class WaylandWindow final : public platform::DesktopWindow {
  public:
    WaylandWindow(WaylandConnection &c, WindowOptions o) : DesktopWindow(std::move(o)), c_(c) {
        logicalSize_.height += chromeHeight();
    }
    ~WaylandWindow() override { close(); }
    WindowBackend backend() const override { return WindowBackend::Wayland; }
    unsigned int capabilities() const override {
        return (c_.dataDevice && c_.serial ? WindowCapabilityClipboard : 0) |
               (c_.textInput ? WindowCapabilityIme : 0);
    }
    void initialize() override {
        assertUiThread();
        if (surface_ || isClosed())
            return;
        if (options_.topmost)
            throw std::runtime_error("Wayland does not support arbitrary topmost windows");
        surface_ = wl_compositor_create_surface(c_.compositor);
        c_.windows[surface_] = this;
        static const wl_surface_listener surfaceListener{[](void *d, wl_surface *, wl_output *o) {
                                                             auto *self = static_cast<WaylandWindow *>(d);
                                                             self->entered_.insert(o);
                                                             self->updateScale();
                                                         },
                                                         [](void *d, wl_surface *, wl_output *o) {
                                                             auto *self = static_cast<WaylandWindow *>(d);
                                                             self->entered_.erase(o);
                                                             self->updateScale();
                                                         }};
        wl_surface_add_listener(surface_, &surfaceListener, this);
        xdgSurface_ = xdg_wm_base_get_xdg_surface(c_.shell, surface_);
        static const xdg_surface_listener xdgListener{[](void *d, xdg_surface *s, uint32_t serial) {
            auto *self = static_cast<WaylandWindow *>(d);
            xdg_surface_ack_configure(s, serial);
            self->configured_ = true;
            self->resize(self->pendingSize_, self->scale_);
            self->requestRedraw();
        }};
        xdg_surface_add_listener(xdgSurface_, &xdgListener, this);
        toplevel_ = xdg_surface_get_toplevel(xdgSurface_);
        static const xdg_toplevel_listener topListener{
            [](void *d, xdg_toplevel *, int32_t w, int32_t h, wl_array *states) {
                auto *self = static_cast<WaylandWindow *>(d);
                self->pendingSize_ = {w > 0 ? float(w) : self->logicalSize_.width,
                                      h > 0 ? float(h) : self->logicalSize_.height};
                self->maximized_ = false;
                for (std::size_t i = 0; i < states->size / sizeof(uint32_t); ++i)
                    if (static_cast<uint32_t *>(states->data)[i] == XDG_TOPLEVEL_STATE_MAXIMIZED)
                        self->maximized_ = true;
                if (self->chrome_)
                    self->chrome_->setMaximized(self->maximized_);
            },
            [](void *d, xdg_toplevel *) { static_cast<WaylandWindow *>(d)->close(); }};
        xdg_toplevel_add_listener(toplevel_, &topListener, this);
        xdg_toplevel_set_app_id(toplevel_, "org.oneui.application");
        pendingSize_ = logicalSize_;
        setTitle(options_.title);
        setMinimumClientSize(minimum_);
        setFullscreen(options_.fullscreen);
#ifdef ONEUI_HAVE_FRACTIONAL_SCALE
        if (c_.viewporter && c_.fractionalManager) {
            viewport_ = wp_viewporter_get_viewport(c_.viewporter, surface_);
            fractional_ = wp_fractional_scale_manager_v1_get_fractional_scale(c_.fractionalManager, surface_);
            static const wp_fractional_scale_v1_listener scaleListener{
                [](void *d, wp_fractional_scale_v1 *, uint32_t scale) {
                    auto *self = static_cast<WaylandWindow *>(d);
                    if (scale) {
                        self->fractionalScale_ = scale / 120.f;
                        self->updateScale();
                    }
                }};
            wp_fractional_scale_v1_add_listener(fractional_, &scaleListener, this);
        }
#endif
        wl_surface_commit(surface_);
        wl_display_flush(c_.display);
        if (options_.visible)
            show();
    }
    void setContent(std::shared_ptr<Widget> widget) override {
        DesktopWindow::setContent(std::move(widget));
        rebuildChrome();
    }
    void show() override {
        initialize();
        shown_ = true;
        requestRedraw();
    }
    int run() override {
        initialize();
        auto& connection = c_;
        while (!connection.windows.empty())
            connection.pump(1000);
        return 0;
    }
    void close() override {
        if (isClosed())
            return;
        assertUiThread();
        finishClose();
        if (chrome_)
            chrome_->detachFromOwner(this);
        if (c_.keyFocus == this) {
            c_.keyFocus = nullptr;
            c_.repeatedKey = 0;
        }
        if (c_.pointerFocus == this)
            c_.pointerFocus = nullptr;
        if (c_.textFocus == this) {
            c_.textFocus = nullptr;
            c_.inputEnabled = false;
        }
        if (frame_) {
            wl_callback_destroy(frame_);
            frame_ = nullptr;
        }
#ifdef ONEUI_HAVE_FRACTIONAL_SCALE
        if (fractional_)
            wp_fractional_scale_v1_destroy(fractional_);
#endif
        if (viewport_)
            wp_viewport_destroy(viewport_);
        if (toplevel_)
            xdg_toplevel_destroy(toplevel_);
        if (xdgSurface_)
            xdg_surface_destroy(xdgSurface_);
        if (surface_) {
            c_.windows.erase(surface_);
            wl_surface_destroy(surface_);
            surface_ = nullptr;
        }
        buffers_.clear();
        wake();
    }
    void minimize() override {
        initialize();
        xdg_toplevel_set_minimized(toplevel_);
    }
    NativeWindowHandle nativeHandle() const override { return surface_; }
    void setTitle(std::wstring title) override {
        options_.title = std::move(title);
        if (toplevel_)
            xdg_toplevel_set_title(toplevel_, unicode::toUtf8(options_.title).c_str());
        if (chrome_)
            chrome_->setTitle(options_.title);
    }
    void setFullscreen(bool value) override {
        options_.fullscreen = value;
        if (toplevel_) {
            if (value)
                xdg_toplevel_set_fullscreen(toplevel_, nullptr);
            else
                xdg_toplevel_unset_fullscreen(toplevel_);
        }
        rebuildChrome();
    }
    void setBorderless(bool value) override {
        options_.borderless = value;
        rebuildChrome();
    }
    void toggleMaximize() override {
        initialize();
        if (maximized_)
            xdg_toplevel_unset_maximized(toplevel_);
        else
            xdg_toplevel_set_maximized(toplevel_);
    }
    void setMinimumClientSize(Size size) override {
        minimum_ = size;
        if (!toplevel_)
            return;
        xdg_toplevel_set_min_size(toplevel_, std::max(0, static_cast<int>(size.width)),
                                  std::max(0, static_cast<int>(size.height + chromeHeight())));
        if (!options_.resizable) {
            xdg_toplevel_set_min_size(toplevel_, logicalSize_.width, logicalSize_.height);
            xdg_toplevel_set_max_size(toplevel_, logicalSize_.width, logicalSize_.height);
        }
    }
    void updateScale() {
        float scale = 1;
        for (auto &pair : c_.outputs)
            if (entered_.count(pair.second->object))
                scale = std::max(scale, float(pair.second->scale));
        resize(logicalSize_, fractionalScale_ > 0 ? fractionalScale_ : scale);
    }
    void paint() {
        if (!shown_ || !configured_ || frame_ || !needsPaint() || !surface_)
            return;
        const auto size = surfacePixelSize();
        buffers_.erase(std::remove_if(buffers_.begin(), buffers_.end(),
                                      [&](const auto &b) {
                                          return !b->busy &&
                                                 (b->width != size.width || b->height != size.height);
                                      }),
                       buffers_.end());
        Buffer *buffer = nullptr;
        for (auto &b : buffers_)
            if (!b->busy && b->width == size.width && b->height == size.height) {
                buffer = b.get();
                break;
            }
        if (!buffer && buffers_.size() >= 3)
            return;
        const auto &pixels = render();
        if (!buffer) {
            auto b = std::make_unique<Buffer>();
            b->width = size.width;
            b->height = size.height;
            b->bytes = pixels.size() * 4;
            int fd = memfd_create("oneui-frame", MFD_CLOEXEC);
            if (fd < 0 || ftruncate(fd, b->bytes) < 0) {
                if (fd >= 0)
                    ::close(fd);
                throw std::runtime_error("Wayland shared memory allocation failed");
            }
            b->data = mmap(nullptr, b->bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (b->data == MAP_FAILED) {
                ::close(fd);
                throw std::runtime_error("Wayland mmap failed");
            }
            auto *pool = wl_shm_create_pool(c_.shm, fd, b->bytes);
            ::close(fd);
            b->object =
                wl_shm_pool_create_buffer(pool, 0, b->width, b->height, b->width * 4, WL_SHM_FORMAT_ARGB8888);
            wl_shm_pool_destroy(pool);
            static const wl_buffer_listener listener{
                [](void *d, wl_buffer *) { static_cast<Buffer *>(d)->busy = false; }};
            wl_buffer_add_listener(b->object, &listener, b.get());
            buffer = b.get();
            buffers_.push_back(std::move(b));
        }
        std::memcpy(buffer->data, pixels.data(), pixels.size() * 4);
        buffer->busy = true;
        wl_surface_set_buffer_scale(surface_, viewport_ ? 1 : static_cast<int>(scale_));
        if (viewport_)
            wp_viewport_set_destination(viewport_, logicalSize_.width, logicalSize_.height);
        wl_surface_attach(surface_, buffer->object, 0, 0);
        wl_surface_damage_buffer(surface_, 0, 0, buffer->width, buffer->height);
        frame_ = wl_surface_frame(surface_);
        static const wl_callback_listener listener{[](void *d, wl_callback *cb, uint32_t) {
            auto *self = static_cast<WaylandWindow *>(d);
            wl_callback_destroy(cb);
            self->frame_ = nullptr;
        }};
        wl_callback_add_listener(frame_, &listener, this);
        wl_surface_commit(surface_);
    }
    void button(uint32_t serial, uint32_t button, bool pressed) {
        const Point p = c_.pointerPosition;
        if (pressed && button == BTN_LEFT) {
            uint32_t edge = 0;
            if (options_.resizable && !maximized_ && !options_.fullscreen) {
                if (p.y < 5)
                    edge |= XDG_TOPLEVEL_RESIZE_EDGE_TOP;
                else if (p.y >= logicalSize_.height - 5)
                    edge |= XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM;
                if (p.x < 5)
                    edge |= XDG_TOPLEVEL_RESIZE_EDGE_LEFT;
                else if (p.x >= logicalSize_.width - 5)
                    edge |= XDG_TOPLEVEL_RESIZE_EDGE_RIGHT;
            }
            if (edge) {
                xdg_toplevel_resize(toplevel_, c_.seat, serial, edge);
                return;
            }
            if ((chrome_ && p.y < 36 && p.x < logicalSize_.width - 138) || dragRegion(p)) {
                xdg_toplevel_move(toplevel_, c_.seat, serial);
                return;
            }
        }
        mouse({p,
               button == BTN_RIGHT    ? MouseButton::Right
               : button == BTN_MIDDLE ? MouseButton::Middle
                                      : MouseButton::Left,
               c_.shift, c_.control, c_.alt, 1},
              pressed ? 1 : 2);
    }

  protected:
    void wake() override { c_.wake(); }
    float chromeHeight() const override { return !options_.borderless && !options_.fullscreen ? 36.f : 0.f; }
    void paintChrome(Canvas &canvas) override {
        if (chrome_) {
            chrome_->setFrame({0, 0, logicalSize_.width, chromeHeight()});
            chrome_->paint(canvas);
        }
    }

  public:
    void mouse(const MouseEvent &event, int action) override {
        const auto alive = lifetime_;
        auto title = chrome_;
        if (title) {
            if (action == 0)
                title->onMouseMove(event);
            else if (action == 1 && event.position.y < chromeHeight())
                title->onMouseDown(event);
            else if (action == 2)
                title->onMouseUp(event);
        }
        if (*alive && !isClosed())
            DesktopWindow::mouse(event, action);
    }

  private:
    void animateChrome() {
        requestAnimationFrame([this](double now) {
            if (chrome_ && chrome_->tickAnimations(now))
                animateChrome();
        });
    }
    void rebuildChrome() {
        if (chrome_)
            chrome_->detachFromOwner(this);
        if (!options_.borderless && !options_.fullscreen) {
            chrome_ = std::make_shared<WindowTitleBar>(options_.title);
            chrome_->setOnMinimize([this] { minimize(); });
            chrome_->setOnMaximize([this] { toggleMaximize(); });
            chrome_->setOnClose([this] { close(); });
            chrome_->attachToOwner(
                this, [this] { requestRedraw(); }, [this](Rect rect) { redrawRect(rect); },
                [this] { animateChrome(); });
        } else
            chrome_.reset();
        requestRedraw();
    }
    WaylandConnection &c_;
    wl_surface *surface_ = nullptr;
    xdg_surface *xdgSurface_ = nullptr;
    xdg_toplevel *toplevel_ = nullptr;
    wl_callback *frame_ = nullptr;
    wp_viewport *viewport_ = nullptr;
#ifdef ONEUI_HAVE_FRACTIONAL_SCALE
    wp_fractional_scale_v1 *fractional_ = nullptr;
#endif
    std::vector<std::unique_ptr<Buffer>> buffers_;
    std::set<wl_output *> entered_;
    Size pendingSize_{}, minimum_{};
    float fractionalScale_ = 0;
    bool configured_ = false, maximized_ = false;
    std::shared_ptr<WindowTitleBar> chrome_;
};

WaylandWindow *WaylandConnection::find(wl_surface *s) const {
    auto it = windows.find(s);
    return it == windows.end() ? nullptr : it->second;
}
void WaylandConnection::global(void *data, wl_registry *registry, uint32_t name, const char *interface,
                               uint32_t version) {
    auto &c = *static_cast<WaylandConnection *>(data);
    if (!std::strcmp(interface, wl_compositor_interface.name))
        c.compositor = static_cast<wl_compositor *>(
            wl_registry_bind(registry, name, &wl_compositor_interface, std::min(version, 4u)));
    else if (!std::strcmp(interface, wl_shm_interface.name))
        c.shm = static_cast<wl_shm *>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
    else if (!std::strcmp(interface, xdg_wm_base_interface.name)) {
        c.shell = static_cast<xdg_wm_base *>(wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));
        static const xdg_wm_base_listener listener{
            [](void *, xdg_wm_base *shell, uint32_t serial) { xdg_wm_base_pong(shell, serial); }};
        xdg_wm_base_add_listener(c.shell, &listener, nullptr);
    } else if (!std::strcmp(interface, wl_seat_interface.name) && !c.seat) {
        c.seat = static_cast<wl_seat *>(
            wl_registry_bind(registry, name, &wl_seat_interface, std::min(version, 5u)));
        static const wl_seat_listener listener{seatCapabilities, [](void *, wl_seat *, const char *) {}};
        wl_seat_add_listener(c.seat, &listener, &c);
    } else if (!std::strcmp(interface, wl_data_device_manager_interface.name))
        c.dataManager = static_cast<wl_data_device_manager *>(
            wl_registry_bind(registry, name, &wl_data_device_manager_interface, std::min(version, 3u)));
    else if (!std::strcmp(interface, zwp_text_input_manager_v3_interface.name))
        c.textManager = static_cast<zwp_text_input_manager_v3 *>(
            wl_registry_bind(registry, name, &zwp_text_input_manager_v3_interface, 1));
    else if (!std::strcmp(interface, wp_viewporter_interface.name))
        c.viewporter =
            static_cast<wp_viewporter *>(wl_registry_bind(registry, name, &wp_viewporter_interface, 1));
#ifdef ONEUI_HAVE_FRACTIONAL_SCALE
    else if (!std::strcmp(interface, wp_fractional_scale_manager_v1_interface.name))
        c.fractionalManager = static_cast<wp_fractional_scale_manager_v1 *>(
            wl_registry_bind(registry, name, &wp_fractional_scale_manager_v1_interface, 1));
#endif
    else if (!std::strcmp(interface, wl_output_interface.name)) {
        auto output = std::make_unique<Output>();
        output->connection = &c;
        output->info.index = c.outputs.size();
        output->info.primary = c.outputs.empty();
        output->object = static_cast<wl_output *>(
            wl_registry_bind(registry, name, &wl_output_interface, std::min(version, 2u)));
        static const wl_output_listener listener{
            [](void *d, wl_output *, int32_t x, int32_t y, int32_t, int32_t, int32_t, const char *make,
               const char *model, int32_t) {
                auto &o = *static_cast<Output *>(d);
                o.info.bounds.x = x;
                o.info.bounds.y = y;
                o.info.name = unicode::fromUtf8(std::string(make ? make : "") + " " + (model ? model : ""));
            },
            [](void *d, wl_output *, uint32_t flags, int32_t w, int32_t h, int32_t) {
                if (flags & WL_OUTPUT_MODE_CURRENT) {
                    auto &o = *static_cast<Output *>(d);
                    o.info.bounds.width = w;
                    o.info.bounds.height = h;
                }
            },
            [](void *d, wl_output *) {
                auto &o = *static_cast<Output *>(d);
                o.info.workArea = o.info.bounds;
                for (const auto& entry : o.connection->windows)
                    entry.second->updateScale();
            },
            [](void *d, wl_output *, int32_t scale) {
                auto &o = *static_cast<Output *>(d);
                o.scale = std::max(1, scale);
                o.info.scale = o.scale;
            }};
        wl_output_add_listener(output->object, &listener, output.get());
        c.outputs[name] = std::move(output);
    }
}
void WaylandConnection::globalRemove(void *d, wl_registry *, uint32_t name) {
    auto &c = *static_cast<WaylandConnection *>(d);
    auto it = c.outputs.find(name);
    if (it != c.outputs.end()) {
        wl_output_destroy(it->second->object);
        c.outputs.erase(it);
        for (const auto& entry : c.windows)
            entry.second->updateScale();
    }
}
void WaylandConnection::seatCapabilities(void *data, wl_seat *seat, uint32_t caps) {
    auto &c = *static_cast<WaylandConnection *>(data);
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !c.keyboardObject) {
        c.keyboardObject = wl_seat_get_keyboard(seat);
        static const wl_keyboard_listener listener{
            [](void *d, wl_keyboard *, uint32_t format, int32_t fd, uint32_t size) {
                auto &c = *static_cast<WaylandConnection *>(d);
                if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 || !size) {
                    ::close(fd);
                    return;
                }
                void *data = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
                ::close(fd);
                if (data == MAP_FAILED)
                    return;
                if (static_cast<char *>(data)[size - 1] != 0) {
                    munmap(data, size);
                    return;
                }
                auto *map = xkb_keymap_new_from_string(
                    c.xkb, static_cast<char *>(data), XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
                munmap(data, size);
                if (!map)
                    return;
                if (c.keyState)
                    xkb_state_unref(c.keyState);
                if (c.keymap)
                    xkb_keymap_unref(c.keymap);
                c.keymap = map;
                c.keyState = xkb_state_new(map);
            },
            [](void *d, wl_keyboard *, uint32_t, wl_surface *s, wl_array *) {
                auto &c = *static_cast<WaylandConnection *>(d);
                c.keyFocus = c.find(s);
                if (c.keyFocus)
                    c.keyFocus->focus(true);
            },
            [](void *d, wl_keyboard *, uint32_t, wl_surface *) {
                auto &c = *static_cast<WaylandConnection *>(d);
                if (c.keyFocus)
                    c.keyFocus->focus(false);
                c.keyFocus = nullptr;
                c.serial = 0;
                c.repeatedKey = 0;
            },
            [](void *d, wl_keyboard *, uint32_t serial, uint32_t, uint32_t key, uint32_t state) {
                auto &c = *static_cast<WaylandConnection *>(d);
                c.serial = serial;
                c.keyboard(key, state == WL_KEYBOARD_KEY_STATE_PRESSED);
                if (state == WL_KEYBOARD_KEY_STATE_PRESSED && c.keymap &&
                    xkb_keymap_key_repeats(c.keymap, key + 8)) {
                    c.repeatedKey = key;
                    c.repeatAt = platform::DesktopWindow::nowMs() + c.repeatDelay;
                } else if (c.repeatedKey == key)
                    c.repeatedKey = 0;
                c.syncInput();
            },
            [](void *d, wl_keyboard *, uint32_t, uint32_t depressed, uint32_t latched, uint32_t locked,
               uint32_t group) {
                auto &c = *static_cast<WaylandConnection *>(d);
                if (!c.keyState)
                    return;
                xkb_state_update_mask(c.keyState, depressed, latched, locked, 0, 0, group);
                c.shift = xkb_state_mod_name_is_active(c.keyState, XKB_MOD_NAME_SHIFT,
                                                       XKB_STATE_MODS_EFFECTIVE) > 0;
                c.control =
                    xkb_state_mod_name_is_active(c.keyState, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) > 0;
                c.alt =
                    xkb_state_mod_name_is_active(c.keyState, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE) > 0;
                c.meta =
                    xkb_state_mod_name_is_active(c.keyState, XKB_MOD_NAME_LOGO, XKB_STATE_MODS_EFFECTIVE) > 0;
            },
            [](void *d, wl_keyboard *, int32_t rate, int32_t delay) {
                auto &c = *static_cast<WaylandConnection *>(d);
                c.repeatRate = std::max(0, rate);
                c.repeatDelay = std::max(0, delay);
            }};
        wl_keyboard_add_listener(c.keyboardObject, &listener, &c);
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && c.keyboardObject) {
        releaseKeyboard(c.keyboardObject);
        c.keyboardObject = nullptr;
        if (c.keyFocus) c.keyFocus->focus(false);
        c.keyFocus = nullptr;
        c.serial = 0;
        c.shift = c.control = c.alt = c.meta = false;
        c.repeatedKey = 0;
    }
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !c.pointerObject) {
        c.pointerObject = wl_seat_get_pointer(seat);
        static const wl_pointer_listener listener{
            [](void *d, wl_pointer *, uint32_t serial, wl_surface *s, wl_fixed_t x, wl_fixed_t y) {
                auto &c = *static_cast<WaylandConnection *>(d);
                c.pointerSerial = serial;
                c.pointerFocus = c.find(s);
                c.pointerPosition = {float(wl_fixed_to_double(x)), float(wl_fixed_to_double(y))};
                c.pointerCursor();
            },
            [](void *d, wl_pointer *, uint32_t, wl_surface *) {
                auto &c = *static_cast<WaylandConnection *>(d);
                if (c.pointerFocus)
                    c.pointerFocus->mouse({{-1, -1}, MouseButton::None}, 0);
                c.pointerFocus = nullptr;
            },
            [](void *d, wl_pointer *, uint32_t, wl_fixed_t x, wl_fixed_t y) {
                auto &c = *static_cast<WaylandConnection *>(d);
                c.pointerPosition = {float(wl_fixed_to_double(x)), float(wl_fixed_to_double(y))};
                if (c.pointerFocus)
                    c.pointerFocus->mouse({c.pointerPosition, MouseButton::Left, c.shift, c.control, c.alt},
                                          0);
                c.pointerCursor();
            },
            [](void *d, wl_pointer *, uint32_t serial, uint32_t, uint32_t button, uint32_t state) {
                auto &c = *static_cast<WaylandConnection *>(d);
                c.serial = serial;
                if (c.pointerFocus)
                    c.pointerFocus->button(serial, button, state == WL_POINTER_BUTTON_STATE_PRESSED);
                c.syncInput();
            },
            [](void *d, wl_pointer *, uint32_t, uint32_t axis, wl_fixed_t value) {
                auto &c = *static_cast<WaylandConnection *>(d);
                if (c.pointerFocus && axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
                    c.pointerFocus->wheel({c.pointerPosition, -float(wl_fixed_to_double(value)), c.shift,
                                           c.control, c.alt, platform::DesktopWindow::nowMs()});
            },
            [](void *, wl_pointer *) {},
            [](void *, wl_pointer *, uint32_t) {},
            [](void *, wl_pointer *, uint32_t, uint32_t) {},
            [](void *, wl_pointer *, uint32_t, int32_t) {}};
        wl_pointer_add_listener(c.pointerObject, &listener, &c);
    } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && c.pointerObject) {
        releasePointer(c.pointerObject);
        c.pointerObject = nullptr;
        c.pointerFocus = nullptr;
    }
}
void WaylandConnection::pointerCursor() {
    if (!pointerObject || !pointerFocus || !cursorTheme)
        return;
    const auto kind = pointerFocus->cursor(pointerPosition);
    if (kind == CursorKind::Hidden) {
        wl_pointer_set_cursor(pointerObject, pointerSerial, nullptr, 0, 0);
        return;
    }
    const char *name = "left_ptr";
    switch (kind) {
    case CursorKind::Pointer:
        name = "hand2";
        break;
    case CursorKind::Text:
        name = "xterm";
        break;
    case CursorKind::Crosshair:
        name = "crosshair";
        break;
    case CursorKind::Grab:
        name = "grabbing";
        break;
    case CursorKind::ResizeHorizontal:
        name = "sb_h_double_arrow";
        break;
    case CursorKind::ResizeVertical:
        name = "sb_v_double_arrow";
        break;
    default:
        break;
    }
    auto *cursor = wl_cursor_theme_get_cursor(cursorTheme, name);
    if (!cursor || !cursor->image_count)
        return;
    auto *image = cursor->images[0];
    wl_pointer_set_cursor(pointerObject, pointerSerial, cursorSurface, image->hotspot_x, image->hotspot_y);
    wl_surface_attach(cursorSurface, wl_cursor_image_get_buffer(image), 0, 0);
    wl_surface_damage(cursorSurface, 0, 0, image->width, image->height);
    wl_surface_commit(cursorSurface);
}
void WaylandConnection::keyboard(uint32_t code, bool pressed, bool repeat) {
    if (!keyFocus || !keyState)
        return;
    const xkb_keysym_t *symbols = nullptr;
    const auto layout = xkb_state_key_get_layout(keyState, code + 8);
    const int count = xkb_keymap_key_get_syms_by_level(keymap, code + 8, layout, 0, &symbols);
    auto symbol = count > 0 ? symbols[0] : xkb_state_key_get_one_sym(keyState, code + 8);
    unsigned int vk = xkb_keysym_to_upper(symbol);
    if (vk < 128)
        vk = platform::asciiVirtualKey(vk);
    switch (symbol) {
    case XKB_KEY_Return:
    case XKB_KEY_KP_Enter:
        vk = 13;
        break;
    case XKB_KEY_Tab:
    case XKB_KEY_ISO_Left_Tab:
        vk = 9;
        break;
    case XKB_KEY_BackSpace:
        vk = 8;
        break;
    case XKB_KEY_Escape:
        vk = 27;
        break;
    case XKB_KEY_Left:
        vk = 37;
        break;
    case XKB_KEY_Right:
        vk = 39;
        break;
    case XKB_KEY_Up:
        vk = 38;
        break;
    case XKB_KEY_Down:
        vk = 40;
        break;
    case XKB_KEY_Home:
        vk = 36;
        break;
    case XKB_KEY_End:
        vk = 35;
        break;
    case XKB_KEY_Page_Up:
        vk = 33;
        break;
    case XKB_KEY_Page_Down:
        vk = 34;
        break;
    case XKB_KEY_Delete:
        vk = 46;
        break;
    case XKB_KEY_Insert:
        vk = 45;
        break;
    case XKB_KEY_Shift_L:
    case XKB_KEY_Shift_R:
        vk = 16;
        break;
    case XKB_KEY_Control_L:
    case XKB_KEY_Control_R:
        vk = 17;
        break;
    case XKB_KEY_Alt_L:
    case XKB_KEY_Alt_R:
        vk = 18;
        break;
    case XKB_KEY_Super_L:
        vk = 91;
        break;
    case XKB_KEY_Super_R:
        vk = 92;
        break;
    default:
        if (symbol >= XKB_KEY_F1 && symbol <= XKB_KEY_F24)
            vk = 112 + symbol - XKB_KEY_F1;
        break;
    }
    KeyEvent event;
    event.key = platform::logicalKey(vk);
    event.virtualKey = vk;
    event.scanCode = code;
    event.pressed = pressed;
    event.repeat = repeat;
    event.shift = shift;
    event.control = control;
    event.alt = alt;
    event.win = meta;
    const bool consumed = keyFocus->key(event);
    const bool printable = platform::printableVirtualKey(vk) || xkb_keysym_to_utf32(symbol) >= 128;
    if (!keyFocus || keyFocus->rawKeyConsumed() ||
        (consumed && !(keyFocus->inputState().editable && printable)) || !pressed || control || meta)
        return;
    // Compositors forward unconsumed keys; IME-handled keys arrive via commit_string.
    char text[128];
    int n = xkb_state_key_get_utf8(keyState, code + 8, text, sizeof(text));
    if (n > 0 && n < static_cast<int>(sizeof(text)))
        keyFocus->text(unicode::fromUtf8({text, static_cast<std::size_t>(n)}));
}
void WaylandConnection::installTextInput() {
    if (!textManager || !seat)
        return;
    textInput = zwp_text_input_manager_v3_get_text_input(textManager, seat);
    static const zwp_text_input_v3_listener listener{
        [](void *d, zwp_text_input_v3 *, wl_surface *s) {
            auto &c = *static_cast<WaylandConnection *>(d);
            c.resetPendingInput();
            c.textFocus = c.find(s);
            c.inputEnabled = false;
            c.syncInput(true);
        },
        [](void *d, zwp_text_input_v3 *, wl_surface *) {
            auto &c = *static_cast<WaylandConnection *>(d);
            if (c.textFocus)
                c.textFocus->composition({}, 0);
            c.textFocus = nullptr;
            c.inputEnabled = false;
            c.inputIdentity = nullptr;
            c.resetPendingInput();
        },
        [](void *d, zwp_text_input_v3 *, const char *text, int32_t begin, int32_t) {
            auto &c = *static_cast<WaylandConnection *>(d);
            std::string_view value = text ? text : "";
            c.pendingPreedit = unicode::fromUtf8(value);
            c.pendingCaret =
                unicode::fromUtf8(value.substr(0, std::min<std::size_t>(std::max(0, begin), value.size())))
                    .size();
            c.hasPreedit = true;
        },
        [](void *d, zwp_text_input_v3 *, const char *text) {
            auto &c = *static_cast<WaylandConnection *>(d);
            c.pendingCommit = unicode::fromUtf8(text ? text : "");
            c.hasCommit = true;
        },
        [](void *d, zwp_text_input_v3 *, uint32_t before, uint32_t after) {
            auto &c = *static_cast<WaylandConnection *>(d);
            c.deleteBefore = before;
            c.deleteAfter = after;
        },
        [](void *d, zwp_text_input_v3 *, uint32_t serial) {
            auto &c = *static_cast<WaylandConnection *>(d);
            const auto current = c.textFocus ? c.textFocus->inputState() : TextInputState{};
            if (!c.inputEnabled || c.inputIdentity != current.identity || c.inputSession != current.session ||
                static_cast<int32_t>(serial - c.inputSessionSerial) < 0) {
                c.resetPendingInput();
                c.syncInput(true);
                return;
            }
            if (c.textFocus) {
                c.textFocus->composition({}, 0);
                if (c.textFocus && (c.deleteBefore || c.deleteAfter)) {
                    auto s = c.textFocus->inputState();
                    const auto range = platform::surroundingDeletionRange(
                        s.text, s.anchor, s.caret, c.deleteBefore, c.deleteAfter);
                    c.textFocus->replaceText(range.first, range.second, c.hasCommit ? c.pendingCommit : L"");
                    c.hasCommit = false; // Deletion + commit is one undoable edit.
                }
                if (c.textFocus && c.hasCommit)
                    c.textFocus->text(c.pendingCommit);
                if (c.textFocus && c.hasPreedit)
                    c.textFocus->composition(c.pendingPreedit, c.pendingCaret);
            }
            c.resetPendingInput();
            // Stale serials are accepted only within the same enabled editor
            // session; never transfer a previous focus's commit to a new leaf.
            if (serial == c.inputSerial) {
                c.inputMethodChange = true;
                c.syncInput(true);
                c.inputMethodChange = false;
            }
        }};
    zwp_text_input_v3_add_listener(textInput, &listener, this);
}
void WaylandConnection::syncInput(bool force) {
    if (!textInput || !textFocus)
        return;
    const auto state = textFocus->inputState();
    if (!state.editable) {
        if (inputEnabled) {
            zwp_text_input_v3_disable(textInput);
            zwp_text_input_v3_commit(textInput);
            ++inputSerial;
            inputEnabled = false;
            inputIdentity = nullptr;
            resetPendingInput();
        }
        return;
    }
    if (!inputEnabled || inputIdentity != state.identity || inputSession != state.session) {
        zwp_text_input_v3_enable(textInput);
        inputEnabled = true;
        inputIdentity = state.identity;
        inputSession = state.session;
        inputSessionSerial = inputSerial + 1;
        resetPendingInput();
        force = true;
    }
    const auto context = state.sensitive ? platform::SurroundingText{} :
        platform::surroundingText(state.text, state.anchor, state.caret);
    const auto& surrounding = context.text;
    const auto caret = context.caret, anchor = context.anchor;
    const Rect r = textFocus->caretRect();
    const std::string signature = surrounding + ":" + std::to_string(caret) + ":" + std::to_string(anchor) +
                                  ":" + std::to_string(r.x) + ":" + std::to_string(r.y) + ":" +
                                  std::to_string(state.sensitive);
    if (!force && signature == inputSignature)
        return;
    inputSignature = signature;
    zwp_text_input_v3_set_text_change_cause(textInput, inputMethodChange
                                                           ? ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_INPUT_METHOD
                                                           : ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_OTHER);
    zwp_text_input_v3_set_surrounding_text(textInput, surrounding.c_str(), caret, anchor);
    zwp_text_input_v3_set_content_type(textInput,
                                       state.sensitive ? ZWP_TEXT_INPUT_V3_CONTENT_HINT_HIDDEN_TEXT |
                                                             ZWP_TEXT_INPUT_V3_CONTENT_HINT_SENSITIVE_DATA
                                                       : 0,
                                       state.sensitive ? ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_PASSWORD
                                                       : ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL);
    zwp_text_input_v3_set_cursor_rectangle(textInput, r.x, r.y, std::max(1, int(r.width)),
                                           std::max(1, int(r.height)));
    zwp_text_input_v3_commit(textInput);
    ++inputSerial;
}
WaylandConnection::WaylandConnection() {
    try {
        display = wl_display_connect(nullptr);
        if (!display)
            throw std::runtime_error("OneUI cannot connect to WAYLAND_DISPLAY");
        if (pipe2(wakeup, O_NONBLOCK | O_CLOEXEC) < 0)
            throw std::runtime_error("Wayland wake pipe failed");
        xkb = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        if (!xkb)
            throw std::runtime_error("Wayland xkb context creation failed");
        registry = wl_display_get_registry(display);
        static const wl_registry_listener listener{global, globalRemove};
        wl_registry_add_listener(registry, &listener, this);
        if (wl_display_roundtrip(display) < 0 || wl_display_roundtrip(display) < 0 || !compositor || !shm ||
            !shell)
            throw std::runtime_error("Wayland compositor lacks wl_compositor, wl_shm or xdg-shell");
        cursorTheme = wl_cursor_theme_load(nullptr, 24, shm);
        cursorSurface = wl_compositor_create_surface(compositor);
        installTextInput();
        if (dataManager && seat) {
            dataDevice = wl_data_device_manager_get_data_device(dataManager, seat);
            static const wl_data_device_listener dataListener{
                [](void *d, wl_data_device *, wl_data_offer *object) {
                    auto &c = *static_cast<WaylandConnection *>(d);
                    auto offer = std::make_unique<Offer>();
                    offer->object = object;
                    static const wl_data_offer_listener listener{
                        [](void *d, wl_data_offer *, const char *mime) {
                            auto &o = *static_cast<Offer *>(d);
                            o.utf8 |= !std::strcmp(mime, "text/plain;charset=utf-8");
                            o.plain |= !std::strcmp(mime, "text/plain");
                        },
                        [](void *, wl_data_offer *, uint32_t) {}, [](void *, wl_data_offer *, uint32_t) {}};
                    wl_data_offer_add_listener(object, &listener, offer.get());
                    c.offers[object] = std::move(offer);
                },
                [](void *d, wl_data_device *, uint32_t serial, wl_surface *, wl_fixed_t, wl_fixed_t,
                   wl_data_offer *object) {
                    // Drag and drop is not advertised in this backend. Reject
                    // its offer explicitly and do not retain it as a selection.
                    if (!object) return;
                    auto& c = *static_cast<WaylandConnection*>(d);
                    wl_data_offer_accept(object, serial, nullptr);
                    wl_data_offer_destroy(object);
                    c.offers.erase(object);
                },
                [](void *, wl_data_device *) {},
                [](void *, wl_data_device *, uint32_t, wl_fixed_t, wl_fixed_t) {},
                [](void *, wl_data_device *) {},
                [](void *d, wl_data_device *, wl_data_offer *object) {
                    auto &c = *static_cast<WaylandConnection *>(d);
                    if (c.selection && c.selection != object) {
                        wl_data_offer_destroy(c.selection);
                        c.offers.erase(c.selection);
                    }
                    c.selection = object;
                }};
            wl_data_device_add_listener(dataDevice, &dataListener, this);
        }
    } catch (...) {
        release();
        throw;
    }
}
WaylandConnection::~WaylandConnection() { release(); }
void WaylandConnection::release() noexcept {
    for (auto &send : sends)
        ::close(send.fd);
    for (auto &item : offers)
        wl_data_offer_destroy(item.first);
    if (source)
        wl_data_source_destroy(source);
    if (textInput)
        zwp_text_input_v3_destroy(textInput);
    if (dataDevice) {
        if (wl_data_device_get_version(dataDevice) >= WL_DATA_DEVICE_RELEASE_SINCE_VERSION)
            wl_data_device_release(dataDevice);
        else wl_data_device_destroy(dataDevice);
    }
    if (keyboardObject)
        releaseKeyboard(keyboardObject);
    if (pointerObject)
        releasePointer(pointerObject);
    if (seat) {
        if (wl_seat_get_version(seat) >= WL_SEAT_RELEASE_SINCE_VERSION)
            wl_seat_release(seat);
        else wl_seat_destroy(seat);
    }
    if (textManager)
        zwp_text_input_manager_v3_destroy(textManager);
    if (dataManager)
        wl_data_device_manager_destroy(dataManager);
    if (cursorSurface)
        wl_surface_destroy(cursorSurface);
    if (cursorTheme)
        wl_cursor_theme_destroy(cursorTheme);
    for (auto &item : outputs)
        wl_output_destroy(item.second->object);
#ifdef ONEUI_HAVE_FRACTIONAL_SCALE
    if (fractionalManager)
        wp_fractional_scale_manager_v1_destroy(fractionalManager);
#endif
    if (viewporter)
        wp_viewporter_destroy(viewporter);
    if (shell)
        xdg_wm_base_destroy(shell);
    if (shm)
        wl_shm_destroy(shm);
    if (compositor)
        wl_compositor_destroy(compositor);
    if (registry)
        wl_registry_destroy(registry);
    if (display)
        wl_display_disconnect(display);
    if (keyState)
        xkb_state_unref(keyState);
    if (keymap)
        xkb_keymap_unref(keymap);
    if (xkb)
        xkb_context_unref(xkb);
    ::close(wakeup[0]);
    ::close(wakeup[1]);
}
std::unique_ptr<Window> WaylandConnection::create(WindowOptions o) {
    return std::make_unique<WaylandWindow>(*this, std::move(o));
}
std::vector<MonitorInfo> WaylandConnection::monitors() {
    std::vector<MonitorInfo> result;
    for (auto &item : outputs)
        result.push_back(item.second->info);
    return result;
}
void WaylandConnection::setClipboard(const std::wstring &value) {
    if (!dataDevice || !serial)
        throw std::runtime_error("Wayland clipboard requires a seat and a user-input serial");
    auto bytes = unicode::toUtf8(value);
    if (bytes.size() > 16u * 1024u * 1024u)
        throw std::runtime_error("Wayland clipboard exceeds 16 MiB");
    if (source)
        wl_data_source_destroy(source);
    ownedText = std::move(bytes);
    source = wl_data_device_manager_create_data_source(dataManager);
    static const wl_data_source_listener listener{
        [](void *, wl_data_source *, const char *) {},
        [](void *d, wl_data_source *, const char *, int32_t fd) {
            auto &c = *static_cast<WaylandConnection *>(d);
            if (c.sends.size() >= 16 || fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK) < 0) {
                ::close(fd);
                return;
            }
            c.sends.push_back({fd, c.ownedText, 0, platform::DesktopWindow::nowMs() + 5000});
        },
        [](void *d, wl_data_source *source) {
            auto &c = *static_cast<WaylandConnection *>(d);
            if (c.source == source)
                c.source = nullptr;
            wl_data_source_destroy(source);
        },
        [](void *, wl_data_source *) {},
        [](void *, wl_data_source *) {},
        [](void *, wl_data_source *, uint32_t) {}};
    wl_data_source_add_listener(source, &listener, this);
    wl_data_source_offer(source, "text/plain;charset=utf-8");
    wl_data_source_offer(source, "text/plain");
    wl_data_device_set_selection(dataDevice, source, serial);
    wl_display_flush(display);
}
std::wstring WaylandConnection::clipboard() {
    if (source)
        return unicode::fromUtf8(ownedText);
    auto it = offers.find(selection);
    if (it == offers.end() || (!it->second->utf8 && !it->second->plain))
        return {};
    int fds[2];
    if (pipe2(fds, O_CLOEXEC) < 0)
        throw std::runtime_error("Wayland clipboard pipe failed");
    // Only our reader is nonblocking. Other applications own the writer and
    // may legitimately use blocking writes for selections larger than a pipe.
    if (fcntl(fds[0], F_SETFL, O_NONBLOCK) < 0) {
        ::close(fds[0]);
        ::close(fds[1]);
        throw std::runtime_error("Wayland clipboard reader configuration failed");
    }
    wl_data_offer_receive(selection, it->second->utf8 ? "text/plain;charset=utf-8" : "text/plain", fds[1]);
    ::close(fds[1]);
    wl_display_flush(display);
    std::string value;
    char bytes[4096];
    const double deadline = platform::DesktopWindow::nowMs() + 1500;
    bool done = false;
    while (platform::DesktopWindow::nowMs() < deadline) {
        const auto n = ::read(fds[0], bytes, sizeof(bytes));
        if (n == 0) {
            done = true;
            break;
        }
        if (n > 0) {
            value.append(bytes, n);
            if (value.size() > 16 * 1024 * 1024)
                break;
        } else if (errno != EAGAIN && errno != EINTR)
            break;
        else {
            pollfd p{fds[0], POLLIN, 0};
            poll(&p, 1, 20);
        }
    }
    ::close(fds[0]);
    if (!done)
        throw std::runtime_error("Wayland clipboard transfer timed out or exceeded 16 MiB");
    return unicode::fromUtf8(value);
}
void WaylandConnection::flushClipboardSends() {
    for (auto it = sends.begin(); it != sends.end();) {
        bool finished = it->offset == it->text.size() || platform::DesktopWindow::nowMs() >= it->deadline;
        if (!finished) {
            const auto n = writePipe(it->fd, it->text.data() + it->offset,
                                     std::min<std::size_t>(65536, it->text.size() - it->offset));
            if (n > 0)
                it->offset += n;
            else if (n < 0 && errno != EAGAIN && errno != EINTR)
                finished = true;
            finished |= it->offset == it->text.size();
        }
        if (finished) {
            ::close(it->fd);
            it = sends.erase(it);
        } else
            ++it;
    }
}
void WaylandConnection::pump(int timeout) {
    char bytes[128];
    while (::read(wakeup[0], bytes, sizeof(bytes)) > 0) {
    }
    if (wl_display_dispatch_pending(display) < 0)
        throw std::runtime_error("Wayland connection lost");
    auto snapshot = windows;
    for (auto pair : snapshot) {
        if (!windows.count(pair.first))
            continue;
        pair.second->dispatchWork();
        if (!windows.count(pair.first))
            continue;
        pair.second->paint();
        if (pair.second->hasWork())
            timeout = std::min(timeout, 16);
    }
    syncInput();
    flushClipboardSends();
    for (const auto &send : sends)
        timeout = std::min(timeout, std::max(0, int(send.deadline - platform::DesktopWindow::nowMs())));
    if (repeatedKey && repeatRate > 0) {
        const auto now = platform::DesktopWindow::nowMs();
        if (now >= repeatAt) {
            keyboard(repeatedKey, true, true);
            repeatAt = now + 1000.0 / repeatRate;
        }
        timeout = std::min(timeout, std::max(1, int(repeatAt - now)));
    }
    if (windows.empty())
        return;
    while (wl_display_prepare_read(display) != 0) {
        if (wl_display_dispatch_pending(display) < 0)
            throw std::runtime_error("Wayland dispatch failed");
    }
    const bool blocked = wl_display_flush(display) < 0 && errno == EAGAIN;
    std::vector<pollfd> fds{
        {wl_display_get_fd(display), static_cast<short>(POLLIN | (blocked ? POLLOUT : 0)), 0},
        {wakeup[0], POLLIN, 0}};
    for (const auto &send : sends)
        fds.push_back({send.fd, POLLOUT, 0});
    int result = poll(fds.data(), fds.size(), timeout);
    if (result > 0 && (fds[0].revents & POLLIN)) {
        if (wl_display_read_events(display) < 0)
            throw std::runtime_error("Wayland read failed");
    } else
        wl_display_cancel_read(display);
    if (result < 0 && errno != EINTR)
        throw std::runtime_error("Wayland poll failed");
    if (fds[0].revents & (POLLHUP | POLLERR))
        throw std::runtime_error("Wayland compositor disconnected");
}
} // namespace
std::shared_ptr<Connection> waylandConnection() { return std::make_shared<WaylandConnection>(); }
} // namespace oneui::linux_platform
