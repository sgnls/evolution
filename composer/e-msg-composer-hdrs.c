/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* msg-composer-hdrs.c
 *
 * Copyright (C) 1999 Ximian, Inc.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-widget.h>

#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtktooltips.h>
#include <libgnomeui/gnome-uidefs.h>
#include <liboaf/liboaf.h>

#include "Composer.h"

#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtktooltips.h>

#include <gal/e-text/e-entry.h>
#include <gal/widgets/e-unicode.h>

#include <camel/camel.h>
#include <e-destination.h>
#include "e-msg-composer-hdrs.h"
#include "mail/mail-config.h"
#include "addressbook/backend/ebook/e-book-util.h"


#define SELECT_NAMES_OAFID "OAFIID:GNOME_Evolution_Addressbook_SelectNames"

/* Indexes in the GtkTable assigned to various items */

#define LINE_FROM    0
#define LINE_REPLYTO 1
#define LINE_TO      2
#define LINE_CC      3
#define LINE_BCC     4
#define LINE_SUBJECT 5

typedef struct {
	GtkWidget *label;
	GtkWidget *entry;
} EMsgComposerHdrPair;

struct _EMsgComposerHdrsPrivate {
	GNOME_Evolution_Addressbook_SelectNames corba_select_names;

	/* The tooltips.  */
	GtkTooltips *tooltips;
	
	GSList *from_options;
	
	/* Standard headers.  */
	EMsgComposerHdrPair from, reply_to, to, cc, bcc, subject;
};


static GtkTableClass *parent_class = NULL;

enum {
	SHOW_ADDRESS_DIALOG,
	SUBJECT_CHANGED,
	HDRS_CHANGED,
	FROM_CHANGED,
	LAST_SIGNAL
};

static gint signals[LAST_SIGNAL];


static gboolean
setup_corba (EMsgComposerHdrs *hdrs)
{
	EMsgComposerHdrsPrivate *priv;
	CORBA_Environment ev;

	priv = hdrs->priv;

	g_assert (priv->corba_select_names == CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);

	priv->corba_select_names = oaf_activate_from_id (SELECT_NAMES_OAFID, 0, NULL, &ev);

	/* OAF seems to be broken -- it can return a CORBA_OBJECT_NIL without
           raising an exception in `ev'.  */
	if (ev._major != CORBA_NO_EXCEPTION || priv->corba_select_names == CORBA_OBJECT_NIL) {
		g_warning ("Cannot activate -- %s", SELECT_NAMES_OAFID);
		CORBA_exception_free (&ev);
		return FALSE;
	}

	CORBA_exception_free (&ev);

	return TRUE;
}

typedef struct {
	EMsgComposerHdrs *hdrs;
	char *string;
} EMsgComposerHdrsAndString;

static void
e_msg_composer_hdrs_and_string_free(EMsgComposerHdrsAndString *emchas)
{
	if (emchas->hdrs)
		gtk_object_unref(GTK_OBJECT(emchas->hdrs));
	g_free(emchas->string);
}

static EMsgComposerHdrsAndString *
e_msg_composer_hdrs_and_string_create(EMsgComposerHdrs *hdrs, const char *string)
{
	EMsgComposerHdrsAndString *emchas;

	emchas = g_new(EMsgComposerHdrsAndString, 1);
	emchas->hdrs = hdrs;
	emchas->string = g_strdup(string);
	if (emchas->hdrs)
		gtk_object_ref(GTK_OBJECT(emchas->hdrs));

	return emchas;
}

static void
address_button_clicked_cb (GtkButton *button,
			   gpointer data)
{
	EMsgComposerHdrsAndString *emchas;
	EMsgComposerHdrs *hdrs;
	EMsgComposerHdrsPrivate *priv;
	CORBA_Environment ev;

	emchas = data;
	hdrs = emchas->hdrs;
	priv = hdrs->priv;

	CORBA_exception_init (&ev);

	GNOME_Evolution_Addressbook_SelectNames_activateDialog (
		priv->corba_select_names, emchas->string, &ev);

	CORBA_exception_free (&ev);
}

static void
from_changed (GtkWidget *item, gpointer data)
{
	EMsgComposerHdrs *hdrs = E_MSG_COMPOSER_HDRS (data);
	
	hdrs->account = gtk_object_get_data (GTK_OBJECT (item), "account");
	gtk_signal_emit (GTK_OBJECT (hdrs), signals [FROM_CHANGED]);
}

