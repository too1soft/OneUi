<script setup lang="ts">
import {
  architectureRows,
  bindingRows,
  cApiGroups,
  cAbiRules,
  componentGroups,
  faqItems,
  layoutComponents,
  navItems,
  platformRows,
  quickStartSteps,
  reviewFindings,
  stats,
  styleProperties,
  stylePseudoStates,
  styleRows,
  testRows
} from '~/data/componentReference'

const pairColumns = [
  { accessorKey: 'name', header: '名称' },
  { accessorKey: 'detail', header: '说明' }
]

const rows = (items: readonly (readonly [string, string])[]) =>
  items.map(([name, detail]) => ({ name, detail }))

const apiColumns = [
  { accessorKey: 'api', header: 'API / 参数' },
  { accessorKey: 'detail', header: '用途与说明' }
]

useSeoMeta({
  title: 'OneUI 官方文档',
  description: 'OneUI 官方文档、完整组件 API、布局与样式指南、C ABI、跨语言调用和代码审查报告。'
})
</script>

<template>
  <div class="doc-shell min-h-screen text-slate-900">
    <header class="sticky top-0 z-40 border-b border-slate-200 bg-white/90 backdrop-blur">
      <UContainer class="flex h-16 items-center justify-between gap-4">
        <a href="#overview" class="flex items-center gap-3">
          <span class="grid size-9 place-items-center rounded-lg bg-slate-950 font-bold text-white">O</span>
          <span>
            <span class="block font-semibold leading-5">OneUI</span>
            <span class="block text-xs text-slate-500">官方文档站</span>
          </span>
        </a>

        <UNavigationMenu :items="navItems.slice(0, 5)" class="hidden xl:block" />

        <UButton
          color="neutral"
          variant="outline"
          icon="i-lucide-book-open"
          to="#component-api"
          label="组件 API"
          class="hidden sm:inline-flex"
        />
      </UContainer>
    </header>

    <UContainer class="grid gap-8 py-8 lg:grid-cols-[250px_minmax(0,1fr)]">
      <aside class="hidden lg:block">
        <div class="sticky top-24 space-y-3">
          <p class="px-3 text-xs font-semibold uppercase tracking-wide text-slate-500">
            文档导航
          </p>
          <UNavigationMenu orientation="vertical" :items="navItems" class="w-full" />
        </div>
      </aside>

      <main class="doc-prose min-w-0 space-y-8">
        <section id="overview" class="overflow-hidden rounded-lg border border-slate-200 bg-white shadow-sm">
          <div class="doc-hero-grid grid gap-8 p-7 lg:grid-cols-[1fr_360px] lg:p-10">
            <div>
              <UBadge color="primary" variant="soft" label="审查日期：2026-05-28" />
              <h1 class="mt-5 text-4xl font-bold tracking-normal text-slate-950 sm:text-5xl">
                OneUI 官方文档
              </h1>
              <p class="mt-5 max-w-3xl text-lg leading-8 text-slate-600">
                OneUI 是一个 C++17 原生自绘桌面 UI 框架，复用 Skia、控件和布局，经 Win32、X11、Wayland 或 Cocoa 接入系统。Windows 为产品主线；Linux 已在 WSLg 构建运行，Cocoa 尚待 Mac 构建。下面明确区分实现、构建和原生验收，不提前承诺全面平台支持。
              </p>
              <div class="mt-7 flex flex-wrap gap-3">
                <UButton label="快速开始" to="#quick-start" icon="i-lucide-rocket" />
                <UButton label="查看完整组件" to="#component-api" color="neutral" variant="outline" icon="i-lucide-boxes" />
              </div>
            </div>

            <UCard>
              <template #header>
                <div class="flex items-center gap-2">
                  <UIcon name="i-lucide-panels-top-left" class="size-5 text-primary" />
                  <span class="font-semibold">当前状态</span>
                </div>
              </template>
              <div class="space-y-4">
                <div v-for="stat in stats" :key="stat.label">
                  <p class="text-sm font-medium text-slate-950">
                    {{ stat.label }}
                  </p>
                  <p class="mt-1 text-sm leading-6 text-slate-500">
                    {{ stat.value }}
                  </p>
                </div>
              </div>
            </UCard>
          </div>
        </section>

        <section id="platforms" class="rounded-lg border border-slate-200 bg-white p-6 shadow-sm">
          <h2 class="text-2xl font-semibold">平台实现与验收矩阵</h2>
          <UTable class="mt-5" :data="rows(platformRows)" :columns="pairColumns" />
          <p class="mt-3 text-sm text-slate-600">
            Linux 使用 ONEUI_LINUX_BACKEND=auto|x11|wayland。Wayland 的位置、激活、输入法和剪贴板权限有明确限制；调用前通过 C++、C 或 Rust 的窗口能力接口查询。托盘、原生文件对话框、系统无障碍桥和签名公证不属于本轮新增平台能力。
          </p>
        </section>

        <section id="quick-start" class="rounded-lg border border-slate-200 bg-white p-6 shadow-sm">
          <UBadge color="primary" variant="soft" label="Getting Started" />
          <h2 class="mt-3 text-2xl font-semibold">
            快速开始
          </h2>
          <p class="mt-3 text-slate-600">
            新手可以把 OneUI 想成“控件树 + 布局容器 + 状态绑定 + 平台窗口”。先创建窗口，再把控件放进布局容器，最后挂到窗口并运行消息循环。
          </p>
          <UTable class="mt-5" :data="rows(quickStartSteps)" :columns="pairColumns" />
          <pre class="mt-5 rounded-lg bg-slate-950 p-4 text-sm text-slate-100"><code>#include &lt;oneui/oneui.h&gt;

