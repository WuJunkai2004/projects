# Bili Spy

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Bili Spy 是一个为Bilibili平台设计的、基于Python的视频弹幕抓取工具。它能根据指定关键词自动搜索视频，并高效地抓取和保存这些视频的弹幕数据。

## 工作流程

项目通过模拟真实用户的操作习惯，实现了一套完整的“搜索-抓取-保存”流程：

1.  **启动浏览器**: 使用 `Selenium` 启动一个Chrome浏览器，并访问Bilibili的视频搜索页面。
2.  **执行搜索**: 根据预设的关键词（默认为“大模型”）执行搜索，并等待搜索结果页面加载完成。
3.  **解析结果**: 智能解析页面中的视频链接，并逐个提取有效的视频URL。
4.  **自动翻页**: 在处理完当前页的全部视频后，程序会自动点击“下一页”按钮，并等待新页面加载，以实现连续抓取。
5.  **获取CID**: 对于每一个视频URL，程序会访问该页面，并从页面源码中通过正则表达式解析出弹幕API所需的关键ID（`cid`）。
6.  **抓取弹幕**: 利用获取到的 `cid`，访问Bilibili的弹幕接口（一个`.xml`文件），并下载完整的弹幕数据。
7.  **数据处理与保存**: 使用 `xmltodict` 将XML格式的弹幕数据转换为Python字典，提取弹幕文本，并以JSON格式追加保存在 `barrage.json` 文件中。
8.  **模拟人类行为**: 在每次抓取之间，程序会随机暂停6到12秒，以降低被Bilibili反爬虫机制发现的风险。

## 特色功能

- **健壮的页面加载机制**: 通过 `WebDriverWait` 和对 `document.readyState` 的判断，确保页面元素完全加载后再执行后续操作，提高了程序的稳定性。
- **智能翻页逻辑**: 在点击“下一页”后，通过判断旧按钮的 `staleness` (是否已过时) 来确认页面已成功跳转，这是一个比简单等待更可靠的策略。
- **高效的链接迭代**: `query` 方法被实现为一个生成器 (`yield`)，它逐个返回视频链接，而不是一次性加载所有链接到内存中，这在处理大量视频时极大地节省了内存。
- **模块化设计**: `search` 和 `barrage` 功能被清晰地分离到不同的模块中，使得代码更易于理解和维护。

## 先决条件

- **Python 3.x**
- **Google Chrome 浏览器**
- **ChromeDriver**: 需要与您的Chrome浏览器版本相匹配。请确保 `chromedriver.exe` 位于您的系统 `PATH` 中，或者与您的脚本在同一目录下。

## 安装

1.  克隆本仓库到本地：
    ```bash
    git clone https://github.com/your-username/BiliSpy.git
    ```
2.  进入项目目录并安装所需的依赖库：
    ```bash
    pip install -r requirements.txt
    ```

## 如何使用

直接以模块方式运行项目即可启动默认的抓取任务：

```bash
python -m bilispy
```

程序将开始执行搜索和抓取，您会在控制台看到实时的进度信息。抓取到的弹幕数据会以JSON格式，每行一个视频的形式，被追加到根目录下的 `barrage.json` 文件中。

### 自定义配置

如果您想修改默认行为，可以直接编辑 `bilispy/__main__.py` 文件：

- **修改搜索关键词**:
  ```python
  # 将 '大模型' 替换为您想搜索的任何关键词
  search_instance = search('你的关键词', keep_open=False)
  ```

- **修改抓取视频数量**:
  ```python
  # 修改 count 的上限值，例如从360改为100
  if count >= 100:
      print("已达到弹幕获取上限，程序结束")
      break
  ```

## 核心模块解析

### `bilispy.search`

该模块负责所有与浏览器自动化和视频搜索相关的任务。

- `search` 类: 封装了 `selenium.webdriver` 的核心功能。
- `wait_for_page_load()`: 一个关键的辅助函数，它会等待直到页面加载完成并且视频列表元素出现，确保后续操作的可靠性。
- `next_page()`: 实现了智能翻页。它不仅仅是点击按钮，还会通过 `EC.staleness_of(old_btn)` 来验证页面是否真的发生了跳转，这是一种非常可靠的翻页判断机制。
- `query()`: 作为生成器，它在遍历视频列表时逐一 `yield` 视频链接，并在完成一页后调用 `next_page()` 和自身，实现递归式的无限滚动抓取。

### `bilispy.barrage`

该模块专注于从单个视频页面中提取弹幕。

- `barrage` 类: 接收一个视频URL作为输入。
- `fetch()`: 核心方法。它首先通过 `requests` 获取视频页面的HTML，然后使用正则表达式 `r'"cid":.(\d+)'` 从中匹配并提取出 `cid`。之后，它构造弹幕API的URL (`https://comment.bilibili.com/{cid}.xml`) 并获取XML数据。
- `get()`: 负责将 `fetch()` 获取到的原始弹幕数据（XML格式）解析为纯文本列表，并利用 `self.cached` 提供缓存功能，避免重复解析。

## 许可证

本项目采用 MIT 许可证。详情请参阅 [LICENSE](LICENSE) 文件。