static GtkWidget *
create_from_optionmenu (EMsgComposerHdrs *hdrs)
{
	GtkWidget *omenu, *menu, *first = NULL;
	const GSList *accounts;
	GtkWidget *item;
	int i = 0, history = 0;
	int default_account;
	
	omenu = gtk_option_menu_new ();
	menu = gtk_menu_new ();

	default_account = mail_config_get_default_account_num ();

	accounts = mail_config_get_accounts ();
	while (accounts) {
		const MailConfigAccount *account;
		char *label;
		char *native_label;
		
		account = accounts->data;
		
		/* this should never ever fail */
		if (!account || !account->name || !account->id) {
			g_assert_not_reached ();
			continue;
		}
		
		if (strcmp (account->name, account->id->address))
			label = g_strdup_printf ("%s <%s> (%s)", account->id->name,
						 account->id->address, account->name);
		else
			label = g_strdup_printf ("%s <%s>", account->id->name, account->id->address);
		
		
		native_label = e_utf8_to_gtk_string (GTK_WIDGET (menu), label);
		item = gtk_menu_item_new_with_label (native_label);
		g_free (native_label);
		g_free (label);
		
		gtk_object_set_data (GTK_OBJECT (item), "account", account_copy (account));
		gtk_signal_connect (GTK_OBJECT (item), "activate",
				    GTK_SIGNAL_FUNC (from_changed), hdrs);
		
		if (i == default_account) {
			first = item;
			history = i;
		}
		
		/* this is so we can later set which one we want */
		hdrs->priv->from_options = g_slist_append (hdrs->priv->from_options, item);
		
		gtk_menu_append (GTK_MENU (menu), item);
		gtk_widget_show (item);
		
		accounts = accounts->next;
		i++;
	}
	
	gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);

	if (first){
		gtk_option_menu_set_history (GTK_OPTION_MENU (omenu), history);
		gtk_signal_emit_by_name (GTK_OBJECT (first), "activate", hdrs);
	}
	
	return omenu;
}

static void
addressbook_entry_changed (BonoboListener    *listener,
			   char              *event_name,
			   CORBA_any         *arg,
			   CORBA_Environment *ev,
			   gpointer           user_data)
{
	EMsgComposerHdrs *hdrs = E_MSG_COMPOSER_HDRS (user_data);

	gtk_signal_emit (GTK_OBJECT (hdrs), signals[HDRS_CHANGED]);
}

static GtkWidget *
create_addressbook_entry (EMsgComposerHdrs *hdrs,
			  const char *name)
{
	EMsgComposerHdrsPrivate *priv;
	GNOME_Evolution_Addressbook_SelectNames corba_select_names;
	Bonobo_Control corba_control;
	GtkWidget *control_widget;
	CORBA_Environment ev;
	BonoboControlFrame *cf;
	Bonobo_PropertyBag pb = CORBA_OBJECT_NIL;

	priv = hdrs->priv;
	corba_select_names = priv->corba_select_names;

	CORBA_exception_init (&ev);

	GNOME_Evolution_Addressbook_SelectNames_addSection (
		corba_select_names, name, name, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev);
		return NULL;
	}

	corba_control =
		GNOME_Evolution_Addressbook_SelectNames_getEntryBySection (
			corba_select_names, name, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	control_widget = bonobo_widget_new_control_from_objref (
		corba_control, CORBA_OBJECT_NIL);

	cf = bonobo_widget_get_control_frame (BONOBO_WIDGET (control_widget));
	pb = bonobo_control_frame_get_control_property_bag (cf, NULL);

	bonobo_event_source_client_add_listener (
		pb, addressbook_entry_changed,
		"Bonobo/Property:change:entry_changed",
		NULL, hdrs);

	return control_widget;
}

static EMsgComposerHdrPair 
header_new_recipient (EMsgComposerHdrs *hdrs, const gchar *name, const gchar *tip)
{
	EMsgComposerHdrsPrivate *priv;
	EMsgComposerHdrPair ret;
	
	priv = hdrs->priv;
	
	ret.label = gtk_button_new_with_label (name);
	GTK_OBJECT_UNSET_FLAGS (ret.label, GTK_CAN_FOCUS);
	gtk_signal_connect_full (
		GTK_OBJECT (ret.label), "clicked",
		GTK_SIGNAL_FUNC (address_button_clicked_cb), NULL,
		e_msg_composer_hdrs_and_string_create(hdrs, name),
		(GtkDestroyNotify) e_msg_composer_hdrs_and_string_free,
		FALSE, FALSE);

	gtk_tooltips_set_tip (
		hdrs->priv->tooltips, ret.label,
		_("Click here for the address book"),
		NULL);

	ret.entry = create_addressbook_entry (hdrs, name);

	return ret;
}