int main() {
  oneui::WindowOptions options;
  options.title = L"OneUI App";
  options.width = 960;
  options.height = 640;
  options.visible = true;

  auto root = std::make_shared&lt;oneui::Stack&gt;(oneui::StackDirection::Column);
  root-&gt;setPadding(oneui::Insets{20.0f});
  root-&gt;setGap(12.0f);

  auto title = std::make_shared&lt;oneui::Label&gt;(L"欢迎使用 OneUI");
  auto button = std::make_shared&lt;oneui::Button&gt;(L"保存");
  button-&gt;setVariant(oneui::ButtonVariant::Primary);

  root-&gt;add(title);
  root-&gt;add(button);

  auto window = oneui::Window::create(options);
  window-&gt;setContent(root);
  return window-&gt;run();
}</code></pre>
        </section>

        <section id="review" class="rounded-lg border border-slate-200 bg-white p-6 shadow-sm">
          <UBadge color="primary" variant="soft" label="Code Review" />
          <h2 class="mt-3 text-2xl font-semibold">
            代码审查报告
          </h2>
          <p class="mt-3 text-slate-600">
            当前代码的分层是清楚的：公开头文件在 <code>include/oneui</code>，核心控件在 <code>src/core</code>，平台窗口在 <code>src/platform</code>，跨语言边界在 <code>src/capi</code>。主要风险不是架构混乱，而是 MVP 阶段 API 扩张较快，文档、聚合头、C ABI 覆盖和测试运行环境需要继续同步。
          </p>
          <div class="mt-5 space-y-3">
            <UCard v-for="finding in reviewFindings" :key="finding.title">
              <div class="flex flex-col gap-3 sm:flex-row sm:items-start">
                <UBadge :color="finding.color" variant="soft" :label="finding.level" class="w-fit" />
                <div>
                  <h3 class="font-semibold">
                    {{ finding.title }}
                  </h3>
                  <p class="mt-1 text-sm leading-6 text-slate-600">
                    {{ finding.body }}
                  </p>
                </div>
              </div>
            </UCard>
          </div>
        </section>

        <section id="architecture" class="rounded-lg border border-slate-200 bg-white p-6 shadow-sm">
          <h2 class="text-2xl font-semibold">
            架构模型
          </h2>
          <p class="mt-3 text-slate-600">
            OneUI 的核心不是浏览器 DOM，而是桌面 UI 框架常见的控件对象树。每个 Widget 知道自己的矩形和行为，布局容器负责给子控件分配矩形，Canvas 负责绘制，Window 负责平台消息循环。
          </p>
          <UTable class="mt-5" :data="rows(architectureRows)" :columns="pairColumns" />
        </section>

        <section id="component-api" class="space-y-6">
          <div class="rounded-lg border border-slate-200 bg-white p-6 shadow-sm">
            <UBadge color="primary" variant="soft" label="Component Reference" />
            <h2 class="mt-3 text-2xl font-semibold">
              完整组件 API
            </h2>
            <p class="mt-3 text-slate-600">
              下面按用途列出当前公开头文件中的组件。每个组件都包含入口头文件、用途、构造方式、参数/API、基础用法和注意事项。特别注意：少数组件已经有单独头文件和实现，但还没有被 <code>&lt;oneui/oneui.h&gt;</code> 聚合，使用时需要直接 include。
            </p>
          </div>

          <section
            v-for="group in componentGroups"
            :id="`component-${group.title}`"
            :key="group.title"
            class="rounded-lg border border-slate-200 bg-white p-6 shadow-sm"
          >
            <h3 class="text-xl font-semibold">
              {{ group.title }}
            </h3>
            <p class="mt-2 text-slate-600">
              {{ group.description }}
            </p>

            <div class="mt-5 space-y-5">
              <UCard v-for="component in group.items" :key="component.name" class="component-card">
                <template #header>
                  <div class="flex flex-col gap-2 md:flex-row md:items-start md:justify-between">
                    <div>
                      <h4 class="text-lg font-semibold">
                        {{ component.name }}
                      </h4>
                      <p class="mt-1 text-sm leading-6 text-slate-600">
                        {{ component.purpose }}
                      </p>
                    </div>
                    <UBadge color="neutral" variant="soft" :label="component.constructor" class="max-w-full text-left" />
                  </div>
                </template>

                <div class="space-y-4">
                  <div>
                    <p class="text-xs font-semibold uppercase tracking-wide text-slate-500">
                      头文件
                    </p>
                    <pre class="mt-2 rounded-md bg-slate-950 p-3 text-sm text-slate-100"><code>{{ component.include }}</code></pre>
                  </div>

                  <div>
                    <p class="text-xs font-semibold uppercase tracking-wide text-slate-500">
                      API 与参数
                    </p>
                    <UTable class="mt-2" :data="component.api.map(([api, detail]) => ({ api, detail }))" :columns="apiColumns" />
                  </div>

                  <div>
                    <p class="text-xs font-semibold uppercase tracking-wide text-slate-500">
                      用法示例
                    </p>
                    <pre class="mt-2 rounded-md bg-slate-950 p-3 text-sm text-slate-100"><code>{{ component.usage }}</code></pre>
                  </div>

                  <ul class="list-disc space-y-1 pl-5 text-sm leading-6 text-slate-600">
                    <li v-for="note in component.notes" :key="note">
                      {{ note }}
                    </li>
                  </ul>
                </div>
              </UCard>
            </div>
          </section>
        </section>

        <section id="layout-api" class="rounded-lg border border-slate-200 bg-white p-6 shadow-sm">
          <UBadge color="primary" variant="soft" label="Layout" />
          <h2 class="mt-3 text-2xl font-semibold">
            布局系统
          </h2>
          <p class="mt-3 text-slate-600">
            布局系统负责“控件放在哪里、占多大”。新手写界面时，建议先选外壳容器，再在局部使用 Stack、Grid、Wrap、ScrollView 等，而不是直接手写大量绝对坐标。
          </p>
          <UTable class="mt-5" :data="rows(layoutComponents)" :columns="pairColumns" />
          <pre class="mt-5 rounded-lg bg-slate-950 p-4 text-sm text-slate-100"><code>auto shell = std::make_shared&lt;oneui::AppShell&gt;();
