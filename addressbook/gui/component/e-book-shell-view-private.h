/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-book-shell-view-private.h
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef E_BOOK_SHELL_VIEW_PRIVATE_H
#define E_BOOK_SHELL_VIEW_PRIVATE_H

#include "e-book-shell-view.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <libebook/e-book.h>
#include <libedataserver/e-categories.h>
#include <libedataserver/e-sexp.h>
#include <libedataserverui/e-source-selector.h>

#include <e-util/gconf-bridge.h>
#include <shell/e-shell-content.h>
#include <shell/e-shell-sidebar.h>
#include <shell/e-activity-handler.h>

#include <eab-menu.h>
#include <eab-gui-util.h>
#include <e-addressbook-view.h>
#include <e-book-shell-sidebar.h>

#include <e-book-shell-view-actions.h>

#define E_BOOK_SHELL_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_BOOK_SHELL_VIEW, EBookShellViewPrivate))

/* Shorthand, requires a variable named "shell_window". */
#define ACTION(name) \
	(E_SHELL_WINDOW_ACTION_##name (shell_window))
#define ACTION_GROUP(name) \
	(E_SHELL_WINDOW_ACTION_GROUP_##name (shell_window))

/* For use in dispose() methods. */
#define DISPOSE(obj) \
	G_STMT_START { \
	if ((obj) != NULL) { g_object_unref (obj); (obj) = NULL; } \
	} G_STMT_END

/* ETable Specifications */
#define ETSPEC_FILENAME		"e-addressbook-view.etspec"

G_BEGIN_DECLS

typedef struct _EditorUidClosure EditorUidClosure;

struct _EditorUidClosure {
	GtkWidget *editor;
	gchar *uid;
	EBookShellView *view;
};

/* List these in the order to be displayed. */
enum {
	CONTACT_SEARCH_NAME_CONTAINS,
	CONTACT_SEARCH_EMAIL_BEGINS_WITH,
	CONTACT_SEARCH_ANY_FIELD_CONTAINS
};

struct _EBookShellViewPrivate {

	/*** Module Data ***/

	ESourceList *source_list;

	/*** UI Management ***/

	GtkActionGroup *contact_actions;

	/*** Other Stuff ***/

	GtkWidget *paned;
	GtkWidget *notebook;
	GtkWidget *preview;

	EActivityHandler *activity_handler;

	GHashTable *uid_to_view;
	GHashTable *uid_to_editor;

	EBook *book;
	guint activity_id;
	gchar *password;

	EABMenu *menu;
};

void		e_book_shell_view_private_init
					(EBookShellView *book_shell_view,
					 EShellViewClass *shell_view_class);
void		e_book_shell_view_private_constructed
					(EBookShellView *book_shell_view);
void		e_book_shell_view_private_dispose
					(EBookShellView *book_shell_view);
void		e_book_shell_view_private_finalize
					(EBookShellView *book_shell_view);

/* Private Utilities */

void		e_book_shell_view_actions_init
					(EBookShellView *book_shell_view);
void		e_book_shell_view_actions_update
					(EBookShellView *book_shell_view);
EAddressbookView *
		e_book_shell_view_get_current_view
					(EBookShellView *book_shell_view);
void		e_book_shell_view_editor_weak_notify
					(EditorUidClosure *closure,
					 GObject *where_the_object_was);
void		e_book_shell_view_update_search_filter
					(EBookShellView *book_shell_view);

G_END_DECLS

#endif /* E_BOOK_SHELL_VIEW_PRIVATE_H */
