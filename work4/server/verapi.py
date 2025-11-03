try:
    from imp import load_source
except ImportError:
    from importlib.util import spec_from_file_location, module_from_spec
    load_source = lambda name, path: module_from_spec(spec_from_file_location(name, path))

import os
import server.vercel as vercel

# 模块缓存字典
_module_cache = {}

def main(handler = vercel.API, port = 8000):
    vercel.start(
        HandlerClass = handler,
        port = port
    )

class handler(vercel.API):
    def vercel(self, url, data, headers):
        if(os.path.isdir(url)):
            self.send_code(200)
            for home in ['index.html','index.htm']:
                if(os.path.isfile(url + home)):
                    self.send_file(url + home)
                    return
            self.send_text( '\n'.join(os.listdir(url)) )
            return

        if(os.path.isfile(url)):
            if(os.path.splitext(url)[1]=='.py'):
                return vercel.ErrorStatu(self, 403)
            self.send_code(200)
            self.send_file(url)
            return

        if(os.path.isfile(url + '.py')):
            mod = _module_cache.get(url)

            if not mod:
                try:
                    mod = load_source(url, url + '.py')
                    _module_cache[url] = mod
                except Exception as e:
                    if url in _module_cache:
                        del _module_cache[url]
                    vercel.verlog(f"Error loading module {url}: {e}")
                    return vercel.ErrorStatu(self, 500)

            if mod:
                mod_handler = getattr(mod, 'handler', None)
                if mod_handler is None:
                    return vercel.ErrorStatu(self, 503, 'Failed to load handler from module')
                vercel_func = getattr(mod_handler, 'vercel', None)
                if vercel_func is None:
                    return vercel.ErrorStatu(self, 503, 'Handler has no vercel method')
                try:
                    vercel_func(self, url, data, headers)
                except Exception as e:
                    print(e)
                    return vercel.ErrorStatu(self, 503)
            return

        vercel.ErrorStatu(self, 404)


if(__name__=='__main__'):
    main( handler )