shell-&gt;setPadding(oneui::Insets{16.0f});
shell-&gt;setGap(12.0f);
shell-&gt;setSidebarWidth(240.0f);
shell-&gt;setHeaderHeight(56.0f);
shell-&gt;setSidebar(sidebar);
shell-&gt;setHeader(topBar);
shell-&gt;setContent(mainContent);</code></pre>
        </section>

        <section id="style-api" class="rounded-lg border border-slate-200 bg-white p-6 shadow-sm">
          <UBadge color="primary" variant="soft" label="Styles" />
          <h2 class="mt-3 text-2xl font-semibold">
            CSS-like 样式系统
          </h2>
          <p class="mt-3 text-slate-600">
            OneUI 的样式系统不是完整浏览器 CSS。它的目标是用熟悉的选择器语法描述颜色、边框、圆角、间距、字号、阴影和伪状态，再由控件转换成自己的强类型样式。
          </p>
          <UTable class="mt-5" :data="rows(styleRows)" :columns="pairColumns" />

          <h3 class="mt-7 text-lg font-semibold">
            支持的伪状态
          </h3>
          <UTable class="mt-3" :data="rows(stylePseudoStates)" :columns="pairColumns" />

          <h3 class="mt-7 text-lg font-semibold">
            支持的常用属性
          </h3>
          <UTable class="mt-3" :data="rows(styleProperties)" :columns="pairColumns" />

          <pre class="mt-5 rounded-lg bg-slate-950 p-4 text-sm text-slate-100"><code>button.primary {
  background: #2563eb;
  color: #ffffff;
  border-color: #2563eb;
  border-width: 1px;
  border-radius: 6px;
  padding: 8px 14px;
}

