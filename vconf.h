/*** vconf/vconf.h -- the vconf library

 ** Author: Zhang Yichao <echaozh@gmail.com>
 ** Created: 2011-05-16
 **/

#ifndef INCLUDED_VCONF_VCONF_H
#define INCLUDED_VCONF_VCONF_H

#undef BEGIN_CPLUSPLUS_WRAP
#ifdef __cplusplus
#define BEGIN_CPLUSPLUS_WRAP extern "C" {
#define END_CPLUSPLUS_WRAP }
#else
#define BEGIN_CPLUSPLUS_WRAP
#define END_CPLUSPLUS_WRAP
#endif

#include <stddef.h>
#include <stdint.h>

BEGIN_CPLUSPLUS_WRAP

struct vconf_url
{
    char *scheme;
    char *user;
    char *password;
    char *host;
    short port;
    char *path;
    char *query;
    char *fragment;
};

struct vconf *vconf_new (const char *conf_str);
void vconf_free (struct vconf *conf);

const char *vconf_get_string (const struct vconf *conf, const char *key);

/* only exact strings "false", "no", "0" & "" evaluates to false
 * anything else evaluates to true */
int vconf_get_bool (const struct vconf *conf, const char *key, int *val);
int vconf_get_int (const struct vconf *conf, const char *key, int32_t *val);
int vconf_get_uint (const struct vconf *conf, const char *key, uint32_t *val);
int vconf_get_long (const struct vconf *conf, const char *key, int64_t *val);
int vconf_get_ulong (const struct vconf *conf, const char *key, uint64_t *val);
int vconf_get_double (const struct vconf *conf, const char *key, double *val);

struct vconf_url *vconf_get_url (const struct vconf *conf, const char *key);
void vconf_free_url (struct vconf_url *url);

struct vconf **vconf_get_list (const struct vconf *conf, const char *key,
                               size_t *count);
void vconf_free_list (struct vconf **list, size_t count);

END_CPLUSPLUS_WRAP

#endif /* INCLUDED_VCONF_VCONF_H */