static void
entry_changed (GtkWidget *entry, EMsgComposerHdrs *hdrs)
{
	gchar *tmp;
	gchar *subject;

	tmp = e_msg_composer_hdrs_get_subject (hdrs);
	subject = e_utf8_to_gtk_string (GTK_WIDGET (entry), tmp);

	gtk_signal_emit (GTK_OBJECT (hdrs), signals[SUBJECT_CHANGED], subject);
	g_free (tmp);

	gtk_signal_emit (GTK_OBJECT (hdrs), signals[HDRS_CHANGED]);
}

static void
create_headers (EMsgComposerHdrs *hdrs)
{
	EMsgComposerHdrsPrivate *priv = hdrs->priv;

	/*
	 * From:
	 */
	priv->from.label = gtk_label_new (_("From:"));
	priv->from.entry = create_from_optionmenu (hdrs);

	/*
	 * Reply-To:
	 */
	priv->reply_to.label = gtk_label_new (_("Reply-To:"));
	priv->reply_to.entry = e_entry_new ();
	gtk_object_set (GTK_OBJECT (priv->reply_to.entry),
			"editable", TRUE,
			"use_ellipsis", TRUE,
			"allow_newlines", FALSE,
			NULL);

	/*
	 * Subject:
	 */
	priv->subject.label = gtk_label_new (_("Subject:"));
	priv->subject.entry = e_entry_new ();
	gtk_object_set (GTK_OBJECT (priv->subject.entry),
			"editable", TRUE,
			"use_ellipsis", TRUE,
			"allow_newlines", FALSE,
			NULL);
	gtk_signal_connect (GTK_OBJECT (priv->subject.entry), "changed",
			    GTK_SIGNAL_FUNC (entry_changed), hdrs);

	/*
	 * To: CC: and Bcc:
	 */
	priv->to = header_new_recipient (
		hdrs, _("To:"),
		_("Enter the recipients of the message"));

	priv->cc = header_new_recipient (
		hdrs, _("Cc:"),
		_("Enter the addresses that will receive a carbon copy of the message"));

	priv->bcc = header_new_recipient (
		hdrs, _("Bcc:"),
		 _("Enter the addresses that will receive a carbon copy of "
		   "the message without appearing in the recipient list of "
		   "the message."));
}

static void
attach_couple (EMsgComposerHdrs *hdrs, EMsgComposerHdrPair *pair, int line)
{
	int pad;
	
	if (GTK_IS_LABEL (pair->label))
		pad = GNOME_PAD;
	else
		pad = 2;
	
	gtk_table_attach (
		GTK_TABLE (hdrs),
		pair->label, 0, 1,
		line, line + 1,
		GTK_FILL, GTK_FILL, pad, pad);
	
	gtk_table_attach (
		GTK_TABLE (hdrs),
		pair->entry, 1, 2,
		line, line + 1,
		GTK_FILL | GTK_EXPAND, 0, 2, 2);
}

static void
attach_headers (EMsgComposerHdrs *hdrs)
{
	EMsgComposerHdrsPrivate *p = hdrs->priv;

	attach_couple (hdrs, &p->from, LINE_FROM);
	attach_couple (hdrs, &p->reply_to, LINE_REPLYTO);
	attach_couple (hdrs, &p->to, LINE_TO);
	attach_couple (hdrs, &p->cc, LINE_CC);
	attach_couple (hdrs, &p->bcc, LINE_BCC);
	attach_couple (hdrs, &p->subject, LINE_SUBJECT);
}

static void
set_pair_visibility (EMsgComposerHdrs *h, EMsgComposerHdrPair *pair, gboolean visible)
{
	if (visible){
		gtk_widget_show (pair->label);
		gtk_widget_show (pair->entry);
	} else {
		gtk_widget_hide (pair->label);
		gtk_widget_hide (pair->entry);
	}
}

