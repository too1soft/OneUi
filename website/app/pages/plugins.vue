<script setup lang="ts">
useSeoMeta({title: '插件目录 · OneUI', description: '发现 OneUI 服务插件、组件包与原生 SDK。按平台选择版本，查看依赖和接入方式。'})
const {data, status, error, refresh} = usePluginCatalog()
const query = ref(''), kind = ref('all'), platform = ref('all')
const labels: Record<string,string> = {service:'服务插件', component:'组件包', sdk:'原生 SDK'}
const platforms: Record<string,string> = {windows:'Windows', linux:'Linux', macos:'macOS', android:'Android', ios:'iOS', harmonyos:'鸿蒙'}
const packages = computed(() => (data.value?.packages || []).filter(p => {
  const needle=query.value.trim().toLowerCase()
  return (kind.value==='all' || p.manifest.kind===kind.value) && (platform.value==='all' || p.manifest.platforms.includes(platform.value)) && (!needle || `${p.manifest.id} ${p.description} ${p.manifest.components?.map(c=>c.name).join(' ')}`.toLowerCase().includes(needle))
}))
const selectedTargets = reactive<Record<string,string>>({})
function reset() { query.value='';kind.value='all';platform.value='all' }
function command(p: import('~/composables/usePluginCatalog').PluginRelease) {
  const target=p.artifacts.find(a=>a.id===selectedTargets[p.manifest.id]) || p.artifacts[0]
  return target ? `oneui-site plugin add --project . --id ${p.manifest.id} --version "^${p.manifest.version}" --target ${target.platform}-${target.profile}-${target.architecture} --linkage ${target.linkage}` : ''
}
</script>
<template>
  <main id="main" class="wrap plugin-page">
    <div class="page-heading"><div><h1>插件目录</h1><p>按需添加能力，让 OneUI 随你的应用扩展。</p></div><NuxtLink class="text-link" to="/docs/plugin-packages">安装与依赖指南 <UIcon name="i-lucide-arrow-up-right" /></NuxtLink></div>
    <div class="plugin-toolbar"><label class="plugin-search">查找插件<input v-model="query" type="search" placeholder="包名、功能或组件名"></label><label>类型<select v-model="kind"><option value="all">全部类型</option><option v-for="(label,id) in labels" :key="id" :value="id">{{ label }}</option></select></label><label>平台接口<select v-model="platform"><option value="all">全部平台</option><option v-for="(label,id) in platforms" :key="id" :value="id">{{ label }}</option></select></label></div>
    <p class="plugin-help">平台标签表示包提供的接口。预编译目标与工具链以每个包的下载信息为准。</p>
    <div v-if="status==='pending' || status==='idle'" class="empty-state" role="status">正在读取插件目录…</div>
    <div v-else-if="error" class="empty-state" role="alert"><h2>暂时无法读取插件目录</h2><p>请检查网络后重新加载。</p><button class="button secondary" @click="refresh()">重新加载</button></div>
    <template v-else><p class="plugin-count" role="status">{{ packages.length }} 个匹配的包</p>
      <div v-if="!packages.length" class="empty-state"><h2>没有匹配的插件</h2><p>换一个关键词，或清除筛选条件。</p><button class="button secondary" @click="reset">清除筛选</button></div>
      <article v-for="p in packages" :key="p.manifest.id+'@'+p.manifest.version" class="plugin-entry">
        <div class="plugin-entry-heading"><div><h2>{{ p.manifest.id }}</h2><p>{{ labels[p.manifest.kind] }} · {{ p.manifest.version }} · {{ p.manifest.license }}</p></div><a class="text-link" :href="`https://github.com/too1soft/OneUi/tree/${p.sourceCommit}/plugins/${p.manifest.id==='oneui.native-sdk'?'native-sdk':p.manifest.id.split('.').slice(1).join('-')}`">源码 <UIcon name="i-lucide-arrow-up-right" /></a></div>
        <p class="plugin-description">{{ p.description }}</p>
        <div class="plugin-platforms"><span v-for="system in p.manifest.platforms" :key="system">{{ platforms[system] }}</span></div>
        <details class="plugin-details"><summary>查看接口、依赖与安装 <UIcon name="i-lucide-chevron-down" /></summary><div class="plugin-details-content">
          <dl><template v-if="p.manifest.services?.length"><dt>服务</dt><dd v-for="s in p.manifest.services" :key="s.id"><code>{{ s.id }}</code> · ABI {{ s.abi }}</dd></template><template v-if="p.manifest.components?.length"><dt>组件</dt><dd v-for="c in p.manifest.components" :key="c.name"><code>{{ c.name }}</code> · 契约 {{ c.version }}</dd></template><dt>包依赖</dt><dd>{{ p.manifest.dependencies?.map(d=>`${d.id} ${d.version}`).join('，') || '无包级依赖' }}</dd></dl>
          <template v-if="p.artifacts.length"><label>预编译目标<select :value="selectedTargets[p.manifest.id] || p.artifacts[0]?.id" @change="selectedTargets[p.manifest.id] = ($event.target as HTMLSelectElement).value"><option v-for="a in p.artifacts" :key="a.id" :value="a.id">{{ a.id }}</option></select></label><CopyCode :code="command(p)" label="安装到当前项目" /><ul class="plugin-downloads"><li v-for="a in p.artifacts" :key="a.file"><a :href="'/files/'+a.file">{{ a.id }} <UIcon name="i-lucide-arrow-down-to-line" /></a><small>{{ a.toolchain }} · {{ fileSize(a.size) }}</small><details><summary>SHA256</summary><code>{{ a.sha256 }}</code></details></li></ul></template>
          <NuxtLink class="text-link" :to="p.manifest.kind==='component'?'/docs/plugin-components':'/docs/plugin-services'">{{ p.manifest.kind==='component'?'组件接入与开发':'服务接入与开发' }} <UIcon name="i-lucide-arrow-right" /></NuxtLink>
        </div></details>
      </article>
    </template>
    <section class="closing"><div><h2>想扩展一种新能力？</h2><p>从公开契约和示例包开始，独立构建、分发和维护。</p></div><NuxtLink class="button secondary" to="/developers">前往开发者中心 <UIcon name="i-lucide-arrow-right" /></NuxtLink></section>
  </main>
</template>
