# coding=utf-8

from http import server
from http import HTTPStatus

import io
import json
import os
import sys
import ast
import inspect
import logging
import threading



class ErrorStatu:
    '''<html><head><meta http-equiv="Content-type" content="text/html; charset=utf-8"><title>%s</title><style type="text/css">
    body {background-color: #f1f1f1;margin: 0;font-family: "Helvetica Neue", Helvetica, Arial, sans-serif;}
    .container { margin: 50px auto 40px auto; width: 600px; text-align: center; }
    h1 { width: 800px; position:relative; left: -100px; letter-spacing: -1px; line-height: 60px; font-size: 60px; font-weight: 100; margin: 0px 0 50px 0; text-shadow: 0 1px 0 #fff; }
    p { color: rgba(0, 0, 0, 0.5); margin: 20px 0; line-height: 1.6; }
    </style></head><body><div class="container"><h1>%d</h1><p><strong>%s</strong></p><p>%s</p></div></body></html>'''
    def __init__(self, Handler, code, more = ''):
        '初始化异常项'
        self.handler = Handler
        self.more = more
        self.Response(code)

    def Error(self, name):
        '命名异常'
        error = name.split('_')
        error = ' '.join(error)
        return error.title()

    def Statu(self, code):
        '通过 code 查找异常'
        for item in HTTPStatus:
            if(code==item):
                return self.Error(item.name)
        raise AttributeError(code)

    def Response(self, code):
        '发生异常页面'
        error = self.Statu(code)
        doc = self.__doc__%(error, code, error, self.more)
        self.handler.send_response(code)
        self.handler.send_text(doc)



class ServerLog:
    def __init__(self, name = 'server'):
        self.log = logging.getLogger(name)
        self.log.setLevel(logging.DEBUG)

        self.formatter = logging.Formatter('%(asctime)s | %(levelname)-7s | %(name)-10s | %(message)s', datefmt='%H:%M:%S')

        self.file_handler = logging.FileHandler('server.log')
        self.file_handler.setFormatter(self.formatter)
        self.log.addHandler(self.file_handler)

        self.cons_handler = logging.StreamHandler(sys.stdout)
        self.cons_handler.setFormatter(self.formatter)
        self.log.addHandler(self.cons_handler)

        self.log.addFilter(self.center)

    def name(self, name):
        '设置日志名称'
        self.log.name = name
        return self

    def center(self, record):
        '将levelname和name居中'
        record.levelname = f"{record.levelname:^7}"
        record.name = f"{record.name:^10}"
        return True

    def __call__(self, *values, sep = ' ', end = '\n', file = None, flush = False, level = logging.INFO):
        if(type(values) == tuple):
            values = sep.join(map(str, values))
        if(type(values) == str):
            values = values.encode('utf-8')
        if(type(values) == bytes):
            values = values.decode('utf-8')
        self.log.log(level, values)
verlog = ServerLog()



class URL(server.SimpleHTTPRequestHandler):
    'URL处理'
    def translate_path(self):
        '获取路径'
        dirname = os.path.abspath( os.getcwd() )
        path = self.path.split('?',1)[0]
        return dirname + path

    def translate_args(self):
        '解析URL附带数据'
        path = self.path
        try:
            args = path.split('?',1)[1]
        except IndexError:
            return {}
        words = args.split('&')
        args = {}
        for word in words:
            word = word.split('=')
            if(len(word)==1):
                word.append('')
            if(word[0] in args.keys()):
                if(type(args[word[0]])!=list):
                    args[word[0]] = [ args[word[0]] ]
                args[word[0]].append( word[1] )
            else:
                args[word[0]] = word[1]
        return args

    def log_message(self, format, *args):
        message = format % args
        verlog.name('server')(message)



