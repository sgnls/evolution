/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#ifndef _E_TREE_MEMORY_CALLBACKS_H_
#define _E_TREE_MEMORY_CALLBACKS_H_

#include <gal/e-table/e-tree-memory.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define E_TREE_MEMORY_CALLBACKS_TYPE        (e_tree_memory_callbacks_get_type ())
#define E_TREE_MEMORY_CALLBACKS(o)          (GTK_CHECK_CAST ((o), E_TREE_MEMORY_CALLBACKS_TYPE, ETreeMemoryCallbacks))
#define E_TREE_MEMORY_CALLBACKS_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TREE_MEMORY_CALLBACKS_TYPE, ETreeMemoryCallbacksClass))
#define E_IS_TREE_MEMORY_CALLBACKS(o)       (GTK_CHECK_TYPE ((o), E_TREE_MEMORY_CALLBACKS_TYPE))
#define E_IS_TREE_MEMORY_CALLBACKS_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TREE_MEMORY_CALLBACKS_TYPE))


typedef GdkPixbuf* (*ETreeMemoryCallbacksIconAtFn)             (ETreeModel *etree, ETreePath path, void *model_data);

typedef gint       (*ETreeMemoryCallbacksColumnCountFn)        (ETreeModel *etree, void *model_data);

typedef gboolean   (*ETreeMemoryCallbacksHasSaveIdFn)          (ETreeModel *etree, void *model_data);
typedef gchar     *(*ETreeMemoryCallbacksGetSaveIdFn)          (ETreeModel *etree, ETreePath path, void *model_data);

typedef void*      (*ETreeMemoryCallbacksValueAtFn)            (ETreeModel *etree, ETreePath path, int col, void *model_data);
typedef void       (*ETreeMemoryCallbacksSetValueAtFn)         (ETreeModel *etree, ETreePath path, int col, const void *val, void *model_data);
typedef gboolean   (*ETreeMemoryCallbacksIsEditableFn)         (ETreeModel *etree, ETreePath path, int col, void *model_data);

typedef	void      *(*ETreeMemoryCallbacksDuplicateValueFn)     (ETreeModel *etm, int col, const void *val, void *data);
typedef	void       (*ETreeMemoryCallbacksFreeValueFn)          (ETreeModel *etm, int col, void *val, void *data);
typedef void      *(*ETreeMemoryCallbacksInitializeValueFn)    (ETreeModel *etm, int col, void *data);
typedef gboolean   (*ETreeMemoryCallbacksValueIsEmptyFn)       (ETreeModel *etm, int col, const void *val, void *data);
typedef char      *(*ETreeMemoryCallbacksValueToStringFn)      (ETreeModel *etm, int col, const void *val, void *data);

typedef struct {
	ETreeMemory parent;

	ETreeMemoryCallbacksIconAtFn icon_at;

	ETreeMemoryCallbacksColumnCountFn     column_count;

	ETreeMemoryCallbacksHasSaveIdFn       has_save_id;
	ETreeMemoryCallbacksGetSaveIdFn       get_save_id;

	ETreeMemoryCallbacksValueAtFn         value_at;
	ETreeMemoryCallbacksSetValueAtFn      set_value_at;
	ETreeMemoryCallbacksIsEditableFn      is_editable;

	ETreeMemoryCallbacksDuplicateValueFn  duplicate_value;
	ETreeMemoryCallbacksFreeValueFn       free_value;
	ETreeMemoryCallbacksInitializeValueFn initialize_value;
	ETreeMemoryCallbacksValueIsEmptyFn    value_is_empty;
	ETreeMemoryCallbacksValueToStringFn   value_to_string;

	gpointer model_data;
} ETreeMemoryCallbacks;

typedef struct {
	ETreeMemoryClass parent_class;
} ETreeMemoryCallbacksClass;

GtkType e_tree_memory_callbacks_get_type (void);

ETreeModel *e_tree_memory_callbacks_new  (ETreeMemoryCallbacksIconAtFn icon_at,

					  ETreeMemoryCallbacksColumnCountFn        column_count,

					  ETreeMemoryCallbacksHasSaveIdFn          has_save_id,
					  ETreeMemoryCallbacksGetSaveIdFn          get_save_id,

					  ETreeMemoryCallbacksValueAtFn            value_at,
					  ETreeMemoryCallbacksSetValueAtFn         set_value_at,
					  ETreeMemoryCallbacksIsEditableFn         is_editable,

					  ETreeMemoryCallbacksDuplicateValueFn     duplicate_value,
					  ETreeMemoryCallbacksFreeValueFn          free_value,
					  ETreeMemoryCallbacksInitializeValueFn    initialize_value,
					  ETreeMemoryCallbacksValueIsEmptyFn       value_is_empty,
					  ETreeMemoryCallbacksValueToStringFn      value_to_string,

					  gpointer                                 model_data);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_TREE_MEMORY_CALLBACKS_H_ */
