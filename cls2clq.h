/*
 * cls2clq.h $Id: cls2clq.h,v 1.1 2005-03-21 12:45:59 jatoivol Exp $
 */

#ifndef __CLS2CLQ_H__
#define __CLS2CLQ_H__

#include "Clique.h"

struct clustlist {
    struct clustlist* next;
    int* variable_set;
};

typedef struct clustlist Cluster_list;


Cluster_list* new_cl_item(int array_size, Cluster_list* next, int* var_set);
/*
 */

Cluster_list* remove_cl_item(Cluster_list* head);
/* Removes one item from the beginning of the list.
 */

int is_subset(Cluster_list* cl_head, int* var_set, int size);
/*
 */

Clique* cl2cliques(Variable* vars, Cluster_list* cl_head, int n_cliques,int n);
/*
 */



#endif
