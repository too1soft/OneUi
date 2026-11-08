<script setup lang="ts">
type Tool = {file:string,platform:string,architecture:string,version:string,size:number,sha256:string}
const {data,status,error,refresh}=useFetch<{tools:Tool[]}>('/api/tools',{server:false})
const labels: Record<string,string> = {windows:'Windows x64',linux:'Linux x64',macos:'macOS Apple Silicon'}
const currentTools = computed(() => {
  const targets = new Map<string,Tool>()
  for (const tool of data.value?.tools || []) {
    const key = `${tool.platform}-${tool.architecture}`
    if (!targets.has(key)) targets.set(key,tool)
  }
  return [...targets.values()]
})
</script>
<template>
  <section class="developer-section tool-downloads" aria-labelledby="tools-title"><h2 id="tools-title">获取命令行工具</h2><p>一个命令管理 SDK 与插件，支持版本锁和离线缓存。下载后将程序改名为 oneui-site，并加入 PATH。</p>
    <p v-if="status==='pending' || status==='idle'" role="status">正在读取工具版本…</p>
    <div v-else-if="error" role="alert"><p>工具下载信息暂时不可用。</p><button class="button secondary" @click="refresh()">重新加载</button></div>
    <div v-else-if="currentTools.length" class="tool-options"><div v-for="tool in currentTools" :key="tool.file"><a class="button secondary" :href="'/files/'+tool.file">{{ labels[tool.platform] || tool.platform+' '+tool.architecture }} <UIcon name="i-lucide-arrow-down-to-line" /></a><p>{{ tool.version }} · {{ fileSize(tool.size) }}</p><details><summary>SHA256</summary><code>{{ tool.sha256 }}</code></details></div></div>
    <p v-if="currentTools.some(tool=>tool.platform==='macos')">macOS 下载后先赋予执行权限：<code>chmod +x /path/to/oneui-site</code>。</p>
    <p v-else>可从源码构建工具。<a class="text-link" href="https://github.com/too1soft/OneUi/tree/main/website/server">查看 Go 工程 <UIcon name="i-lucide-arrow-up-right" /></a></p>
  </section>
</template>
