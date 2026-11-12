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
