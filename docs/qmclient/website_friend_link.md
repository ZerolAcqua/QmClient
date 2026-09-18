# 官网友链部署记录

## 2026-09-17

- 用户确认在 `https://qmclient.icu/` 公共页脚显示「友链：Shengyan」，目标为 `https://shengyan.art/`。
- 沿用页脚导航样式，在 GitHub 后添加链接；新标签页打开，使用 `rel="noopener noreferrer"`。
- 官网由 Nginx 从 `/var/www/qmclient.icu` 提供静态资源，SSH 用户为 `ubuntu`。
- 当前客户端仓库、相邻项目及服务器常用源码目录未找到官网 Vue 源码。本次定点修改已部署的公共页脚组件，生成 `assets/index-Dk2mjzX5-friend-20260917.js`，原始资源保留。
- 106 个 HTML 入口只更新脚本资源文件名，避免 7 天静态资源缓存继续使用旧页脚；未修改 Nginx 配置或重启服务。

## 验证与后续维护

- 使用 PowerShell `Invoke-WebRequest` 检查首页、`/docs/getting-started/`、`/changelog/`、新脚本和友链目标，均返回 HTTP 200。
- 线上新脚本包含目标 URL 与「友链：Shengyan」。只读逐字节比较确认脚本仅增加一个链接节点，106 个 HTML 仅改变资源引用。
- 内置浏览器读取页面持续超时，未完成视觉与实际点击检查。
- 未执行构建、客户端测试或代码 gate；本地持久改动仅此文档，部署脚本保存在忽略的 `tmp/` 中。本次不涉及客户端版本。
- 下次从官网源码重新发布前，应将该链接补入源码的公共页脚组件，否则完整部署会覆盖此定点修改。

## 回滚

服务器备份：`/home/ubuntu/backups/qmclient-site-friend-20260917T125124Z/before.tar.gz`。

备份包含原脚本及全部 106 个 HTML。需要撤回本次修改时，在服务器执行：

```sh
sudo tar -xzf /home/ubuntu/backups/qmclient-site-friend-20260917T125124Z/before.tar.gz -C /var/www/qmclient.icu
```

回滚恢复旧资源引用；新资源可保留为未引用文件，无需重启 Nginx。若官网之后已有新部署，先核对当前版本再决定是否使用此备份。