class DATA(URL):
    '数据处理'
    def parse_form(self, data):
        '解析 application/x-www-form-urlencoded 数据'
        data = data.decode('utf-8')
        words = data.split('&')
        data = {}
        for word in words:
            word = word.split('=')
            if(len(word)==1):
                word.append('')
            if(word[0] in data.keys()):
                if(type(data[word[0]])!=list):
                    data[word[0]] = [ data[word[0]] ]
                data[word[0]].append( word[1] )
            else:
                data[word[0]] = word[1]
        return data

    def parse_json(self, data):
        '解析 application/json 数据'
        data = data.decode('utf-8')
        data = json.loads(data)
        return data

    def parse_xml(self, data):
        '解析 text/xml 数据'
        data = data.decode('utf-8')
        try:
            import xmltodict as xml
        except ImportError:
            raise TypeError('415 Unsupported Media Type')
        else:
            return xml.parsers(data)
        
    def parse_text(self, data):
        if(type(data) == str):
            data = data.encode('utf-8')
        if(type(data) == bytes):
            data = data.decode('utf-8')
        return {
            'raw': data
        }

    def parse_data(self, data: bytearray):
        def bytesplit(data: bytes, sep: bytes):
            '分割数据块'
            start = 0
            for cur in range(len(data) - len(sep) + 1):
                if data[cur] == sep[0] and data[cur + 1] == sep[1] \
                and data[cur:cur + len(sep)] == sep:
                    yield data[start:cur]
                    start = cur + len(sep)
            yield data[start:]
        def bytepack(data: bytearray):
            '将字节打包成字典'
            result = {
                "Content-Disposition": "",
                "name": "",
                "filename": "",
                "Content-Type": "",
                "Content" : None    # io.BytesIO()
            }
            void_line = 0
            data_start = 0
            for item in bytesplit(data, b'\r\n'):
                if void_line == 2:
                    result["Content"] = io.BytesIO(data[data_start:])
                    break
                else:
                    data_start += len(item) + 2
                if item.startswith(b'Content-Disposition:'):
                    attr = item.split(b';')
                    result["Content-Disposition"] = attr[0]
                    for i in range(1, len(attr)):
                        attr[i] = attr[i].strip()
                        if attr[i].startswith(b'name='):
                            result["name"] = attr[i][6:-1]
                        elif attr[i].startswith(b'filename='):
                            result["filename"] = attr[i][10:-1].decode()
                elif item.startswith(b'Content-Type:'):
                    result["Content-Type"] = item[14:]
                elif item == b'':
                    void_line += 1
            return result

        '解析 multipart/form-data 数据'
        # 获取 boundary
        boundary = ''
        for item in bytesplit(data, b'\r\n'):
            if item.startswith(b'--'):
                boundary = item
                break
        if not boundary:
            # 如果没有找到 boundary，返回错误
            ErrorStatu(self, 400, 'Bad Request')
            return

        # 分割数据块并解析
        result = []
        for part in bytesplit(data, boundary):
            if len(part) <= 2 or part.startswith(b'--'):
                continue
            result.append(bytepack(part))
        return result


#============ data translate ============#
    def translate_post(self):
        '区分 post 类型'
        try:
            length = int(self.headers['content-length'])
        except TypeError:
            return None
        data = self.rfile.read(length)
        if('Content-Type' in self.headers):
            method = self.headers['Content-Type'].split(';')[0]
            if  (method == 'application/json'):
                return self.parse_json(data)
            elif(method == 'application/x-www-form-urlencoded'):
                return self.parse_form(data)
            elif(method == 'application/xml'):
                return self.parse_xml(data)
            elif(method == 'multipart/form-data'):
                return self.parse_data(data)
            elif(method == 'text/plain'):
                return self.parse_text(data)



class COOKIE(DATA):
#============ cookie optional ============#
    def cookie_set(self, item, value):
        cookie = '{}={}aa=ss; Path=/'.format(item, value)
        self.send_header('Set-Cookie', cookie)

    def cookie_set_batch(self, cookies):
        for item in cookies:
            self.cookie_set(item, cookies[item])

    def cookie_delete(self, item):
        cookie = '{}=; Expires=Thu, 01-Jan-1970 00:00:00 GMT; Max-Age=0; Path=/'.format(item)
        self.send_header('Set-Cookie', cookie)

    def cookie_delete_batch(self, items):
        for item in items:
            self.cookie_delete(item)


