<script setup lang="ts">
import type { Artifact, Release } from '~/composables/useReleaseCatalog'
const props = defineProps<{ artifact: Artifact, release: Release }>()
const origin = ref('https://your-oneui-site.example')
onMounted(() => { origin.value = window.location.origin })
const platformName: Record<string, string> = { windows: 'Windows', linux: 'Linux', macos: 'macOS' }
const command = computed(() => {
  const target = props.artifact.id.replace(/-(shared|static)$/, '')
  return 'oneui-site sdk fetch --url ' + origin.value + ' --version ' + props.release.version + ' --target ' + target + ' --linkage ' + props.artifact.linkage + (origin.value.startsWith('http://') ? ' --allow-http' : '')
})
</script>
<template>
  <article class="artifact-row">
    <div class="artifact-main">
      <div><h3>{{ platformName[artifact.platform] }} <span>{{ artifact.architecture }}{{ artifact.profile === 'legacy' ? ' · Win7 兼容版' : '' }}</span></h3><p>{{ artifact.minimumOS }} · {{ artifact.linkage === 'shared' ? '动态 SDK' : '静态 SDK' }}</p></div>
      <div class="artifact-actions">
        <a class="button primary" :href="'/files/' + artifact.file" :download="artifact.file"><UIcon name="i-lucide-download" />完整 SDK · {{ fileSize(artifact.size) }}</a>
        <a v-if="artifact.runtime" class="button secondary" :href="'/files/' + artifact.runtime.file" :download="artifact.runtime.file">仅 DLL · {{ fileSize(artifact.runtime.size) }}</a>
      </div>
    </div>
    <dl class="artifact-meta"><div><dt>工具链</dt><dd>{{ artifact.toolchain }}</dd></div><div><dt>验证</dt><dd>{{ artifact.validation }}</dd></div><div><dt>签名</dt><dd>{{ artifact.signature }}</dd></div></dl>
    <p v-if="artifact.limitations.length" class="artifact-limitations">{{ artifact.limitations.join('；') }}</p>
    <p v-if="artifact.runtime" class="artifact-runtime-note">单独下载的 DLL 需重命名为 <code>oneui.dll</code>。请搭配同版本、同架构 SDK 使用。</p>
    <details class="download-details"><summary>校验值、依赖与自动接入 <UIcon name="i-lucide-chevron-down" /></summary>
      <p class="filename">{{ artifact.file }}</p><label>SDK SHA256<input :value="artifact.sha256" readonly @focus="($event.target as HTMLInputElement).select()"></label>
      <template v-if="artifact.runtime"><p class="filename">{{ artifact.runtime.file }}</p><label>DLL SHA256<input :value="artifact.runtime.sha256" readonly @focus="($event.target as HTMLInputElement).select()"></label></template>
      <p>系统依赖：{{ artifact.systemDependencies.join('、') || '使用目标系统自带运行环境' }}</p><CopyCode :code="command" label="Go 程序自带 SDK 下载命令" /><NuxtLink to="/docs/sdk-quick-start">首次接入与手动下载说明</NuxtLink>
    </details>
  </article>
</template>
