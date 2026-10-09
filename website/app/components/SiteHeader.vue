<script setup lang="ts">
const expanded = ref(false)
const color = useColorMode()
const route = useRoute()
const links = [{ to: '/', label: '概览' }, { to: '/downloads', label: '下载' }, { to: '/docs', label: '文档' }, { to: '/docs/14-component-reference', label: '组件参考' }]
watch(() => route.path, () => { expanded.value = false })
</script>

<template>
  <a class="skip-link" href="#main">跳到正文</a>
  <header class="site-header">
    <div class="site-header-inner">
      <NuxtLink to="/" class="brand" aria-label="OneUI 首页"><span class="brand-mark" aria-hidden="true">O</span><span>OneUI</span></NuxtLink>
      <nav class="desktop-nav" aria-label="主导航">
        <NuxtLink v-for="link in links" :key="link.to" :to="link.to" :class="{ current: route.path === link.to }">{{ link.label }}</NuxtLink>
      </nav>
      <div class="header-actions">
        <button class="icon-button" aria-label="切换浅色或深色主题" @click="color.preference = color.value === 'dark' ? 'light' : 'dark'"><UIcon name="i-lucide-sun-moon" /></button>
        <a class="icon-button" href="https://github.com/too1soft/OneUi" aria-label="GitHub 源码"><UIcon name="i-lucide-github" /></a>
        <button class="icon-button mobile-menu" :aria-expanded="expanded" aria-controls="mobile-nav" aria-label="打开导航" @click="expanded = !expanded"><UIcon :name="expanded ? 'i-lucide-x' : 'i-lucide-menu'" /></button>
      </div>
    </div>
    <nav v-if="expanded" id="mobile-nav" class="mobile-nav" aria-label="移动导航"><NuxtLink v-for="link in links" :key="link.to" :to="link.to">{{ link.label }}</NuxtLink></nav>
  </header>
</template>