#============ response ============#
class SEND(COOKIE):
    def send_file(self, path):
        self.end_headers()
        try:
            f = open(path, 'rb')
        except IOError:
            raise IOError('404 Not Found')
        else:
            self.copyfile(f, self.wfile)
            f.close()

    def send_text(self, text):
        self.end_headers()
        enc = sys.getfilesystemencoding()
        encoded = text.encode(enc, 'surrogateescape')
        f = io.BytesIO()
        f.write(encoded)
        f.seek(0)
        self.copyfile(f, self.wfile)
        f.close()

    def send_json(self, data: dict | list):
        for header_str in self._headers_buffer:
            if header_str.startswith(b'Content-Type:'):
                self._headers_buffer.remove(header_str)
                break
        self.send_header('Content-Type', 'application/json')
        self.send_text(json.dumps(data, ensure_ascii=False))

    def send_headers(self, headers):
        for i in headers:
            self.send_header(i,headers[i])

    def send_code(self,code):
        self.send_response(code)


class API(SEND):
    server_version = 'vercelHTTP/1.0'
    def do_GET(self):
        self.method = 'GET'
        self.vercel(self.translate_path(), self.translate_args(), self.headers)

    def do_POST(self):
        self.method = 'POST'
        self.vercel(self.translate_path(), self.translate_post(), self.headers)

    def do_HEAD(self):
        self.method = 'HEAD'
        self.vercel(self.translate_path(), self.translate_post(), self.headers)

    def do_CONNECT(self):
        pass

    def do_OPTIONS(self):
        pass

    def do_FATCH(self):
        pass

    def do_PUT(self):
        pass

    def do_DELETE(self):
        pass


class register:
    def __init__(self, func):
        self.func = func
        self.func_args_names = inspect.getfullargspec(func).args
        self.func.__globals__['handler'] = self

    def vercel(self, response, url, data, headers):
        log_printer = verlog.name(self.func.__name__)
        available_content = {
            'response': response,
            'url': url,
            'data': data,
            'headers': headers
        }
        kwargs = {}
        for name in self.func_args_names:
            if name in available_content:
                kwargs[name] = available_content[name]
        try:
            self.func.__globals__['print'] = log_printer
            self.func(**kwargs)
        except TypeError as e:
            log_printer(f"RuntimeError: {e}", level = logging.ERROR)
            ErrorStatu(self, 500, 'Internal Server Error')


class daemon:
    def __init__(self, func):
        self.func = func
        self.thread = None

    def __call__(self, *args, **kwargs):
        """Call to the function"""
        if self.thread is None or not self.thread.is_alive():
            self.thread = threading.Thread(target=self.func, args=args, kwargs=kwargs)
            self.thread.daemon = True
            self.thread.start()
        else:
            verlog.name('daemon')(f"Thread {self.thread.name} is already running.", level=logging.WARNING)


def start(HandlerClass = API,
          ServerClass  = server.ThreadingHTTPServer,
          protocol = "HTTP/1.0", port = 8000, bind = None):
    import socket
    info = socket.getaddrinfo(bind, port, 
                               type  = socket.SOCK_STREAM,
                               flags = socket.AI_PASSIVE)[0]
    ServerClass.address_family = info[0]
    HandlerClass.protocol_version = protocol
    with ServerClass(info[4], HandlerClass) as httpd:
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            sys.exit(0)



'''HTTP/1.1协议中共定义了八种方法（有时也叫“动作”）来表明Request-URI指定的资源的不同操作方式：
. OPTIONS - 返回服务器针对特定资源所支持的HTTP请求方法。
                   也可以利用向Web服务器发送'*'的请求来测试服务器的功能性。
. HEAD    - 向服务器索要与GET请求相一致的响应，只不过响应体将不会被返回。
                这一方法可以在不必传输整个响应内容的情况下，就可以获取包含在响应消息头中的元信息。
. GET     - 向特定的资源发出请求。
                注意：GET方法不应当被用于产生“副作用”的操作中，例如在web app.中。
                其中一个原因是GET可能会被网络蜘蛛等随意访问。
. POST    - 向指定资源提交数据进行处理请求（例如提交表单或者上传文件）。
                数据被包含在请求体中。POST请求可能会导致新的资源的建立和/或已有资源的修改。
. PUT     - 向指定资源位置上传其最新内容。
. DELETE  - 请求服务器删除Request-URI所标识的资源。
. TRACE   - 回显服务器收到的请求，主要用于测试或诊断。
. CONNECT - HTTP/1.1协议中预留给能够将连接改为管道方式的代理服务器。'''