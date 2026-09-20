#!/usr/bin/env python3
"""由 deploy.py 通过 SSH 标准输入调用；只操作已确认的网站目录。"""

from __future__ import annotations

import ctypes
import os
import shutil
import subprocess
import sys
import tarfile
from pathlib import Path


def main() -> None:
	archive = Path(sys.argv[1])
	stage = Path(sys.argv[2])
	site = Path("/var/www/qmclient.icu")
	config = Path("/etc/nginx/sites-available/qmclient.icu")
	if not site.is_dir() or site.is_symlink():
		raise ValueError("站点目录与预期不符")
	if stage.parent != site.parent or not stage.name.startswith(".qmclient-site-") or stage.exists():
		raise ValueError("临时目录不符合预期")
	if archive.parent != Path("/home/ubuntu") or not archive.name.startswith(".qmclient-website-"):
		raise ValueError("上传目录不符合预期")
	original = config.read_bytes()
	routes = """
    # 旧文档与日志已下线，统一返回新版首页。
    location = /docs { return 301 /; }
    location ^~ /docs/ { return 301 /; }
    location = /changelog { return 301 /; }
    location ^~ /changelog/ { return 301 /; }

    # 原生模块使用正确的 MIME 类型，旧资源路径不回退到首页。
    location ^~ /assets/ {
        types { text/javascript mjs js; text/css css; image/svg+xml svg; }
        try_files $uri =404;
        expires 7d;
        add_header Cache-Control "public, max-age=604800";
    }
"""
	marker = b"    location / {\n"
	if "# 旧文档与日志已下线" not in original.decode("utf-8"):
		if original.count(marker) != 1:
			raise ValueError("主页路由无法唯一定位")
		updated = original.replace(marker, routes.encode("utf-8") + marker)
	else:
		updated = original

	# 原子交换目录，避免访问者撞上站点暂时缺失的窗口。
	libc = ctypes.CDLL(None, use_errno=True)
	rename = libc.renameat2
	rename.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_uint]
	rename.restype = ctypes.c_int

	def exchange() -> None:
		if rename(-100, os.fsencode(stage), -100, os.fsencode(site), 2):
			raise OSError(ctypes.get_errno(), "无法原子替换站点")

	swapped = False
	changed_config = False
	cleanup_stage = True
	try:
		stage.mkdir(mode=0o755)
		with tarfile.open(archive) as package:
			package.extractall(stage, filter="data")
		if not (stage / "index.html").is_file() or not (stage / "assets/app.mjs").is_file():
			raise ValueError("上传内容缺少网站入口")
		if (stage / "docs").exists() or (stage / "changelog").exists():
			raise ValueError("新站不得含有已下线的文档或日志")
		for path in stage.rglob("*"):
			path.chmod(0o755 if path.is_dir() else 0o644)
		if updated != original:
			config.write_bytes(updated)
			changed_config = True
		subprocess.run(["nginx", "-t"], check=True)
		exchange()
		swapped = True
		subprocess.run(["systemctl", "reload", "nginx"], check=True)
	except BaseException:
		if swapped:
			cleanup_stage = False
			exchange()
			cleanup_stage = True
		if changed_config:
			config.write_bytes(original)
			subprocess.run(["nginx", "-t"], check=True)
			subprocess.run(["systemctl", "reload", "nginx"], check=True)
		raise
	finally:
		# 成功时此目录为旧站；失败时为未上线的新站。配置原文仅驻留内存。
		if cleanup_stage and stage.exists():
			shutil.rmtree(stage)
		archive.unlink(missing_ok=True)
	print("Deployed qmclient.icu; previous website removed; no backup retained.")


if __name__ == "__main__":
	main()
