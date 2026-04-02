#!/usr/bin/env bash
# 在项目目录下运行此脚本会生成 arg_helper_bundle.zip
set -e
OUT=arg_helper_bundle.zip
FILES="main.cpp facts.json canned_responses.txt README.md CMakeLists.txt create_zip.sh"
if command -v zip >/dev/null 2>&1; then
  zip -r "$OUT" $FILES
  echo "已生成 $OUT"
else
  echo "未检测到 zip 命令，请安装 zip 或手动打包。"
  exit 1
fi
