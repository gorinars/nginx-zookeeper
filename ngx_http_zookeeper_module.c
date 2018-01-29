/**
 * Nginx Zookeeper
 *
 * @author Timandes White <timands@gmail.com>
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <zookeeper/zookeeper.h>

static char *ngx_http_zookeeper_path_parser(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_http_zookeeper_host_parser(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_http_zookeeper_value_parser(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_zookeeper_init_module(ngx_cycle_t *cycle);
static void ngx_http_zookeeper_exit_master(ngx_cycle_t *cycle);
static void *ngx_http_zookeeper_create_main_conf(ngx_conf_t *cf);
static char *ngx_http_zookeeper_init_main_conf(ngx_conf_t *cf, void *conf);

// Configurations
typedef struct {
    ngx_str_t host;
    ngx_str_t path;
    ngx_str_t value;
    char *cHost;
    char *cPath;
    char *cValue;
    zhandle_t *handle;
} ngx_http_zookeeper_main_conf_t;

// Directives
static ngx_command_t ngx_http_zookeeper_commands[] = {
    {
        ngx_string("zookeeper_path"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_http_zookeeper_path_parser,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ngx_http_zookeeper_main_conf_t, path),
        NULL
    },
    {
        ngx_string("zookeeper_host"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_http_zookeeper_host_parser,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ngx_http_zookeeper_main_conf_t, host),
        NULL
    },
    {
        ngx_string("zookeeper_value"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_http_zookeeper_value_parser,
        NGX_HTTP_MAIN_CONF_OFFSET,
        offsetof(ngx_http_zookeeper_main_conf_t, value),
        NULL
    },

    ngx_null_command
};

// Context
static ngx_http_module_t ngx_http_zookeeper_module_ctx = {
    NULL,                                  /* preconfiguration */
    NULL,                                  /* postconfiguration */
    ngx_http_zookeeper_create_main_conf,   /* create main configuration */
    ngx_http_zookeeper_init_main_conf,     /* init main configuration */
    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */
    NULL,                                  /* create location configration */
    NULL                                   /* merge location configration */
};

// Module
ngx_module_t ngx_http_zookeeper_module = {
    NGX_MODULE_V1,
    &ngx_http_zookeeper_module_ctx,        /* module context */
    ngx_http_zookeeper_commands,           /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    ngx_http_zookeeper_init_module,        /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    ngx_http_zookeeper_exit_master,        /* exit master */
    NGX_MODULE_V1_PADDING
};

// Parse configuration 
static char *ngx_http_zookeeper_path_parser(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_conf_set_str_slot(cf, cmd, conf);
    return NGX_CONF_OK;
}

// Parse configuration 
static char *ngx_http_zookeeper_host_parser(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_conf_set_str_slot(cf, cmd, conf);
    return NGX_CONF_OK;
}

// Parse configuration 
static char *ngx_http_zookeeper_value_parser(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_conf_set_str_slot(cf, cmd, conf);
    return NGX_CONF_OK;
}


