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

4.编译并安装
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
#检查文件，防止nfs等其他挂载服务挂掉了
zcms_health_check_file /mnt/wwwroot/.health_check;

location / {
            root   /web/wwwroot;
            index  index.shtml;
            
            #开启zcms同步
            zcms on;
            
            #对应的源目录，一般为nfs的挂载目录
            zcms_site_root /mnt/wwwroot/;
			
			#默认首页，只能指定给一个文件
            zcms_root_index index.shtml;
			
			#缓存检查间隔
            zcms_sync_interval 60s;
			
            #可开启文件缓存以提高性能,检查间隔依赖于该配置
            open_file_cache max=1000 inactive=120s; 
            open_file_cache_valid 60s;
}
```


