import { resolve } from 'node:path'

export default defineNuxtConfig({
  modules: ['@nuxt/ui'],

  css: ['~/assets/css/main.css'],

  devtools: { enabled: false },

  colorMode: { preference: 'light', fallback: 'light' },

  icon: {
    provider: 'none',
    serverBundle: false,
    clientBundle: {
      scan: true,
      icons: ['lucide:sun-moon', 'lucide:github', 'lucide:menu', 'lucide:x', 'lucide:copy',
        'lucide:arrow-down-to-line', 'lucide:arrow-up-right', 'lucide:package-check',
        'lucide:file-check-2', 'lucide:arrow-right', 'lucide:panels-top-left',
        'lucide:mouse-pointer-2', 'lucide:monitor', 'lucide:download',
        'lucide:chevron-down', 'lucide:package-open', 'lucide:info', 'lucide:list', 'lucide:search']
    }
  },

  fonts: {
    providers: {
      adobe: false,
      bunny: false,
      fontshare: false,
      fontsource: false,
      google: false,
      googleicons: false
    }
  },

  future: {
    compatibilityVersion: 4
  },

  compatibilityDate: '2026-05-27',

  nitro: {
    output: {
      dir: resolve('../out/portal-build/nuxt'),
      publicDir: resolve('server/web')
    },
    prerender: {
      routes: ['/', '/downloads', '/docs'],
      crawlLinks: true,
      failOnError: true
    }
  },

  app: {
    head: {
      htmlAttrs: {
        lang: 'zh-CN'
      },
      title: 'OneUI · 官方文档与 SDK',
      meta: [
        {
          name: 'description',
          content: 'OneUI 官方文档、入门指南、完整组件 API、C ABI、构建打包和代码审查报告。'
        }
      ]
    }
  }
})
