/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Michael Zucchi <NotZed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>

#include <glib/gi18n.h>

#include "libedataserver/e-account-list.h"

#include "shell/e-shell.h"
#include "e-util/e-account-utils.h"
#include "e-util/e-util.h"

#include "e-mail-folder-utils.h"
#include "e-mail-session.h"
#include "em-event.h"
#include "em-filter-rule.h"
#include "em-utils.h"
#include "mail-folder-cache.h"
#include "mail-mt.h"
#include "mail-ops.h"
#include "mail-send-recv.h"
#include "mail-tools.h"

#define d(x)

/* ms between status updates to the gui */
#define STATUS_TIMEOUT (250)

/* pseudo-uri to key the send task on */
#define SEND_URI_KEY "send-task:"

/* Prefix for window size GConf keys */
#define GCONF_KEY_PREFIX "/apps/evolution/mail/send_recv"

/* send/receive email */

/* ********************************************************************** */
/*  This stuff below is independent of the stuff above */

/* this stuff is used to keep track of which folders filters have accessed, and
 * what not. the thaw/refreeze thing doesn't really seem to work though */
struct _folder_info {
	gchar *uri;
	CamelFolder *folder;
	time_t update;

	/* How many times updated, to slow it
	 * down as we go, if we have lots. */
	gint count;
};

struct _send_data {
	GList *infos;

	GtkDialog *gd;
	gint cancelled;

	/* Since we're never asked to update
	 * this one, do it ourselves. */
	CamelFolder *inbox;
	time_t inbox_update;

	GMutex *lock;
	GHashTable *folders;

	GHashTable *active;	/* send_info's by uri */
};

typedef enum {
	SEND_RECEIVE,		/* receiver */
	SEND_SEND,		/* sender */
	SEND_UPDATE,		/* imap-like 'just update folder info' */
	SEND_INVALID
} send_info_t;

typedef enum {
	SEND_ACTIVE,
	SEND_CANCELLED,
	SEND_COMPLETE
} send_state_t;

struct _send_info {
	send_info_t type;		/* 0 = fetch, 1 = send */
	GCancellable *cancellable;
	CamelSession *session;
	CamelService *service;
	gboolean keep_on_server;
	send_state_t state;
	GtkWidget *progress_bar;
	GtkWidget *cancel_button;
	GtkWidget *status_label;

	gint again;		/* need to run send again */

	gint timeout_id;
	gchar *what;
	gint pc;

	GtkWidget *send_account_label;
	gchar *send_url;

	/*time_t update;*/
	struct _send_data *data;
};

static CamelFolder *
		receive_get_folder		(CamelFilterDriver *d,
						 const gchar *uri,
						 gpointer data,
						 GError **error);

static struct _send_data *send_data = NULL;
static GtkWidget *send_recv_dialog = NULL;

static void
free_folder_info (struct _folder_info *info)
{
	/*camel_folder_thaw (info->folder);	*/
	mail_sync_folder (info->folder, NULL, NULL);
	g_object_unref (info->folder);
	g_free (info->uri);
	g_free (info);
}

static void
free_send_info (struct _send_info *info)
{
	if (info->cancellable != NULL)
		g_object_unref (info->cancellable);
	if (info->session != NULL)
		g_object_unref (info->session);
	if (info->service != NULL)
		g_object_unref (info->service);
	if (info->timeout_id != 0)
		g_source_remove (info->timeout_id);
	g_free (info->what);
	g_free (info->send_url);
	g_free (info);
}

static struct _send_data *
setup_send_data (EMailSession *session)
{
	struct _send_data *data;

	if (send_data == NULL) {
		send_data = data = g_malloc0 (sizeof (*data));
		data->lock = g_mutex_new ();
		data->folders = g_hash_table_new_full (
			g_str_hash, g_str_equal,
			(GDestroyNotify) NULL,
			(GDestroyNotify) free_folder_info);
		data->inbox =
			e_mail_session_get_local_folder (
			session, E_MAIL_LOCAL_FOLDER_LOCAL_INBOX);
		g_object_ref (data->inbox);
		data->active = g_hash_table_new_full (
			g_str_hash, g_str_equal,
			(GDestroyNotify) g_free,
			(GDestroyNotify) free_send_info);
	}

	return send_data;
}

static void
receive_cancel (GtkButton *button,
                struct _send_info *info)
{
	if (info->state == SEND_ACTIVE) {
		g_cancellable_cancel (info->cancellable);
		if (info->status_label)
			gtk_label_set_text (
				GTK_LABEL (info->status_label),
				_("Canceling..."));
		info->state = SEND_CANCELLED;
	}
	if (info->cancel_button)
		gtk_widget_set_sensitive (info->cancel_button, FALSE);
}

static void
free_send_data (void)
{
	struct _send_data *data = send_data;

	g_return_if_fail (g_hash_table_size (data->active) == 0);

	if (data->inbox) {
		mail_sync_folder (data->inbox, NULL, NULL);
		/*camel_folder_thaw (data->inbox);		*/
		g_object_unref (data->inbox);
	}

	g_list_free (data->infos);
	g_hash_table_destroy (data->active);
	g_hash_table_destroy (data->folders);
	g_mutex_free (data->lock);
	g_free (data);
	send_data = NULL;
}

static void
cancel_send_info (gpointer key,
                  struct _send_info *info,
                  gpointer data)
{
	receive_cancel (GTK_BUTTON (info->cancel_button), info);
}

static void
hide_send_info (gpointer key,
                struct _send_info *info,
                gpointer data)
{
	info->cancel_button = NULL;
	info->progress_bar = NULL;
	info->status_label = NULL;

	if (info->timeout_id != 0) {
		g_source_remove (info->timeout_id);
		info->timeout_id = 0;
	}
}

