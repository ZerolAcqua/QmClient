import { detectPlatform, downloadFor, savePreference } from './preferences.mjs?v=1.0.0';
import { initializeMotion } from './motion.mjs?v=1.0.0';
import { initializePreview } from './preview.mjs?v=1.0.0';

const root = document.documentElement;
const themeToggle = document.querySelector('#theme-toggle');
function updateThemeButton() {
  const isDark = root.dataset.theme === 'dark';
  themeToggle.setAttribute('aria-label', isDark ? '切换到浅色模式' : '切换到深色模式');
  themeToggle.querySelector('use').setAttribute('href', isDark ? '#i-sun' : '#i-moon');
  document.querySelector('meta[name="theme-color"]').content = isDark ? '#151b19' : '#f7f8f5';
}
themeToggle.hidden = false;
themeToggle.addEventListener('click', () => {
  root.dataset.theme = root.dataset.theme === 'dark' ? 'light' : 'dark';
  savePreference('qm-site-theme', root.dataset.theme);
  updateThemeButton();
});
updateThemeButton();

const menu = document.querySelector('#main-nav');
const menuToggle = document.querySelector('#menu-toggle');
function closeMenu() {
  menu.classList.remove('is-open');
  menuToggle.setAttribute('aria-expanded', 'false');
  menuToggle.setAttribute('aria-label', '打开导航');
}
menuToggle.hidden = false;
menuToggle.addEventListener('click', () => {
  const isOpen = menu.classList.toggle('is-open');
  menuToggle.setAttribute('aria-expanded', String(isOpen));
  menuToggle.setAttribute('aria-label', isOpen ? '关闭导航' : '打开导航');
});
menu.querySelectorAll('a').forEach(link => link.addEventListener('click', closeMenu));
document.addEventListener('keydown', event => {
  if (event.key === 'Escape' && menu.classList.contains('is-open')) {
    closeMenu();
    menuToggle.focus();
  }
});
document.addEventListener('click', event => {
  if (!event.target.closest('.site-header')) closeMenu();
});
root.classList.add('js');

// iPad 桌面浏览器会报告 Macintosh，不将其当成 macOS 安装目标。
const isTabletDesktop = /Macintosh/.test(navigator.userAgent) && navigator.maxTouchPoints > 1;
const platform = isTabletDesktop ? null : detectPlatform(navigator.userAgent);
document.querySelectorAll('[data-platform]').forEach(card => {
  card.href = downloadFor(card.dataset.platform);
  const recommended = card.dataset.platform === platform;
  card.classList.toggle('recommended', recommended);
  card.querySelector('.download-badge').hidden = !recommended;
});

document.querySelector('#copy-group').addEventListener('click', async () => {
  const status = document.querySelector('#copy-status');
  try {
    await navigator.clipboard.writeText('1076765929');
    status.textContent = '群号已复制，在 QQ 中搜索即可加入。';
  } catch {
    status.textContent = '请在 QQ 中搜索群号：1076765929';
  }
});

initializeMotion();
initializePreview();
