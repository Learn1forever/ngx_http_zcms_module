# ngx_http_zcms_module 安装说明
ZCMS nginx plugin
 
## 安装
1. 下载代码
```
git clone https://github.com/rainsky2008/ngx_http_zcms_module.git</code>
```

2. 进入nginx代码目录，打补丁
```
cd nginx
patch -p1 < /web/ngx_http_zcms_module/zcms.patch
```

3.在nginx编译中加入ngx_http_zcms_module
```
./configure --add-module=/web/ngx_http_zcms_module
```
3.编译并安装
```
make && make install
```

5.如果采用tengine，可以使用动态编译方式
```
./dso_tool --add-module=/web/ngx_http_zcms_module/
```
 
## 使用：
在nginx配置中开启 zcms同步 
```
location / {
            root   /web/wwwroot;
            index  index.shtml index.html index.htm;
            
            #开启zcms同步
            zcms on;
            
            #对应的源目录，一般为nfs的挂载目录
            zcms_site_root /mnt/wwwroot/;
            
            zcms_sync_interval 60s;
			
            #可开启文件缓存以提高性能,检查间隔依赖于该配置
            open_file_cache max=1000 inactive=120s; 
            open_file_cache_valid 60s;
}
```


