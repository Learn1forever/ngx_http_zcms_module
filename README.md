# ngx_http_zcms_module
ZCMS nginx plugin

安装
1.进入nginx目录
<code>patch -p1 < /web/ngx_http_zcms_module/zcms.patch</code>

2.在nginx编译中 
<code>./configure --add-module=/web/ngx_http_zcms_module</code>

3.编译
<code>make && make install</code>

4.如果采用tengine，可以使用动态编译方式
<code>./dso_tool --add-module=/web/ngx_http_zcms_module/</code>

使用：
在nginx配置中开启 zcms同步 
<pre><code>
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
</code></pre>


