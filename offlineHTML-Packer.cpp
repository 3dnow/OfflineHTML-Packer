#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <regex>
#include <map>
#include <set>
#include <filesystem>
#include <windows.h>
#include <urlmon.h>
#include <wincrypt.h>

#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "crypt32.lib")

namespace fs = std::filesystem;

// 辅助函数：UTF-8 字符串转宽字符串 (供 Windows API 使用)
std::wstring Utf8ToWstring(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

// 辅助函数：根据 URL 后缀猜测 MIME 类型
std::string GetMimeType(const std::string& url) {
    std::string lowerUrl = url;
    std::transform(lowerUrl.begin(), lowerUrl.end(), lowerUrl.begin(), ::tolower);
    if (lowerUrl.find(".png") != std::string::npos) return "image/png";
    if (lowerUrl.find(".gif") != std::string::npos) return "image/gif";
    if (lowerUrl.find(".webp") != std::string::npos) return "image/webp";
    if (lowerUrl.find(".svg") != std::string::npos) return "image/svg+xml";
    return "image/jpeg"; // 默认 fallback
}

// 辅助函数：下载 URL 到内存并转换为 Base64 字符串
std::string DownloadAndEncodeBase64(const std::string& url) {
    std::wstring wUrl = Utf8ToWstring(url);
    IStream* pStream = nullptr;

    std::cout << "  正在下载: " << url << "..." << std::endl;
    HRESULT hr = URLOpenBlockingStreamW(nullptr, wUrl.c_str(), &pStream, 0, nullptr);
    if (FAILED(hr) || !pStream) {
        std::cerr << "  [错误] 下载失败: " << url << std::endl;
        return "";
    }

    // 从 Stream 读取数据到 buffer
    std::vector<BYTE> buffer;
    BYTE temp[4096];
    ULONG bytesRead;
    while (SUCCEEDED(pStream->Read(temp, sizeof(temp), &bytesRead)) && bytesRead > 0) {
        buffer.insert(buffer.end(), temp, temp + bytesRead);
    }
    pStream->Release();

    if (buffer.empty()) return "";

    // 使用 Windows API 进行 Base64 编码 (无换行符)
    DWORD destSize = 0;
    CryptBinaryToStringA(buffer.data(), (DWORD)buffer.size(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &destSize);

    std::string base64Str(destSize, '\0');
    CryptBinaryToStringA(buffer.data(), (DWORD)buffer.size(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &base64Str[0], &destSize);

    // 移除尾部可能多余的 null 终结符
    if (!base64Str.empty() && base64Str.back() == '\0') {
        base64Str.pop_back();
    }

    return base64Str;
}

// =========================================================================
// 核心分离: 提取 URL 和 写入 JS 逻辑
// =========================================================================

// 从 HTML 中提取所有引用的图片 URL
std::set<std::string> ExtractUrlsFromHtml(const std::string& htmlContent) {
    std::set<std::string> urls;
    std::regex img_regex(R"(<img[^>]+(?:src|data-src)\s*=\s*(["'])(http[s]?://[^"']+)\1)", std::regex_constants::icase);
    auto words_begin = std::sregex_iterator(htmlContent.begin(), htmlContent.end(), img_regex);
    auto words_end = std::sregex_iterator();
    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        urls.insert((*i)[2].str());
    }
    return urls;
}

// 将数据库写入 JS 文件 (通过 Object.assign 支持平滑合并多个 JS 文件)
void WriteJSDB(const std::string& jsOutputPath, const std::map<std::string, std::string>& db) {
    std::ofstream jsFile(jsOutputPath, std::ios::binary);
    if (!jsFile) {
        std::cerr << "无法创建 JS 文件: " << jsOutputPath << std::endl;
        return;
    }

    jsFile << "// Auto-generated local image database\n";
    jsFile << "window.LocalImageDB = window.LocalImageDB || {};\n";
    jsFile << "Object.assign(window.LocalImageDB, {\n";

    bool first = true;
    for (const auto& pair : db) {
        if (!first) jsFile << ",\n";
        jsFile << "    \"" << pair.first << "\": \"" << pair.second << "\"";
        first = false;
    }
    jsFile << "\n});\n";
    jsFile.close();
}

