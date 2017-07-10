/*
 * Copyright (C) 2007-2015 www.zving.com
 * author:lanjun@zving.com 
 * version 0.1 2015-11-01 
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct {
    ngx_flag_t   enable;
    ngx_str_t   sync_interval;
    ngx_str_t    zcms_site_root;
} ngx_http_zcms_loc_conf_t;


static ngx_int_t ngx_http_zcms_init(ngx_conf_t *cf);
static void *ngx_http_zcms_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_zcms_merge_loc_conf(ngx_conf_t *cf, void *parent,
    void *child);
    


static ngx_command_t  ngx_http_zcms_commands[] = {

    { ngx_string("zcms"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_zcms_loc_conf_t, enable),
      NULL },

    { ngx_string("zcms_sync_interval"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_zcms_loc_conf_t, sync_interval),
      NULL },

    { ngx_string("zcms_site_root"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_zcms_loc_conf_t, zcms_site_root),
      NULL },


      ngx_null_command
};


static ngx_http_module_t  ngx_http_zcms_module_ctx = {
    NULL,                                /* preconfiguration */
    ngx_http_zcms_init,                /* postconfiguration */

    NULL,                                /* create main configuration */
    NULL,                                /* init main configuration */

    NULL,                                /* create server configuration */
    NULL,                                /* merge server configuration */

    ngx_http_zcms_create_loc_conf,     /* create location configuration */
    ngx_http_zcms_merge_loc_conf       /* merge location configuration */
};


