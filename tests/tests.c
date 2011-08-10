/*** tests/tests.c -- tests

 ** Copyright (c) 2011 Vobile. All rights reserved.
 ** Author: Zhang Yichao <zhang_yichao@vobile.cn>
 ** Created: 2011-05-17
 **/

#include <vconf/vconf.h>

#include <errno.h>
#include <stdio.h>

typedef int (*run_case) (const struct vconf *conf);
struct case_desc
{
    const char *conf;
    run_case runner;
};

int case1 (const struct vconf *conf);
int case2 (const struct vconf *conf);
int case3 (const struct vconf *conf);
int case4 (const struct vconf *conf);
int case5 (const struct vconf *conf);
int case6 (const struct vconf *conf);
int case7 (const struct vconf *conf);
int case8 (const struct vconf *conf);

static struct case_desc good_confs[] = {
    {"   a : 0x123   \n", case1},
    {"   b : zk://host/1/2/3;zk://host2:123/1;zk://host3;host4;host5:123;/1\n",
     case2},
    {"c = a b c d e f g h i j k l m n o p q r s t u v w x y z a b c d e f g h "
     "i j k l m n o p q r s t u v w x y z\t\t\\ \n", case3},
    {"d =  ;false ; no ;0;123;\n", case4},
    {"e : \\\n\n", case5},
    {"f\\  : \\", case6},
    {"g : '\n '", case7},
    {"h : '\\''", case8},
};

static const char *bad_confs[] = {
    "b c: d",
    "c d",
    " : e",
    "f #: 123",
    "g\\ h: ",
    "h : 'abc\n"
};

int case1 (const struct vconf *conf)
{
    int32_t val;
    return !vconf_get_int (conf, "a", &val) && val == 0x123;
}

int case2 (const struct vconf *conf)
{
    size_t count;
    struct vconf **list = vconf_get_list (conf, "b", &count);
    if ((!list && errno != ENOMEM) || count != 6)
        return 0;

    int pass = 1;
    size_t i;
    for (i = 0; i < count; ++i) {
        struct vconf_url *url = vconf_get_url (list[i], "b");
        if (!url && errno != ENOMEM)
            pass = 0;
        vconf_free_url (url);
    }
    vconf_free_list (list, count);
    return pass;
}

int case3 (const struct vconf *conf)
{
    return vconf_get_string (conf, "c")
        && !strcmp (vconf_get_string (conf, "c"), "a b c d e f g h i j k l m n "
                    "o p q r s t u v w x y z a b c d e f g h i j k l m n o p q "
                    "r s t u v w x y z\t\t ");
}

int case4 (const struct vconf *conf)
{
    size_t count;
    struct vconf **list = vconf_get_list (conf, "d", &count);
    if ((!list && errno != ENOMEM) || count != 5)
        return 0;

    static const int correct[] = {0, 0, 0, 0, 1};
    int pass = 1;
    size_t i;
    for (i = 0; i < count; ++i) {
        int val = 1;
        if (vconf_get_bool (list[i], "d", &val) || val != correct[i])
            pass = 0;
    }
    vconf_free_list (list, count);
    return pass;
}

int case5 (const struct vconf *conf)
{
    return vconf_get_string (conf, "e")
        && !strcmp (vconf_get_string (conf, "e"), "\n");
}

int case6 (const struct vconf *conf)
{
    return vconf_get_string (conf, "f\\")
        &&!strcmp (vconf_get_string (conf, "f\\"), "");
}

int case7 (const struct vconf *conf)
{
    return vconf_get_string (conf, "g")
        &&!strcmp (vconf_get_string (conf, "g"), "\n ");
}

int case8 (const struct vconf *conf)
{
    return vconf_get_string (conf, "h")
        &&!strcmp (vconf_get_string (conf, "h"), "'");
}

int good_case (struct case_desc *c)
{
    int pass = 0;
    struct vconf *conf = vconf_new (c->conf);
    if (!conf)
        return 0;
    pass = c->runner (conf);
    vconf_free (conf);
    return pass;
}

int bad_case (const char *s)
{
    int pass = 0;
    struct vconf *conf = vconf_new (s);
    pass = !conf;
    vconf_free (conf);
    return pass;
}

int main ()
{
    size_t total = 0, failed = 0;

    size_t i;
    for (i = 0; i < sizeof (good_confs) / sizeof (*good_confs); ++i) {
        printf ("test case #%u... ", ++total);
        if (good_case (&good_confs[i]))
            printf ("passed\n");
        else {
            printf ("failed\n");
            ++failed;
        }
    }
    for (i = 0; i < sizeof (bad_confs) / sizeof (*bad_confs); ++i) {
        printf ("test case #%u... ", ++total);
        if (bad_case (bad_confs[i]))
            printf ("passed\n");
        else {
            printf ("failed\n");
            ++failed;
        }
    }

    printf ("total tests: %u, failed: %u\n", total, failed);
    if (!failed)
        printf ("all tests passed\n");
    return !!failed;
}

