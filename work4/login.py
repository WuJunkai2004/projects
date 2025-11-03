import server.vercel as vercel
import time

@vercel.register
def login(response):
    #客户端以get的方式请求此接口以保持登录状态
    #返回一个长度8的字符串，和时间有关，尽量包含base64字符
    t = int(time.time())
    s = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-="
    result = ""
    for i in range(8):
        index = (t >> (i * 6)) & 0x3F
        result += s[index]
    response.send_code(200)
    response.send_header("Content-Type", "text/plain; charset=utf-8")
    response.send_text(result)