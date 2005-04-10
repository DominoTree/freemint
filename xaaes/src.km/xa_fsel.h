/*
 * $Id$
 * 
 * XaAES - XaAES Ain't the AES (c) 1992 - 1998 C.Graham
 *                                 1999 - 2003 H.Robbers
 *                                        2004 F.Naumann & O.Skancke
 *
 * A multitasking AES replacement for FreeMiNT
 *
 * This file is part of XaAES.
 *
 * XaAES is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * XaAES is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with XaAES; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _xa_fsel_h
#define _xa_fsel_h

#include "global.h"
#include "xa_types.h"

#define DRV_MAX ('z'-('a'-1) + '9'-('0'-1))
#define NAME_MAX 128

struct fsel_data;
typedef void fsel_handler(enum locks lock, struct fsel_data *fs, const char *path, const char *file);
struct fsel_data
{
	struct xa_window *wind;
	XA_TREE *form;
	XA_TREE *menu;
	struct xa_client *owner;
	struct scroll_entry *selected_dir;
	struct scroll_entry *selected_file;
	fsel_handler	*selected;
	fsel_handler	*canceled;
	Path root;
	Path path;
	char fslash[2];
	char fs_pattern[NAME_MAX * 2];
	char filter[NAME_MAX * 2];
	char file  [NAME_MAX + 2];		/* Is the tedindo->te_ptext of FS_FILE */
	long fcase,trunc;
	int drives;
	int clear_on_folder_change;
	int ok;
	int done;
	bool tfile;
	bool treeview;	
};


void open_fileselector(enum locks lock, struct xa_client *client, struct fsel_data *fs,
		       const char *path, const char *file, const char *title,
		       fsel_handler *s, fsel_handler *c);

void close_fileselector(enum locks lock, struct fsel_data *fs);

void init_fsel(void);

AES_function
	XA_fsel_input,
	XA_fsel_exinput;

#endif /* _xa_fsel_h */
