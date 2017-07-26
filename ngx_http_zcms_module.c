/*
 * Copyright (C) 2007-2017 www.zving.com
 * author:lanjun@zving.com cs@zving.com
 * version 0.1 2015-11-01
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

typedef struct {
  ngx_flag_t enable;
  ngx_str_t sync_interval;
  ngx_str_t zcms_site_root;
  ngx_str_t zcms_root_index;
  ngx_str_t zcms_health_check_file;
} ngx_http_zcms_loc_conf_t;

static ngx_int_t ngx_http_zcms_init(ngx_conf_t *cf);
static void *ngx_http_zcms_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_zcms_merge_loc_conf(ngx_conf_t *cf, void *parent,
                                          void *child);

static ngx_command_t ngx_http_zcms_commands[] = {

    {ngx_string("zcms"),
     NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_FLAG,
     ngx_conf_set_flag_slot, NGX_HTTP_LOC_CONF_OFFSET,
     offsetof(ngx_http_zcms_loc_conf_t, enable), NULL},

    {ngx_string("zcms_sync_interval"), NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF |
                                           NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
     ngx_conf_set_str_slot, NGX_HTTP_LOC_CONF_OFFSET,
     offsetof(ngx_http_zcms_loc_conf_t, sync_interval), NULL},

    {ngx_string("zcms_site_root"), NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF |
                                       NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
     ngx_conf_set_str_slot, NGX_HTTP_LOC_CONF_OFFSET,
     offsetof(ngx_http_zcms_loc_conf_t, zcms_site_root), NULL},

    {ngx_string("zcms_root_index"), NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF |
                                       NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
     ngx_conf_set_str_slot, NGX_HTTP_LOC_CONF_OFFSET,
     offsetof(ngx_http_zcms_loc_conf_t, zcms_root_index), NULL},

    {ngx_string("zcms_health_check_file"),
     NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF |
         NGX_CONF_TAKE1,
     ngx_conf_set_str_slot, NGX_HTTP_LOC_CONF_OFFSET,
     offsetof(ngx_http_zcms_loc_conf_t, zcms_health_check_file), NULL},

    ngx_null_command};

static ngx_http_module_t ngx_http_zcms_module_ctx = {
    NULL,               /* preconfiguration */
    ngx_http_zcms_init, /* postconfiguration */

    NULL, /* create main configuration */
    NULL, /* init main configuration */

    NULL, /* create server configuration */
    NULL, /* merge server configuration */

    ngx_http_zcms_create_loc_conf, /* create location configuration */
    ngx_http_zcms_merge_loc_conf   /* merge location configuration */
};

ngx_module_t ngx_http_zcms_module = {
    NGX_MODULE_V1,
    &ngx_http_zcms_module_ctx, /* module context */
    ngx_http_zcms_commands,    /* module directives */
    NGX_HTTP_MODULE,           /* module type */
    NULL,                      /* init master */
    NULL,                      /* init module */
    NULL,                      /* init process */
    NULL,                      /* init thread */
    NULL,                      /* exit thread */
    NULL,                      /* exit process */
    NULL,                      /* exit master */
    NGX_MODULE_V1_PADDING};

