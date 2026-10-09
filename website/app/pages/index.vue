<script setup lang="ts">
useSeoMeta({ title: 'OneUI · 原生桌面 UI 与预编译 SDK', description: '下载 OneUI 预编译 SDK，使用 C++、C 或 Rust 构建 Windows、Linux 和 macOS 桌面界面。版本文档、组件参考和接入示例。' })
const cmake = 'find_package(OneUI CONFIG REQUIRED)\nadd_executable(my_app main.cpp)\ntarget_link_libraries(my_app PRIVATE OneUI::oneui)'
const topics = [
  { title: '布局与页面', text: '从一行工具栏到完整工作区。用 Stack、Grid、SplitView 和声明式页面组合界面。', to: '/docs/52-layout-recipes-and-presets', icon: 'i-lucide-panels-top-left' },
  { title: '控件与交互', text: '表单、虚拟列表、树和表格，复用状态绑定、文字输入与统一样式。', to: '/docs/14-component-reference', icon: 'i-lucide-mouse-pointer-2' },
  { title: '桌面系统能力', text: '窗口、菜单、托盘、通知与停靠。按实际平台能力接入，清楚了解支持边界。', to: '/docs/sdk-platforms', icon: 'i-lucide-monitor' }
]
</script>

<template>
  <main id="main">
    <section class="hero wrap">
      <div class="hero-copy">
        <h1>专注你的应用，<br><span>界面交给 OneUI。</span></h1>
        <p>一个原生桌面 UI 框架，一套版本化 SDK。<br class="wide-only">用 C++、C 或 Rust 接入，下载后直接构建你的应用。</p>
        <div class="hero-actions"><NuxtLink class="button primary" to="/downloads">下载 DLL / SDK <UIcon name="i-lucide-arrow-down-to-line" /></NuxtLink><NuxtLink class="button secondary" to="/docs/sdk-quick-start">开始接入 <UIcon name="i-lucide-arrow-up-right" /></NuxtLink></div>
        <p class="hero-note">Windows · Win7 兼容构建 · Linux · macOS<br>当前提供预览版，各平台验收状态随版本公布。</p>
      </div>
      <div class="integration-preview" aria-label="SDK 接入流程">
        <div class="preview-title"><UIcon name="i-lucide-package-check" /><span>从 SDK 到你的应用</span><span class="tag">预编译</span></div>
        <div class="preview-step"><span class="step-index">1</span><div><strong>选择适合的版本</strong><p>系统、架构和链接方式，明确匹配。</p></div></div>
        <div class="preview-step"><span class="step-index">2</span><div><strong>锁定并共享缓存</strong><p>同一版本下载一次，多个项目复用。</p></div></div>
        <div class="preview-step"><span class="step-index">3</span><div><strong>构建自己的业务代码</strong><p>框架与底层依赖已预先编译。</p></div></div>
        <div class="preview-bottom"><UIcon name="i-lucide-file-check-2" />匹配头文件 · CMake 接入 · SHA256 校验</div>
      </div>
    </section>
    <section class="platform-strip"><div class="wrap"><span>同一套控件与布局</span><strong>Win32</strong><strong>Cocoa</strong><strong>X11 / Wayland</strong><NuxtLink to="/docs/sdk-platforms">查看支持矩阵 <UIcon name="i-lucide-arrow-right" /></NuxtLink></div></section>
    <section class="section wrap">
      <div class="section-heading"><h2>从基础控件，搭到完整桌面应用。</h2><NuxtLink to="/docs">浏览全部文档 <UIcon name="i-lucide-arrow-right" /></NuxtLink></div>
      <div class="topic-list"><NuxtLink v-for="topic in topics" :key="topic.title" :to="topic.to"><UIcon :name="topic.icon" /><h3>{{ topic.title }}</h3><p>{{ topic.text }}</p><span>阅读指南 <UIcon name="i-lucide-arrow-up-right" /></span></NuxtLink></div>
    </section>
    <section class="section integration-section wrap">
      <div><h2>一份 SDK，<br>接入熟悉的工具链。</h2><p>动态链接按需分发运行库，静态链接复用编译好的库。每个项目独立锁定版本，升级节奏由你决定。</p><NuxtLink class="text-link" to="/docs/sdk-quick-start">查看 C++ / C / Rust 接入 <UIcon name="i-lucide-arrow-right" /></NuxtLink></div>
      <CopyCode :code="cmake" label="CMakeLists.txt · 下载 SDK 后" />
    </section>
    <section class="closing wrap"><div><h2>先体验，再接入。</h2><p>OneUI 试验台集中展示布局、性能和桌面能力，对应示例与支持状态都在文档中。</p></div><NuxtLink class="button secondary" to="/docs/unified-lab-capabilities">查看试验台能力 <UIcon name="i-lucide-arrow-right" /></NuxtLink></section>
  </main>
</template>
