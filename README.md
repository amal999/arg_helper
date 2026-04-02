# arg_helper (JSON facts 版本)

功能概述
- 支持中文/英文输入检测与回应生成
- 本地知识库改为 facts.json（JSON），每条事实包含 topic、lang、source、text
- 支持保存/载入常用回应（canned_responses.txt）
- 支持复制到剪贴板（Windows/macOS/Linux）
- 提供 CMake 构建文件与简单的打包脚本

facts.json 说明（示例格式）
[
  {
    "topic": "climate",
    "lang": "zh",
    "source": "IPCC 第六次评估报告 (2021)",
    "text": "全球平均气温在过去一百年中显著上升，多个独立观测数据一致。"
  },
  ...
]

编译（依赖 nlohmann::json）
- Debian/Ubuntu:
    sudo apt update
    sudo apt install nlohmann-json3-dev build-essential cmake
  然后:
    mkdir build && cd build
    cmake ..
    make
  或直接:
    g++ -std=c++17 main.cpp -O2 -o arg_helper -I/usr/include

- 如果没有系统包，可以把 single-header nlohmann/json.hpp 放到项目目录并改 include 路径。

运行
  ./arg_helper
  在提示下输入 topic（例如 climate / vaccines / 工作），然后输入对方陈述（支持中文或英文）。
  使用命令：
    help          显示帮助
    reload facts  重新加载 facts.json
    list canned   列出常用回应
    save n        保存第 n 条回应到常用
    use n         复制第 n 条回应到剪贴板
    exit          退出

打包为 ZIP
- Linux/macOS:
    ./create_zip.sh
- Windows (PowerShell):
    Compress-Archive -Path main.cpp,facts.json,canned_responses.txt,README.md,CMakeLists.txt -DestinationPath arg_helper_bundle.zip

扩展选项（可选，我可以帮你）
- 把 facts.json 加上更多字段：tags、updated_at、confidence 等，并在界面中展示来源 URL。
- 提供一个 GUI（基于 Dear ImGui）并生成可执行安装包。
- 将项目上传到 GitHub（我可以生成 repo 布局与 README 并帮你提交 PR）。
