import re
import requests
import xmltodict

header = {
    'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36',
    'Accept-Language': 'zh-CN,zh;q=0.9',
    'Accept-Encoding': 'gzip, deflate, br',
    'Referer': 'https://www.bilibili.com/',
}

class barrage:
    def __init__(self, video_url):
        self.video_url = video_url
        self.barrages = {}
        self.cached   = []

    def fetch(self) -> bool:
        # get cid
        try:
            html = requests.get(self.video_url, headers = header)
            html.raise_for_status()
            html_text = html.text
        except:
            return False
        cid_pattern = r'"cid":.(\d+)'
        cid_match = re.search(cid_pattern, html_text)
        if cid_match:
            cid_text = cid_match.group()
        else:
            return False
        cid = re.findall(r'\d+', cid_text)[0]
        # get barrage data
        barrage_url = f'https://comment.bilibili.com/{cid}.xml'
        try:
            barrage_response = requests.get(barrage_url, headers = header)
            barrage_response.raise_for_status()
            barrage_response.encoding = 'utf-8'
            barrage_text = barrage_response.text
            barrage_dict = xmltodict.parse(barrage_text)
            self.barrages = barrage_dict['i']['d']
            return True
        except:
            return False

    def get(self):
        if self.cached:
            return self.cached
        for content in self.barrages:
            self.cached.append(content['#text'])
        return self.cached