// =========================================================================
// 功能 2：修改 HTML 注入 JS 脚本，实现无缝替换 (支持注入多个 JS)
// =========================================================================
void PatchHTML(const std::string& htmlPath, const std::vector<std::string>& scriptSrcs) {
    std::ifstream file(htmlPath, std::ios::binary);
    if (!file) {
        std::cerr << "无法打开 HTML 文件: " << htmlPath << std::endl;
        return;
    }
    std::string htmlContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    // 核心修改：将所有的 src="http..." 替换为 data-src="http..." 阻断网络请求
    std::regex img_src_regex(R"((<img[^>]*?)\bsrc\s*=\s*(["'])(http[s]?://[^"']+)\2)", std::regex_constants::icase);
    std::string replacement = "$1data-src=$2$3$2 src=$2data:image/gif;base64,R0lGODlhAQABAAD/ACwAAAAAAQABAAACADs=$2";
    htmlContent = std::regex_replace(htmlContent, img_src_regex, replacement);

    std::string injectCode = "\n    <!-- Offline image support script -->\n";
    bool needInject = false;

    // 动态判断并注入所需脚本 (避免重复注入)
    for (const auto& src : scriptSrcs) {
        if (htmlContent.find(src) == std::string::npos) {
            injectCode += "    <script src=\"" + src + "\"></script>\n";
            needInject = true;
        }
    }

    // 仅注入一次执行逻辑
    if (htmlContent.find("LocalImageDB[originalUrl]") == std::string::npos) {
        injectCode +=
            "    <script>\n"
            "        (function() {\n"
            "            function replaceImgs() {\n"
            "                if (!window.LocalImageDB) return;\n"
            "                document.querySelectorAll('img[data-src]').forEach(img => {\n"
            "                    let originalUrl = img.getAttribute('data-src');\n"
            "                    if (window.LocalImageDB[originalUrl]) {\n"
            "                        img.src = window.LocalImageDB[originalUrl];\n"
            "                        img.removeAttribute('data-src');\n"
            "                    }\n"
            "                });\n"
            "            }\n"
            "            if (document.readyState === 'loading') {\n"
            "                document.addEventListener('DOMContentLoaded', replaceImgs);\n"
            "            } else {\n"
            "                replaceImgs();\n"
            "            }\n"
            "        })();\n"
            "    </script>\n";
        needInject = true;
    }

    if (needInject) {
        size_t bodyEndPos = htmlContent.rfind("</body>");
        if (bodyEndPos != std::string::npos) {
            htmlContent.insert(bodyEndPos, injectCode);
        }
        else {
            htmlContent += injectCode;
        }

        std::ofstream outFile(htmlPath, std::ios::binary);
        outFile << htmlContent;
        outFile.close();
        std::cout << "成功: HTML 文件已修改并注入相关脚本" << std::endl;
    }
    else {
        std::cout << "提示: HTML 已包含相关逻辑，无需重复修改。" << std::endl;
    }
}

