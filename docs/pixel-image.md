# Owned pixel images

`PixelImage` retains immutable RGBA/BGRA pixels and one renderer resource. The
caller must keep the allocation immutable until its original owner is released.
`makeOwned` checks dimensions, stride, overflow and the exact last-row bound;
trailing padding is not required. It does not copy the pixel allocation.

`drawPixelImage` uses the optional `PixelImageCanvas` interface. The existing
`Canvas` vtable is unchanged, and custom canvases continue through `drawPixels`.
The Skia backend creates one raster image per immutable image, with
`SkData::MakeWithProc` and `SkImages::RasterFromData`. Recreating a canvas during
a repaint preserves that image identity. This allows Skia to reuse its resources;
it does not promise that a GPU upload never occurs after context loss or eviction.
The existing premultiplied-alpha convention and Mitchell sampling are preserved.

Renderer resources retain the original pixel owner, never their `PixelImage`,
which would create an ownership cycle. Factories and losing concurrent resources
are created/released outside the resource mutex. Only one backend resource is
retained per image; a different backend can create an uncached resource.

`RealtimeFrameView` keeps the image with its immutable frame. `submitDamage`
first creates a mutable copy, retires the cached image, and continues through the
raw-pixel path. An outstanding renderer image keeps the old immutable pixels
alive, so applying damage cannot mutate an already submitted draw. Original-owner
callbacks remain outside the view mutex. The private stored-frame definition
moves into its implementation; the view still stores one shared pointer and its
public object layout is unchanged.

This is an independently implemented image-lifetime optimization. The comparison
reference is RustDesk `23e5bb471a040d62f0a6ad52591fb30784265b73`,
`flutter/lib/utils/image.dart`: decoding produces a `ui.Image`, then the painter
reuses that image. No RustDesk source was copied or translated into OneUI.

`oneui_pixel_image_tests` checks the actual Skia Canvas extension, unchanged
pixel addresses and image identity across canvas instances, RGBA/BGRA colors,
padded stride, last-row bounds, concurrent resource publication, delayed owner
release, damage isolation, reentrant release callbacks and custom-Canvas fallback.
The existing realtime-frame behavior test remains in place.

The remote project's isolated Android SDK builder checks the allocated sections
of the old view/canvas objects against the frozen SDK before replacing them. All
three Android ABIs compile and link the tests; only x86_64 has been executed on
the owned `WykFullAndroid` emulator. The shared SDK and website packages have not
been replaced. Public-relay rendering observations have not qualified this change
as an FPS improvement or as RustDesk performance parity.

Pixel drawing moved from `skia_canvas.cpp` (1109 to 1093 physical lines) into
`skia_pixel_image.cpp` (70 lines). The canvas keeps adapters; image ownership and
renderer-resource lifetime now have their own boundary. Other canvas text,
gradient and shadow responsibilities remain in the existing file.

## Android picture recording and raster ownership

Android now records an immutable `SkPicture` on the widget owner thread. A
dedicated `PictureRasterWorker` owns EGL/Ganesh, including creation, replay,
swapping, reset and backend destruction. Its EGL context stays current on that
exclusive thread. Widget state and application services remain on the UI thread;
the worker receives pictures and source stamps, never Widget pointers/callbacks.

One picture may be in flight and one pending; a newer submission replaces the
pending picture. Released pictures can invoke pixel-owner callbacks, so retirement
occurs outside the queue mutex. Lifecycle operations discard pending work and
wait for in-flight work before returning. Each attach/reset advances a generation;
old completions do not change the new generation's counters. A rendering failure
latches the surface unavailable and discards its pending picture until recovery.

`SkiaSurface::present()` now acknowledges queue acceptance. The private host's
`frames` field counts queued pictures, and remains separate from actual swaps.
`latestThreadRenderStatistics()` observes the most recently painted surface on
the calling UI thread, without retaining the surface. Counters reset per generation,
except the backend's context/resize totals. Non-Android callers receive no snapshot.

`FramePaintScope` records only nonzero application frame IDs actually drawn through
`RealtimeFrameView` and intersecting the current clip. Frame ID zero opts static
images out. At most 16 simultaneous sources are recorded, with explicit overflow.
Source identities survive frame replacement but are renewed after clearing a view.
`sourceFrameChangesSwapped` compares source/frame pairs in adjacent successfully
swapped pictures. It includes reappearing sources and is not a globally unique
frame count. Successful EGL swaps are not compositor presentation fences.

The reference remains RustDesk's bounded RGBA notification and Flutter image
lifetime. The auxiliary Flutter source at engine
`69c8c61792f04cc809dfef0c910414fb9afc06cd`, `Shell::OnAnimatorDraw`, posts drawing
to its raster task runner. That source explains the thread boundary; the actual
RustDesk APK's Flutter engine version has not been verified. This OneUI worker is
an independent implementation, without copied RustDesk or Flutter source.

Native Android tests check slow raster backpressure, latest-picture retention,
duplicate repaint accounting, controlled failure, generation retirement,
pixel-release callback reentry, backend thread/destruction ownership, nested and
clipped paint traces, and more than 16 successive (non-concurrent) frame sources.
All three Android ABIs compile/link these tests; only x86_64 executes on the owned
emulator. A public HEVC candidate has additionally passed real input and Home
recovery with a fresh consumed source ID reaching a post-foreground native swap.
The public website and shared SDK remain unchanged; full RustDesk performance
and physical-device acceptance are still unproven.

The former 220-line `skia_surface_android.cpp` is now a 90-line recording adapter.
Its existing EGL lifecycle moved to `egl_picture_backend.cpp` (151 lines); bounded
picture scheduling and completion ownership reside in `picture_raster_worker.cpp`
(157 lines). The large shared canvas does not acquire these responsibilities.