static void
headers_set_visibility (EMsgComposerHdrs *h, gint visible_flags)
{
	EMsgComposerHdrsPrivate *p = h->priv;

	set_pair_visibility (h, &p->from, visible_flags & E_MSG_COMPOSER_VISIBLE_FROM);
	set_pair_visibility (h, &p->reply_to, visible_flags & E_MSG_COMPOSER_VISIBLE_REPLYTO);
	set_pair_visibility (h, &p->cc, visible_flags & E_MSG_COMPOSER_VISIBLE_CC);
	set_pair_visibility (h, &p->bcc, visible_flags & E_MSG_COMPOSER_VISIBLE_BCC);
	set_pair_visibility (h, &p->subject, visible_flags & E_MSG_COMPOSER_VISIBLE_SUBJECT);
}

void
e_msg_composer_set_hdrs_visible (EMsgComposerHdrs *hdrs, gint visible_flags)
{
	g_return_if_fail (hdrs != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));

	headers_set_visibility (hdrs, visible_flags);
	gtk_widget_queue_resize (GTK_WIDGET (hdrs));
}

static void
setup_headers (EMsgComposerHdrs *hdrs, gint visible_flags)
{
	create_headers (hdrs);
	attach_headers (hdrs);

	/*
	 * To: is always visible
	 */
	gtk_widget_show (hdrs->priv->to.label);
	gtk_widget_show (hdrs->priv->to.entry);
	
	headers_set_visibility (hdrs, visible_flags);
}
		

