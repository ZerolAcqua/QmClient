// 安装包使用已发布的官方资产名，latest 链接随 GitHub 稳定版更新。
const releases = 'https://github.com/wxj881027/QmClient/releases/latest';
const assets = {
  windows: 'QmClient-windows.zip',
  macos: 'QmClient-macOS.dmg',
  linux: 'QmClient-ubuntu.tar.xz',
  android: 'QmClient-android.apk',
};

export function detectPlatform(userAgent) {
  if (/android/i.test(userAgent)) return 'android';
  if (/iphone|ipad|ipod/i.test(userAgent)) return null;
  if (/windows/i.test(userAgent)) return 'windows';
  if (/macintosh|mac os x/i.test(userAgent)) return 'macos';
  if (/linux/i.test(userAgent)) return 'linux';
  return null;
}

export function downloadFor(platform) {
  return Object.hasOwn(assets, platform) ? `${releases}/download/${assets[platform]}` : releases;
}

export function motionAllowed(systemReduced, savedPreference) {
  return !systemReduced && savedPreference !== 'off';
}

export function readPreference(key) {
  try { return localStorage.getItem(key); } catch { return null; }
}

export function savePreference(key, value) {
  try { localStorage.setItem(key, value); } catch { /* 存储不可用时仍保留当前会话的选择。 */ }
}
