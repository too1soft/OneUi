<script setup lang="ts">
import { documents, documentCategories, documentationVersion } from '~/composables/useDocuments'
useSeoMeta({ title: '文档中心 · OneUI', description: 'OneUI 预编译 SDK 接入教程、布局控件、桌面能力和 API 参考。' })
</script>
<template>
  <main id="main" class="wrap docs-shell"><DocsSidebar /><div class="docs-content"><div class="page-heading"><div><h1>文档中心</h1><p>从下载 SDK 开始，逐步构建你的桌面应用。</p></div></div><div class="doc-intro"><p>当前文档快照：<strong>{{ documentationVersion }}</strong>。平台状态以对应版本的下载说明和支持矩阵为准。</p><NuxtLink class="button primary" to="/docs/sdk-quick-start">用预编译 SDK 开始 <UIcon name="i-lucide-arrow-right" /></NuxtLink></div><section v-for="category in documentCategories" :key="category" class="doc-index-section"><h2>{{ category }}</h2><ul><li v-for="doc in documents.filter(doc => doc.category === category)" :key="doc.slug"><NuxtLink :to="'/docs/' + doc.slug">{{ doc.title }}<UIcon name="i-lucide-arrow-up-right" /></NuxtLink></li></ul></section></div></main>
</template>
