/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TREE_MEMORY_H_
#define _E_TREE_MEMORY_H_

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gal/e-table/e-tree-model.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define E_TREE_MEMORY_TYPE        (e_tree_memory_get_type ())
#define E_TREE_MEMORY(o)          (GTK_CHECK_CAST ((o), E_TREE_MEMORY_TYPE, ETreeMemory))
#define E_TREE_MEMORY_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TREE_MEMORY_TYPE, ETreeMemoryClass))
#define E_IS_TREE_MEMORY(o)       (GTK_CHECK_TYPE ((o), E_TREE_MEMORY_TYPE))
#define E_IS_TREE_MEMORY_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TREE_MEMORY_TYPE))

typedef struct ETreeMemory ETreeMemory;
typedef struct ETreeMemoryPriv ETreeMemoryPriv;
typedef struct ETreeMemoryClass ETreeMemoryClass;

struct ETreeMemory {
	ETreeModel base;
	ETreeMemoryPriv *priv;
};

struct ETreeMemoryClass {
	ETreeModelClass parent_class;
};

GtkType     e_tree_memory_get_type  (void);
void        e_tree_memory_construct (ETreeMemory *etree);
ETreeMemory *e_tree_memory_new       (void);

/* node operations */
ETreePath e_tree_memory_node_insert        (ETreeMemory *etree, ETreePath parent, int position, gpointer node_data);
ETreePath e_tree_memory_node_insert_id     (ETreeMemory *etree, ETreePath parent, int position, gpointer node_data, char *id);
ETreePath e_tree_memory_node_insert_before (ETreeMemory *etree, ETreePath parent, ETreePath sibling, gpointer node_data);
gpointer  e_tree_memory_node_remove        (ETreeMemory *etree, ETreePath path);

/* Freeze and thaw */
void e_tree_memory_freeze (ETreeMemory *etree);
void e_tree_memory_thaw (ETreeMemory *etree);

void     e_tree_memory_set_expanded_default         (ETreeMemory *etree, gboolean expanded);
gpointer e_tree_memory_node_get_data                (ETreeMemory *etm, ETreePath node);
void     e_tree_memory_node_set_data                (ETreeMemory *etm, ETreePath node, gpointer node_data);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_TREE_MEMORY_H */
