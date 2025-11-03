import os
import requests

import server.vercel as vercel
from datetime import datetime

API_KEY = os.getenv("GEMINI_API_KEY", "")
if not API_KEY:
    raise ValueError("GEMINI_API_KEY is not set in environment variables")

BASE_URL = "https://generativelanguage.googleapis.com/v1beta"
MODEL_NAME = "models/gemini-flash-latest"
URL = f"{BASE_URL}/{MODEL_NAME}:generateContent"


system_prompt = """# CORE DIRECTIVE
You are a highly intelligent AI assistant designed to run in a user's command-line interface (CLI). Your name is "Gemini-CLI".

# CONTEXT
- You are interacting with the user via a terminal.
- The user is likely a developer or a technical person.
- Current server time information is provided below. Use it when the user asks about time.

# RULES OF INTERACTION
- Be concise and to the point. Get straight to the answer.
- Provide practical, actionable information.
- For programming questions, provide code that can be directly used.
- Do not use verbose explanations unless the user asks for more details.
- Your knowledge cutoff is around early 2023. For events after that, state that you might not have the latest information.
- CRITICAL: You MUST respond to the user in Simplified Chinese.

# FORMATTING REQUIREMENTS
- Use '#' for titles only. Start your response directly without a title if it's not needed.
- DO NOT use any other Markdown formatting like **bold**, *italics*, or `backticks`.
- Present code directly without enclosing it in Markdown code blocks.
- For lists, use a simple hyphen (-) or numbers (1., 2.).

# EXAMPLES
---
GOOD EXAMPLE:
User: 写一个python http server
Assistant:
# Python HTTP Server
python -m http.server 8000

---
BAD EXAMPLE:
User: 写一个python http server
Assistant:
好的，当然可以！您可以使用 Python 内置的 `http.server` 模块来快速启动一个简单的 HTTP 服务器。这是一个非常方便的功能，适合本地开发和文件共享。

您只需要在终端中运行以下命令：
```python
python -m http.server 8000
这将在 8000 端口上启动一个服务器。希望这对您有帮助！"""


def get_current_time():
    now = datetime.now()
    return now.strftime("%Y-%m-%d %H:%M:%S")

def get_finally_prompt(user_prompt):
    return f'''[System Context]
> Current Datetime: {get_current_time()}
Use the information above to answer the following user question.
It is very important to remember this context and use it when necessary.
[User Question]
{user_prompt}'''

# username:
#   - {role: "user"/"model", content: "text"}
history = {}

@vercel.register
def chat(response, headers, data):
    if('raw' not in data.keys() or 'Auth' not in headers.keys()):
        response.send_code(400)
        response.send_json({"error": "Invalid request format"})
        return
    prompt = data['raw']
    user = headers['Auth']
    if user not in history.keys():
        history[user] = []
    # 添加当前用户输入到历史记录
    history[user].append({"role": "user", "content": prompt})

    # 构建完整的对话历史
    contents = []

    for msg in history[user]:
        contents.append({
            "role": msg["role"],
            "parts": [{"text": msg["content"]}]
        })

    headers = {
        "Content-Type": "application/json",
        "X-goog-api-key": API_KEY
    }
    payload = {
        "contents": contents,
        "systemInstruction": {
            "parts": [{"text": system_prompt}]
        }
    }
    response.send_code(200)
    response.send_header("Content-Type", "text/plain; charset=utf-8")
    try:
        res = requests.post(URL, headers=headers, json=payload)
        res.raise_for_status()
        result = res.json()
        if "candidates" in result and len(result["candidates"]) > 0:
            content = result["candidates"][0]["content"]["parts"][0]["text"]
            # 将AI回复添加到历史记录
            history[user].append({"role": "model", "content": content})
            # 发送回复
            response.send_text(content)
            return
        else:
            # 错误，未收到有效回复，删去
            if history[user][-1]["role"] == "user":
                del history[user][-1]
            response.send_text("No valid response from AI")
            return
    except Exception as e:
        response.send_text(f"Error: {str(e)}")
        return
