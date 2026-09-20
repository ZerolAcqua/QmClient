# QmClient 官网 1.0.0

2026-09-20：用户确认重写公开官网，删除网站文档与更新日志，直接上线且不保留旧站备份。旧 `/docs`、`/changelog` 及子路径重定向至首页。仅替换 `qmclient.icu` 的静态站点和上述路由；域名下现有客户端 API、WebSocket、语音服务继续由原 Nginx 配置提供。

## 结构

- `index.html`：语义化页面内容，下载和社区入口无需 JavaScript 即可使用。
- `assets/site.css`：排版、主题和响应式布局。
- `assets/preview.css`：客户端概念预览与交互展示。
- `assets/motion.css`：动效、静态退化与减少动态效果支持。
- `assets/app.mjs`、`preferences.mjs`、`motion.mjs`、`preview.mjs`：独立交互模块。
- `tests/preferences.test.mjs`：平台识别、下载资产、减少动态效果规则。
- `deploy.py`：上传静态文件，原子切换站点，删除旧站，不生成旧站备份。

此项目无需安装依赖或构建。`python -m http.server 4173 --bind 127.0.0.1 --directory website` 可从仓库根目录预览。

## 设计参考

自主实现原生 CSS/Web Animations 交互，不复制 SmoothUI 源码。参考其 [组件目录](https://smoothui.dev/docs/components)、[Dynamic Island](https://smoothui.dev/docs/components/dynamic-island)、[Animated Tabs](https://smoothui.dev/docs/components/animated-tabs)、[Magnetic Button](https://smoothui.dev/docs/components/magnetic-button)、[Scroll Reveal Paragraph](https://smoothui.dev/docs/components/scroll-reveal-paragraph) 和 [Spring Scale In](https://smoothui.dev/docs/components/spring-scale-in)。

页面中的客户端画面是可交互的概念展示，标有“交互预览”，不承诺与某一发行版截图完全相同。所有下载使用 GitHub Releases；不硬编码当前客户端版本，不把网站版本写入客户端版本文件。

## 验证

2026-09-20 已部署至 <https://qmclient.icu/>，旧站目录已删除，未生成备份。网站版本为 `1.0.0`。

- `python qmclient_scripts/gate/check_gate.py --mode quick`：PASS，11 项通过、0 警告、0 失败。
- `ruff check website/deploy.py website/deploy_remote.py`：通过。新增 Python 脚本与 JavaScript 模块静态语法检查通过。
- 本地浏览器核对首屏排版与语音预览切换；线上首页及新 CSS/JavaScript 返回 200，`.mjs` 正确返回 `text/javascript`。
- `/docs`、`/docs/getting-started`、`/changelog` 及示例子路径返回 301 至首页；旧样式资源返回 404。
- 四个平台的 GitHub 安装包链接均通过 HEAD 检查，返回 200。
- 只读审查发现部署失败后的目录清理边界需收紧，已修正：若交换回旧站失败，不删除仍持有旧站的临时目录。最终未发现阻塞上线的问题。
- 按用户要求补充测试代码，未运行编译与测试套件。手机断点、深色主题与减少动态效果已实现，尚未完成浏览器逐项实看验收。
