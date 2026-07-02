export default defineNuxtConfig({
  modules: ['@nuxt/ui'],

  css: ['~/assets/css/main.css'],

  devtools: { enabled: false },

  future: {
    compatibilityVersion: 4
  },

  compatibilityDate: '2026-05-27',

  nitro: {
    prerender: {
      routes: ['/']
    }
  },

  app: {
    head: {
      htmlAttrs: {
        lang: 'zh-CN'
      },
      title: 'OneUI 官方文档',
      meta: [
        {
          name: 'description',
          content: 'OneUI 官方文档、入门指南、完整组件 API、C ABI、构建打包和代码审查报告。'
        }
      ]
    }
  }
})