// Init module
static ngx_int_t ngx_http_zookeeper_init_module(ngx_cycle_t *cycle)
{
    ngx_http_zookeeper_main_conf_t *zmf;
    int status;

    if (ngx_test_config) // ignore znode registering when testing configuration
        return NGX_OK;

    zmf = (ngx_http_zookeeper_main_conf_t *)ngx_http_cycle_get_module_main_conf(cycle, ngx_http_zookeeper_module);
    if (NULL == zmf) {
        ngx_log_error(NGX_LOG_WARN, cycle->log, 0, "ngx_http_zookeeper_module: Fail to get configuration");
        ngx_log_stderr(0, "ngx_http_zookeeper_module: Fail to get configuration");
        return NGX_ERROR;
    }

    if (zmf->host.len <= 0) {
        ngx_log_error(NGX_LOG_WARN, cycle->log, 0, "No zookeeper host was given");
        return NGX_OK;
    }
    if (zmf->path.len <= 0) {
        ngx_log_error(NGX_LOG_WARN, cycle->log, 0, "No zookeeper path was given");
        return NGX_OK;
    }
    if (zmf->value.len <= 0) {
        ngx_log_error(NGX_LOG_WARN, cycle->log, 0, "No zookeeper value was given");
        return NGX_OK;
    }
    if (NULL == zmf->cHost) {
        ngx_log_error(NGX_LOG_WARN, cycle->log, 0, "Impossible cHost");
        ngx_log_stderr(0, "Impossible cHost");
        return NGX_ERROR;
    }
    if (NULL == zmf->cPath) {
        ngx_log_error(NGX_LOG_WARN, cycle->log, 0, "Impossible cPath");
        ngx_log_stderr(0, "Impossible cPath");
        return NGX_ERROR;
    }
    if (NULL == zmf->cValue) {
        ngx_log_error(NGX_LOG_WARN, cycle->log, 0, "Impossible cValue");
        ngx_log_stderr(0, "Impossible cValue");
        return NGX_ERROR;
    }

    // init zookeeper
    zmf->handle = zookeeper_init(zmf->cHost, NULL, 10000, 0, NULL, 0);
    if (NULL == zmf->handle) {
        ngx_log_error(NGX_LOG_WARN, cycle->log, 0, "Fail to init zookeeper instance");
        ngx_log_stderr(0, "Fail to init zookeeper instance");
        return NGX_ERROR;
    }

    // create node
    status = zoo_create(zmf->handle, zmf->cPath, zmf->cValue, zmf->value.len, &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL, NULL, 0);
    if (ZOK != status) {
        ngx_log_error(NGX_LOG_WARN, cycle->log, 0, "Fail to create zookeeper node");
        ngx_log_stderr(0, "Fail to create zookeeper node");
        zookeeper_close(zmf->handle);
        zmf->handle = NULL;
        return NGX_ERROR;
    }

    return NGX_OK;
}

// Exit master
static void ngx_http_zookeeper_exit_master(ngx_cycle_t *cycle)
{
    ngx_http_zookeeper_main_conf_t *zmf;

    zmf = (ngx_http_zookeeper_main_conf_t *)ngx_http_cycle_get_module_main_conf(cycle, ngx_http_zookeeper_module);
    if (zmf
            && zmf->handle)
        zookeeper_close(zmf->handle);
}

// Create main conf
static void *ngx_http_zookeeper_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_zookeeper_main_conf_t *retval;
    retval = ngx_pcalloc(cf->pool, sizeof(ngx_http_zookeeper_main_conf_t));
    if (NULL == retval) {
        ngx_log_stderr(0, "Fail to create main conf of nginx-zookeeper");
        return NULL;
    }

    retval->host.len = 0;
    retval->host.data = NULL;
    retval->cHost = NULL;
    retval->path.len = 0;
    retval->path.data = NULL;
    retval->cPath = NULL;
    retval->value.len = 0;
    retval->value.data = NULL;
    retval->cValue = NULL;
    retval->handle = NULL;

    return retval;
}

// Init main conf
static char *ngx_http_zookeeper_init_main_conf(ngx_conf_t *cf, void *conf)
{
    ngx_http_zookeeper_main_conf_t *mf = conf;


    if (NULL == mf) {
        ngx_log_stderr(0, "Impossible conf");
        return NGX_CONF_ERROR;
    }

    // host
    if (mf->host.len <= 0)
        ngx_log_stderr(0, "WARNING: No zookeeper_host was given");
    else {
        mf->cHost = malloc(mf->host.len + 1);
        if (NULL == mf->cHost) {
            ngx_log_stderr(0, "Fail to malloc for host");
            return NGX_CONF_ERROR;
        }
        memcpy(mf->cHost, mf->host.data, mf->host.len);
        mf->cHost[mf->host.len] = 0;
    }

    // path
    if (mf->path.len <= 0)
        ngx_log_stderr(0, "WARNING: No zookeeper_path was given");
    else {
        mf->cPath = malloc(mf->path.len + 1);
        if (NULL == mf->cPath) {
            ngx_log_stderr(0, "Fail to malloc for path");
            return NGX_CONF_ERROR;
        }
        memcpy(mf->cPath, mf->path.data, mf->path.len);
        mf->cPath[mf->path.len] = 0;
    }
    
    // value (to be added to node)
    if (mf->value.len <= 0)
        ngx_log_stderr(0, "WARNING: No zookeeper_value was given");
    else {
        mf->cValue = malloc(mf->value.len + 1);
        if (NULL == mf->cValue) {
            ngx_log_stderr(0, "Fail to malloc for value");
            return NGX_CONF_ERROR;
        }
        memcpy(mf->cValue, mf->value.data, mf->value.len);
        mf->cValue[mf->value.len] = 0;
    }

    return NGX_CONF_OK;
}