static ngx_int_t ngx_http_zcms_handler(ngx_http_request_t *r) {
  u_char *last,*location;
  size_t root,len;
  size_t alias;
  ngx_str_t path, source_file_path, destination_file_path, index;
  ngx_log_t *log;
  ngx_open_file_info_t of, src_of;

  ngx_http_core_loc_conf_t *clcf;
  ngx_http_zcms_loc_conf_t *zlcf;

  ngx_copy_file_t cf;

  time_t interval;

  if (!(r->method & (NGX_HTTP_GET | NGX_HTTP_HEAD | NGX_HTTP_POST))) {
    return NGX_HTTP_NOT_ALLOWED;
  }

  zlcf = ngx_http_get_module_loc_conf(r, ngx_http_zcms_module);
  if (!zlcf->enable) {
    return NGX_DECLINED;
  }

  if (access((char *)(zlcf->zcms_health_check_file.data), 0)) {
    return NGX_DECLINED;
  }

  log = r->connection->log;

  index = zlcf->zcms_root_index;

  if(index.len == 0){
     index = (ngx_str_t) ngx_string("index.shtml");;
  }

  /*
   * ngx_http_map_uri_to_path() allocates memory for terminating '\0'
   * so we do not need to reserve memory for '/' for possible redirect
   */

  last = ngx_http_map_uri_to_path(r, &path, &root, 0);

  if (last == NULL) {
    return NGX_HTTP_INTERNAL_SERVER_ERROR;
  }

  path.len = last - path.data;

  ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0, "http path.data: \"%s\"",
                 path.data);

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

  destination_file_path = path;

  alias = clcf->alias;

  if (r->uri.data[r->uri.len - 1] == '/') {
    destination_file_path.len = destination_file_path.len + index.len + 1;
    destination_file_path.data = ngx_pnalloc(r->pool, destination_file_path.len);

    last = ngx_copy(destination_file_path.data, path.data, path.len);
    last = ngx_cpystrn(last, index.data, index.len + 1);

    // get the site wwwroot file
    source_file_path.len = zlcf->zcms_site_root.len + r->uri.len + 1;
    source_file_path.data = ngx_pnalloc(r->pool, source_file_path.len + index.len);

    last = ngx_copy(source_file_path.data, zlcf->zcms_site_root.data,
                    zlcf->zcms_site_root.len);
    last = ngx_cpystrn(last, r->uri.data + alias, r->uri.len - alias + 1);
    last = ngx_cpystrn(last, index.data, index.len + 1);
  } else {
    source_file_path.len = zlcf->zcms_site_root.len + r->uri.len + 1;
    source_file_path.data = ngx_pnalloc(r->pool, source_file_path.len);
    last = ngx_copy(source_file_path.data, zlcf->zcms_site_root.data,
                    zlcf->zcms_site_root.len);
    last = ngx_cpystrn(last, r->uri.data + alias, r->uri.len - alias + 1);
  }

  // last = ngx_cpystrn(last, r->uri.data, r->uri.len+1);

  ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0, "destination_file_path: \"%s\"",
                 destination_file_path.data);
  ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0, "source_file_path: \"%s\"",
                 source_file_path.data);

  if (ngx_open_cached_file(clcf->open_file_cache, &destination_file_path, &of,
                           r->pool) == NGX_FILE_ERROR) {
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0, "dest file not exists: \"%s\"",
                   destination_file_path.data);
    if (ngx_open_cached_file(clcf->open_file_cache, &source_file_path, &of,
                             r->pool) == NGX_OK) {
      ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0, "source file exists: \"%s\"",
                     source_file_path.data);
      ngx_create_full_path(destination_file_path.data, 0755);

      cf.size = of.size;
      // cf.buf_size = 0;
      cf.access = 0644;
      cf.time = of.mtime;
      cf.log = r->connection->log;

      //if source file is dir,redirect dir/
      if(of.is_dir){
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, log, 0, "zcms http dir");

        ngx_http_clear_location(r);

        r->headers_out.location = ngx_list_push(&r->headers_out.headers);
        if (r->headers_out.location == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        len = r->uri.len + 1;

        if (!clcf->alias && clcf->root_lengths == NULL && r->args.len == 0) {
            location = path.data + clcf->root.len;

            *last = '/';

        } else {
            if (r->args.len) {
                len += r->args.len + 1;
            }

            location = ngx_pnalloc(r->pool, len);
            if (location == NULL) {
                ngx_http_clear_location(r);
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }

            last = ngx_copy(location, r->uri.data, r->uri.len);

            *last = '/';

            if (r->args.len) {
                *++last = '?';
                ngx_memcpy(++last, r->args.data, r->args.len);
            }
        }

        r->headers_out.location->hash = 1;
        ngx_str_set(&r->headers_out.location->key, "Location");
        r->headers_out.location->value.len = len;
        r->headers_out.location->value.data = location;

        return NGX_HTTP_MOVED_PERMANENTLY;
      }

      if (ngx_copy_file(source_file_path.data, destination_file_path.data, &cf) ==
          NGX_OK) {
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0, "copy file success: \"%s\"",
                       source_file_path.data);
        return NGX_DECLINED;
      }
    }
    return NGX_HTTP_NOT_FOUND;
  } else {
    if (zlcf->sync_interval.len > 0) {
      interval = ngx_parse_time(&(zlcf->sync_interval), 1);
      time_t now = ngx_time();
      ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0, "destination_file_path mtime: %d", of.mtime);
      ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0, "now mtime: %d", now);
      ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0, "interval: %d", interval);
      if (now - of.mtime < interval) {
        // return NGX_DECLINED;
      }

      ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                     "destination_file_path last copy accessed: %d", of.accessed);

      if (now - of.accessed < interval) {
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                       "interval from  last access: %d", now - of.accessed);
        return NGX_DECLINED;
      } else {
        of.is_copy = 1;
        (void)ngx_open_cached_file(clcf->open_file_cache, &destination_file_path, &of,
                                   r->pool);
      }
    }

    ngx_memzero(&src_of, sizeof(ngx_open_file_info_t));

    src_of.test_only = 1;
    if (ngx_open_cached_file(NULL, &source_file_path, &src_of, r->pool) ==
        NGX_OK) {
      if (of.mtime != src_of.mtime || of.size != src_of.size) {
        cf.size = src_of.size;
        // cf.buf_size = 0;
        cf.access = 0644;
        cf.time = src_of.mtime;
        cf.log = r->connection->log;

        // overwrite the old file
        if (ngx_delete_file(destination_file_path.data) != NGX_OK) {
          return NGX_DECLINED;
        }
        if (ngx_copy_file(source_file_path.data, destination_file_path.data, &cf) ==
            NGX_OK) {
          return NGX_DECLINED;
        }
      }
    } else {
      ngx_delete_file(destination_file_path.data);
      ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0,
                     "source_file_path is not found : \"%s\"",
                     source_file_path.data);
    }

    return NGX_DECLINED;
  }

  return NGX_DECLINED;
}

