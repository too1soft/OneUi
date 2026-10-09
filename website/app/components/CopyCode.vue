<script setup lang="ts">
const props = defineProps<{ code: string, label?: string }>()
const feedback = ref('')
async function copy() {
  try { await navigator.clipboard.writeText(props.code); feedback.value = '已复制' }
  catch { feedback.value = '复制失败，请选中代码手动复制' }
}
</script>
<template>
  <div class="code-block"><div class="code-caption"><span>{{ label || '接入示例' }}</span><button @click="copy"><UIcon name="i-lucide-copy" />复制</button></div><pre><code>{{ code }}</code></pre><p v-if="feedback" class="copy-feedback" role="status">{{ feedback }}</p></div>
</template>
