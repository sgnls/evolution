/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camelMimeMessage.h : class for a mime message */

/* 
 *
 * Copyright (C) 1999 Bertrand Guiheneuf <Bertrand.Guiheneuf@inria.fr> .
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */


#ifndef CAMEL_MIME_MESSAGE_H
#define CAMEL_MIME_MESSAGE_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <gtk/gtk.h>
#include "camel-mime-part.h"
#include "camel-folder.h"
#include "camel-session.h"



#define CAMEL_MIME_MESSAGE_TYPE     (camel_mime_message_get_type ())
#define CAMEL_MIME_MESSAGE(obj)     (GTK_CHECK_CAST((obj), CAMEL_MIME_MESSAGE_TYPE, CamelMimeMessage))
#define CAMEL_MIME_MESSAGE_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_MIME_MESSAGE_TYPE, CamelMimeMessageClass))
#define IS_CAMEL_MIME_MESSAGE(o)    (GTK_CHECK_TYPE((o), CAMEL_MIME_MESSAGE_TYPE))



typedef struct 
{
	CamelMimePart *parent_class;

	/* header fields */
	GString *received_date;
	GString *sent_date;

	GString *subject;
	GString *reply_to;

	GString *from;
	GHashTable *recipients;
	/*   -> each value is a GList of address strings */
	/*      each key is a recipient type string in lower-case */
	
	/* other fields */
	GHashTable *flags; /* boolean values */
	/* gboolean expunged;
	   Will be enabled if proves necessary in the future */
	guint message_number; /* set by folder object when retrieving message */
	CamelFolder *folder;
	CamelSession *session;
	
} CamelMimeMessage;



typedef struct {
	CamelDataWrapperClass parent_class;
	
	/* Virtual methods */	
	void  (*set_received_date) (CamelMimeMessage *mime_message, GString *received_date);
	GString * (*get_received_date) (CamelMimeMessage *mime_message);
	GString * (*get_sent_date) (CamelMimeMessage *mime_message);
	void  (*set_reply_to) (CamelMimeMessage *mime_message, GString *reply_to);
	GString * (*get_reply_to) (CamelMimeMessage *mime_message);
	void  (*set_subject) (CamelMimeMessage *mime_message, GString *subject);
	GString * (*get_subject) (CamelMimeMessage *mime_message);
	void  (*set_from) (CamelMimeMessage *mime_message, GString *from);
	GString * (*get_from) (CamelMimeMessage *mime_message);
	void (*add_recipient) (CamelMimeMessage *mime_message, GString *recipient_type, GString *recipient); 
	void (*remove_recipient) (CamelMimeMessage *mime_message, GString *recipient_type, GString *recipient);
	GList * (*get_recipients) (CamelMimeMessage *mime_message, GString *recipient_type);
	void  (*set_flag) (CamelMimeMessage *mime_message, GString *flag, gboolean value);
	gboolean  (*get_flag) (CamelMimeMessage *mime_message, GString *flag);

	void  (*set_message_number) (CamelMimeMessage *mime_message, guint number);
	guint  (*get_message_number) (CamelMimeMessage *mime_message);

} CamelMimeMessageClass;



/* Standard Gtk function */
GtkType camel_mime_message_get_type (void);


/* public methods */
CamelMimeMessage *camel_mime_message_new_with_session (CamelSession *session);

void set_received_date (CamelMimeMessage *mime_message, GString *received_date);
GString *get_received_date (CamelMimeMessage *mime_message);
GString *get_sent_date (CamelMimeMessage *mime_message);
void set_reply_to (CamelMimeMessage *mime_message, GString *reply_to);
GString *get_reply_to (CamelMimeMessage *mime_message);
void set_subject (CamelMimeMessage *mime_message, GString *subject);
GString *get_subject (CamelMimeMessage *mime_message);
void set_from (CamelMimeMessage *mime_message, GString *from);
GString *get_from (CamelMimeMessage *mime_message);

void add_recipient (CamelMimeMessage *mime_message, GString *recipient_type, GString *recipient);
void remove_recipient (CamelMimeMessage *mime_message, GString *recipient_type, GString *recipient);
GList *get_recipients (CamelMimeMessage *mime_message, GString *recipient_type);

void set_flag (CamelMimeMessage *mime_message, GString *flag, gboolean value);
gboolean get_flag (CamelMimeMessage *mime_message, GString *flag);

guint get_message_number (CamelMimeMessage *mime_message);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_MIME_MESSAGE_H */