static void
dialog_destroy_cb (struct _send_data *data,
                   GObject *deadbeef)
{
	g_hash_table_foreach (data->active, (GHFunc) hide_send_info, NULL);
	data->gd = NULL;
	send_recv_dialog = NULL;
}

static void
dialog_response (GtkDialog *gd,
                 gint button,
                 struct _send_data *data)
{
	switch (button) {
	case GTK_RESPONSE_CANCEL:
		d(printf("cancelled whole thing\n"));
		if (!data->cancelled) {
			data->cancelled = TRUE;
			g_hash_table_foreach (data->active, (GHFunc) cancel_send_info, NULL);
		}
		gtk_dialog_set_response_sensitive (gd, GTK_RESPONSE_CANCEL, FALSE);
		break;
	default:
		d(printf("hiding dialog\n"));
		g_hash_table_foreach (data->active, (GHFunc) hide_send_info, NULL);
		data->gd = NULL;
		/*gtk_widget_destroy((GtkWidget *)gd);*/
		break;
	}
}

static GStaticMutex status_lock = G_STATIC_MUTEX_INIT;
static gchar *format_url (CamelService *service);

static gint
operation_status_timeout (gpointer data)
{
	struct _send_info *info = data;

	if (info->progress_bar) {
		g_static_mutex_lock (&status_lock);

		gtk_progress_bar_set_fraction (
			GTK_PROGRESS_BAR (info->progress_bar),
			info->pc / 100.0);
		if (info->what)
			gtk_label_set_text (
				GTK_LABEL (info->status_label),
				info->what);
		if (info->service != NULL && info->send_account_label) {
			gchar *tmp = format_url (info->service);

			gtk_label_set_markup (
				GTK_LABEL (info->send_account_label), tmp);

			g_free (tmp);
		}

		g_static_mutex_unlock (&status_lock);

		return TRUE;
	}

	return FALSE;
}

static void
set_send_status (struct _send_info *info,
                 const gchar *desc,
                 gint pc)
{
	g_static_mutex_lock (&status_lock);

	g_free (info->what);
	info->what = g_strdup (desc);
	info->pc = pc;

	g_static_mutex_unlock (&status_lock);
}

static void
set_transport_service (struct _send_info *info,
                       const gchar *transport_uid)
{
	CamelService *service;

	g_static_mutex_lock (&status_lock);

	service = camel_session_get_service (info->session, transport_uid);

	if (CAMEL_IS_TRANSPORT (service)) {
		if (info->service != NULL)
			g_object_unref (info->service);
		info->service = g_object_ref (service);
	}

	g_static_mutex_unlock (&status_lock);
}

/* for camel operation status */
static void
operation_status (CamelOperation *op,
                  const gchar *what,
                  gint pc,
                  struct _send_info *info)
{
	set_send_status (info, what, pc);
}

static gchar *
format_url (CamelService *service)
{
	CamelProvider *provider;
	CamelSettings *settings;
	const gchar *display_name;
	const gchar *host = NULL;
	const gchar *path = NULL;
	gchar *pretty_url = NULL;

	provider = camel_service_get_provider (service);
	settings = camel_service_get_settings (service);
	display_name = camel_service_get_display_name (service);

	if (CAMEL_IS_NETWORK_SETTINGS (settings))
		host = camel_network_settings_get_host (
			CAMEL_NETWORK_SETTINGS (settings));

	if (CAMEL_IS_LOCAL_SETTINGS (settings))
		path = camel_local_settings_get_path (
			CAMEL_LOCAL_SETTINGS (settings));

	g_return_val_if_fail (provider != NULL, NULL);

	if (display_name != NULL && *display_name != '\0') {
		if (host != NULL && *host != '\0')
			pretty_url = g_markup_printf_escaped (
				"<b>%s (%s)</b>: %s",
				display_name, provider->protocol, host);
		else if (path != NULL)
			pretty_url = g_markup_printf_escaped (
				"<b>%s (%s)</b>: %s",
				display_name, provider->protocol, path);
		else
			pretty_url = g_markup_printf_escaped (
				"<b>%s (%s)</b>",
				display_name, provider->protocol);

	} else {
		if (host != NULL && *host != '\0')
			pretty_url = g_markup_printf_escaped (
				"<b>%s</b>: %s",
				provider->protocol, host);
		else if (path != NULL)
			pretty_url = g_markup_printf_escaped (
				"<b>%s</b>: %s",
				provider->protocol, path);
		else
			pretty_url = g_markup_printf_escaped (
				"<b>%s</b>", provider->protocol);
	}

	return pretty_url;
}

static send_info_t
get_receive_type (CamelService *service)
{
	CamelProvider *provider;
	CamelURL *url;
	gboolean is_local_delivery;

	url = camel_service_new_camel_url (service);
	is_local_delivery = em_utils_is_local_delivery_mbox_file (url);
	camel_url_free (url);

	/* mbox pointing to a file is a 'Local delivery'
	 * source which requires special processing. */
	if (is_local_delivery)
		return SEND_RECEIVE;

	provider = camel_service_get_provider (service);

	if (provider == NULL)
		return SEND_INVALID;

	if (provider->object_types[CAMEL_PROVIDER_STORE]) {
		if (provider->flags & CAMEL_PROVIDER_IS_STORAGE)
			return SEND_UPDATE;
		else
			return SEND_RECEIVE;
	}

	if (provider->object_types[CAMEL_PROVIDER_TRANSPORT])
		return SEND_SEND;

	return SEND_INVALID;
}

