/*** /vconf.c -- vconf library

 ** Copyright (c) 2011 Vobile. All rights reserved.
 ** Author: Zhang Yichao <zhang_yichao@vobile.cn>
 ** Created: 2011-05-16
 **/

#include "vconf.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <sys/queue.h>

struct kvpair
{
    char *key;
    char *val;
    LIST_ENTRY (kvpair) entries;
};

enum vconf_state {IN_KEY, AFTER_KEY, IN_VALUE, IN_QUOTE, IN_SQUOTE, AFTER_SLASH,
                  IN_COMMENT, BAD};

struct strbuf
{
    char *str;
    size_t cap;
    size_t len;
};

struct vconf
{
    LIST_HEAD (kvhead, kvpair) kvpairs;

    /* for parser */
    enum vconf_state states[3]; /* at most 3 levels: in_val/quoted/escaped */
    size_t stop;
    struct strbuf key, val, spaces;
};

static int proc_key (struct vconf *conf, const char *cp);
static int proc_sep (struct vconf *conf, const char *cp);
static int proc_val (struct vconf *conf, const char *cp);
static int proc_quo (struct vconf *conf, const char *cp);
static int proc_sqt (struct vconf *conf, const char *cp);
static int proc_esc (struct vconf *conf, const char *cp);
static int proc_cmt (struct vconf *conf, const char *cp);

typedef int (*char_proc) (struct vconf *conf, const char *cp);
static char_proc s_procs[BAD] = {proc_key, proc_sep, proc_val, proc_quo,
                                 proc_sqt, proc_esc, proc_cmt};

static char *trim_str (char *s)
{
    s += strspn (s, " \t\n");
    char *cp;
    for (cp = &s[strlen (s)]; cp-- != s && isspace (*cp);)
        ;
    cp[1] = 0;
    return s;
}

static int append_str (struct strbuf *s, char c)
{
    if (!c)
        return 0;

    if (!s->str) {
        /* most keys are expected to be shorter than 32, and most values 64 */
        s->cap = 64;
        s->str = malloc (s->cap);
        if (!s->str)
            return -1;
    } else if (s->len == s->cap) {
        char *p = realloc (s->str, s->cap * 2);
        if (!p)
            return -1;
        s->str = p;
        s->cap *= 2;
    }

    s->str[s->len++] = c;
    return 0;
}

static int append_spaces (struct vconf *conf)
{
    struct strbuf *const spaces = &conf->spaces, *const val = &conf->val;
    if (!spaces->len)
        return 0;

    size_t cap = val->cap;
    while (val->len + spaces->len >= cap)
        cap *= 2;
    char *p = realloc (val->str, cap * 2);
    if (!p)
        return -1;
    val->str = p;
    val->cap = cap;
    memcpy (&val->str[val->len], spaces->str, spaces->len);
    val->len += spaces->len;
    spaces->len = 0;
    return 0;
}

static int add_field (struct vconf *conf)
{
    struct strbuf *const key = &conf->key, *const val = &conf->val;

    struct kvpair *kvp = calloc (1, sizeof (*kvp));
    if (!kvp)
        return -1;

    LIST_INSERT_HEAD (&conf->kvpairs, kvp, entries);
    kvp->key = strndup (key->str, key->len);
    kvp->val = strndup (val->str, val->len);

    /* size_t i; */
    /* for (i = val->len; i--;) { */
    /*     if (!isspace (val->str[i]) || (i && val->str[i - 1] == '\\')) */
    /*         /\* not space, or space suspected to be escaped *\/ */
    /*         break; */
    /* } */
    /* kvp->val = strndup (val->str, i + 1); */

    if (!kvp->key || !kvp->val)
        return -1;

    /* /\* trim val, remove quotes & backslashes *\/ */
    /* char *a = kvp->val, *b = a, *c = NULL; */
    /* while (*a) { */
    /*     if (!isspace (*a)) { */
    /*         if (c) {  /\* suspected trailing spaces are actually internal *\/ */
    /*             while (c < a) */
    /*                 *b++ = *c++; */
    /*             c = NULL; */
    /*         } */
    /*         if (*a == '\\') { */
    /*             *b++ = *++a; */
    /*             if (!*a) */
    /*                 break; */
    /*         } else */
    /*             *b++ = *a; */
    /*     } else if (!c)          /\* maybe trailing spaces *\/ */
    /*         c = a; */

    /*     ++a; */
    /* } */

    /* if (b > kvp->val && b[-1]) */
    /*     *b = 0; */
    return 0;
}