// =========================================================================
// 主函数测试入口 (采用智能 2-Pass 扫描提取公共图片)
// =========================================================================
int main(int argc, char* argv[]) {
    CoInitialize(nullptr);

    if (argc < 2) {
        std::cerr << "用法: " << argv[0] << " <目标HTML文件路径 或 HTML所在目录>" << std::endl;
        return 1;
    }

    fs::path targetPath = fs::path(argv[1]);
    if (!fs::exists(targetPath)) {
        std::cerr << "错误：找不到路径 " << targetPath.string() << std::endl;
        return 1;
    }

    // 收集所有需要处理的 HTML 文件
    std::vector<fs::path> targetHtmls;
    if (fs::is_directory(targetPath)) {
        std::cout << "检测到目录模式，正在收集 HTML 文件..." << std::endl;
        for (const auto& entry : fs::directory_iterator(targetPath)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".html" || ext == ".htm") {
                    targetHtmls.push_back(entry.path());
                }
            }
        }
    }
    else if (fs::is_regular_file(targetPath)) {
        std::cout << "检测到单文件模式..." << std::endl;
        targetHtmls.push_back(targetPath);
    }

    // ==================================================================
    // 阶段 1 (Pass 1)：不下载，仅解析 URL，统计各个图片出现的频率
    // ==================================================================
    std::map<std::string, int> urlFileCount;
    std::map<std::string, std::set<std::string>> htmlToUrls;

    std::cout << "\n--- 阶段 1: 分析网页依赖关系 ---" << std::endl;
    for (const auto& path : targetHtmls) {
        std::ifstream file(path, std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        auto urls = ExtractUrlsFromHtml(content);
        htmlToUrls[path.string()] = urls;
        for (const auto& url : urls) {
            urlFileCount[url]++; // 记录该图片被几个不同的 HTML 引用了
        }
    }

    // ==================================================================
    // 阶段 2 (Pass 2)：下载图片，智能切分 "公共库" 和 "私有库"
    // ==================================================================
    std::cout << "\n--- 阶段 2: 智能下载与分离入库 ---" << std::endl;

    std::map<std::string, std::string> globalImageCache; // URL -> Base64 内存缓存
    std::map<std::string, std::string> sharedDB;         // 收集公共图片数据

    fs::path emojiDir;
    std::string sharedScriptSrc;

    if (fs::is_directory(targetPath)) {
        emojiDir = targetPath / "emoji_js";
        fs::create_directories(emojiDir);
        sharedScriptSrc = "emoji_js/emoji_shared.js"; // 目录模式的公共库路径
    }
    else {
        emojiDir = targetPath.parent_path();
        sharedScriptSrc = "emoji_shared.js";          // 即使是单文件，逻辑上也准备好
    }

    for (const auto& path : targetHtmls) {
        std::cout << "\n>>> 处理文件: " << path.filename().string() << std::endl;
        const auto& urls = htmlToUrls[path.string()];

        std::map<std::string, std::string> localDB; // 当前 HTML 私有图片
        std::vector<std::string> scriptsToInject;
        bool usesShared = false;

        for (const auto& url : urls) {
            // 如果内存里没有，发起下载
            if (globalImageCache.find(url) == globalImageCache.end()) {
                std::string b64 = DownloadAndEncodeBase64(url);
                if (!b64.empty()) {
                    globalImageCache[url] = "data:" + GetMimeType(url) + ";base64," + b64;
                }
                else {
                    globalImageCache[url] = ""; // 下载失败，标记为空以防重复尝试
                }
            }

            std::string data = globalImageCache[url];
            if (data.empty()) continue; // 跳过失败的图

            // 核心逻辑：出现 >1 次即划归为公共库，否则为私有库
            if (urlFileCount[url] > 1) {
                sharedDB[url] = data;
                usesShared = true;
            }
            else {
                localDB[url] = data;
            }
        }

        // 组装需要注入的 JS
        if (usesShared) {
            scriptsToInject.push_back(sharedScriptSrc);
        }

        if (!localDB.empty()) {
            std::string jsFileName = "emoji_" + path.stem().string() + ".js";
            fs::path localJsPath = emojiDir.empty() ? fs::path(jsFileName) : emojiDir / jsFileName;
            WriteJSDB(localJsPath.string(), localDB);

            std::string localScriptSrc = fs::is_directory(targetPath) ? ("emoji_js/" + jsFileName) : jsFileName;
            scriptsToInject.push_back(localScriptSrc);
            std::cout << "  -> 提取 " << localDB.size() << " 张特有图片到 " << jsFileName << std::endl;
        }

        if (!scriptsToInject.empty()) {
            PatchHTML(path.string(), scriptsToInject);
        }
        else {
            std::cout << "--- 没有提取到有效图片，跳过修改 HTML ---" << std::endl;
        }
    }

    // 收尾工作：将所有拦截的公共图片一次性写入共享库
    if (!sharedDB.empty()) {
        fs::path sharedJsPath = emojiDir.empty() ? fs::path("emoji_shared.js") : emojiDir / "emoji_shared.js";
        WriteJSDB(sharedJsPath.string(), sharedDB);
        std::cout << "\n==========================================" << std::endl;
        std::cout << "成功: 提炼出 " << sharedDB.size() << " 张多文件共享图片，生成 -> " << sharedJsPath.filename().string() << std::endl;
        std::cout << "==========================================" << std::endl;
    }

    CoUninitialize();
    std::cout << "\n全部处理完成！存储空间已被极致压缩。" << std::endl;
    return 0;
}