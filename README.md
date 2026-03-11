HtmlImagePacker 📦

A lightning-fast, zero-dependency C++ CLI tool for Windows that converts remote HTML images into offline, self-contained Base64 JavaScript databases. Say goodbye to broken images and local CORS issues!

一款极速、零依赖的 Windows C++ 命令行工具。它能将 HTML 中的远程图片抓取并打包为本地 Base64 JavaScript 数据库，让你拔掉网线也能“双击完美离线浏览网页”！

Why HtmlImagePacker?

When saving HTML files locally, browsers enforce strict security policies (CORS). If you try to fetch local resources using AJAX/Fetch from a file:/// URL, it gets blocked. This means traditional "ZIP + JS unpack" methods require setting up a local server (python -m http.server).

HtmlImagePacker solves this beautifully:
It downloads all remote images, encodes them into Base64 within a pure .js file, and injects a tiny script into your HTML. Because <script> tags are exempt from local CORS restrictions, you can just double-click the HTML to view it perfectly offline, without any local server!

✨ Key Features

🚀 Zero Dependencies: Written in pure C++ using native Windows APIs (urlmon.lib, crypt32.lib). No Node.js, Python, or heavy runtimes required.

🚫 Network Request Blocking: Smartly replaces <img src="http..."> with a 1x1 transparent GIF and a data-src attribute. This completely prevents the browser from trying to ping dead/remote URLs when offline, ensuring instant load times.

🧠 Smart 2-Pass Deduplication (Storage Saver):

Pass 1: Scans all HTML files in a directory to build a dependency graph.

Pass 2: If an image is used in multiple HTML files, it is downloaded exactly once and placed into a single emoji_shared.js. Unique images go into private emoji_[name].js.

Result: Massive disk space savings. 10,000 HTMLs using the same logo will only store that logo's Base64 string once.

📂 Batch Processing: Drop a single file or an entire directory of HTMLs.

🛠️ How it Works (Output Structure)

Input:
📂 MyWebPages
 ├── page1.html (uses logo.png, img1.jpg)
 └── page2.html (uses logo.png, img2.jpg)

Output after running `HtmlImagePacker.exe MyWebPages`:
📂 MyWebPages
 ├── page1.html (Patched)
 ├── page2.html (Patched)
 └── 📂 emoji_js
      ├── emoji_shared.js (Contains logo.png)
      ├── emoji_page1.js  (Contains img1.jpg)
      └── emoji_page2.js  (Contains img2.jpg)


🚀 Usage

Just pass the target file or folder as an argument:

# Process a single file
HtmlImagePacker.exe "C:\path\to\index.html"

# Process a whole directory
HtmlImagePacker.exe "C:\path\to\html_folder"


🔨 Build Instructions

Open the source code in Visual Studio 2025 (or VS 2019/2022) as a C++ Console Application.
Make sure to compile for Release (x64 or x86). No external libraries (vcpkg/NuGet) are needed.

<a id="中文说明"></a> 🇨🇳 中文说明

为什么要做这个工具？

当我们把包含远程图片的 HTML 网页保存到本地时，往往面临两个痛点：断网后图片裂开；如果要打包成本地依赖，浏览器对于 file:/// 协议有极其严格的跨域安全限制（导致前端 fetch 无法读取同目录下的图片资源或压缩包），通常逼着你必须启动一个本地 Web Server 才能看。

HtmlImagePacker 完美优雅地解决了这个问题：
它将所有图片下载并转码为 Base64，写入单纯的 .js 字典文件。利用 <script> 标签天然不受本地跨域限制的特性，配合注入极简的原生 JS 渲染逻辑，实现了拷到任何电脑上，拔掉网线，直接双击 HTML 就能完美呈现图文的极致体验！

✨ 核心特色 (Selling Points)

🚀 纯血原生，零依赖：使用纯 C++ 配合 Windows 原生 API (urlmon & crypt32) 编写。无需安装 Python 环境，无需 Node.js，编译出一个几百 KB 的免安装绿色 exe，随时随地可用。

🚫 物理级网络阻断：不仅是替换图片，程序会把 HTML 中原本的 src="http..." 改写为 data-src，并填入一个 1x1 像素的透明本地图片。这彻底阻止了离线打开网页时，浏览器因为尝试连接失效网址而导致的卡顿，做到秒开。

🧠 两遍扫描，极限去重算法 (2-Pass Deduplication)：

第一遍扫描：像雷达一样极速解析目录下所有 HTML，统计每张图片被多少个网页引用。

第二遍扫描：将被多个网页引用的图片智能提取到 emoji_shared.js（公共库），被单个网页独享的图片存入 emoji_[文件名].js（私有库）。

结果：一万个网页共用同一个 Logo 和背景图，在硬盘上永远只存一份 Base64 数据，彻底告别批量打包造成的存储空间爆炸！

📂 全自动批处理：支持拖拽单个文件，也支持直接喂给它一个包含成百上千 HTML 的文件夹。

🛠️ 处理后的文件结构示例

假设你有这样一个目录：
📂 MyWebPages
 ├── page1.html (引用了 logo.png 和 图A.jpg)
 └── page2.html (引用了 logo.png 和 图B.jpg)

执行命令 `HtmlImagePacker.exe MyWebPages` 后，目录会变成：
📂 MyWebPages
 ├── page1.html (已被自动注入离线加载脚本)
 ├── page2.html (已被自动注入离线加载脚本)
 └── 📂 emoji_js
      ├── emoji_shared.js (提取出的公共库，包含 logo.png)
      ├── emoji_page1.js  (私有库，包含 图A.jpg)
      └── emoji_page2.js  (私有库，包含 图B.jpg)


🚀 使用方法

可以在命令行调用，或者直接把文件/文件夹拖拽到 exe 图标上执行：

# 处理单个文件
HtmlImagePacker.exe "C:\path\to\index.html"

# 批量处理整个目录
HtmlImagePacker.exe "C:\path\to\html_folder"


🔨 编译指南

本项目使用标准 C++ 编写。只需在 Visual Studio 2025 (向下兼容 VS 2019/2022) 中创建一个空的 C++ 控制台项目，粘贴代码并选择 Release 模式编译即可。不依赖任何第三方第三方库 (如 Boost 或 libcurl)。

If you find this tool helpful, please give it a ⭐ on GitHub! / 如果这个工具对你有帮助，欢迎点个 ⭐ 支持一下！
