<script setup lang="ts">
import { documents, documentCategories, documentationVersion } from '~/composables/useDocuments'
const expanded = ref(false), query = ref('')
const route = useRoute()
const results = computed(() => documents.filter(doc => (doc.title + ' ' + doc.search).toLowerCase().includes(query.value.trim().toLowerCase())))
watch(() => route.path, () => { expanded.value = false })
</script>
<template>
  <aside class="docs-sidebar">
    <button class="button secondary docs-toggle" :aria-expanded="expanded" aria-controls="docs-nav" @click="expanded = !expanded"><UIcon name="i-lucide-list" />文档目录</button>
    <div id="docs-nav" :class="{ expanded }" class="docs-nav-content">
      <NuxtLink to="/docs" class="docs-home">文档中心</NuxtLink><p class="docs-version">{{ documentationVersion }}</p>
      <label class="search-label"><UIcon name="i-lucide-search" /><input v-model="query" type="search" aria-label="搜索文档" placeholder="搜索文档…"></label>
      <p v-if="query" class="search-count" role="status">{{ results.length }} 篇匹配文档</p>
      <p v-if="!results.length" class="search-count">没有匹配结果，请尝试控件名称或清空关键词。</p>
      <template v-for="category in documentCategories" :key="category">
        <details v-if="results.some(doc => doc.category === category)" :open="category !== '工程记录' || !!query"><summary>{{ category }}</summary><NuxtLink v-for="doc in results.filter(doc => doc.category === category)" :key="doc.slug" :to="'/docs/' + doc.slug" :class="{ selected: route.params.slug === doc.slug }">{{ doc.title.replace(/^OneUI\s*/, '') }}</NuxtLink></details>
      </template>
    </div>
  </aside>
</template>