static gboolean
get_keep_on_server (CamelService *service)
{
	GObjectClass *class;
	CamelSettings *settings;
	gboolean keep_on_server = FALSE;

	settings = camel_service_get_settings (service);
	class = G_OBJECT_GET_CLASS (settings);

	/* XXX This is a POP3-specific setting. */
	if (g_object_class_find_property (class, "keep-on-server") != NULL)
		g_object_get (
			settings, "keep-on-server",
			&keep_on_server, NULL);

	return keep_on_server;
}

static struct _send_data *
build_dialog (GtkWindow *parent,
              EMailSession *session,
              CamelFolder *outbox,
              EAccount *outgoing_account,
              gboolean allow_send)
{
	GtkDialog *gd;
	GtkWidget *table;
	gint row, num_sources;
	GList *list = NULL;
	struct _send_data *data;
	GtkWidget *container;
	GtkWidget *send_icon;
	GtkWidget *recv_icon;
	GtkWidget *scrolled_window;
	GtkWidget *label;
	GtkWidget *status_label;
	GtkWidget *progress_bar;
	GtkWidget *cancel_button;
	EMailAccountStore *account_store;
	CamelService *transport = NULL;
	struct _send_info *info;
	gchar *pretty_url;
	EMEventTargetSendReceive *target;
	GQueue queue = G_QUEUE_INIT;

	account_store = e_mail_session_get_account_store (session);

	/* Convert the outgoing account to a CamelTransport. */
	if (outgoing_account != NULL) {
		gchar *transport_uid;

		transport_uid = g_strdup_printf (
			"%s-transport", outgoing_account->uid);
		transport = camel_session_get_service (
			CAMEL_SESSION (session), transport_uid);
		g_free (transport_uid);
	}

	send_recv_dialog = gtk_dialog_new_with_buttons (
		_("Send & Receive Mail"), parent, 0, NULL);

	gd = GTK_DIALOG (send_recv_dialog);
	gtk_window_set_modal (GTK_WINDOW (send_recv_dialog), FALSE);
	gtk_window_set_icon_name (GTK_WINDOW (gd), "mail-send-receive");
	gtk_window_set_default_size (GTK_WINDOW (gd), 600, 200);

	e_restore_window (
		GTK_WINDOW (gd),
		"/org/gnome/evolution/mail/send-recv-window/",
		E_RESTORE_WINDOW_SIZE);

	gtk_widget_ensure_style ((GtkWidget *) gd);

	container = gtk_dialog_get_action_area (gd);
	gtk_container_set_border_width (GTK_CONTAINER (container), 6);

	container = gtk_dialog_get_content_area (gd);
	gtk_container_set_border_width (GTK_CONTAINER (container), 0);

	cancel_button = gtk_button_new_with_mnemonic (_("Cancel _All"));
	gtk_button_set_image (
		GTK_BUTTON (cancel_button),
		gtk_image_new_from_stock (
			GTK_STOCK_CANCEL, GTK_ICON_SIZE_BUTTON));
	gtk_widget_show (cancel_button);
	gtk_dialog_add_action_widget (gd, cancel_button, GTK_RESPONSE_CANCEL);

	num_sources = gtk_tree_model_iter_n_children (
		GTK_TREE_MODEL (account_store), NULL);

	/* Check to see if we have to send any mails.
	 * If we don't, don't display the SMTP row in the table. */
	if (outbox && CAMEL_IS_TRANSPORT (transport)
	 && (camel_folder_get_message_count (outbox) -
		camel_folder_get_deleted_message_count (outbox)) == 0)
		num_sources--;

	table = gtk_table_new (num_sources, 4, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (table), 6);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 6);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_set_border_width (
		GTK_CONTAINER (scrolled_window), 6);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (scrolled_window),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	container = gtk_dialog_get_content_area (gd);
	gtk_scrolled_window_add_with_viewport (
		GTK_SCROLLED_WINDOW (scrolled_window), table);
	gtk_box_pack_start (
		GTK_BOX (container), scrolled_window, TRUE, TRUE, 0);
	gtk_widget_show (scrolled_window);

	/* must bet setup after send_recv_dialog as it may re-trigger send-recv button */
	data = setup_send_data (session);

	row = 0;
	e_mail_account_store_queue_enabled_services (account_store, &queue);
	while (!g_queue_is_empty (&queue)) {
		CamelService *service;
		const gchar *uid;

		service = g_queue_pop_head (&queue);
		uid = camel_service_get_uid (service);

		/* see if we have an outstanding download active */
		info = g_hash_table_lookup (data->active, uid);
		if (info == NULL) {
			send_info_t type = SEND_INVALID;

			type = get_receive_type (service);

			if (type == SEND_INVALID || type == SEND_SEND)
				continue;

			info = g_malloc0 (sizeof (*info));
			info->type = type;
			info->session = g_object_ref (session);
			info->service = g_object_ref (service);
			info->keep_on_server = get_keep_on_server (service);
			info->cancellable = camel_operation_new ();
			info->state = allow_send ? SEND_ACTIVE : SEND_COMPLETE;
			info->timeout_id = g_timeout_add (
				STATUS_TIMEOUT, operation_status_timeout, info);

			g_signal_connect (
				info->cancellable, "status",
				G_CALLBACK (operation_status), info);

			g_hash_table_insert (
				data->active, g_strdup (uid), info);
			list = g_list_prepend (list, info);

		} else if (info->progress_bar != NULL) {
			/* incase we get the same source pop up again */
			continue;

		} else if (info->timeout_id == 0)
			info->timeout_id = g_timeout_add (
				STATUS_TIMEOUT, operation_status_timeout, info);

		recv_icon = gtk_image_new_from_icon_name (
			"mail-inbox", GTK_ICON_SIZE_LARGE_TOOLBAR);
		pretty_url = format_url (service);
		label = gtk_label_new (NULL);
		gtk_label_set_ellipsize (
			GTK_LABEL (label), PANGO_ELLIPSIZE_END);
		gtk_label_set_markup (GTK_LABEL (label), pretty_url);
		g_free (pretty_url);

		progress_bar = gtk_progress_bar_new ();

		cancel_button = gtk_button_new_from_stock (GTK_STOCK_CANCEL);

		status_label = gtk_label_new (
			(info->type == SEND_UPDATE) ?
			_("Updating...") : _("Waiting..."));
		gtk_label_set_ellipsize (
			GTK_LABEL (status_label), PANGO_ELLIPSIZE_END);

		/* g_object_set(data->label, "bold", TRUE, NULL); */
		gtk_misc_set_alignment (GTK_MISC (label), 0, .5);
		gtk_misc_set_alignment (GTK_MISC (status_label), 0, .5);

		gtk_table_attach (
			GTK_TABLE (table), recv_icon,
			0, 1, row, row + 2, 0, 0, 0, 0);
		gtk_table_attach (
			GTK_TABLE (table), label,
			1, 2, row, row + 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
		gtk_table_attach (
			GTK_TABLE (table), progress_bar,
			2, 3, row, row + 2, 0, 0, 0, 0);
		gtk_table_attach (
			GTK_TABLE (table), cancel_button,
			3, 4, row, row + 2, 0, 0, 0, 0);
		gtk_table_attach (
			GTK_TABLE (table), status_label,
			1, 2, row + 1, row + 2, GTK_EXPAND | GTK_FILL, 0, 0, 0);

		info->progress_bar = progress_bar;
		info->status_label = status_label;
		info->cancel_button = cancel_button;
		info->data = data;

		g_signal_connect (
			cancel_button, "clicked",
			G_CALLBACK (receive_cancel), info);

		row = row + 2;
	}

	/* we also need gd during emition to be able to catch Cancel All */
	data->gd = gd;
	target = em_event_target_new_send_receive (
		em_event_peek (), table, data, row, EM_EVENT_SEND_RECEIVE);
	e_event_emit (
		(EEvent *) em_event_peek (), "mail.sendreceive",
		(EEventTarget *) target);

	/* Skip displaying the SMTP row if we've got no outbox,
	 * outgoing account or unsent mails. */
	if (allow_send && outbox && CAMEL_IS_TRANSPORT (transport)
	 && (camel_folder_get_message_count (outbox) -
		camel_folder_get_deleted_message_count (outbox)) != 0) {

		info = g_hash_table_lookup (data->active, SEND_URI_KEY);
		if (info == NULL) {
			info = g_malloc0 (sizeof (*info));
			info->type = SEND_SEND;
			info->session = g_object_ref (session);
			info->service = g_object_ref (transport);
			info->keep_on_server = FALSE;
			info->cancellable = camel_operation_new ();
			info->state = SEND_ACTIVE;
			info->timeout_id = g_timeout_add (
				STATUS_TIMEOUT, operation_status_timeout, info);

			g_signal_connect (
				info->cancellable, "status",
				G_CALLBACK (operation_status), info);

			g_hash_table_insert (
				data->active, g_strdup (SEND_URI_KEY), info);
			list = g_list_prepend (list, info);
		} else if (info->timeout_id == 0)
			info->timeout_id = g_timeout_add (
				STATUS_TIMEOUT, operation_status_timeout, info);

		send_icon = gtk_image_new_from_icon_name (
			"mail-outbox", GTK_ICON_SIZE_LARGE_TOOLBAR);
		pretty_url = format_url (transport);
		label = gtk_label_new (NULL);
		gtk_label_set_ellipsize (
			GTK_LABEL (label), PANGO_ELLIPSIZE_END);
		gtk_label_set_markup (GTK_LABEL (label), pretty_url);

		g_free (pretty_url);

		progress_bar = gtk_progress_bar_new ();
		cancel_button = gtk_button_new_from_stock (GTK_STOCK_CANCEL);

		status_label = gtk_label_new (_("Waiting..."));
		gtk_label_set_ellipsize (
			GTK_LABEL (status_label), PANGO_ELLIPSIZE_END);

		gtk_misc_set_alignment (GTK_MISC (label), 0, .5);
		gtk_misc_set_alignment (GTK_MISC (status_label), 0, .5);

		gtk_table_attach (
			GTK_TABLE (table), send_icon,
			0, 1, row, row + 2, 0, 0, 0, 0);
		gtk_table_attach (
			GTK_TABLE (table), label,
			1, 2, row, row + 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
		gtk_table_attach (
			GTK_TABLE (table), progress_bar,
			2, 3, row, row + 2, 0, 0, 0, 0);
		gtk_table_attach (
			GTK_TABLE (table), cancel_button,
			3, 4, row, row + 2, 0, 0, 0, 0);
		gtk_table_attach (
			GTK_TABLE (table), status_label,
			1, 2, row + 1, row + 2, GTK_EXPAND | GTK_FILL, 0, 0, 0);

		info->progress_bar = progress_bar;
		info->cancel_button = cancel_button;
		info->data = data;
		info->status_label = status_label;
		info->send_account_label = label;

		g_signal_connect (
			cancel_button, "clicked",
			G_CALLBACK (receive_cancel), info);
	}

	gtk_widget_show_all (table);

	if (parent != NULL)
		gtk_widget_show (GTK_WIDGET (gd));

	g_signal_connect (gd, "response", G_CALLBACK (dialog_response), data);

	g_object_weak_ref ((GObject *) gd, (GWeakNotify) dialog_destroy_cb, data);

	data->infos = list;

	return data;
}

static void
update_folders (gchar *uri,
                struct _folder_info *info,
                gpointer data)
{
	time_t now = *((time_t *) data);

	d(printf("checking update for folder: %s\n", info->uri));

	/* let it flow through to the folders every 10 seconds */
	/* we back off slowly as we progress */
	if (now > info->update + 10 + info->count *5) {
		d(printf("upating a folder: %s\n", info->uri));
		/*camel_folder_thaw(info->folder);
		  camel_folder_freeze (info->folder);*/
		info->update = now;
		info->count++;
	}
}

static void
receive_status (CamelFilterDriver *driver,
                enum camel_filter_status_t status,
                gint pc,
                const gchar *desc,
                gpointer data)
{
	struct _send_info *info = data;
	time_t now = time (NULL);

	/* let it flow through to the folder, every now and then too? */
	g_hash_table_foreach (info->data->folders, (GHFunc) update_folders, &now);

	if (info->data->inbox && now > info->data->inbox_update + 20) {
		d(printf("updating inbox too\n"));
		/* this doesn't seem to work right :( */
		/*camel_folder_thaw(info->data->inbox);
		  camel_folder_freeze (info->data->inbox);*/
		info->data->inbox_update = now;
	}

	/* we just pile them onto the port, assuming it can handle it.
	 * We could also have a receiver port and see if they've been processed
	 * yet, so if this is necessary its not too hard to add */
	/* the mail_gui_port receiver will free everything for us */
	switch (status) {
	case CAMEL_FILTER_STATUS_START:
	case CAMEL_FILTER_STATUS_END:
		set_send_status (info, desc, pc);
		break;
	case CAMEL_FILTER_STATUS_ACTION:
		set_transport_service (info, desc);
		break;
	default:
		break;
	}
}

/* when receive/send is complete */
static void
receive_done (gpointer data)
{
	struct _send_info *info = data;
	const gchar *uid;

	uid = camel_service_get_uid (info->service);
	g_return_if_fail (uid != NULL);

	/* if we've been called to run again - run again */
	if (info->type == SEND_SEND && info->state == SEND_ACTIVE && info->again) {
		CamelFolder *local_outbox;

		local_outbox =
			e_mail_session_get_local_folder (
			E_MAIL_SESSION (info->session),
			E_MAIL_LOCAL_FOLDER_OUTBOX);

		g_return_if_fail (CAMEL_IS_TRANSPORT (info->service));

		info->again = 0;
		mail_send_queue (
			E_MAIL_SESSION (info->session),
			local_outbox,
			CAMEL_TRANSPORT (info->service),
			E_FILTER_SOURCE_OUTGOING,
			info->cancellable,
			receive_get_folder, info,
			receive_status, info,
			receive_done, info);
		return;
	}

	if (info->progress_bar) {
		const gchar *text;

		gtk_progress_bar_set_fraction (
			GTK_PROGRESS_BAR (info->progress_bar), 1.0);

		if (info->state == SEND_CANCELLED)
			text = _("Canceled.");
		else {
			text = _("Complete.");
			info->state = SEND_COMPLETE;
		}

		gtk_label_set_text (GTK_LABEL (info->status_label), text);
	}

	if (info->cancel_button)
		gtk_widget_set_sensitive (info->cancel_button, FALSE);

	/* remove/free this active download */
	d(printf("%s: freeing info %p\n", G_STRFUNC, info));
	if (info->type == SEND_SEND)
		g_hash_table_steal (info->data->active, SEND_URI_KEY);
	else
		g_hash_table_steal (info->data->active, uid);
	info->data->infos = g_list_remove (info->data->infos, info);

	if (g_hash_table_size (info->data->active) == 0) {
		if (info->data->gd)
			gtk_widget_destroy ((GtkWidget *) info->data->gd);
		free_send_data ();
	}

	free_send_info (info);
}

/* although we dont do anythign smart here yet, there is no need for this interface to
 * be available to anyone else.
 * This can also be used to hook into which folders are being updated, and occasionally
 * let them refresh */
static CamelFolder *
receive_get_folder (CamelFilterDriver *d,
                    const gchar *uri,
                    gpointer data,
                    GError **error)
{
	struct _send_info *info = data;
	CamelFolder *folder;
	struct _folder_info *oldinfo;
	gpointer oldkey, oldinfoptr;

	g_mutex_lock (info->data->lock);
	oldinfo = g_hash_table_lookup (info->data->folders, uri);
	g_mutex_unlock (info->data->lock);

	if (oldinfo) {
		g_object_ref (oldinfo->folder);
		return oldinfo->folder;
	}

	/* FIXME Not passing a GCancellable here. */
	folder = e_mail_session_uri_to_folder_sync (
		E_MAIL_SESSION (info->session), uri, 0, NULL, error);
	if (!folder)
		return NULL;

	/* we recheck that the folder hasn't snuck in while we were loading it... */
	/* and we assume the newer one is the same, but unref the old one anyway */
	g_mutex_lock (info->data->lock);

	if (g_hash_table_lookup_extended (
			info->data->folders, uri, &oldkey, &oldinfoptr)) {
		oldinfo = (struct _folder_info *) oldinfoptr;
		g_object_unref (oldinfo->folder);
		oldinfo->folder = folder;
	} else {
		oldinfo = g_malloc0 (sizeof (*oldinfo));
		oldinfo->folder = folder;
		oldinfo->uri = g_strdup (uri);
		g_hash_table_insert (info->data->folders, oldinfo->uri, oldinfo);
	}

	g_object_ref (folder);

	g_mutex_unlock (info->data->lock);

	return folder;
}

/* ********************************************************************** */

static void
get_folders (CamelStore *store,
             GPtrArray *folders,
             CamelFolderInfo *info)
{
	while (info) {
		if (camel_store_can_refresh_folder (store, info, NULL)) {
			if ((info->flags & CAMEL_FOLDER_NOSELECT) == 0) {
				gchar *folder_uri;

				folder_uri = e_mail_folder_uri_build (
					store, info->full_name);
				g_ptr_array_add (folders, folder_uri);
			}
		}

		get_folders (store, folders, info->child);
		info = info->next;
	}
}

static void
main_op_cancelled_cb (GCancellable *main_op,
                      GCancellable *refresh_op)
{
	g_cancellable_cancel (refresh_op);
}

struct _refresh_folders_msg {
	MailMsg base;

	struct _send_info *info;
	GPtrArray *folders;
	CamelStore *store;
	CamelFolderInfo *finfo;
};

static gchar *
refresh_folders_desc (struct _refresh_folders_msg *m)
{
	return g_strdup_printf(_("Checking for new mail"));
}

static void
refresh_folders_exec (struct _refresh_folders_msg *m,
                      GCancellable *cancellable,
                      GError **error)
{
	CamelFolder *folder;
	gint i;
	GError *local_error = NULL;
	gulong handler_id = 0;

	if (cancellable)
		handler_id = g_signal_connect (
			m->info->cancellable, "cancelled",
			G_CALLBACK (main_op_cancelled_cb), cancellable);

	get_folders (m->store, m->folders, m->finfo);

	camel_operation_push_message (m->info->cancellable, _("Updating..."));

	for (i = 0; i < m->folders->len; i++) {
		folder = e_mail_session_uri_to_folder_sync (
			E_MAIL_SESSION (m->info->session),
			m->folders->pdata[i], 0,
			cancellable, &local_error);
		if (folder) {
			/* FIXME Not passing a GError here. */
			camel_folder_synchronize_sync (
				folder, FALSE, cancellable, NULL);
			camel_folder_refresh_info_sync (folder, cancellable, NULL);
			g_object_unref (folder);
		} else if (local_error != NULL) {
			g_warning ("Failed to refresh folders: %s", local_error->message);
			g_clear_error (&local_error);
		}

		if (g_cancellable_is_cancelled (m->info->cancellable))
			break;

		if (m->info->state != SEND_CANCELLED)
			camel_operation_progress (
				m->info->cancellable, 100 * i / m->folders->len);
	}

	camel_operation_pop_message (m->info->cancellable);

	if (cancellable)
		g_signal_handler_disconnect (m->info->cancellable, handler_id);
}

static void
refresh_folders_done (struct _refresh_folders_msg *m)
{
	receive_done (m->info);
}

static void
refresh_folders_free (struct _refresh_folders_msg *m)
{
	gint i;

	for (i = 0; i < m->folders->len; i++)
		g_free (m->folders->pdata[i]);
	g_ptr_array_free (m->folders, TRUE);

	camel_store_free_folder_info (m->store, m->finfo);
	g_object_unref (m->store);
}

static MailMsgInfo refresh_folders_info = {
	sizeof (struct _refresh_folders_msg),
	(MailMsgDescFunc) refresh_folders_desc,
	(MailMsgExecFunc) refresh_folders_exec,
	(MailMsgDoneFunc) refresh_folders_done,
	(MailMsgFreeFunc) refresh_folders_free
};

static gboolean
receive_update_got_folderinfo (MailFolderCache *folder_cache,
                               CamelStore *store,
                               CamelFolderInfo *info,
                               gpointer data)
{
	if (info) {
		GPtrArray *folders = g_ptr_array_new ();
		struct _refresh_folders_msg *m;
		struct _send_info *sinfo = data;

		m = mail_msg_new (&refresh_folders_info);
		m->store = store;
		g_object_ref (store);
		m->folders = folders;
		m->info = sinfo;
		m->finfo = info;

		mail_msg_unordered_push (m);

		/* do not free folder info, we will free it later */
		return FALSE;
	} else {
		receive_done (data);
	}

	return TRUE;
}

static void
receive_update_got_store (CamelStore *store,
                          struct _send_info *info)
{
	MailFolderCache *folder_cache;

	folder_cache = e_mail_session_get_folder_cache (
		E_MAIL_SESSION (info->session));

	if (store) {
		mail_folder_cache_note_store (
			folder_cache, store, info->cancellable,
			receive_update_got_folderinfo, info);
	} else {
		receive_done (info);
	}
}

static GtkWidget *
send_receive (GtkWindow *parent,
              EMailSession *session,
              gboolean allow_send)
{
	CamelFolder *local_outbox;
	struct _send_data *data;
	EAccount *account;
	GList *scan;

	if (send_recv_dialog != NULL) {
		if (parent != NULL && gtk_widget_get_realized (send_recv_dialog)) {
			gtk_window_present (GTK_WINDOW (send_recv_dialog));
		}
		return send_recv_dialog;
	}

	if (!camel_session_get_online (CAMEL_SESSION (session)))
		return send_recv_dialog;

	account = e_get_default_account ();
	if (!account || !account->transport->url)
		return send_recv_dialog;

	local_outbox =
		e_mail_session_get_local_folder (
		session, E_MAIL_LOCAL_FOLDER_OUTBOX);

	data = build_dialog (
		parent, session, local_outbox, account, allow_send);

	for (scan = data->infos; scan != NULL; scan = scan->next) {
		struct _send_info *info = scan->data;

		if (!CAMEL_IS_SERVICE (info->service))
			continue;

		switch (info->type) {
		case SEND_RECEIVE:
			mail_fetch_mail (
				CAMEL_STORE (info->service),
				info->keep_on_server,
				E_FILTER_SOURCE_INCOMING,
				info->cancellable,
				receive_get_folder, info,
				receive_status, info,
				receive_done, info);
			break;
		case SEND_SEND:
			/* todo, store the folder in info? */
			mail_send_queue (
				session, local_outbox,
				CAMEL_TRANSPORT (info->service),
				E_FILTER_SOURCE_OUTGOING,
				info->cancellable,
				receive_get_folder, info,
				receive_status, info,
				receive_done, info);
			break;
		case SEND_UPDATE:
			receive_update_got_store (
				CAMEL_STORE (info->service), info);
			break;
		default:
			break;
		}
	}

	return send_recv_dialog;
}

GtkWidget *
mail_send_receive (GtkWindow *parent,
                   EMailSession *session)
{
	return send_receive (parent, session, TRUE);
}

GtkWidget *
mail_receive (GtkWindow *parent,
              EMailSession *session)
{
	return send_receive (parent, session, FALSE);
}

struct _auto_data {
	EAccount *account;
	EMailSession *session;
	gint period;		/* in seconds */
	gint timeout_id;
};

static GHashTable *auto_active;

static gboolean
auto_timeout (gpointer data)
{
	CamelService *service;
	CamelSession *session;
	struct _auto_data *info = data;

	session = CAMEL_SESSION (info->session);

	service = camel_session_get_service (
		session, info->account->uid);
	g_return_val_if_fail (CAMEL_IS_SERVICE (service), TRUE);

	if (camel_session_get_online (session))
		mail_receive_service (service);

	return TRUE;
}

static void
auto_account_removed (EAccountList *eal,
                      EAccount *ea,
                      gpointer dummy)
{
	struct _auto_data *info = g_object_get_data((GObject *)ea, "mail-autoreceive");

	g_return_if_fail (info != NULL);

	if (info->timeout_id) {
		g_source_remove (info->timeout_id);
		info->timeout_id = 0;
	}
}

static void
auto_account_finalized (struct _auto_data *info)
{
	if (info->session != NULL)
		g_object_unref (info->session);
	if (info->timeout_id)
		g_source_remove (info->timeout_id);
	g_free (info);
}

static void
auto_account_commit (struct _auto_data *info)
{
	gint period, check;

	check = info->account->enabled
		&& e_account_get_bool (info->account, E_ACCOUNT_SOURCE_AUTO_CHECK)
		&& e_account_get_string (info->account, E_ACCOUNT_SOURCE_URL);
	period = e_account_get_int (info->account, E_ACCOUNT_SOURCE_AUTO_CHECK_TIME) * 60;
	period = MAX (60, period);

	if (info->timeout_id
	    && (!check
		|| period != info->period)) {
		g_source_remove (info->timeout_id);
		info->timeout_id = 0;
	}
	info->period = period;
	if (check && info->timeout_id == 0)
		info->timeout_id = g_timeout_add_seconds (info->period, auto_timeout, info);
}

static void
auto_account_added (EAccountList *eal,
                    EAccount *ea,
                    EMailSession *session)
{
	struct _auto_data *info;

	info = g_malloc0 (sizeof (*info));
	info->account = ea;
	info->session = g_object_ref (session);
	g_object_set_data_full (
		G_OBJECT (ea), "mail-autoreceive", info,
		(GDestroyNotify) auto_account_finalized);
	auto_account_commit (info);
}

static void
auto_account_changed (EAccountList *eal,
                      EAccount *ea,
                      gpointer dummy)
{
	struct _auto_data *info;

	info = g_object_get_data (G_OBJECT (ea), "mail-autoreceive");

	if (info != NULL)
		auto_account_commit (info);
}

static void
auto_online (EShell *shell)
{
	EIterator *iter;
	EAccountList *accounts;
	EShellSettings *shell_settings;
	struct _auto_data *info;
	gboolean can_update_all;

	if (!e_shell_get_online (shell))
		return;

	shell_settings = e_shell_get_shell_settings (shell);

	can_update_all =
		e_shell_settings_get_boolean (
			shell_settings, "mail-check-on-start") &&
		e_shell_settings_get_boolean (
			shell_settings, "mail-check-all-on-start");

	accounts = e_get_account_list ();
	for (iter = e_list_get_iterator ((EList *) accounts);
	     e_iterator_is_valid (iter);
	     e_iterator_next (iter)) {
		EAccount *account = (EAccount *) e_iterator_get (iter);

		if (!account || !account->enabled)
			continue;

		info = g_object_get_data (
			G_OBJECT (account), "mail-autoreceive");
		if (info && (info->timeout_id || can_update_all))
			auto_timeout (info);
	}

	if (iter)
		g_object_unref (iter);
}

/* call to setup initial, and after changes are made to the config */
/* FIXME: Need a cleanup funciton for when object is deactivated */
void
mail_autoreceive_init (EMailSession *session)
{
	EShell *shell;
	EShellSettings *shell_settings;
	EAccountList *accounts;
	EIterator *iter;

	g_return_if_fail (E_IS_MAIL_SESSION (session));

	if (auto_active)
		return;

	accounts = e_get_account_list ();
	auto_active = g_hash_table_new (g_str_hash, g_str_equal);

	g_signal_connect (
		accounts, "account-added",
		G_CALLBACK (auto_account_added), session);
	g_signal_connect (
		accounts, "account-removed",
		G_CALLBACK (auto_account_removed), NULL);
	g_signal_connect (
		accounts, "account-changed",
		G_CALLBACK (auto_account_changed), NULL);

	for (iter = e_list_get_iterator ((EList *) accounts);
	     e_iterator_is_valid (iter);
	     e_iterator_next (iter))
		auto_account_added (
			accounts, (EAccount *)
			e_iterator_get (iter), session);

	shell = e_shell_get_default ();
	shell_settings = e_shell_get_shell_settings (shell);

	if (e_shell_settings_get_boolean (
		shell_settings, "mail-check-on-start")) {
		auto_online (shell);

		/* also flush outbox on start */
		if (e_shell_get_online (shell))
			mail_send (session);
	}

	g_signal_connect (
		shell, "notify::online",
		G_CALLBACK (auto_online), NULL);
}

/* We setup the download info's in a hashtable, if we later
 * need to build the gui, we insert them in to add them. */
void
mail_receive_service (CamelService *service)
{
	struct _send_info *info;
	struct _send_data *data;
	CamelSession *session;
	CamelFolder *local_outbox;
	const gchar *uid;
	send_info_t type = SEND_INVALID;

	g_return_if_fail (CAMEL_IS_SERVICE (service));

	uid = camel_service_get_uid (service);
	session = camel_service_get_session (service);

	data = setup_send_data (E_MAIL_SESSION (session));
	info = g_hash_table_lookup (data->active, uid);

	if (info != NULL)
		return;

	type = get_receive_type (service);

	if (type == SEND_INVALID || type == SEND_SEND)
		return;

	info = g_malloc0 (sizeof (*info));
	info->type = type;
	info->progress_bar = NULL;
	info->status_label = NULL;
	info->session = g_object_ref (session);
	info->service = g_object_ref (service);
	info->keep_on_server = get_keep_on_server (service);
	info->cancellable = camel_operation_new ();
	info->cancel_button = NULL;
	info->data = data;
	info->state = SEND_ACTIVE;
	info->timeout_id = 0;

	g_signal_connect (
		info->cancellable, "status",
		G_CALLBACK (operation_status), info);

	d(printf("Adding new info %p\n", info));

	g_hash_table_insert (data->active, g_strdup (uid), info);

	switch (info->type) {
	case SEND_RECEIVE:
		mail_fetch_mail (
			CAMEL_STORE (service),
			info->keep_on_server,
			E_FILTER_SOURCE_INCOMING,
			info->cancellable,
			receive_get_folder, info,
			receive_status, info,
			receive_done, info);
		break;
	case SEND_SEND:
		/* todo, store the folder in info? */
		local_outbox =
			e_mail_session_get_local_folder (
			E_MAIL_SESSION (session),
			E_MAIL_LOCAL_FOLDER_OUTBOX);
		mail_send_queue (
			E_MAIL_SESSION (session),
			local_outbox,
			CAMEL_TRANSPORT (service),
			E_FILTER_SOURCE_OUTGOING,
			info->cancellable,
			receive_get_folder, info,
			receive_status, info,
			receive_done, info);
		break;
	case SEND_UPDATE:
		receive_update_got_store (CAMEL_STORE (service), info);
		break;
	default:
		g_return_if_reached ();
	}
}

void
mail_send (EMailSession *session)
{
	CamelFolder *local_outbox;
	CamelService *service;
	EAccount *account;
	struct _send_info *info;
	struct _send_data *data;
	send_info_t type = SEND_INVALID;
	gchar *transport_uid;

	g_return_if_fail (E_IS_MAIL_SESSION (session));

	account = e_get_default_transport ();
	if (account == NULL || account->transport->url == NULL)
		return;

	data = setup_send_data (session);
	info = g_hash_table_lookup (data->active, SEND_URI_KEY);
	if (info != NULL) {
		info->again++;
		d(printf("send of %s still in progress\n", transport->url));
		return;
	}

	transport_uid = g_strconcat (account->uid, "-transport", NULL);

	service = camel_session_get_service (
		CAMEL_SESSION (session), transport_uid);

	if (!CAMEL_IS_TRANSPORT (service)) {
		g_free (transport_uid);
		return;
	}

	d(printf("starting non-interactive send of '%s'\n", transport->url));

	type = get_receive_type (service);

	if (type == SEND_INVALID) {
		g_free (transport_uid);
		return;
	}

	info = g_malloc0 (sizeof (*info));
	info->type = SEND_SEND;
	info->progress_bar = NULL;
	info->status_label = NULL;
	info->session = g_object_ref (session);
	info->service = g_object_ref (service);
	info->keep_on_server = FALSE;
	info->cancellable = NULL;
	info->cancel_button = NULL;
	info->data = data;
	info->state = SEND_ACTIVE;
	info->timeout_id = 0;

	d(printf("Adding new info %p\n", info));

	g_hash_table_insert (data->active, g_strdup (SEND_URI_KEY), info);

	/* todo, store the folder in info? */
	local_outbox =
		e_mail_session_get_local_folder (
		session, E_MAIL_LOCAL_FOLDER_OUTBOX);

	g_free (transport_uid);

	g_return_if_fail (CAMEL_IS_TRANSPORT (service));

	mail_send_queue (
		session, local_outbox,
		CAMEL_TRANSPORT (service),
		E_FILTER_SOURCE_OUTGOING,
		info->cancellable,
		receive_get_folder, info,
		receive_status, info,
		receive_done, info);
}