static void set_state (struct vconf *conf, enum vconf_state state)
{
    conf->states[conf->stop] = state;
}

static void push_state (struct vconf *conf, enum vconf_state state)
{
    conf->states[++conf->stop] = state;
}

static void pop_state (struct vconf *conf)
{
    if (conf->stop)
        --conf->stop;
}

static int proc_key (struct vconf *conf, const char *cp)
{
    struct strbuf *const key = &conf->key;
    switch (*cp) {
    case 0:                     /* eof */
    case'\r': case '\n':        /* eol */
    case '#':                   /* comment, almost eol */
        if (key->len) {         /* key without value */
            errno = EINVAL;
            return -1;
        } else if (*cp == '#')
            set_state (conf, IN_COMMENT);
        break;

    case ' ': case '\t':
        if (key->len)
            set_state (conf, AFTER_KEY);
        break;

    case ':': case '=':         /* key-value separator */
        if (key->len)
            set_state (conf, IN_VALUE);
        else {                  /* value without key */
            errno = EINVAL;
            return -1;
        }

    default:
        return append_str (key, *cp);
    }
    return 0;
}

static int proc_sep (struct vconf *conf, const char *cp)
{
    switch (*cp) {
    case 0:
    case ' ': case '\t':        /* skip the blanks */
        break;
    case ':': case '=':
        set_state (conf, IN_VALUE);
        break;
    case '\r': case '\n':
    case '#':                   /* key without value */
    default:                    /* bad separator */
        errno = EINVAL;
        return -1;
    }
    return 0;
}

static int proc_val (struct vconf *conf, const char *cp)
{
    struct strbuf *const key = &conf->key, *const val = &conf->val;
    switch (*cp) {
    case 0:
    case '\r': case '\n':
    case '#':
        if (add_field (conf) == -1)
            return -1;
        key->len = 0;
        val->len = 0;
        conf->spaces.len = 0;
        set_state (conf, *cp == '#' ? IN_COMMENT : IN_KEY);
        return 0;

    case '\\':          /* escape the char after it */
        push_state (conf, AFTER_SLASH);
        return append_spaces (conf);

    case '"':
        push_state (conf, IN_QUOTE);
        return append_spaces (conf);
    case '\'':
        push_state (conf, IN_SQUOTE);
        return append_spaces (conf);

    case ' ': case '\t':
        if (!val->len)
            break;
        return append_str (&conf->spaces, *cp);

    default:
        if (append_spaces (conf) == -1)
            return -1;
        return append_str (val, *cp);
    }
    return 0;
}

static int proc_quo (struct vconf *conf, const char *cp)
{
    struct strbuf *const val = &conf->val;
    switch (*cp) {
    case 0:                     /* unfinished quote */
        return -1;
    case '\\':
        push_state (conf, AFTER_SLASH);
        break;
    case '"':
        pop_state (conf);
        break;
    default:
        return append_str (val, *cp);
    }
    return 0;
}

static int proc_sqt (struct vconf *conf, const char *cp)
{
    struct strbuf *const val = &conf->val;
    switch (*cp) {
    case 0:                     /* unfinished single quote */
        return -1;
    case '\\':
        push_state (conf, AFTER_SLASH);
        break;
    case '\'':
        pop_state (conf);
        break;
    default:
        return append_str (val, *cp);
    }
    return 0;
}

static int proc_esc (struct vconf *conf, const char *cp)
{
    struct strbuf *const val = &conf->val;
    if (!*cp) {
        set_state (conf, IN_KEY);
        return add_field (conf);
    } else if (*cp == '\r' && cp[1] != '\n') { /* Mac style eof? */
        pop_state (conf);
        return append_str (val, '\n');
    } else if (*cp != '\r') {   /* append the newline in the next round */
        pop_state (conf);
        return append_str (val, *cp);
    }
    return 0;
}

