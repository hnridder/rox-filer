/*
 * $Id$
 *
 * Copyright (C) 2001, the ROX-Filer team.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* diritem.c - get details about files */

/* Don't load icons larger than 400K (this is rather excessive, basically
 * we just want to stop people crashing the filer with huge icons).
 */
#define MAX_ICON_SIZE (400 * 1024)

#include "config.h"

#include <gtk/gtk.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "global.h"

#include "diritem.h"
#include "support.h"
#include "gui_support.h"
#include "mount.h"
#include "pixmaps.h"
#include "type.h"
#include "usericons.h"

/****************************************************************
 *			EXTERNAL INTERFACE			*
 ****************************************************************/

void diritem_init(void)
{
	read_globicons();
}

/* Bring this item's structure uptodate.
 * Use diritem_stat() instead the first time to initialise the item.
 */
void diritem_restat(guchar *path, DirItem *item, gboolean make_thumb)
{
	struct stat	info;

	if (item->image)
	{
		pixmap_unref(item->image);
		item->image = NULL;
	}
	item->flags = 0;
	item->mime_type = NULL;

	if (mc_lstat(path, &info) == -1)
	{
		item->lstat_errno = errno;
		item->base_type = TYPE_ERROR;
		item->size = 0;
		item->mode = 0;
		item->mtime = item->ctime = item->atime = 0;
		item->uid = (uid_t) -1;
		item->gid = (gid_t) -1;
	}
	else
	{
		item->lstat_errno = 0;
		item->size = info.st_size;
		item->mode = info.st_mode;
		item->atime = info.st_atime;
		item->ctime = info.st_ctime;
		item->mtime = info.st_mtime;
		item->uid = info.st_uid;
		item->gid = info.st_gid;

		if (S_ISLNK(info.st_mode))
		{
			if (mc_stat(path, &info))
				item->base_type = TYPE_ERROR;
			else
				item->base_type =
					mode_to_base_type(info.st_mode);

			item->flags |= ITEM_FLAG_SYMLINK;
		}
		else
		{
			item->base_type = mode_to_base_type(info.st_mode);

			if (item->base_type == TYPE_DIRECTORY)
			{
				if (mount_is_mounted(path))
					item->flags |= ITEM_FLAG_MOUNT_POINT
							| ITEM_FLAG_MOUNTED;
				else if (g_hash_table_lookup(fstab_mounts,
								path))
					item->flags |= ITEM_FLAG_MOUNT_POINT;
			}
		}
	}

	if (item->base_type == TYPE_DIRECTORY &&
			!(item->flags & ITEM_FLAG_MOUNT_POINT))
	{
		uid_t	uid = info.st_uid;
		int	path_len;
		guchar	*tmp;
		struct stat	icon;

		check_globicon(path, item);

		/* It's a directory:
		 *
		 * - If it contains a .DirIcon.png then that's the icon
		 * - If it contains an AppRun then it's an application
		 * - If it contains an AppRun but no .DirIcon then try to
		 *   use AppIcon.xpm as the icon.
		 *
		 * .DirIcon.png and AppRun must have the same owner as the
		 * directory itself, to prevent abuse of /tmp, etc.
		 * For symlinks, we want the symlink's owner.
		 */

		path_len = strlen(path);
		/* (sizeof(string) includes \0) */
		tmp = g_malloc(path_len + sizeof("/.DirIcon.png"));

		/* Try to find .DirIcon.png... */
		sprintf(tmp, "%s/%s", path, ".DirIcon.png");
		if (mc_lstat(tmp, &info) == 0 && info.st_uid == uid)
		{
			if (info.st_size > MAX_ICON_SIZE ||
						!S_ISREG(info.st_mode))
			{
				/* Don't let nasty files cause us trouble */
				/* (Note: slight race here) */
				item->image = im_appdir;
				pixmap_ref(item->image);
			}
			else
				item->image = g_fscache_lookup(pixmap_cache,
								tmp);
		}
			
		/* Try to find AppRun... */
		strcpy(tmp + path_len + 1, "AppRun");
		if (mc_lstat(tmp, &info) == 0 && info.st_uid == uid)
			item->flags |= ITEM_FLAG_APPDIR;
			
		/* Are we still missing an icon for this app? */
		if (item->flags & ITEM_FLAG_APPDIR && !item->image)
		{
			strcpy(tmp + path_len + 4, "Icon.xpm");

			if (mc_lstat(tmp, &icon) == 0 &&
					S_ISREG(icon.st_mode) &&
					icon.st_size <= MAX_ICON_SIZE)
			{
				item->image = g_fscache_lookup(pixmap_cache,
								 tmp);
			}

			if (!item->image)
			{
				item->image = im_appdir;
				pixmap_ref(item->image);
			}
		}

		g_free(tmp);
	}
	else if (item->base_type == TYPE_FILE)
	{
		/* Type determined from path before checking for executables
		 * because some mounts show everything as executable and we
		 * still want to use the file name to set the mime type.
		 */
		item->mime_type = type_from_path(path);

		/* Note: for symlinks we need the mode of the target */
		if (info.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
		{
			/* Note that the flag is set for ALL executable
			 * files, but the mime_type is only special_exec
			 * if the file doesn't have a known extension.
			 */
			item->flags |= ITEM_FLAG_EXEC_FILE;
		}

		if (!item->mime_type)
			item->mime_type = item->flags & ITEM_FLAG_EXEC_FILE
						? &special_exec
						: &text_plain;

		if (make_thumb)
			item->image = g_fscache_lookup(pixmap_cache, path);
		else
			item->image = g_fscache_lookup_full(pixmap_cache,
								path, FALSE);
		if (!item->image)
			check_globicon(path, item);
	}
	else
		check_globicon(path, item);


	if (!item->mime_type)
		item->mime_type = mime_type_from_base_type(item->base_type);

	if (!item->image)
	{
		if (item->base_type == TYPE_ERROR)
		{
			item->image = im_error;
			pixmap_ref(im_error);
		}
		else
			item->image = type_to_icon(item->mime_type);
	}
}

/* Fill in the item structure with the appropriate details.
 * 'leafname' field is set to NULL; text_width is unset.
 */
void diritem_stat(guchar *path, DirItem *item, gboolean make_thumb)
{
	item->leafname = NULL;
	item->may_delete = FALSE;
	item->image = NULL;

	diritem_restat(path, item, make_thumb);
}

/* Frees all fields in the icon, but does not free the icon structure
 * itself (because it might be part of a larger structure).
 */
void diritem_clear(DirItem *item)
{
	g_return_if_fail(item != NULL);

	pixmap_unref(item->image);
	g_free(item->leafname);
}