ngx_module_t  ngx_http_zcms_module = {
    NGX_MODULE_V1,
    &ngx_http_zcms_module_ctx,         /* module context */
    ngx_http_zcms_commands,            /* module directives */
    NGX_HTTP_MODULE,                     /* module type */
    NULL,                                /* init master */
    NULL,                                /* init module */
    NULL,                                /* init process */
    NULL,                                /* init thread */
    NULL,                                /* exit thread */
    NULL,                                /* exit process */
    NULL,                                /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_int_t
ngx_http_zcms_handler(ngx_http_request_t *r)
{
    u_char                    *last;
    size_t                     root;
    ngx_str_t                  path,dir_index_from,dir_index_to;
    //ngx_log_t                 *log;
    ngx_open_file_info_t       of,src_of;

    ngx_http_core_loc_conf_t  *clcf;
    ngx_http_zcms_loc_conf_t  *zlcf;

    ngx_copy_file_t           cf;
	
	time_t interval;
	
    if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD|NGX_HTTP_POST))) {
        return NGX_HTTP_NOT_ALLOWED;
    }
    
    zlcf = ngx_http_get_module_loc_conf(r, ngx_http_zcms_module);
    if (!zlcf->enable) {
        return NGX_DECLINED;
    }

    //log = r->connection->log;

    /*
     * ngx_http_map_uri_to_path() allocates memory for terminating '\0'
     * so we do not need to reserve memory for '/' for possible redirect
     */

    last = ngx_http_map_uri_to_path(r, &path, &root, 0);
    if (last == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    path.len = last - path.data;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                   "http path.data: \"%s\"", path.data);

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    ngx_memzero(&of, sizeof(ngx_open_file_info_t));

    of.read_ahead = clcf->read_ahead;
    of.directio = clcf->directio;
    of.valid = clcf->open_file_cache_valid;
    of.min_uses = clcf->open_file_cache_min_uses;
    of.errors = clcf->open_file_cache_errors;
    of.events = clcf->open_file_cache_events;
    of.test_only = 1;

    if (ngx_http_set_disable_symlinks(r, clcf, &path, &of) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                   "r->uri.data: \"%s\"", r->uri.data);    
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                   " r->uri.len: \"%d\"",  r->uri.len);   
	
    dir_index_to = path;
        
    dir_index_from.len = zlcf->zcms_site_root.len + r->uri.len+1;
    dir_index_from.data = ngx_pnalloc(r->pool,dir_index_from.len);
    last = ngx_copy(dir_index_from.data, zlcf->zcms_site_root.data, zlcf->zcms_site_root.len);
    last = ngx_cpystrn(last, r->uri.data, r->uri.len+1);
    
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                   "dir_index_to: \"%s\"", dir_index_to.data);
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                   "dir_index_from: \"%s\"", dir_index_from.data);       

        
    if (ngx_open_cached_file(clcf->open_file_cache,&dir_index_to, &of, r->pool) 
        == NGX_FILE_ERROR){
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                   "file not exists: \"%s\"", dir_index_to.data);
        if (ngx_open_cached_file(clcf->open_file_cache,&dir_index_from, &of, r->pool) 
            == NGX_OK){
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                   "from file exists: \"%s\"", dir_index_from.data);    
            ngx_create_full_path(dir_index_to.data, 0755);
            
             cf.size = of.size;
             //cf.buf_size = 0;
             cf.access = 0644;
             cf.time = of.mtime;
             cf.log = r->connection->log;
            if(ngx_copy_file(dir_index_from.data, dir_index_to.data, &cf) == NGX_OK){
                ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                   "copy file success: \"%s\"", dir_index_from.data);    
                return NGX_DECLINED;
            }
        }
        return NGX_HTTP_NOT_FOUND;        
    }else{
		if(zlcf->sync_interval.len > 0){
			interval = ngx_parse_time(&(zlcf->sync_interval), 1);
			time_t  now = ngx_time();
		    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                   "to_file mtime: %d", of.mtime);   
			ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                   "now mtime: %d", now); 
			ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                   "interval: %d", interval); 				   
		    if(now-of.mtime < interval){ 
			    //return NGX_DECLINED;
		    }
			
			ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                   "to_file last copy accessed: %d",  of.accessed);

		    if(now - of.accessed < interval){ 
			    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                   "interval from  last access: %d", now-of.accessed);
			    return NGX_DECLINED;
		    }else{
				of.is_copy = 1;
				(void)ngx_open_cached_file(clcf->open_file_cache,&dir_index_to, &of, r->pool);
			}				
		}
		

				   
				   
        ngx_memzero(&src_of, sizeof(ngx_open_file_info_t));

        src_of.test_only = 1;
        if (ngx_open_cached_file(NULL,&dir_index_from, &src_of,r->pool)==NGX_OK){
            if(of.mtime!= src_of.mtime 
                ||  of.size!= src_of.size){
                cf.size = src_of.size;
                 //cf.buf_size = 0;
                cf.access = 0644;
                cf.time = src_of.mtime;
                cf.log = r->connection->log;  
		    
	        //overwrite the old file
		if(ngx_delete_file(dir_index_to.data) != NGX_OK){
		    return NGX_DECLINED;
		}
                if(ngx_copy_file(dir_index_from.data, dir_index_to.data, &cf) == NGX_OK){
                    return NGX_DECLINED;
                }
            }
        }else{
            ngx_delete_file(dir_index_to.data);
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                   "zcms_site_root file not find : \"%s\"", dir_index_from.data);
        }
        
        return NGX_DECLINED;
    }
    
    return NGX_DECLINED;
}


static void *
ngx_http_zcms_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_zcms_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_zcms_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    conf->enable = NGX_CONF_UNSET;
    //conf->sync_interval = NGX_CONF_UNSET_UINT;

    return conf;
}


static char *
ngx_http_zcms_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_zcms_loc_conf_t *prev = parent;
    ngx_http_zcms_loc_conf_t *conf = child;

    ngx_conf_merge_value(conf->enable, prev->enable, 0);
    ngx_conf_merge_str_value(conf->zcms_site_root, prev->zcms_site_root, "");
    ngx_conf_merge_str_value(conf->sync_interval, prev->sync_interval, "60s");

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_zcms_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt       *h;
    ngx_http_core_main_conf_t *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_zcms_handler;

    return NGX_OK;
}

