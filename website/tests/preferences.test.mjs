import assert from 'node:assert/strict';
import test from 'node:test';
import { detectPlatform, downloadFor, motionAllowed } from '../assets/preferences.mjs';

test('Android 的 Linux 标识不会选中桌面 Linux 安装包', () => {
  assert.equal(detectPlatform('Mozilla/5.0 (Linux; Android 14; Pixel 8)'), 'android');
});

test('识别桌面平台，未知设备使用发布页而非错误安装包', () => {
  assert.equal(detectPlatform('Windows NT 10.0; Win64; x64'), 'windows');
  assert.equal(detectPlatform('Macintosh; Intel Mac OS X 10_15_7'), 'macos');
  assert.equal(detectPlatform('X11; Linux x86_64'), 'linux');
  assert.equal(detectPlatform('iPhone; CPU iPhone OS 18_0 like Mac OS X'), null);
  assert.equal(detectPlatform(''), null);
  assert.equal(downloadFor(null), 'https://github.com/wxj881027/QmClient/releases/latest');
});

test('各平台对应真实发布资产，不把签名或更新描述当成安装包', () => {
  assert.ok(downloadFor('windows').endsWith('/QmClient-windows.zip'));
  assert.ok(downloadFor('macos').endsWith('/QmClient-macOS.dmg'));
  assert.ok(downloadFor('linux').endsWith('/QmClient-ubuntu.tar.xz'));
  assert.ok(downloadFor('android').endsWith('/QmClient-android.apk'));
});

test('系统减少动态效果和用户关闭动效都优先于默认开启动效', () => {
  assert.equal(motionAllowed(false, null), true);
  assert.equal(motionAllowed(false, 'off'), false);
  assert.equal(motionAllowed(true, 'on'), false);
  assert.equal(motionAllowed(true, null), false);
  assert.equal(motionAllowed(false, 'on'), true);
});
