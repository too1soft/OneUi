import fs from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'node:url'
import { Marked } from 'marked'

const site = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..')
const root = path.resolve(site, '..')
const docsDir = path.join(root, 'docs')
const escape = value => value.replace(/[&<>"']/g, char => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[char])
const slugify = text => text.toLowerCase().replace(/<[^>]*>/g, '').replace(/[^\p{L}\p{N}_\-\s]/gu, '').trim().replace(/\s/g, '-')
const files = fs.readdirSync(docsDir).filter(name => name.endsWith('.md')).sort()
const names = new Set(files.map(name => name.slice(0, -3)))
const primary = new Map([
  ['sdk-quick-start', '开始使用'], ['sdk-release-guide', '开始使用'], ['portal-deployment', '开始使用'],
  ['07-component-inventory', '控件与交互'], ['14-component-reference', '控件与交互'],
  ['13-authoring-guide', '布局与样式'], ['20-style-sheet', '布局与样式'],
  ['37-native-desktop-backends', '桌面与平台'], ['unified-lab-capabilities', '试验台'],
  ['unified-lab-implementation', '试验台'], ['c-abi-integration', '语言接入'],
  ['sdk-platforms', '桌面与平台'], ['sdk-rust', '语言接入']
])
function category(name) {
  if (primary.has(name)) return primary.get(name)
  if (/style|layout|authoring|declarative|form-field|composition|effects|motion/.test(name)) return '布局与样式'
  if (/component|list|tree|popup|shell|terminal|fonts|workspace-docking/.test(name)) return '控件与交互'
  if (/platform|backend|windows7|titlebar|accessibility|dpi/.test(name)) return '桌面与平台'
  if (/render|performance|particle|frame-profile/.test(name)) return '渲染与性能'
  return '工程记录'
}
const documents = files.filter(name => name !== 'README.md').map(filename => {
  const slug = filename.slice(0, -3)
  const source = fs.readFileSync(path.join(docsDir, filename), 'utf8')
  const toc = [], headings = new Map()
  function href(value) {
    if (/^(https?:|mailto:|#)/.test(value)) return value
    if (/^[a-z][a-z0-9+.-]*:/i.test(value)) return '#'
    const [base, anchor] = value.split('#')
    const destination = path.posix.normalize(path.posix.join('docs', base))
    const target = path.posix.basename(destination, '.md')
    if (destination.startsWith('docs/') && names.has(target) && target !== 'README') return '/docs/' + target + (anchor ? '#' + anchor : '')
    if (destination === 'docs/README.md') return '/docs'
    return 'https://github.com/too1soft/OneUi/blob/main/' + destination + (anchor ? '#' + anchor : '')
  }
  const marked = new Marked({ gfm: true, renderer: {
    heading(token) {
      const text = this.parser.parseInline(token.tokens)
      const base = slugify(token.text), count = headings.get(base) || 0
      headings.set(base, count + 1)
      const id = base + (count ? '-' + count : '')
      if (token.depth >= 2 && token.depth <= 3) toc.push({ id, text: token.text.replace(/[\x60*_]/g, ''), depth: token.depth })
      return '<h' + token.depth + ' id="' + escape(id) + '">' + text + '</h' + token.depth + '>'
    },
    link(token) { return '<a href="' + escape(href(token.href)) + '">' + this.parser.parseInline(token.tokens) + '</a>' },
    image(token) { return '<a href="' + escape(href(token.href)) + '">' + escape(token.text || '查看图片') + '</a>' },
    html(token) { return token.text.startsWith('<!--') ? '' : escape(token.text) },
    code(token) { return '<div class="doc-code"><button type="button" data-copy-code aria-label="复制这段代码">复制代码</button><pre><code>' + escape(token.text) + '</code></pre></div>' }
  } })
  const html = marked.parse(source)
  const title = source.match(/^#\s+(.+)$/m)?.[1].replace(/[\x60*_]/g, '') || slug
  return { slug, title, category: category(slug), html, toc,
    search: source.replace(/[\x60#*|<>]/g, '').slice(0, 70000),
    description: source.split('\n').filter(line => line.trim() && !/^[#<!|\x60]/.test(line)).slice(0, 2).join(' ').slice(0, 130) }
})
documents.sort((a,b) => Number(primary.has(b.slug)) - Number(primary.has(a.slug)) || a.slug.localeCompare(b.slug))
fs.mkdirSync(path.join(site, 'app/data'), { recursive: true })
fs.writeFileSync(path.join(site, 'app/data/docs.generated.json'), JSON.stringify({ version: '0.1.0-preview.1', documents }))
console.log('Prepared ' + documents.length + ' bundled documents.')
