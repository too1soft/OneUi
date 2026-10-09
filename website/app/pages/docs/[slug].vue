<script setup lang="ts">
import { documents, documentationVersion } from '~/composables/useDocuments'
const route = useRoute()
const document = computed(() => documents.find(doc => doc.slug === route.params.slug))
if (!document.value) throw createError({ statusCode: 404, statusMessage: '文档不存在' })
useSeoMeta({ title: () => document.value?.title + ' · OneUI', description: () => document.value?.description })
const copied = ref(''), content = ref<HTMLElement>()
async function copy(event: MouseEvent) {
  const target = event.target as HTMLElement
  if (!target.closest('[data-copy-code]')) return
  const text = target.closest('.doc-code')?.querySelector('code')?.textContent || ''
  try { await navigator.clipboard.writeText(text); copied.value = '代码已复制' }
  catch { copied.value = '复制失败，请选中代码手动复制' }
}
</script>
<template>
  <main id="main" class="wrap docs-shell" v-if="document"><DocsSidebar /><div class="docs-content"><div class="doc-breadcrumb"><NuxtLink to="/docs">文档</NuxtLink><span>/</span><span>{{ document.category }}</span></div><p class="document-version">{{ documentationVersion }} · 代码块可点击复制</p><p v-if="document.category === '工程记录'" class="historical-note">此页保留工程讨论与阶段记录；当前下载产物的支持范围请以<NuxtLink to="/docs/sdk-platforms">发布矩阵</NuxtLink>为准。</p><div ref="content" class="prose" v-html="document.html" @click="copy"></div><p v-if="copied" role="status" class="copy-toast">{{ copied }}</p><div class="doc-end"><NuxtLink to="/docs">返回文档目录</NuxtLink><NuxtLink to="/downloads">下载对应 SDK <UIcon name="i-lucide-arrow-right" /></NuxtLink></div></div><aside v-if="document.toc.length" class="doc-toc"><strong>本页内容</strong><a v-for="item in document.toc" :key="item.id" :href="'#' + item.id" :class="{ nested: item.depth === 3 }">{{ item.text }}</a></aside></main>
</template>
