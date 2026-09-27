#include "plots.hpp"
#include "support/recording_canvas.h"
#include <GL/gl.h>
#include <iostream>
#include <iterator>
#include <stdexcept>

using oneui::test_support::RecordingCanvas;
static void require(bool value, const char* message) { if(!value)throw std::runtime_error(message); }
static bool same(Color a, Color b) { return a.r==b.r && a.g==b.g && a.b==b.b && a.a==b.a; }
static bool same(Rect a, Rect b) { return a.x==b.x && a.y==b.y && a.width==b.width && a.height==b.height; }
static void geometryTests() {
  Model m;
  Plot plot(m,PlotKind::Particles);
  std::size_t cases=0;
  // Reuse the plot across size/load/mode changes, including shrinking a cache.
  for(int load : {0,2,1,0,2}) for(float time : {0.f,1.25f,300.f,3600.f})
    for(Rect frame : {Rect{10.25f,20.5f,1100,600},Rect{-4.25f,3.5f,390,230}}) {
      m.load=load;m.time=time;plot.setFrame(frame);
      m.particleMode=ParticleMode::Reference;
      RecordingCanvas original;plot.paint(original);
      require(original.fillRects.size()==static_cast<std::size_t>(m.particles()),"Particle count changed");
      for(auto mode : {ParticleMode::Precomputed,ParticleMode::Batch,ParticleMode::Combined}) {
        m.particleMode=mode;RecordingCanvas actual;plot.paint(actual);
        require(actual.fillRects.size()==original.fillRects.size(),"Batch fallback lost particles");
        for(std::size_t i=0;i<original.fillRects.size();++i) {
          const auto& a=original.fillRects[i];const auto& b=actual.fillRects[i];
          require(same(a.rect,b.rect) && same(a.color,b.color) && a.radius==b.radius,"Geometry/order/color changed");
        }
        require(actual.saves==original.saves && actual.restores==original.restores,"Canvas state changed");
        ++cases;
      }
    }
  RecordingCanvas empty;empty.fillRoundedRects(nullptr,0);
  require(empty.fillRects.empty(),"Empty batch drew content");
  std::cout<<"Exact geometry/fallback parity: "<<cases<<" cases\n";
}
class ParticleScene final : public Widget {
public:
  Model model;
  Plot plot{model,PlotKind::Particles};
  void paint(Canvas& canvas) override {
    const auto b=frame();canvas.fillRect(b,BG);
    plot.setFrame({b.x+10.25f,b.y+10.5f,b.width-20,b.height-20});plot.paint(canvas);
    // Exercise ordered source-over alpha and a fractional clip separately.
    const RoundedRectFill items[]={{{15.25f,20.5f,55,31},{255,90,20,100},8},
      {{32,25.25f,30,40},{30,220,255,160},15},{{47,20,36,28},{120,190,20,255},0}};
    canvas.save();canvas.clipRect({20.5f,20.25f,45.75f,40});
    if(model.particleMode==ParticleMode::Batch || model.particleMode==ParticleMode::Combined)
      canvas.fillRoundedRects(items,std::size(items));
    else for(const auto& item:items)canvas.fillRect(item.rect,item.color,item.radius);
    canvas.restore();
  }
};
static std::vector<unsigned char> gpuPixels(Window& window) {
  require(wglGetCurrentContext()!=nullptr,"No current GL context for actual framebuffer readback");
  require(WindowFromDC(wglGetCurrentDC())==static_cast<HWND>(window.nativeHandle()),"Wrong GL window");
  RECT r{};GetClientRect(static_cast<HWND>(window.nativeHandle()),&r);
  std::vector<unsigned char> pixels(static_cast<std::size_t>(r.right)*r.bottom*4);
  GLint previousBuffer=0,previousPack=0;
  glGetIntegerv(GL_READ_BUFFER,&previousBuffer);glGetIntegerv(GL_PACK_ALIGNMENT,&previousPack);
  glReadBuffer(GL_FRONT);glPixelStorei(GL_PACK_ALIGNMENT,1);
  glReadPixels(0,0,r.right,r.bottom,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
  const auto error=glGetError();
  glReadBuffer(previousBuffer);glPixelStorei(GL_PACK_ALIGNMENT,previousPack);
  require(error==GL_NO_ERROR,"GL readback failed");
  std::size_t nonBackground=0;
  for(std::size_t i=0;i<pixels.size();i+=4)
    if(pixels[i]!=pixels[0] || pixels[i+1]!=pixels[1] || pixels[i+2]!=pixels[2])++nonBackground;
  require(nonBackground>500,"Framebuffer capture is empty/uniform");
  return pixels;
}
static std::vector<unsigned char> rasterPixels(Window& window,const std::filesystem::path& path) {
  require(window.captureFramePng(path.wstring()),"Raster PNG capture failed");
  std::ifstream file(path,std::ios::binary);
  return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
int main(int argc,char** argv) {
  try {
    const bool gpu=argc>1 && std::string(argv[1])=="--gpu";
    const auto output=std::filesystem::absolute(argc>2?argv[2]:"particle-test-output");
    std::filesystem::create_directories(output);
    geometryTests();
    SetEnvironmentVariableW(L"ONEUI_ENABLE_GPU",gpu?L"1":L"0");
    std::size_t cases=0;
    for(int width : {640,1320}) {
      auto window=Window::create(L"OneUI particle parity",width,600);
      auto scene=std::make_shared<ParticleScene>();scene->model.load=2;
      window->setContent(scene);window->initialize();window->show();
      for(float scale : {1.f,1.25f,1.5f}) for(float time : {0.f,1.25f,300.f}) {
        window->setContentScale(scale);scene->model.time=time;
        std::vector<unsigned char> reference;
        for(auto mode : {ParticleMode::Reference,ParticleMode::Precomputed,ParticleMode::Batch,ParticleMode::Combined}) {
          scene->model.particleMode=mode;window->requestRedraw();
          UpdateWindow(static_cast<HWND>(window->nativeHandle()));
          require(window->rendererInfo().backend==(gpu?RenderBackend::OpenGL:RenderBackend::Software),"Backend mismatch");
          const auto name=std::to_string(width)+"-"+std::to_string(scale)+"-"+std::to_string(time)+"-"+particleModeName(mode);
          const auto pixels=gpu?gpuPixels(*window):rasterPixels(*window,output/(name+".png"));
          if(mode==ParticleMode::Reference)reference=pixels;
          else {require(pixels==reference,"Rendered pixels differ");++cases;}
        }
      }
      window->close();
    }
    std::cout<<(gpu?"Actual OpenGL front buffer":"Raster PNG")<<" exact parity: "<<cases<<" cases; 100/125/150% content scale\n";
    return 0;
  } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