static void *ngx_http_zcms_create_loc_conf(ngx_conf_t *cf) {
  ngx_http_zcms_loc_conf_t *conf;

  conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_zcms_loc_conf_t));
  if (conf == NULL) {
    return NULL;
  }

  conf->enable = NGX_CONF_UNSET;
  // conf->sync_interval = NGX_CONF_UNSET_UINT;

  return conf;
}

static char *ngx_http_zcms_merge_loc_conf(ngx_conf_t *cf, void *parent,
                                          void *child) {
  ngx_http_zcms_loc_conf_t *prev = parent;
  ngx_http_zcms_loc_conf_t *conf = child;

  ngx_conf_merge_value(conf->enable, prev->enable, 0);
  ngx_conf_merge_str_value(conf->zcms_site_root, prev->zcms_site_root, "");
  ngx_conf_merge_str_value(conf->zcms_root_index, prev->zcms_root_index, "");
  ngx_conf_merge_str_value(conf->sync_interval, prev->sync_interval, "60s");
  ngx_conf_merge_str_value(conf->zcms_health_check_file,
                           prev->zcms_health_check_file, ".health_check");
  return NGX_CONF_OK;
}

static ngx_int_t ngx_http_zcms_init(ngx_conf_t *cf) {
  ngx_http_handler_pt *h;
  ngx_http_core_main_conf_t *cmcf;

  cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

  h = ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers);
  if (h == NULL) {
    return NGX_ERROR;
  }

  *h = ngx_http_zcms_handler;

  return NGX_OK;
}
