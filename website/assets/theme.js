// 先设置主题，避免首屏闪烁；存储不可用时保留可用的默认主题。
try {
  const savedTheme = localStorage.getItem('qm-site-theme');
  if (savedTheme === 'dark' || savedTheme === 'light') {
    document.documentElement.dataset.theme = savedTheme;
  }
} catch {
  // 隐私模式不影响主题和页面内容。
}
