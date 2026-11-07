<script setup lang="ts">
useSeoMeta({ title: '开发者中心 · OneUI', description: '使用预编译 SDK 构建应用，通过服务插件和组件包扩展 OneUI。接入指南、插件目录与开发文档。' })
const mode = ref<'app'|'service'|'component'>('app')
const paths = {
  app: { title: '构建应用', description: '下载匹配目标的 SDK，编译你的应用代码。用版本锁保持团队构建一致。', to: '/docs/sdk-quick-start', action: '阅读应用接入指南', code: 'find_package(OneUI CONFIG REQUIRED)\nadd_executable(my_app main.cpp)\ntarget_link_libraries(my_app PRIVATE OneUI::oneui)' },
  service: { title: '接入系统能力', description: '在应用中选择服务包，由每个应用自己的运行时管理依赖、服务租约和清理。', to: '/docs/plugin-packages', action: '安装第一个服务插件', code: 'oneui-site plugin add --project . --id oneui.vpn --version "^0.1.0" --target windows-modern-x86_64 --linkage shared' },
  component: { title: '扩展界面组件', description: '在独立包中实现控件。页面编译器和运行时读取同一份契约，校验属性、事件和组件版本。', to: '/docs/plugin-components', action: '开发第一个组件', code: '<OneUI_Status\n  :label="label"\n  :value="value"\n  @activate="activate"\n/>' }
}
const selected = computed(() => paths[mode.value])
</script>
<template>
  <main id="main" class="wrap developer-page">
    <div class="page-heading"><div><h1>开发者中心</h1><p>从一个应用开始，扩展到你需要的能力。</p></div><NuxtLink class="text-link" to="/downloads">获取 SDK 与工具 <UIcon name="i-lucide-arrow-down-to-line" /></NuxtLink></div>
    <section class="developer-start" aria-labelledby="start-title">
      <div><h2 id="start-title">你准备从哪里开始？</h2><p>OneUI 提供界面底座，插件封装可选能力。应用选择依赖，插件可以独立演进。</p>
        <div class="developer-choices" role="group" aria-label="选择开发场景">
          <button v-for="(path, key) in paths" :key="key" :aria-pressed="mode === key" :class="{active: mode === key}" @click="mode = key"><strong>{{ path.title }}</strong><UIcon name="i-lucide-arrow-right" /></button>
        </div>
      </div>
      <div class="developer-example"><p>{{ selected.description }}</p><CopyCode :key="mode" :code="selected.code" :label="selected.title" /><NuxtLink class="text-link" :to="selected.to">{{ selected.action }} <UIcon name="i-lucide-arrow-right" /></NuxtLink></div>
    </section>
    <section class="developer-section"><div class="section-heading"><h2>为应用选择合适的扩展。</h2><NuxtLink class="text-link" to="/plugins">浏览插件目录 <UIcon name="i-lucide-arrow-right" /></NuxtLink></div>
      <div class="extension-row"><div><h3>服务插件</h3><p>VPN、系统集成与原生服务</p></div><p>通过版本化 C 函数表访问。服务运行时独立于界面渲染，可在没有窗口的程序中使用。</p><NuxtLink class="text-link" to="/docs/plugin-services">服务开发指南 <UIcon name="i-lucide-arrow-up-right" /></NuxtLink></div>
      <div class="extension-row"><div><h3>组件包</h3><p>自定义控件与专用可视化</p></div><p>使用公开 Widget 和绘制接口实现控件，在 .one 页面中使用属性绑定、事件和主题样式。</p><NuxtLink class="text-link" to="/docs/plugin-components">组件开发指南 <UIcon name="i-lucide-arrow-up-right" /></NuxtLink></div>
    </section>
    <section class="developer-section developer-resources"><h2>接入时用得上的资料。</h2><div><NuxtLink to="/docs/sdk-quick-start"><h3>SDK 与构建</h3><p>下载、版本锁和 CMake 接入</p></NuxtLink><NuxtLink to="/docs/plugin-packages"><h3>包与依赖</h3><p>安装、离线恢复和源码依赖</p></NuxtLink><NuxtLink to="/docs/14-component-reference"><h3>标准组件</h3><p>布局、控件与交互接口</p></NuxtLink><NuxtLink to="/docs/plugin-platforms"><h3>平台装配</h3><p>桌面与移动宿主的接入方式</p></NuxtLink></div></section>
    <ToolDownloads />
    <section class="closing"><div><h2>把通用能力做成可复用的包。</h2><p>从官方示例开始，维护自己的包名、版本和兼容范围。</p></div><a class="button secondary" href="https://github.com/too1soft/OneUi/tree/main/plugins">查看插件源码 <UIcon name="i-lucide-github" /></a></section>
  </main>
</template>
