const fs = require('node:fs');
const path = require('node:path');
const assert = require('node:assert/strict');
const { chromium } = require(process.env.PLAYWRIGHT_MODULE || 'playwright');

(async () => {
  const base = process.env.ONEUI_SITE_URL || 'http://127.0.0.1:3187';
  const root = path.resolve(__dirname, '../..');
  const shots = path.join(root, '.impeccable/review');
  const report = { base, checks: [], screenshots: [], errors: [], failedRequests: [] };
  fs.mkdirSync(shots, { recursive: true });
  const browser = await chromium.launch({ headless: true });
  try {
    const context = await browser.newContext({ viewport: { width: 1440, height: 1000 }, permissions: ['clipboard-read', 'clipboard-write'] });
    const page = await context.newPage();
    page.on('pageerror', e => report.errors.push(e.message));
    page.on('requestfailed', r => report.failedRequests.push({ url: r.url(), error: r.failure()?.errorText }));
    async function capture(name) {
      await page.screenshot({ path: path.join(shots, name), fullPage: true, animations: 'disabled' });
      report.screenshots.push(name);
      const dimensions = await page.evaluate(() => ({ viewport: innerWidth, width: document.documentElement.scrollWidth }));
      assert.ok(dimensions.width <= dimensions.viewport, 'horizontal overflow: ' + name);
    }
    await page.goto(base, { waitUntil: 'networkidle' });
    assert.match(await page.locator('h1').innerText(), /专注你的应用/);
    await capture('desktop.png');
    await page.setViewportSize({ width: 390, height: 844 });
    await page.getByRole('button', { name: '打开导航' }).click();
    assert.equal(await page.locator('#mobile-nav').isVisible(), true);
    await page.locator('#mobile-nav').getByRole('link', { name: '下载', exact: true }).click();
    await page.locator('.artifact-row').first().waitFor();
    assert.equal(await page.locator('.artifact-row').count(), 4);
    await capture('mobile.png');
    await page.getByRole('button', { name: '稳定版', exact: true }).click();
    await page.getByRole('heading', { name: '暂未发布稳定版' }).waitFor();
    assert.equal(await page.locator('a[download]').count(), 0);
    await page.getByRole('button', { name: '查看预览版' }).click();
    await page.getByRole('combobox', { name: /^操作系统/ }).selectOption('macos');
    assert.equal(await page.locator('.artifact-row').count(), 1);
    await page.getByRole('combobox', { name: /^链接方式/ }).selectOption('static');
    assert.match(await page.locator('a[download]').getAttribute('href'), /macos-native-arm64-static/);
    await page.getByRole('combobox', { name: /^操作系统/ }).selectOption('linux');
    assert.equal(await page.locator('.artifact-row').count(), 1);
    await page.getByRole('combobox', { name: /^操作系统/ }).selectOption('windows');
    await page.getByRole('combobox', { name: /^链接方式/ }).selectOption('shared');
    await page.getByRole('combobox', { name: /^架构/ }).selectOption('x86_64');
    assert.equal(await page.locator('.artifact-row').count(), 2);
    await page.setViewportSize({ width: 1440, height: 1000 });
    await page.locator('.download-details summary').first().click();
    await page.locator('.code-caption button').first().click();
    await page.getByText('已复制', { exact: true }).waitFor();
    const command = await page.evaluate(() => navigator.clipboard.readText());
    assert.match(command, /sdk fetch.*--version 0.1.0-preview.1/);
    const href = await page.locator('a[download]').first().getAttribute('href');
    const head = await context.request.head(base + href);
    assert.equal(head.status(), 200);
    const range = await context.request.get(base + href, { headers: { Range: 'bytes=0-63' } });
    assert.equal(range.status(), 206);
    assert.equal((await range.body()).length, 64);
    report.checks.push('mobile menu, channels, empty stable state, platform/linkage/architecture filters, copy command, download HEAD/Range');
    await page.goto(base + '/docs/sdk-quick-start', { waitUntil: 'networkidle' });
    await page.getByRole('heading', { name: '使用预编译 SDK', exact: true }).waitFor();
    await capture('docs-desktop.png');
    await page.locator('[data-copy-code]').first().click();
    await page.getByText('代码已复制', { exact: true }).waitFor();
    await page.getByRole('searchbox', { name: '搜索文档' }).fill('NotificationService');
    assert.ok(await page.locator('.docs-sidebar details a').count() > 0);
    await page.getByRole('searchbox', { name: '搜索文档' }).fill('no-document-matches-987654321');
    assert.match((await page.locator('.search-count').allTextContents()).join(' '), /没有匹配结果/);
    await page.getByRole('searchbox', { name: '搜索文档' }).fill('');
    await page.getByRole('button', { name: '切换浅色或深色主题' }).click();
    assert.ok(await page.locator('html').evaluate(e => e.classList.contains('dark')));
    report.checks.push('deep-link documentation, code copy, full-text search, empty results, theme switch');
    const catalog = await (await context.request.get(base + '/api/releases')).json();
    for (const artifact of catalog.releases.flatMap(r => r.artifacts)) {
      const response = await context.request.head(base + '/files/' + artifact.file);
      assert.equal(response.status(), 200);
      assert.equal(Number(response.headers()['content-length']), artifact.size);
    }
    report.checks.push('all 12 download endpoints and declared file sizes');
    assert.deepEqual(report.errors, []);
    assert.deepEqual(report.failedRequests, []);
    report.passed = true;
  } catch (error) {
    report.passed = false; report.failure = error.stack; process.exitCode = 1;
  } finally {
    await browser.close();
    fs.writeFileSync(path.join(root, 'out/native-release-review/portal-browser-tests.json'), JSON.stringify(report, null, 2));
    console.log(JSON.stringify(report, null, 2));
  }
})();