/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	EMsgComposerHdrs *hdrs;
	EMsgComposerHdrsPrivate *priv;
	GSList *l;

	hdrs = E_MSG_COMPOSER_HDRS (object);
	priv = hdrs->priv;

	if (priv->corba_select_names != CORBA_OBJECT_NIL) {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		CORBA_Object_release (priv->corba_select_names, &ev);
		CORBA_exception_free (&ev);
	}

	gtk_object_destroy (GTK_OBJECT (priv->tooltips));
	
	l = priv->from_options;
	while (l) {
		MailConfigAccount *account;
		GtkWidget *item = l->data;
		
		account = gtk_object_get_data (GTK_OBJECT (item), "account");
		account_destroy (account);
		
		l = l->next;
	}
	g_slist_free (priv->from_options);
	
	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (EMsgComposerHdrsClass *class)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (class);
	object_class->destroy = destroy;

	parent_class = gtk_type_class (gtk_table_get_type ());

	signals[SHOW_ADDRESS_DIALOG] =
		gtk_signal_new ("show_address_dialog",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EMsgComposerHdrsClass,
						   show_address_dialog),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	signals[SUBJECT_CHANGED] =
		gtk_signal_new ("subject_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EMsgComposerHdrsClass,
						   subject_changed),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE,
				1, GTK_TYPE_STRING);

	signals[HDRS_CHANGED] =
		gtk_signal_new ("hdrs_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EMsgComposerHdrsClass,
						   hdrs_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	signals[FROM_CHANGED] =
		gtk_signal_new ("from_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EMsgComposerHdrsClass,
						   from_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
init (EMsgComposerHdrs *hdrs)
{
	EMsgComposerHdrsPrivate *priv;

	priv = g_new0 (EMsgComposerHdrsPrivate, 1);

	priv->tooltips = gtk_tooltips_new ();

	hdrs->priv = priv;
}


GtkType
e_msg_composer_hdrs_get_type (void)
{
	static GtkType type = 0;

	if (type == 0) {
		static const GtkTypeInfo info = {
			"EMsgComposerHdrs",
			sizeof (EMsgComposerHdrs),
			sizeof (EMsgComposerHdrsClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		type = gtk_type_unique (gtk_table_get_type (), &info);
	}

	return type;
}

GtkWidget *
e_msg_composer_hdrs_new (gint visible_flags)
{
	EMsgComposerHdrs *new;
	EMsgComposerHdrsPrivate *priv;
	
	new = gtk_type_new (e_msg_composer_hdrs_get_type ());
	priv = new->priv;
	
	if (! setup_corba (new)) {
		gtk_widget_destroy (GTK_WIDGET (new));
		return NULL;
	}
	
	setup_headers (new, visible_flags);

	return GTK_WIDGET (new);
}


static void
set_recipients (CamelMimeMessage *msg, GtkWidget *entry_widget, const gchar *type)
{
	EDestination **destv;
	CamelInternetAddress *addr;
	char *string = NULL, *dest_str = NULL;
	int i;
	
	bonobo_widget_get_property (BONOBO_WIDGET (entry_widget), "text", &string, NULL);
	destv = e_destination_importv (string);
	
	g_message ("importv: [%s]", string);
	
	if (destv) {
		dest_str = e_destination_get_address_textv (destv);
		g_message ("destination is: %s", dest_str);
		
		/* dest_str has been utf8 encoded 2x by this point...not good */
		
		if (dest_str && *dest_str) {
			addr = camel_internet_address_new ();
			camel_address_unformat (CAMEL_ADDRESS (addr), dest_str);
			
			/* TODO: In here, we could cross-reference the names with an alias book
			   or address book, it should be sufficient for unformat to do the parsing too */
			
			camel_mime_message_set_recipients (msg, type, addr);
			
			camel_object_unref (CAMEL_OBJECT (addr));
			
			g_free (dest_str);
		}
		
		for (i = 0; destv[i]; i++) {
			e_destination_touch (destv[i]);
			gtk_object_unref (GTK_OBJECT (destv[i]));
		}
		g_free (destv);
	}
	
	g_free (string);
}

void
e_msg_composer_hdrs_to_message (EMsgComposerHdrs *hdrs,
				CamelMimeMessage *msg)
{
	CamelInternetAddress *addr;
	gchar *subject;
	
	g_return_if_fail (hdrs != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));
	g_return_if_fail (msg != NULL);
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (msg));
	
	subject = e_msg_composer_hdrs_get_subject (hdrs);
	camel_mime_message_set_subject (msg, subject);
	g_free (subject);
	
	addr = e_msg_composer_hdrs_get_from (hdrs);
	camel_mime_message_set_from (msg, addr);
	camel_object_unref (CAMEL_OBJECT (addr));
	
	addr = e_msg_composer_hdrs_get_reply_to (hdrs);
	if (addr) {
		camel_mime_message_set_reply_to (msg, addr);
		camel_object_unref (CAMEL_OBJECT (addr));
	}
	
	set_recipients (msg, hdrs->priv->to.entry, CAMEL_RECIPIENT_TYPE_TO);
	set_recipients (msg, hdrs->priv->cc.entry, CAMEL_RECIPIENT_TYPE_CC);
	set_recipients (msg, hdrs->priv->bcc.entry, CAMEL_RECIPIENT_TYPE_BCC);
}


static void
set_entry (BonoboWidget *bonobo_widget,
	   const GList *list)
{
	GString *string;
	const GList *p;
	
	string = g_string_new ("");
	for (p = list; p != NULL; p = p->next) {
		if (string->str[0] != '\0')
			g_string_append (string, ", ");
		g_string_append (string, p->data);
	}
	
	bonobo_widget_set_property (BONOBO_WIDGET (bonobo_widget), "text", string->str, NULL);
	
	g_string_free (string, TRUE);
}


/* FIXME: yea, this could be better... but it's doubtful it'll be used much */
void
e_msg_composer_hdrs_set_from_account (EMsgComposerHdrs *hdrs,
				      const char *account_name)
{
	GtkOptionMenu *omenu;
	GtkWidget *item;
	GSList *l;
	int i = 0;
	int default_account = 0;
	
	g_return_if_fail (hdrs != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));
	
	omenu = GTK_OPTION_MENU (hdrs->priv->from.entry);
	
	default_account = mail_config_get_default_account_num ();

	/* find the item that represents the account and activate it */
	l = hdrs->priv->from_options;
	while (l) {
		MailConfigAccount *account;
		item = l->data;
		
		account = gtk_object_get_data (GTK_OBJECT (item), "account");
		if ((account_name && !strcmp (account_name, account->name)) ||
		    (!account_name && i == default_account)) {
			/* set the correct optionlist item */
			gtk_option_menu_set_history (omenu, i);
			gtk_signal_emit_by_name (GTK_OBJECT (item), "activate", hdrs);
			
			return;
		}
		
		l = l->next;
		i++;
	}
}

void
e_msg_composer_hdrs_set_reply_to (EMsgComposerHdrs *hdrs,
				  const char *reply_to)
{
	g_return_if_fail (hdrs != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));
	
	bonobo_widget_set_property (BONOBO_WIDGET (hdrs->priv->reply_to.entry),
				    "text", reply_to, NULL);
}

