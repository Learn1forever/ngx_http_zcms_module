# ngx_http_zcms_module
ZCMS nginx plugin

安装：

在nginx编译中，增加 
<code>./configure --add-module=/web/ngx_http_zcms_module</code>


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
			
            #也可开启文件缓存以提高性能
            #open_file_cache max=1000 inactive=20s; 
            #open_file_cache_valid 60s;
}
</code></pre>
