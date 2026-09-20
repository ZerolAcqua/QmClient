#!/usr/bin/env python3
"""将静态官网上传到已确认的站点，不保留旧站备份。"""

from __future__ import annotations

import argparse
import shlex
import subprocess
import tarfile
import tempfile
from datetime import datetime, timezone
from pathlib import Path


def main() -> None:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--key", type=Path, required=True)
	args = parser.parse_args()
	if not args.key.is_file():
		parser.error("指定的 SSH 私钥不存在")
	source = Path(__file__).resolve().parent
	token = datetime.now(timezone.utc).strftime("%Y%m%d%H%M%S%f")
	remote_archive = f"/home/ubuntu/.qmclient-website-{token}.tar.gz"
	stage = f"/var/www/.qmclient-site-{token}"
	connection = ["-i", str(args.key), "-o", "BatchMode=yes", "-o", "ConnectTimeout=15"]
	host = "ubuntu@42.194.185.210"
	temporary_root = source.parent / "tmp"
	temporary_root.mkdir(exist_ok=True)
	with tempfile.TemporaryDirectory(prefix="website-deploy-", dir=temporary_root) as temp:
		archive = Path(temp) / "website.tar.gz"
		with tarfile.open(archive, "w:gz") as package:
			package.add(source / "index.html", arcname="index.html")
			package.add(source / "assets", arcname="assets")
		subprocess.run(["scp", *connection, str(archive), f"{host}:{remote_archive}"], check=True)
		command = shlex.join(["sudo", "-n", "python3", "-", remote_archive, stage])
		subprocess.run(
			["ssh", *connection, host, command],
			input=(source / "deploy_remote.py").read_bytes(),
			check=True,
		)


if __name__ == "__main__":
	main()
