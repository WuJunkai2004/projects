from .search import search
from .barrage import barrage

import argparse
import json
import os

import random
import time


def ensure_json_suffix(path: str) -> str:
    if not path.lower().endswith('.json'):
        return path + '.json'
    return path


def main():
    parser = argparse.ArgumentParser(description='从 B 站搜索结果抓取弹幕并保存到文件')
    parser.add_argument('keyword', help='搜索关键词（必需）')
    parser.add_argument('-n', '--number', type=int, default=360, help='最多处理的视频数量（默认: 360）')
    parser.add_argument('-o', '--output', default='barrage.json', help='输出文件路径（默认: barrage.json），如果没有 .json 后缀则自动补上')
    parser.add_argument('-k', '--keep-open', action='store_true', help='运行结束后保持浏览器窗口打开（用于调试）')
    parser.add_argument('--min-wait', type=float, default=6.0, help='每个视频之间随机等待的最小秒数（默认 6）')
    parser.add_argument('--max-wait', type=float, default=12.0, help='每个视频之间随机等待的最大秒数（默认 12）')

    args = parser.parse_args()

    out_path = ensure_json_suffix(args.output)

    # 确保输出目录存在
    out_dir = os.path.dirname(out_path)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)

    search_instance = search(args.keyword, keep_open=args.keep_open)

    count = 0
    try:
        for video_url in search_instance.query():
            print(f"正在解析 {video_url}")
            barrage_instance = barrage(video_url)
            if not barrage_instance.fetch():
                print("弹幕获取失败")
                continue

            barrage_data = barrage_instance.get()
            print(f"已获取 {len(barrage_data)} 条弹幕")

            # 每条记录作为一行 JSON 写入（追加模式），保持原有行为
            barrage_str = json.dumps(barrage_data, ensure_ascii=False)
            with open(out_path, 'a', encoding='utf-8') as f:
                print(f"{barrage_str}", file=f)

            count += 1
            print(f"已完成 {count} 个视频的弹幕获取")

            if args.number is not None and count >= args.number:
                print("已达到弹幕获取上限，程序结束")
                break

            # 随机等待，避免触发反爬
            wait_time = random.random() * max(0.0, (args.max_wait - args.min_wait)) + args.min_wait
            time.sleep(wait_time)

    except KeyboardInterrupt:
        print('\n收到中断，正在退出...')
    finally:
        search_instance.stop()


if __name__ == '__main__':
    main()