static int proc_cmt (struct vconf *conf, const char *cp)
{
    set_state (conf, *cp == '\r' || *cp == '\n' ? IN_KEY : IN_COMMENT);
    return 0;
}

struct vconf *vconf_new (const char *conf_str)
{
    struct vconf *conf;
    if (!(conf = calloc (1, sizeof (*conf))))
        goto err;

    set_state (conf, IN_KEY);
    const char *cp = conf_str;
    do {
        if (s_procs[conf->states[conf->stop]] (conf, cp) == -1)
            goto err;
    } while (*cp++);

    if (conf->key.str) {
        free (conf->key.str);
        conf->key.str = NULL;
    }
    if (conf->val.str) {
        free (conf->val.str);
        conf->val.str = NULL;
    }
    if (conf->spaces.str) {
        free (conf->spaces.str);
        conf->spaces.str = NULL;
    }
    return conf;

err:
    if (conf)
        vconf_free (conf);
    return NULL;
}

void vconf_free (struct vconf *conf)
{
    if (!conf)
        return;

    if (conf->key.str)
        free (conf->key.str);
    if (conf->val.str)
        free (conf->val.str);
    if (conf->spaces.str)
        free (conf->spaces.str);

    struct kvpair *kvp;
    while ((kvp = LIST_FIRST (&conf->kvpairs))) {
        if (kvp->key)
            free (kvp->key);
        if (kvp->val)
            free (kvp->val);
        LIST_REMOVE (kvp, entries);
        free (kvp);
    }
    free (conf);
}

const char *vconf_get_string (const struct vconf *conf, const char *key)
{
    struct kvpair *kvp;
    LIST_FOREACH (kvp, &conf->kvpairs, entries) {
        if (!strcmp (kvp->key, key))
            return kvp->val;
    }
    return NULL;
}

int vconf_get_bool (const struct vconf *conf, const char *key, int *val)
{
    const char *s = vconf_get_string (conf, key);
    if (!s)
        return ENOENT;
    *val =  *s && strcmp (s, "no") && strcmp (s, "false") && strcmp (s, "0");
    return 0;
}

static double vconf_strtod (const char *s, char **cp, int base)
{
    return strtod (s, cp);
}

#define DEFINE_GET_NUMERAL(name, type, func)    \
    int vconf_get_##name (const struct vconf *conf, const char *key,    \
                          type *val)                                    \
    {                                                                   \
        const char *s = vconf_get_string (conf, key);                   \
        if (!s)                                                         \
            return ENOENT;                                              \
        char *cp;                                                       \
        type n = func (s, &cp, 0);                                      \
        if (*cp)                                                        \
            return EINVAL;                                              \
        *val = n;                                                       \
        return 0;                                                       \
    }

DEFINE_GET_NUMERAL (int, int32_t, strtol);
DEFINE_GET_NUMERAL (uint, uint32_t, strtoul);
DEFINE_GET_NUMERAL (long, int64_t, strtoll);
DEFINE_GET_NUMERAL (ulong, uint64_t, strtoull);
DEFINE_GET_NUMERAL (double, double, vconf_strtod);

#undef DEFINE_GET_NUMERAL

static int parse_path (struct vconf_url *url, char *s)
{
    url->path = s;
    char *cp;
    if ((cp = strchr (s, '?'))) {
        *cp = 0;
        s = url->query = &cp[1];
    }
    if ((cp = strchr (s,'#'))) {
        *cp = 0;
        url->fragment = &cp[1];
    }
    return 0;
}

static int parse_host (struct vconf_url *url, char *s)
{
    url->host = s;
    char *cp;
    if ((cp = strchr (s, ':'))) {
        /* it's convenient to use the colon as the delimiter. without a port
         * number, we'll have to strdup() the path part, since there is no
         * convenient char to contain the delimiter. */
        *cp = 0;
        url->port = strtoul (&cp[1], &cp, 10);
        if (!url->port || (*cp != '/' && *cp)) {
            errno = EINVAL;
            return -1;
        }
        return parse_path (url, cp);
    } else if ((cp = strchr (s, '/'))) {
        char *ss = strdup (cp);
        if (!ss)
            return -1;
        return parse_path (url, ss);
    } else
        return 0;
}

