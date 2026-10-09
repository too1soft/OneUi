<script setup lang="ts">
useSeoMeta({ title: '下载 DLL 与 SDK · OneUI', description: '按版本、系统与架构下载 OneUI 预编译 SDK 或 Windows DLL，无需重新编译框架。' })
const { data, status, error, refresh } = useReleaseCatalog()
const channel = ref('preview'), version = ref(''), platform = ref('windows'), arch = ref('x86_64'), linkage = ref('shared'), profile = ref('modern')
const releases = computed(() => (data.value?.releases || []).filter(r => r.channel === channel.value))
const selected = computed(() => releases.value.find(r => r.version === version.value) || releases.value[0])
const architectures = computed(() => [...new Set((selected.value?.artifacts || []).filter(a => a.platform === platform.value).map(a => a.architecture))])
const artifacts = computed(() => (selected.value?.artifacts || []).filter(a => a.platform === platform.value && a.linkage === linkage.value && (arch.value === 'all' || a.architecture === arch.value) && (platform.value !== 'windows' || profile.value === 'all' || a.profile === profile.value)))
watch(architectures, values => {
  if (values.length && arch.value !== 'all' && !values.includes(arch.value)) arch.value = values.includes('x86_64') ? 'x86_64' : values[0]!
}, { immediate: true })
watch(channel, () => { version.value = '' })
const channelLabels: Record<string,string> = { stable: '稳定版', preview: '预览版', experimental: '实验版' }
const archLabels: Record<string,string> = { x86_64: 'x64 · 64 位', x86: 'x86 · 32 位', arm64: 'ARM64 · Apple 芯片' }
function showAllTargets() { arch.value = 'all'; profile.value = 'all' }
</script>
<template>
  <main id="main" class="wrap download-page">
    <div class="page-heading"><div><h1>下载 OneUI</h1><p>框架已编译好。选择目标平台，开始构建你的应用。</p></div><NuxtLink class="text-link" to="/docs/sdk-quick-start">阅读接入指南 <UIcon name="i-lucide-arrow-up-right" /></NuxtLink></div>
    <div class="download-intro">
      <p><strong>首次接入，下载完整 SDK。</strong>包含运行库、头文件与链接文件；静态版包含所需静态依赖。</p>
      <p>已有匹配版本的接入文件？Windows 动态版也可单独下载 DLL。Linux 提供 .so，macOS 提供 .dylib，均在 SDK 内。</p>
    </div>
    <div class="channel-tabs" role="group" aria-label="发布渠道"><button v-for="(label, key) in channelLabels" :key="key" :aria-pressed="channel === key" :class="{ active: channel === key }" @click="channel = key">{{ label }}</button></div>
    <div v-if="status === 'pending' || status === 'idle'" class="empty-state" role="status">正在读取发布清单…</div>
    <div v-else-if="error" class="empty-state" role="alert"><h2>暂时无法读取版本</h2><p>请检查网络，或联系站点管理员确认下载服务已启动。</p><button class="button secondary" @click="refresh()">重新加载</button></div>
    <div v-else-if="!selected" class="empty-state"><UIcon name="i-lucide-package-open" /><h2>暂未发布{{ channelLabels[channel] }}</h2><p>可以查看其他发布渠道。预览版和实验版的验收状态与限制会在下载包旁说明。</p><button v-if="channel !== 'preview'" class="button secondary" @click="channel = 'preview'">查看预览版</button></div>
    <template v-else>
      <section class="release-summary"><div><h2>{{ selected.version }} <span class="tag">{{ channelLabels[selected.channel] }}</span></h2><p>ABI {{ selected.abi }} · {{ selected.publishedAt }} · <NuxtLink to="/docs/sdk-release-guide">版本说明</NuxtLink></p></div><label>历史版本<select v-model="version" aria-label="历史版本"><option value="">最新{{ channelLabels[channel] }}</option><option v-for="release in releases" :key="release.version" :value="release.version">{{ release.version }}</option></select></label></section>
      <details v-if="selected.notes.length" :key="selected.version" class="release-notes-disclosure"><summary>本版本说明 <UIcon name="i-lucide-chevron-down" /></summary><ul class="release-notes"><li v-for="note in selected.notes" :key="note">{{ note }}</li></ul></details>
      <div class="download-filters">
        <label>操作系统<select v-model="platform"><option value="windows">Windows</option><option value="macos">macOS</option><option value="linux">Linux</option></select></label>
        <label>架构<select v-model="arch"><option value="all">全部架构</option><option v-for="value in architectures" :key="value" :value="value">{{ archLabels[value] || value }}</option></select></label>
        <label>链接方式<select v-model="linkage"><option value="shared">动态链接</option><option value="static">静态链接</option></select></label>
        <label v-if="platform === 'windows'">系统版本<select v-model="profile"><option value="modern">Windows 10 / 11</option><option value="legacy">Windows 7 兼容版</option><option value="all">全部兼容版本</option></select></label>
      </div>
      <div class="download-results"><p aria-live="polite">{{ artifacts.length }} 个匹配的 SDK</p><button v-if="arch !== 'all' || (platform === 'windows' && profile !== 'all')" class="text-link" @click="showAllTargets">查看该系统全部架构与兼容版本</button></div>
      <div v-if="!artifacts.length" class="empty-state"><h3>这个组合暂未提供下载</h3><p>请切换系统、架构、链接方式或版本；未发布的目标不会提供占位文件。</p></div>
      <ArtifactDownload v-for="artifact in artifacts" :key="selected.version + artifact.id" :artifact="artifact" :release="selected" />
      <div class="download-help"><UIcon name="i-lucide-info" /><p>版本、架构和 ABI 需要与应用一致。包内不包含目标系统的桌面库或系统框架。Android、iOS 和鸿蒙暂未提供正式 SDK。<NuxtLink to="/docs/sdk-platforms">查看平台要求</NuxtLink></p></div>
      <div class="download-footer"><p>团队统一锁定 SDK 版本，多个项目共享本机下载缓存。</p><NuxtLink to="/docs/sdk-quick-start">配置自动下载与版本锁 <UIcon name="i-lucide-arrow-right" /></NuxtLink></div>
    </template>
  </main>
</template>
