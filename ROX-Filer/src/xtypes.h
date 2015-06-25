/*
 * ROX-Filer, filer for the ROX desktop project
 * By Thomas Leonard, <tal197@users.sourceforge.net>.
 *
 * Extended filesystem attribute support, particularly for MIME types
 */

#ifndef _XTYPES_H
#define _XTYPES_H

/* Know attribute names */
#define XATTR_MIME_TYPE "user.mime_type"
#define XATTR_HIDDEN    "user.hidden"
#define XATTR_LABEL		"user.label"

/* If set, do not use extended attributes */
extern Option o_xattr_ignore;    /* Set up in xattr_init() */

/* Prototypes */
void xattr_init(void);

/* path may be NULL to test for support in libc */
int xattr_supported(const char *path);

int xattr_have(const char *path);
gchar *xattr_get(const char *path, const char *attr, int *len);
int xattr_set(const char *path, const char *attr,
	    const char *value, int value_len);
int xattr_rem(const char *path,
		const char *attr);

MIME_type *xtype_get(const gchar *path);
gint xtype_set(const char *path, const MIME_type *type);

gboolean xattrcmp(const gchar *path, const gchar *attr,
		const gchar *val);

/* Label support */
GdkColor *xlabel_get(const char *);

/* Xattr browser */
void xattrs_browser(DirItem *, const guchar *);
GArray* xattr_list(const gchar *);
GArray* copy_array(GArray *);
GArray* compare_arrays(GArray *,GArray *);

#endif
