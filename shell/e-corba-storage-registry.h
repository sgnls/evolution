/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-corba-storage-registry.h
 *
 * Copyright (C) 2000  Ximian, Inc.
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli
 */

#ifndef __E_CORBA_STORAGE_REGISTRY_H__
#define __E_CORBA_STORAGE_REGISTRY_H__

#include <bonobo/bonobo-object.h>

#include "Evolution.h"
#include "e-storage-set.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_CORBA_STORAGE_REGISTRY            (e_corba_storage_registry_get_type ())
#define E_CORBA_STORAGE_REGISTRY(obj)            (GTK_CHECK_CAST ((obj), E_TYPE_CORBA_STORAGE_REGISTRY, ECorbaStorageRegistry))
#define E_CORBA_STORAGE_REGISTRY_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), E_TYPE_CORBA_STORAGE_REGISTRY, ECorbaStorageRegistryClass))
#define E_IS_CORBA_STORAGE_REGISTRY(obj)         (GTK_CHECK_TYPE ((obj), E_TYPE_CORBA_STORAGE_REGISTRY))
#define E_IS_CORBA_STORAGE_REGISTRY_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_CORBA_STORAGE_REGISTRY))


typedef struct _ECorbaStorageRegistry        ECorbaStorageRegistry;
typedef struct _ECorbaStorageRegistryPrivate ECorbaStorageRegistryPrivate;
typedef struct _ECorbaStorageRegistryClass   ECorbaStorageRegistryClass;

struct _ECorbaStorageRegistry {
	BonoboObject parent;

	ECorbaStorageRegistryPrivate *priv;
};

struct _ECorbaStorageRegistryClass {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_StorageRegistry__epv epv;
};


GtkType                e_corba_storage_registry_get_type   (void);
void                   e_corba_storage_registry_construct  (ECorbaStorageRegistry     *corba_storage_registry,
							    EStorageSet               *storage_set);
ECorbaStorageRegistry *e_corba_storage_registry_new        (EStorageSet               *storage_set);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __E_CORBA_STORAGE_REGISTRY_H__ */
