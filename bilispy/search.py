import selenium.webdriver as webdriver
from selenium.webdriver.common.by import By
from selenium.webdriver.support.ui import WebDriverWait
from selenium.webdriver.support import expected_conditions as EC
from selenium.common.exceptions import TimeoutException

from typing import Generator
import time


def finished(message: str):
    def wrapper(func):
        def inner(*args, **kwargs):
            result = func(*args, **kwargs)
            print(message)
            return result
        return inner
    return wrapper

def is_valid_url(url: str) -> bool:
    return url.startswith("https://www.bilibili.com/video/BV")


class search:
    def __init__(self, keyword="大模型", keep_open=True):
        opts = webdriver.ChromeOptions()
        if keep_open:
            opts.add_experimental_option("detach", True)
        self.driver = webdriver.Chrome(options=opts)
        self.keyword = keyword
        self.__is_started = False
        self.url = f"https://search.bilibili.com/all?keyword={self.keyword}"

    def wait_for_page_load(self, timeout: int = 15, condition=None, poll_frequency: float = 0.5) -> bool:
        if condition is None:
            condition = EC.presence_of_element_located((By.XPATH, "//div[contains(@class, 'video-list')]") )
        try:
            wait = WebDriverWait(self.driver, timeout, poll_frequency=poll_frequency)
            wait.until(lambda d: d.execute_script("return document.readyState") == "complete")
            wait.until(condition)
            return True
        except Exception:
            return False

    @finished("搜索页面已加载")
    def start(self):
        if self.__is_started:
            return
        self.__is_started = True
        self.driver.get(self.url)
        self.wait_for_page_load()

    @finished("已翻到下一页")
    def next_page(self, timeout: int = 10) -> bool:
        try:
            wait = WebDriverWait(self.driver, timeout)
            # 等待下一页按钮可点击
            next_btn = wait.until(EC.element_to_be_clickable((By.XPATH, "//button[text()='下一页']")))
        except TimeoutException:
            return False
        # 在点击前保存对按钮的引用，用于后续等待它变为 stale
        try:
            old_btn = next_btn
            # 确保按钮可见于视窗再点击，减少 click 被拦截的概率
            try:
                self.driver.execute_script("arguments[0].scrollIntoView({block: 'center'});", old_btn)
            except Exception:
                pass
            old_btn.click()
        except Exception:
            # 点击失败
            return False
        # 点击后等待页面更新：优先等待旧按钮变为 stale（被替换或移除），
        # 如果 staleness 超时，则回退等待视频列表元素出现/刷新。
        if self.wait_for_page_load(timeout=15, condition=EC.staleness_of(old_btn)):
            return True
        # 尝试等待视频列表的出现/刷新作为翻页成功的判据
        return self.wait_for_page_load(timeout=15, condition=EC.presence_of_element_located((By.XPATH, "//div[contains(@class, 'video-list')]")))

    def query(self) -> Generator[str, None, None]:
        self.start()
        time.sleep(5)  # 等待页面稳定
        video_list = self.driver.find_element(By.XPATH, "//div[contains(@class, 'video-list')]") # 页面上只有一个包含视频标题的元素，所以使用 find_element
        video_cards = video_list.find_elements(By.CLASS_NAME, "bili-video-card")  # 获取所有视频卡片元素
        print(f"在搜索结果中找到 {len(video_cards)} 个视频:")
        print("-" * 40)
        for card in video_cards:
            # 获取A标签中的href属性
            try:
                a_tag = card.find_element(By.XPATH, ".//a")
                href = a_tag.get_attribute("href")
                if is_valid_url(href):
                    yield href
            except:
                pass
        self.next_page()
        yield from self.query()

    def stop(self):
        try:
            self.driver.quit()
        except Exception:
            pass