/* FIXME: these shouldn't take GLists, they should take CamelInternetAddress's */
void
e_msg_composer_hdrs_set_to (EMsgComposerHdrs *hdrs,
			    const GList *to_list)
{
	g_return_if_fail (hdrs != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));
	
	set_entry (BONOBO_WIDGET (hdrs->priv->to.entry), to_list);
}

void
e_msg_composer_hdrs_set_cc (EMsgComposerHdrs *hdrs,
			    const GList *cc_list)
{
	g_return_if_fail (hdrs != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));
	
	set_entry (BONOBO_WIDGET (hdrs->priv->cc.entry), cc_list);
}

void
e_msg_composer_hdrs_set_bcc (EMsgComposerHdrs *hdrs,
			     const GList *bcc_list)
{
	g_return_if_fail (hdrs != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));
	
	set_entry (BONOBO_WIDGET (hdrs->priv->bcc.entry), bcc_list);
}

void
e_msg_composer_hdrs_set_subject (EMsgComposerHdrs *hdrs,
				 const char *subject)
{
	g_return_if_fail (hdrs != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs));
	g_return_if_fail (subject != NULL);
	
	gtk_object_set (GTK_OBJECT (hdrs->priv->subject.entry),
			"text", subject,
			NULL);
}


CamelInternetAddress *
e_msg_composer_hdrs_get_from (EMsgComposerHdrs *hdrs)
{
	const MailConfigAccount *account;
	CamelInternetAddress *addr;
	
	g_return_val_if_fail (hdrs != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);
	
	account = hdrs->account;
	if (!account || !account->id) {
		/* FIXME: perhaps we should try the default account? */
		return NULL;
	}
	
	addr = camel_internet_address_new ();
	camel_internet_address_add (addr, account->id->name, account->id->address);
	
	return addr;
}

CamelInternetAddress *
e_msg_composer_hdrs_get_reply_to (EMsgComposerHdrs *hdrs)
{
	CamelInternetAddress *addr;
	gchar *reply_to;
	
	g_return_val_if_fail (hdrs != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);
	
	gtk_object_get (GTK_OBJECT (hdrs->priv->reply_to.entry),
			"text", &reply_to, NULL);
	
	if (!reply_to || *reply_to == '\0') {
		g_free (reply_to);
		return NULL;
	}
	
	addr = camel_internet_address_new ();
	if (camel_address_unformat (CAMEL_ADDRESS (addr), reply_to) == -1) {
		g_free (reply_to);
		camel_object_unref (CAMEL_OBJECT (addr));
		return NULL;
	}
	
	g_free (reply_to);
	
	return addr;
}

/* FIXME this is currently unused and broken.  */
GList *
e_msg_composer_hdrs_get_to (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (hdrs != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);
	
	g_assert_not_reached ();
	
	return NULL;
}

/* FIXME this is currently unused and broken.  */
GList *
e_msg_composer_hdrs_get_cc (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (hdrs != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);
	
	g_assert_not_reached ();
	
	return NULL;
}

/* FIXME this is currently unused and broken.  */
GList *
e_msg_composer_hdrs_get_bcc (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (hdrs != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);
	
	g_assert_not_reached ();
	
	return NULL;
}

char *
e_msg_composer_hdrs_get_subject (EMsgComposerHdrs *hdrs)
{
	gchar *subject;
	
	g_return_val_if_fail (hdrs != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);
	
	gtk_object_get (GTK_OBJECT (hdrs->priv->subject.entry),
			"text", &subject, NULL);
	
	return subject;
}


GtkWidget *
e_msg_composer_hdrs_get_reply_to_entry (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (hdrs != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);

	return hdrs->priv->reply_to.entry;
}

GtkWidget *
e_msg_composer_hdrs_get_to_entry (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (hdrs != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);

	return hdrs->priv->to.entry;
}

GtkWidget *
e_msg_composer_hdrs_get_cc_entry (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (hdrs != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);
	
	return hdrs->priv->cc.entry;
}

GtkWidget *
e_msg_composer_hdrs_get_bcc_entry (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (hdrs != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);

	return hdrs->priv->bcc.entry;
}

GtkWidget *
e_msg_composer_hdrs_get_subject_entry (EMsgComposerHdrs *hdrs)
{
	g_return_val_if_fail (hdrs != NULL, NULL);
	g_return_val_if_fail (E_IS_MSG_COMPOSER_HDRS (hdrs), NULL);

	return hdrs->priv->subject.entry;
}