static int parse_auth (struct vconf_url *url, char *s)
{
    char *cp;
    if ((cp = strchr (s, '@'))) {
        url->user = s;
        *cp = 0;
        s = &cp[1];

        if ((cp = strchr (url->user, ':'))) {
            *cp = 0;
            url->password = &cp[1];
        }
    }
    return parse_host (url, s);
}

static int parse_url (struct vconf_url *url, const char *s)
{
    char *ss = strdup (s);
    if (!ss)
        return -1;

    if (*ss == '/') {
        if (parse_path (url, ss) == -1) {
            free (ss);
            return -1;
        }
    } else {
        char *cp;
        if ((cp = strstr (ss, "://"))) {
            url->scheme = ss;
            *cp = 0;
            ss = &cp[3];
        }
        if (parse_auth (url, ss) == -1) {
            free (url->scheme);
            if (!url->port && url->path)
                free (url->path);
            return -1;
        }
    }

    return 0;
}

struct vconf_url *vconf_get_url (const struct vconf *conf, const char *key)
{
    const char *s = vconf_get_string (conf, key);
    if (!s || !*s) {
        errno = s ? EINVAL : ENOENT;
        return NULL;
    }

    struct vconf_url *url = calloc (1, sizeof (*url));
    if (!url)
        return NULL;

    if (parse_url (url, s) == -1) {
        free (url);
        return NULL;
    } else
        return url;
}

void vconf_free_url (struct vconf_url *url)
{
    if (!url)
        return;

    if (url->scheme)
        free (url->scheme);
    else if (url->user)
        free (url->user);
    else if (url->host)
        free (url->host);
    else if (url->path) {
        free (url->path);
        url->path = NULL;
    }

    if (!url->port && url->path)
        free (url->path);

    free (url);
}


struct vconf **vconf_get_list (const struct vconf *conf, const char *key,
                              size_t *count)
{
    const char *s = vconf_get_string (conf, key);
    if (!s) {
        errno = ENOENT;
        return NULL;
    }
    char *ss = strdup (s);
    if (!ss)
        return NULL;

    size_t n = 0;
    char *cp = ss;
    while (*cp) {
        if (*cp++ == ';')
            ++n;
    }
    if (cp[-1] != ';')
        ++n;

    if (!n) {
        free (ss);
        *count = 0;
        errno = 0;
        return NULL;
    }

    char *k = strdup (key);
    struct vconf **list = calloc (n, sizeof (*list));
    if (!k || !list)
        goto err;

    cp = ss;
    char *kk = k;
    size_t i;
    for (i = 0; i < n; ++i) {
        list[i] = calloc (1, sizeof (struct vconf));
        if (!list[i])
            goto err;
        struct kvpair *kvp = calloc (1, sizeof (*kvp));
        if (!kvp)
            goto err;
        LIST_INSERT_HEAD (&list[i]->kvpairs, kvp, entries);

        kvp->key = kk;
        k = NULL;
        kvp->val = cp;
        ss = NULL;
        cp = strchr (cp, ';');
        if (cp)
            *cp++ = 0;
        kvp->val = trim_str (kvp->val);
    }

    *count = n;
    return list;

err:
    if (ss)
        free (ss);
    if (k)
        free (k);
    if (list)
        vconf_free_list (list, n);
    return NULL;
}

void vconf_free_list (struct vconf **list, size_t count)
{
    if (!list || !count)
        return;

    if (!list[0])
        free (list);

    struct kvpair *kvp = LIST_FIRST (&list[0]->kvpairs);
    if (kvp) {
        free (kvp->key);
        free (kvp->val);
    }

    size_t i;
    for (i = 0; i < count; ++i) {
        if (list[i]) {
            struct kvpair *kvp = LIST_FIRST (&list[i]->kvpairs);
            if (kvp) {
                LIST_REMOVE (kvp, entries);
                free (kvp);
            }
            free (list[i]);
        }
    }

    free (list);
}
