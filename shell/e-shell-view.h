/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-view.h
 *
 * Copyright (C) 2000  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli
 */

#ifndef _E_SHELL_VIEW_H_
#define _E_SHELL_VIEW_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>

#include "e-shell.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */


#define E_TYPE_SHELL_VIEW			(e_shell_view_get_type ())
#define E_SHELL_VIEW(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_SHELL_VIEW, EShellView))
#define E_SHELL_VIEW_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_SHELL_VIEW, EShellViewClass))
#define E_IS_SHELL_VIEW(obj)			(GTK_CHECK_TYPE ((obj), E_TYPE_SHELL_VIEW))
#define E_IS_SHELL_VIEW_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_SHELL_VIEW))

typedef struct _EShellView        EShellView;
typedef struct _EShellViewPrivate EShellViewPrivate;
typedef struct _EShellViewClass   EShellViewClass;

enum _EShellViewShowFoldersMode {
	E_SHELL_VIEW_SHOW_FOLDERS_HIDDEN,
	E_SHELL_VIEW_SHOW_FOLDERS_TRANSIENT,
	E_SHELL_VIEW_SHOW_FOLDERS_STICKY
};
typedef enum _EShellViewShowFoldersMode EShellViewShowFoldersMode;


struct _EShellView {
	GnomeApp parent;

	EShellViewPrivate *priv;
};

struct _EShellViewClass {
	GnomeAppClass parent_class;
};


GtkType    e_shell_view_get_type     (void);
void       e_shell_view_construct    (EShellView *shell_view,
				      EShell     *shell);
GtkWidget *e_shell_view_new          (EShell     *shell);

gboolean   e_shell_view_display_uri  (EShellView *shell_view,
				      const char *uri);

void  e_shell_view_show_shortcuts  (EShellView *shell_view,
				    gboolean    show);
void  e_shell_view_show_folders    (EShellView *shell_view,
				    gboolean    show);

EShell    *e_shell_view_get_shell    (EShellView *shell_view);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_SHELL_VIEW_H_ */