button.primary:hover {
  background: #1d4ed8;
}

input.search:focus {
  border-color: #2563eb;
  outline-color: #93c5fd;
  outline-width: 2px;
  outline-offset: 2px;
}</code></pre>
        </section>

        <section id="cpp-api" class="rounded-lg border border-slate-200 bg-white p-6 shadow-sm">
          <h2 class="text-2xl font-semibold">
            C++ API 入口
          </h2>
          <p class="mt-3 text-slate-600">
            C++ 是 OneUI 当前最完整的使用方式。大部分控件都以头文件 + <code>std::shared_ptr&lt;Widget&gt;</code> 的形式公开。常规入口是 <code>&lt;oneui/oneui.h&gt;</code>，但当前聚合头缺少部分新增控件，缺少时直接 include 单组件头文件。
          </p>
          <pre class="mt-4 rounded-lg bg-slate-950 p-4 text-sm text-slate-100"><code>#include &lt;oneui/oneui.h&gt;
#include &lt;oneui/controls/toast.h&gt;
#include &lt;oneui/controls/icon_button.h&gt;
#include &lt;oneui/layout/top_bar.h&gt;</code></pre>
        </section>

        <section id="c-api" class="rounded-lg border border-slate-200 bg-white p-6 shadow-sm">
          <UBadge color="primary" variant="soft" label="C ABI" />
          <h2 class="mt-3 text-2xl font-semibold">
            C ABI 覆盖范围
          </h2>
          <p class="mt-3 text-slate-600">
            <code>oneui_c_api.h</code> 是跨语言调用的关键。它暴露不透明句柄 <code>OneUiWindow</code>、<code>OneUiWidget</code>、<code>OneUiStyleSheet</code>，并用普通 C 函数创建、配置和销毁对象。
          </p>
          <UTable class="mt-5" :data="rows(cApiGroups)" :columns="pairColumns" />

          <h3 class="mt-7 text-lg font-semibold">
            C ABI 调用规则
          </h3>
          <UTable class="mt-3" :data="rows(cAbiRules)" :columns="pairColumns" />

          <pre class="mt-5 rounded-lg bg-slate-950 p-4 text-sm text-slate-100"><code>OneUiWindowOptions options = {0};
