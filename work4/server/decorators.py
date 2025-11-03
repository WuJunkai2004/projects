import mimetypes
import json
import os
from functools import wraps

def auto_content_type(cls):
    """
    自动为API类的响应方法注入Content-Type头的装饰器
    """
    original_send_file = cls.send_file
    original_send_text = cls.send_text
    original_send_json = cls.send_json

    @wraps(original_send_file)
    def enhanced_send_file(self, filepath):
        # 自动检测并设置Content-Type
        content_type, _ = mimetypes.guess_type(filepath)
        if content_type:
            self.send_header('Content-Type', content_type)
        return original_send_file(self, filepath)

    @wraps(original_send_text)
    def enhanced_send_text(self, text):
        self.send_header('Content-Type', 'text/plain; charset=utf-8')
        return original_send_text(self, text)

    @wraps(original_send_json)
    def enhanced_send_json(self, data):
        self.send_header('Content-Type', 'application/json; charset=utf-8')
        return original_send_json(self, data)

    cls.send_file = enhanced_send_file
    cls.send_text = enhanced_send_text
    cls.send_json = enhanced_send_json

    return cls