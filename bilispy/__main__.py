from .search  import search
from .barrage import barrage

import json
import random
import time

search_instance = search('大模型', keep_open = False)
count = 0

for video_url in search_instance.query():
    print(f"正在解析 {video_url}")
    barrage_instance = barrage(video_url)
    if(not barrage_instance.fetch()):
        print("弹幕获取失败")
        continue
    
    count += 1
    barrage_data = barrage_instance.get()
    print(f"已获取 {len(barrage_data)} 条弹幕")
    barrage_str = json.dumps(barrage_data, ensure_ascii=False)
    with open(f'barrage.json', 'a', encoding='utf-8') as f:
        print(f"{barrage_str}", file=f)
    
    print(f"已完成 {count} 个视频的弹幕获取")

    if count >= 360:
        print("已达到弹幕获取上限，程序结束")
        break

    # 随机等待 6 到 12 秒，模拟人类行为，避免被反爬虫机制检测到
    wait_time = random.random() * 6 + 6
    time.sleep(wait_time)

search_instance.stop()