options.title = L"OneUI App";
options.width = 900;
options.height = 600;
options.visible = 1;
options.resizable = 1;

OneUiWindow* window = oneui_window_create(&options);
OneUiWidget* root = oneui_stack_create(OneUiStackDirectionColumn);
OneUiWidget* button = oneui_button_create(L"保存");

oneui_stack_set_gap(root, 12.0f);
oneui_stack_add(root, button);
oneui_window_set_content(window, root);

int code = oneui_window_run(window);

oneui_widget_destroy(button);
oneui_widget_destroy(root);
oneui_window_destroy(window);</code></pre>
        </section>

        <section id="bindings" class="rounded-lg border border-slate-200 bg-white p-6 shadow-sm">
          <h2 class="text-2xl font-semibold">
            跨语言调用
          </h2>
          <p class="mt-3 text-slate-600">
            能加载目标平台 DLL、.so 或 .dylib，并正确处理 C 结构、UTF-8、所有权和回调的语言，都可接入已暴露的 ABI 子集。仓库提供 oneui-sys 和安全 Rust crate；完整签名以公开头和支持符号清单为准。
          </p>
          <UTable class="mt-5" :data="rows(bindingRows)" :columns="pairColumns" />
        </section>

        <section id="remote" class="rounded-lg border border-slate-200 bg-white p-6 shadow-sm">
          <h2 class="text-2xl font-semibold">
            远程会话相关能力
          </h2>
          <div class="mt-5 grid gap-4 md:grid-cols-3">
            <UCard>
              <h3 class="font-semibold">RealtimeFrameView</h3>
              <p class="mt-2 text-sm leading-6 text-slate-600">
                接收 BGRA/RGBA 最新帧，支持 ActualSize、Fit、Fill、Stretch 和脏区域更新；NV12 转换尚未实现。
              </p>
            </UCard>
            <UCard>
              <h3 class="font-semibold">RemoteInputRegion</h3>
              <p class="mt-2 text-sm leading-6 text-slate-600">
                把本地指针坐标转换成内容坐标、归一化坐标和远端屏幕坐标，并转发原始键盘事件。
              </p>
            </UCard>
            <UCard>
              <h3 class="font-semibold">Monitor</h3>
              <p class="mt-2 text-sm leading-6 text-slate-600">
                枚举显示器 bounds、workArea、scale、primary 和 name。当前 Win32 实现更完整。
              </p>
            </UCard>
          </div>
        </section>

        <section id="build-test" class="rounded-lg border border-slate-200 bg-white p-6 shadow-sm">
          <h2 class="text-2xl font-semibold">
            构建与测试
          </h2>
          <p class="mt-3 text-slate-600">
            文档站是独立 Nuxt 项目，位于 <code>website/</code>。OneUI 使用 CMake 构建并按功能域运行 CTest；Windows 预设保留，POSIX 构建与验收步骤见仓库 docs/37-native-desktop-backends.md。缺失环境的验收不能用静默跳过替代。
          </p>
          <pre class="mt-4 rounded-lg bg-slate-950 p-4 text-sm text-slate-100"><code>cmake --preset ucrt64
cmake --build --preset ucrt64
ctest --test-dir build/ucrt64 --output-on-failure

cd website
npm ci
npm run build</code></pre>
          <UTable class="mt-5" :data="testRows.map(([name, result, detail]) => ({ name, result, detail }))" :columns="[
            { accessorKey: 'name', header: '测试' },
            { accessorKey: 'result', header: '状态' },
            { accessorKey: 'detail', header: '说明' }
          ]" />
        </section>

        <section id="faq" class="rounded-lg border border-slate-200 bg-white p-6 shadow-sm">
          <h2 class="text-2xl font-semibold">
            FAQ
          </h2>
          <UAccordion :items="faqItems" class="mt-4" />
        </section>
      </main>
    </UContainer>
  </div>
</template>
