/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* gal-define-views-dialog.c
 * Copyright (C) 2000  Helix Code, Inc.
 * Author: Chris Lahey <clahey@helixcode.com>
 *
 * This library is free software; you can redistribute it and/or
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <gnome.h>
#include "gal-define-views-dialog.h"
#include "gal-define-views-model.h"
#include "gal-view-new-dialog.h"
#include <gal/e-table/e-table-scrolled.h>

static void gal_define_views_dialog_init		(GalDefineViewsDialog		 *card);
static void gal_define_views_dialog_class_init	(GalDefineViewsDialogClass	 *klass);
static void gal_define_views_dialog_set_arg (GtkObject *o, GtkArg *arg, guint arg_id);
static void gal_define_views_dialog_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void gal_define_views_dialog_destroy (GtkObject *object);

static GnomeDialogClass *parent_class = NULL;
#define PARENT_TYPE gnome_dialog_get_type()

/* The arguments we take */
enum {
	ARG_0,
};

typedef struct {
	char         *title;
	ETableModel  *model;
	GalDefineViewsDialog *names;
} GalDefineViewsDialogChild;

GtkType
gal_define_views_dialog_get_type (void)
{
	static GtkType type = 0;

	if (!type) {
		static const GtkTypeInfo info =
		{
			"GalDefineViewsDialog",
			sizeof (GalDefineViewsDialog),
			sizeof (GalDefineViewsDialogClass),
			(GtkClassInitFunc) gal_define_views_dialog_class_init,
			(GtkObjectInitFunc) gal_define_views_dialog_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		type = gtk_type_unique (PARENT_TYPE, &info);
	}

	return type;
}

static void
gal_define_views_dialog_class_init (GalDefineViewsDialogClass *klass)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*) klass;

	parent_class = gtk_type_class (PARENT_TYPE);

	object_class->set_arg = gal_define_views_dialog_set_arg;
	object_class->get_arg = gal_define_views_dialog_get_arg;
	object_class->destroy = gal_define_views_dialog_destroy;
}

/* ETable creation */
#define SPEC "<ETableSpecification cursor-mode=\"line\" draw-grid=\"true\">" \
	     "<ETableColumn model_col= \"0\" _title=\"Name\" expansion=\"1.0\" minimum_width=\"18\" resizable=\"true\" cell=\"string\" compare=\"string\"/>" \
             "<ETableState> <column source=\"0\"/> <grouping> </grouping> </ETableState>" \
	     "</ETableSpecification>"

/* For use from libglade. */
GtkWidget *gal_define_views_dialog_create_etable(char *name, char *string1, char *string2, int int1, int int2);

GtkWidget *
gal_define_views_dialog_create_etable(char *name, char *string1, char *string2, int int1, int int2)
{
	GtkWidget *table;
	ETableModel *model;
	model = gal_define_views_model_new();
	table = e_table_scrolled_new(model, NULL, SPEC, NULL);
	gtk_object_set_data(GTK_OBJECT(table), "GalDefineViewsDialog::model", model);
	return table;
}

/* Button callbacks */

static void
gdvd_button_new_dialog_callback(GtkWidget *widget, int button, GalDefineViewsDialog *dialog)
{
	gchar *name;
	GalView *view;
	switch (button) {
	case 0:
		gtk_object_get(GTK_OBJECT(widget),
			       "name", &name,
			       NULL);
		view = gal_view_new();
		gtk_object_set(GTK_OBJECT(view),
			       "name", name,
			       NULL);
		gal_define_views_model_append(GAL_DEFINE_VIEWS_MODEL(dialog->model), view);
		gtk_object_unref(GTK_OBJECT(view));
		break;
	}
	gnome_dialog_close(GNOME_DIALOG(widget));
}

static void
gdvd_button_new_callback(GtkWidget *widget, GalDefineViewsDialog *dialog)
{
	GtkWidget *view_new_dialog = gal_view_new_dialog_new();
	gtk_signal_connect(GTK_OBJECT(view_new_dialog), "clicked",
			   GTK_SIGNAL_FUNC(gdvd_button_new_dialog_callback), dialog);
	gtk_widget_show(GTK_WIDGET(view_new_dialog));
}

static void
gdvd_connect_signal(GalDefineViewsDialog *dialog, char *widget_name, char *signal, GtkSignalFunc handler)
{
	GtkWidget *widget;

	widget = glade_xml_get_widget(dialog->gui, widget_name);

	if (widget)
		gtk_signal_connect(GTK_OBJECT(widget), signal, handler, dialog);
}

static void
gal_define_views_dialog_init (GalDefineViewsDialog *dialog)
{
	GladeXML *gui;
	GtkWidget *widget;
	GtkWidget *etable;

	gui = glade_xml_new (GAL_GLADEDIR "/gal-define-views.glade", NULL);
	dialog->gui = gui;

	widget = glade_xml_get_widget(gui, "table-top");
	if (!widget) {
		return;
	}
	gtk_widget_ref(widget);
	gtk_widget_unparent(widget);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(dialog)->vbox), widget, TRUE, TRUE, 0);
	gtk_widget_unref(widget);

	gnome_dialog_append_buttons(GNOME_DIALOG(dialog),
				    GNOME_STOCK_BUTTON_OK,
				    GNOME_STOCK_BUTTON_CANCEL,
				    NULL);

	gdvd_connect_signal(dialog, "button-new", "clicked", GTK_SIGNAL_FUNC(gdvd_button_new_callback));

	dialog->model = NULL;
	etable = glade_xml_get_widget(dialog->gui, "custom-table");
	if (etable) {
		dialog->model = gtk_object_get_data(GTK_OBJECT(etable), "GalDefineViewsDialog::model");
	}
	
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, TRUE, FALSE);
}

static void
gal_define_views_dialog_destroy (GtkObject *object) {
	GalDefineViewsDialog *gal_define_views_dialog = GAL_DEFINE_VIEWS_DIALOG(object);
	
	gtk_object_unref(GTK_OBJECT(gal_define_views_dialog->gui));
}

GtkWidget*
gal_define_views_dialog_new (void)
{
	GtkWidget *widget = GTK_WIDGET (gtk_type_new (gal_define_views_dialog_get_type ()));
	return widget;
}

static void
gal_define_views_dialog_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	GalDefineViewsDialog *editor;

	editor = GAL_DEFINE_VIEWS_DIALOG (o);
	
	switch (arg_id){
	default:
		return;
	}
}

static void
gal_define_views_dialog_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	GalDefineViewsDialog *gal_define_views_dialog;

	gal_define_views_dialog = GAL_DEFINE_VIEWS_DIALOG (object);

	switch (arg_id) {
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}
