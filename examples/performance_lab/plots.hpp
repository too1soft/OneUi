#pragma once
#include "telemetry.hpp"
#include <array>
#include <cmath>
#include <functional>
#include <oneui/oneui.h>
#include "platform/shared/circle_mesh_experiment.h"
using namespace oneui;
constexpr Color color(unsigned hex) {
  return Color(static_cast<unsigned char>(hex >> 16),
               static_cast<unsigned char>(hex >> 8),
               static_cast<unsigned char>(hex));
}
constexpr Color BG = color(0x101517), PANEL = color(0x171e21),
                LINE = color(0x2b3539), TEXT = color(0xe5ece8),
                MUTED = color(0x96aaa4), ACCENT = color(0xc4ed87),
                TEAL = color(0x61c9bb), ORANGE = color(0xecad72);
constexpr float TAU = 6.2831853071795864769f;
enum class ParticleMode { Reference, Precomputed, Batch, Combined, Mesh };
inline constexpr ParticleMode DEFAULT_PARTICLE_MODE = ParticleMode::Mesh;
inline const char* particleModeName(ParticleMode mode) {
  switch (mode) {
    case ParticleMode::Reference: return "reference";
    case ParticleMode::Precomputed: return "precomputed";
    case ParticleMode::Batch: return "batch";
    case ParticleMode::Combined: return "combined";
    case ParticleMode::Mesh: return "mesh";
  }
  return "reference";
}
struct ParticleConstants {
  float radius, angle, velocity, size;
  Color ink{0, 0, 0, 0};
};
inline ParticleConstants particleConstants(int i) {
  const float seed = float(i), v = seed * .618034f, r = std::sqrt(v - std::floor(v));
  return {r, seed * 2.399963f, .25f + (1 - r) * .7f, i % 13 == 0 ? 3.f : 1.6f,
          i % 11 == 0 ? ORANGE : i % 3 == 0 ? ACCENT : color(0x4b9f98)};
}
struct Model {
  ParticleMode particleMode = DEFAULT_PARTICLE_MODE;
  std::uint64_t meshDraws = 0, meshFallbacks = 0, meshVertices = 0;
  const char* meshReason = "not-requested";
  const char* meshLastFallback = "none";
  int load = 1, tab = 0, selected = 0;
  bool running = true;
  float time = 0, speed = 1;
  Telemetry telemetry;
  int particles() const { return std::array<int, 3>{500, 2500, 10000}[load]; }
  int points() const { return std::array<int, 3>{360, 1200, 4000}[load]; }
  int rows() const { return std::array<int, 3>{1000, 10000, 100000}[load]; }
  const wchar_t *loadName() const {
    return std::array<const wchar_t *, 3>{L"轻量", L"标准", L"高压"}[load];
  }
};
inline void stroke(Canvas &c, const std::vector<Point> &pts, Color ink,
                   float width = 1) {
  CanvasPath p;
  for (size_t i = 0; i < pts.size(); ++i) {
    if (i == 0)
      p.moveTo(pts[i]);
    else
      p.lineTo(pts[i]);
  }
  c.strokePath(p, ink, width);
}
enum class PlotKind { Signals, Particles, Spectrum, Frames };
class Plot final : public Widget {
  Model &m;
  PlotKind kind;
  float pointer = -1;
  std::vector<ParticleConstants> particleConstants_;
  std::vector<RoundedRectFill> particleRects_;

public:
  Plot(Model &model, PlotKind k) : m(model), kind(k) {}
  bool onMouseMove(const MouseEvent &e) override {
    if (kind == PlotKind::Signals) {
      pointer = contains(e.position) ? e.position.x : -1;
      invalidate();
    }
    return false;
  }
  void paint(Canvas &c) override {
    const auto b = frame();
    if (b.width <= 0 || b.height <= 0)
      return;
    const float x = b.x, y = b.y, w = b.width, h = b.height, t = m.time;
    c.save();
    c.clipRect(b);
    if (kind == PlotKind::Signals) {
      for (int i = 0; i < 5; ++i)
        c.fillRect({x, y + h * i / 4, w, 1}, LINE);
      for (int i = 0; i < 9; ++i)
        c.fillRect({x + w * i / 8, y, 1, h}, color(0x222d30));
      for (int ch = 0; ch < 3; ++ch) {
        std::vector<Point> points;
        points.reserve(m.points());
        for (int i = 0; i < m.points(); ++i) {
          float u = float(i) / (m.points() - 1), z = u * 12 + t;
          float v = ch == 0 ? 0.30f + 0.15f * std::sin(z * 1.4f) +
                                  0.07f * std::cos(z * 3.8f)
                    : ch == 1 ? 0.52f + 0.13f * std::sin(z * 1.3f + 2.f) +
                                    0.035f * std::cos(z * 7.f)
                              : 0.75f + 0.07f * std::sin(z * 2.4f) +
                                    0.035f * std::sin(z * 5.2f);
          points.push_back({x + u * w, y + v * h});
        }
        stroke(c, points, std::array<Color, 3>{ACCENT, TEAL, ORANGE}[ch],
               ch == 0 ? 2.5f : 1.7f);
      }
      if (pointer >= x && pointer <= x + w)
        c.fillRect({pointer, y, 1, h}, color(0x859991));
    } else if (kind == PlotKind::Particles) {
      float cx = x + w * .5f, cy = y + h * .5f;
      for (int j = 1; j < 4; ++j) {
        std::vector<Point> p;
        for (int i = 0; i <= 120; ++i) {
          float a = float(i) / 120 * TAU;
          p.push_back({cx + std::cos(a) * w * .14f * j,
                       cy + std::sin(a) * h * .14f * j});
        }
        stroke(c, p, color(0x283437));
      }
      if (m.particleMode == ParticleMode::Reference) {
        // Keep the original loop as the A/B reference, outside optimized-loop branches.
        for (int i = 0; i < m.particles(); ++i) {
          float seed = float(i), v = seed * .618034f,
                r = std::sqrt(v - std::floor(v)),
                a = seed * 2.399963f + t * (.25f + (1 - r) * .7f),
                swirl = std::sin(a * 3 + t) * .035f;
          float xx = cx + std::cos(a) * (r + swirl) * w * .46f,
                yy = cy + std::sin(a) * r * h * .45f,
                sz = i % 13 == 0 ? 3.f : 1.6f;
          c.fillRect({xx, yy, sz, sz},
                     i % 11 == 0  ? ORANGE
                     : i % 3 == 0 ? ACCENT
                                  : color(0x4b9f98),
                     sz / 2);
        }
      } else {
        const bool precompute = m.particleMode == ParticleMode::Precomputed ||
                                m.particleMode == ParticleMode::Combined || m.particleMode == ParticleMode::Mesh;
        const bool batch = m.particleMode == ParticleMode::Batch ||
                           m.particleMode == ParticleMode::Combined || m.particleMode == ParticleMode::Mesh;
        if (precompute && particleConstants_.size() != static_cast<std::size_t>(m.particles())) {
          particleConstants_.resize(m.particles());
          for (int i = 0; i < m.particles(); ++i)
            particleConstants_[i] = particleConstants(i);
        }
        if (batch) particleRects_.resize(m.particles());
        for (int i = 0; i < m.particles(); ++i) {
          if (precompute) {
            const auto& p = particleConstants_[i];
            const float a = p.angle + t * p.velocity, swirl = std::sin(a * 3 + t) * .035f;
            const float xx = cx + std::cos(a) * (p.radius + swirl) * w * .46f,
                        yy = cy + std::sin(a) * p.radius * h * .45f;
            if (batch) particleRects_[i] = {{xx, yy, p.size, p.size}, p.ink, p.size / 2};
            else c.fillRect({xx, yy, p.size, p.size}, p.ink, p.size / 2);
            continue;
          }
          float seed = float(i), v = seed * .618034f,
                r = std::sqrt(v - std::floor(v)),
                a = seed * 2.399963f + t * (.25f + (1 - r) * .7f),
                swirl = std::sin(a * 3 + t) * .035f;
          float xx = cx + std::cos(a) * (r + swirl) * w * .46f,
                yy = cy + std::sin(a) * r * h * .45f,
                sz = i % 13 == 0 ? 3.f : 1.6f;
          const Color ink = i % 11 == 0 ? ORANGE : i % 3 == 0 ? ACCENT : color(0x4b9f98);
          if (batch) particleRects_[i] = {{xx, yy, sz, sz}, ink, sz / 2};
          else c.fillRect({xx, yy, sz, sz}, ink, sz / 2);
        }
        bool meshDrawn = false;
        if (m.particleMode == ParticleMode::Mesh) {
          const auto result = oneui::rendering::experimental::tryCircleMesh(c,particleRects_.data(),particleRects_.size());
          m.meshReason=result.reason;meshDrawn=result.drawn;
          if(meshDrawn){m.meshDraws+=result.draws;m.meshVertices+=result.vertices;}
          else {++m.meshFallbacks;m.meshLastFallback=result.reason;}
        }
        if (batch && !meshDrawn) c.fillRoundedRects(particleRects_.data(), particleRects_.size());
      }
    } else if (kind == PlotKind::Spectrum) {
      for (int i = 0; i < 28; ++i) {
        float u = float(i) / 28,
              v = .12f + .75f * (std::sin(u * 8 - t * 1.7f) * .5f + .5f) *
                             (.7f + .3f * std::cos(t * 2 + u * 17));
        c.fillRect({x + u * w, y, w / 28 - 2, h}, color(0x23312d), 1);
        c.fillRect({x + u * w, y + h * (1 - v), w / 28 - 2, h * v},
                   i > 20 ? TEAL : ACCENT, 1);
      }
    } else {
      for (float v : {16.667f, 33.333f})
        c.fillRect({x, y + h * (1 - v / 50), w, 1}, color(0x394239));
      int i = 0;
      for (double dt : m.telemetry.recent) {
        float v = std::min(float(dt / 50), 1.f);
        c.fillRect({x + i * w / 240, y + h * (1 - v),
                    std::max(w / 240 - 1, 1.f), h * v},
                   dt > 33.34 ? ORANGE : TEAL);
        ++i;
      }
    }
    c.restore();
  }
};
