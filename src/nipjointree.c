/* nipjointree.c
 * Authors: Janne Toivola, Mikko Korpela
 * Version: $Id: nipjointree.c,v 1.5 2010-12-17 18:15:56 jatoivol Exp $
 */

#include "nipjointree.h"


/*
#define NIP_DEBUG_CLIQUE
*/

/*
#define NIP_DEBUG_RETRACTION
*/

static int nip_message_pass(nip_clique c1, nip_sepset s, nip_clique c2);

static int nip_clique_var_index(nip_clique c, nip_variable v);



static int nip_clique_marked(nip_clique c);

static void nip_join_tree_dfs(nip_clique start, 
			      void (*cFuncPointer)(nip_clique, double*),
			      void (*sFuncPointer)(nip_sepset, double*),
			      double* ptr);
static void  nip_retract_clique(nip_clique c, double* ptr);
static void  nip_retract_sepset(nip_sepset s, double* ptr);
static void     nip_clique_mass(nip_clique c, double* ptr);
static void nip_neg_sepset_mass(nip_sepset s, double* ptr);

static void nip_remove_sepset(nip_clique c, nip_sepset s);


nip_clique nip_new_clique(nip_variable vars[], int num_of_vars){
  nip_clique c;
  int *cardinality;
  int *reorder;
  int *indices;
  int i, j;
  unsigned long temp;

  c = (nip_clique) malloc(sizeof(nip_clique_struct));
  if(!c){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
    return NULL;
  }    

  cardinality = (int *) calloc(num_of_vars, sizeof(int));
  if(!cardinality){
    free(c);
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
    return NULL;
  }    

  reorder = (int *) calloc(num_of_vars, sizeof(int));
  if(!reorder){
    free(c);
    free(cardinality);
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
    return NULL;
  }    

  indices = (int *) calloc(num_of_vars, sizeof(int));
  if(!indices){
    free(c);
    free(cardinality);
    free(reorder);
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
    return NULL;
  }    

  /* reorder[i] is the place of i:th variable (in the sense of this program) 
   * in the array variables[].
   * Because of this, vars[] can be given in any order.
   */

  /* init (needed?) */
  for(i = 0; i < num_of_vars; i++)
    indices[i] = 0;

  /* Create the reordering table: O(num_of_vars^2) i.e. stupid but working.
   * Note the temporary use of indices array. */
  for(i = 0; i < num_of_vars; i++){
    temp = nip_variable_id(vars[i]);
    for(j = 0; j < num_of_vars; j++){
      if(nip_variable_id(vars[j]) > temp)
	indices[j]++; /* counts how many greater variables there are */
    }
  }

  for(i = 0; i < num_of_vars; i++)
    reorder[indices[i]] = i; /* fill the reordering */

  c->variables = (nip_variable *) calloc(num_of_vars, sizeof(nip_variable));
  if(!(c->variables)){
    free(c);
    free(cardinality);
    free(reorder);
    free(indices);
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
    return NULL;
  }

  /* JJ_NOTE: reordering probably not required anymore... */
  for(i = 0; i < num_of_vars; i++){
    cardinality[i] = NIP_CARDINALITY(vars[reorder[i]]);
    c->variables[i] = vars[reorder[i]];
  }
  
  c->p = nip_new_potential(cardinality, num_of_vars, NULL);
  c->original_p = nip_new_potential(cardinality, num_of_vars, NULL);

  /* Propagation of error */
  if(c->p == NULL || c->original_p == NULL){
    free(cardinality);
    free(indices);
    free(reorder);
    free(c->variables);
    nip_free_potential(c->p);
    nip_free_potential(c->original_p);
    free(c);
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_GENERAL, 1);
    return NULL;
  }

  free(cardinality);
  free(indices);
  free(reorder);
  c->sepsets = NULL;
  c->mark = NIP_MARK_OFF;

  return c;
}


void nip_free_clique(nip_clique c){
  nip_sepset_link l1, l2;
  nip_clique cln;
  nip_sepset s;

  if(c == NULL)
    return;
  
  /* clean the list of sepsets */
  l1 = c->sepsets;
  while(l1 != NULL){
    l2 = l1->fwd;
    s = (nip_sepset)l1->data;

    /* Remove sepsets from the cliques. */
    cln = s->first_neighbour;
    nip_remove_sepset(cln, s);
    cln = s->second_neighbour;
    nip_remove_sepset(cln, s);

    /* Free the sepset. */
    nip_free_sepset(s);

    l1=l2;
  }
  /* clean the rest */
  nip_free_potential(c->p);
  nip_free_potential(c->original_p);
  free(c->variables);
  free(c);
  return;
}


int nip_add_sepset(nip_clique c, nip_sepset s){

  nip_sepset_link new = (nip_sepset_link) 
    malloc(sizeof(nip_sepsetlink_struct));
  if(!new){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
    return NIP_ERROR_OUTOFMEMORY;
  }

  new->data = s;
  new->fwd = c->sepsets;
  new->bwd = NULL;
  if(c->sepsets != NULL)
    c->sepsets->bwd = new;
  c->sepsets = new;

  return 0;
}


static void nip_remove_sepset(nip_clique c, nip_sepset s){
  nip_sepset_link l;

  if(!(c && s)){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_NULLPOINTER, 1);
    return;
  }
  l = c->sepsets;
  while(l != NULL){
    if(((nip_sepset)l->data) == s){

      if(l->bwd == NULL)
	c->sepsets = l->fwd;
      else
	l->bwd->fwd = l->fwd;

      if(l->fwd != NULL)
	l->fwd->bwd = l->bwd;

      free(l);
      return;
    }      
    l = l->fwd;
  }
  return;
}


/*
 * ATTENTION! Check what this does when num_of_vars == 0.
 */
nip_sepset nip_new_sepset(nip_variable vars[], int num_of_vars, 
			  nip_clique neighbour_a, nip_clique neighbour_b){

  nip_sepset s;
  int *cardinality = NULL;
  int *reorder = NULL;
  int *indices = NULL;
  int i, j;
  unsigned long temp;

  s = (nip_sepset) malloc(sizeof(nip_sepset_struct));

  if(!s){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
    return NULL;
  }

  if(num_of_vars){
    cardinality = (int *) calloc(num_of_vars, sizeof(int));
    if(!cardinality){
      free(s);
      nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
      return NULL;
    }

    reorder = (int *) calloc(num_of_vars, sizeof(int));
    if(!reorder){
      free(s);
      free(cardinality);
      nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
      return NULL;
    }

    indices = (int *) calloc(num_of_vars, sizeof(int));
    if(!indices){
      free(s);
      free(cardinality);
      free(reorder);
      nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
      return NULL;
    }
  }
  else{
    cardinality = NULL;
    reorder = NULL;
    indices = NULL;
  }

  if(num_of_vars){
    s->variables = (nip_variable *) calloc(num_of_vars, sizeof(nip_variable));
    if(!s->variables){
      free(cardinality);
      free(reorder);
      free(indices);
      free(s);
      nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
      return NULL;
    }
  }

  /* reorder[i] is the place of i:th variable (in the sense of this program) 
   * in the array variables[].
   * Because of this, vars[] can be given in any order.
   */

  /* init (needed?) */
  for(i = 0; i < num_of_vars; i++)
    indices[i] = 0;

  /* Create the reordering table: O(num_of_vars^2) i.e. stupid but working.
   * Note the temporary use of indices array. */
  for(i = 0; i < num_of_vars; i++){
    temp = nip_variable_id(vars[i]);
    for(j = 0; j < num_of_vars; j++){
      if(nip_variable_id(vars[j]) > temp)
	indices[j]++; /* counts how many greater variables there are */
    }
  }

  for(i = 0; i < num_of_vars; i++)
    reorder[indices[i]] = i; /* fill the reordering */


  /* JJ_NOTE: reordering probably not required anymore... */
  for(i = 0; i < num_of_vars; i++){
    cardinality[i] = NIP_CARDINALITY(vars[reorder[i]]);
    s->variables[i] = vars[reorder[i]];
  }

  s->old = nip_new_potential(cardinality, num_of_vars, NULL);
  s->new = nip_new_potential(cardinality, num_of_vars, NULL);

  /* Propagation of error */
  if(s->old == NULL || s->new == NULL){
    free(cardinality);
    free(indices);
    free(reorder);
    free(s->variables);
    nip_free_potential(s->old);
    nip_free_potential(s->new);
    free(s);
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_GENERAL, 1);
    return NULL;
  }

  free(cardinality); /* the array was copied ? */
  free(reorder);
  free(indices);
  s->first_neighbour = neighbour_a;
  s->second_neighbour = neighbour_b;

  return s;
}


void nip_free_sepset(nip_sepset s){
  if(s){
    if(s->old->num_of_vars)
      free(s->variables);
    
    nip_free_potential(s->old);
    nip_free_potential(s->new);
    free(s);
  }
  return;
}


nip_potential nip_create_potential(nip_variable variables[], 
				   int num_of_vars, 
				   double data[]){
  /*
   * Suppose we get an array of variables with IDs {5, 2, 3, 4, 0, 1}.
   * In this case, temp_array will be              {5, 2, 3, 4, 0, 1},
   * and reorder will be                           {4, 5, 1, 2, 3, 0}.
   *
   * If we get variables with IDs                  {6, 9, 3, 1, 5, 8},
   * temp_array will be                            {3, 5, 1, 0, 2, 4},
   * and reorder will be                           {3, 2, 4, 0, 5, 1}.
   *
   * temp_array is an array {x_0, x_1, ..., x_N-1}, where x_i
   * is a number indicating how many smaller IDs there are in
   * the variables[] array than the ID of variables[i]
   *
   * reorder is an array {x_0, x_1, ..., x_N-1}, where x_i is
   * the index in variables[] of the variable with the (i+1) -smallest ID.
   * For example, x_0 tells us where in the original array we can find
   * the variable with the smallest ID.
   */

  int i, j, card_temp, index, size_of_data = 1;

  int *cardinality;
  int *indices;
  int *temp_array;
  int *reorder;

  unsigned long temp;
  nip_potential p;

  if((cardinality = (int *) malloc(num_of_vars * sizeof(int))) == NULL) {
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
    return NULL;
  }

  if((indices = (int *) malloc(num_of_vars * sizeof(int))) == NULL) {
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
    free(cardinality);
    return NULL;
  }

  if((temp_array = (int *) malloc(num_of_vars * sizeof(int))) == NULL) {
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
    free(cardinality);
    free(indices);
    return NULL;
  }

  if((reorder = (int *) malloc(num_of_vars * sizeof(int))) == NULL) {
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
    free(cardinality);
    free(indices);
    free(temp_array);
    return NULL;
  }

  /* reorder[i] is the place of i:th variable (in the sense of this program) 
   * in the array variables[] */

  /* init (needed?) */
  for(i = 0; i < num_of_vars; i++)
    temp_array[i] = 0;

  /* Create the reordering table: O(num_of_vars^2) i.e. stupid but working.
   * Note the temporary use of indices array. */
  for(i = 0; i < num_of_vars; i++){
    temp = nip_variable_id(variables[i]);
    for(j = 0; j < num_of_vars; j++){
      if(nip_variable_id(variables[j]) > temp)
	temp_array[j]++; /* counts how many greater variables there are */
    }
  }

  for(i = 0; i < num_of_vars; i++)
    reorder[temp_array[i]] = i; /* fill the reordering */

  /* Figure out some stuff */
  for(i = 0; i < num_of_vars; i++){
    size_of_data *= NIP_CARDINALITY(variables[i]); /* optimal? */
    cardinality[i] = NIP_CARDINALITY(variables[reorder[i]]);
  }

  /* Create a potential */
  p = nip_new_potential(cardinality, num_of_vars, NULL);

  /* Propagation of error */
  if(p == NULL){
    free(cardinality);
    free(indices);
    free(temp_array);
    free(reorder);
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_GENERAL, 1);
    return NULL;
  }
  
  /*
   * NULL data is accepted.
   * In that case, potential will be uniformly distributed.
   */
  if(data != NULL)

    /* Copy the contents to their correct places */
    /******************************************************* 
	  JJT: In principle it is a bit ugly to do this 
	  at this level. If potential.c changes, this has to 
	  be revised too!!! 
    ********************************************************/
    for(i = 0; i < size_of_data; i++){

      /*
       * Now this is the trickiest part.
       * Find out indices (in the internal order of the program,
       * determined by the variable IDs).
       */
      nip_inverse_mapping(p, i, indices); 

      /* calculate the address in the original array */
      index = 0;
      card_temp = 1;

      /* THE mapping */
      for(j = 0; j < num_of_vars; j++){
	index += indices[temp_array[j]] * card_temp;
	card_temp *= cardinality[temp_array[j]];
      }

      /* set the value (in a little ugly way) */
      /* data is being copied => free(data) somewhere */
      p->data[i] = data[index];
    }

  free(cardinality);
  free(indices);
  free(temp_array);
  free(reorder);

  return p;
}


/* NOTE: don't use this. This is just a bad idea we had... */
double* nip_reorder_potential(nip_variable vars[], nip_potential p){

  int old_flat_index, new_flat_index;
  int i, j;
  int *old_indices, *new_indices;
  int *new_card;
  unsigned long smallest_id = 0;
  unsigned long this_id = 0;
  unsigned long biggest_taken = 0;
  int smallest_index = 0;
  double *new_data;
  int card_temp;

  /* Simple (stupid) checks */
  if(!p){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_NULLPOINTER, 1);
    return NULL;
  }
  if(!vars){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_NULLPOINTER, 1);
    return NULL;
  }
  old_indices = (int *) calloc(p->num_of_vars, sizeof(int));
  if(!old_indices){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
    return NULL;
  }
  new_indices = (int *) calloc(p->num_of_vars, sizeof(int));
  if(!new_indices){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
    free(old_indices);
    return NULL;
  }
  new_card = (int *) calloc(p->num_of_vars, sizeof(int));
  if(!new_card){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
    free(old_indices);
    free(new_indices);
    return NULL;
  }
  new_data = (double *) calloc(p->size_of_data, sizeof(double));
  if(!new_data){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
    free(old_indices);
    free(new_indices);
    free(new_card);
    return NULL;
  }
  for(old_flat_index = 0; old_flat_index < p->size_of_data; old_flat_index++){

    /* 1. old_flat_index -> old_indices (inverse_mapping) */
    nip_inverse_mapping(p, old_flat_index, old_indices);

    /* FIXME: This is totally wrong. */
    /* 2. "old_indices" is re-ordered according to "variables"
     *    -> new_indices */
    for(i = 0; i < p->num_of_vars; i++){

      for(j = 0; j < p->num_of_vars; j++){
	this_id = vars[j]->id;
	if(j != 0 && i != 0){
	  if(this_id < smallest_id &&
	     this_id > biggest_taken){
	    smallest_index = j;
	    smallest_id = this_id;
	  }
	}
	else if(j != 0 && i == 0){
	  if(this_id < smallest_id){
	    smallest_index = j;
	    smallest_id = this_id;
	  }
	}
	else if(j == 0 && i != 0){
	  if(this_id > biggest_taken){
	    smallest_index = j;
	    smallest_id = this_id;
	  }
	}
	else{
	  smallest_index = j;
	  smallest_id = this_id;
	}
      }

      new_indices[smallest_index] = old_indices[i];
      new_card[smallest_index] = p->cardinality[i];
      biggest_taken = vars[smallest_index]->id;
    }
    /* <\totally wrong> */

    /* 3. new_indices -> new_flat_index (look at get_ppointer()) */
    new_flat_index = 0;
    card_temp = 1;

    for(i = 0; i < p->num_of_vars; i++){
      new_flat_index += new_indices[i] * card_temp;
      card_temp *= new_card[i];
    }

    printf("New flat index == %d\n", new_flat_index);

    /* 4. */
    new_data[new_flat_index] = p->data[old_flat_index];
  }

  free(old_indices);
  free(new_indices);
  free(new_card);

  /* Pointer to allocated memory. Potential p remains alive also. */
  return new_data;
}


void nip_unmark_clique(nip_clique c){
  if(c != NULL)
    c->mark = NIP_MARK_OFF;
  return;
}


/*
 * Returns 0 if clique is not marked, 1 if it is. (This could be a macro...)
 */
static int nip_clique_marked(nip_clique c){
  if (c)
    return (c->mark == NIP_MARK_ON);
  return 0;
}


int nip_clique_size(nip_clique c){
  if (c)
    return c->p->num_of_vars; /* macro? */
  return 0;
}


int nip_sepset_size(nip_sepset s){
  if (s)
    return s->old->num_of_vars; /* macro? */
  return 0;
}


int nip_distribute_evidence(nip_clique c){

  int retval;
  nip_sepset_link l;
  nip_sepset s;

#ifdef NIP_DEBUG_CLIQUE
  printf("Distributing evidence in ");
  print_clique(c);
#endif

  /* mark */
  c->mark = NIP_MARK_ON;

  /* pass the messages */
  l = c->sepsets;
  while (l != 0){
    s = l->data;
    if(!nip_clique_marked(s->first_neighbour)){
      retval = nip_message_pass(c, s, s->first_neighbour); /* pass a message */
      if(retval != NIP_NO_ERROR){
	nip_report_error(__FILE__, __LINE__, NIP_ERROR_GENERAL, 1);
	return NIP_ERROR_GENERAL;
      }
    }
    else if(!nip_clique_marked(s->second_neighbour)){
      retval = nip_message_pass(c, s, s->second_neighbour); /* pass a message */
      if(retval != NIP_NO_ERROR){
	nip_report_error(__FILE__, __LINE__, NIP_ERROR_GENERAL, 1);
	return NIP_ERROR_GENERAL;
      }
    }
    l = l->fwd;
  }

  /* call neighboring cliques */
  l = c->sepsets;
  while (l != 0){
    s = l->data;
    if(!nip_clique_marked(s->first_neighbour)){
      if(nip_distribute_evidence(s->first_neighbour) != NIP_NO_ERROR){
	nip_report_error(__FILE__, __LINE__, NIP_ERROR_GENERAL, 1);
	return NIP_ERROR_GENERAL;
      }
    }
    else if(!nip_clique_marked(s->second_neighbour)){
      if(nip_distribute_evidence(s->second_neighbour) != NIP_NO_ERROR){
	nip_report_error(__FILE__, __LINE__, NIP_ERROR_GENERAL, 1);
	return NIP_ERROR_GENERAL;
      }
    }
    l = l->fwd;
  }
  return NIP_NO_ERROR;
}


int nip_collect_evidence(nip_clique c1, nip_sepset s12, nip_clique c2){

  int retval;
  nip_sepset_link l;
  nip_sepset s;

  /* mark */
  c2->mark = NIP_MARK_ON;

  /* call neighboring cliques */
  l = c2->sepsets;
  while (l != NULL){
    s = l->data;
    if(!nip_clique_marked(s->first_neighbour)){
      if(nip_collect_evidence(c2, s, s->first_neighbour) != NIP_NO_ERROR){
	nip_report_error(__FILE__, __LINE__, NIP_ERROR_GENERAL, 1);
	return NIP_ERROR_GENERAL;
      }
    }
    else if(!nip_clique_marked(s->second_neighbour))
      if(nip_collect_evidence(c2, s, s->second_neighbour) != NIP_NO_ERROR){
	nip_report_error(__FILE__, __LINE__, NIP_ERROR_GENERAL, 1);
	return NIP_ERROR_GENERAL;
      }
    l = l->fwd;
  }

  /* pass the message to c1 */
  if((c1 != NULL) && (s12 != NULL)){
    retval = nip_message_pass(c2, s12, c1);
    if(retval != NIP_NO_ERROR){
      nip_report_error(__FILE__, __LINE__, NIP_ERROR_GENERAL, 1);
      return NIP_ERROR_GENERAL;
    }
  }

#ifdef NIP_DEBUG_CLIQUE
  if(c1 != NULL && c2 != NULL){
    printf("Collecting evidence from ");
    print_clique(c2);
    printf(" to ");
    print_clique(c1);
  }
#endif

  return NIP_NO_ERROR;
}


/*
 * Method for passing messages between cliques.
 * The message goes from clique c1 through sepset s to clique c2.
 * Returns an error code.
 */
static int nip_message_pass(nip_clique c1, nip_sepset s, nip_clique c2){
  int i, j = 0, k = 0;
  int retval;
  int *mapping;

  /* save the newer potential as old by switching the pointers */
  nip_potential temp;
  temp = s->old;
  s->old = s->new;
  s->new = temp;

  /*
   * Marginalise (projection).
   * First: select the variables. This takes O(n^2)
   */
  mapping = nip_mapper(c1->variables, s->variables, 
		       c1->p->num_of_vars, s->new->num_of_vars);

  /* Information flows from clique c1 to sepset s. */
  retval = nip_general_marginalise(c1->p, s->new, mapping);
  if(retval != NIP_NO_ERROR){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_GENERAL, 1);
    free(mapping);
    return NIP_ERROR_GENERAL;
  }

  j = 0; k = 0;

  /*
   * Update (absorption).
   */
  for(i=0; i < c2->p->num_of_vars; i++){
    if(k == s->new->num_of_vars)
      break; /* all found */
    for(j=0; j < s->new->num_of_vars; j++)
      if(nip_equal_variables((c2->variables)[i], (s->variables)[j])){
	mapping[j] = i;
	k++;
	break;
      }
  }

  /* Information flows from sepset s to clique c2. */
  retval = nip_update_potential(s->new, s->old, c2->p, mapping);
  if(retval != NIP_NO_ERROR){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_GENERAL, 1);
    free(mapping);
    return NIP_ERROR_GENERAL;
  }

  free(mapping);

  return NIP_NO_ERROR;
}


int nip_init_clique(nip_clique c, nip_variable child, 
		    nip_potential p, int transient){
  int i, j = 0, k = 0;
  int *mapping = NULL;
  int retval;
  nip_variable var = NULL;
  nip_variable* parents = nip_get_parents(child);

  if(p->num_of_vars < c->p->num_of_vars){
    mapping = (int *) calloc(p->num_of_vars, sizeof(int));
    if(!mapping){
      nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
      return NIP_ERROR_OUTOFMEMORY;
    }    
    
    /***************************************************************/
    /* HEY! parents[] NOT assumed to be in any particular order!   */
    /* But the variables of p are assumed to be in the same order  */
    /* as in the clique c!                                         */
    /***************************************************************/
    
    /* initialisation with conditional distributions 
       first: select the variables (in a stupid but working way) */
    for(i=0; i < c->p->num_of_vars; i++){
      if(k == p->num_of_vars)
	break; /* all found */
      var = (c->variables)[i];
      
      for(j=0; j < p->num_of_vars - 1; j++)
	if(nip_equal_variables(var, parents[j]))
	  mapping[k++] = i;
      
      if(nip_equal_variables(var, child))
	mapping[k++] = i;
    }
  }

  /* rest the case */
  retval = nip_init_potential(p, c->p, mapping);
  if(retval != NIP_NO_ERROR){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_GENERAL, 1);
    free(mapping);
    return NIP_ERROR_GENERAL;
  }

  /* Some extra work is done here,
   * because only the last initialisation counts. */
  if(!transient){
    retval = nip_init_potential(p, c->original_p, mapping);
    if(retval != NIP_NO_ERROR){
      nip_report_error(__FILE__, __LINE__, NIP_ERROR_GENERAL, 1);
      free(mapping);
      return NIP_ERROR_GENERAL;
    }
  }
  /* This should be an optional phase: in timeslice models 
   * some clique potentials need to be initialised but still 
   * be retractable... */

  free(mapping); /* free(NULL) is O.K. */
  return NIP_NO_ERROR;
}


int nip_marginalise_clique(nip_clique c, nip_variable v, double r[]){
  int index = nip_clique_var_index(c, v);
  int retval;

  /* variable not in this clique => NIP_ERROR */
  if(index == -1){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_INVALID_ARGUMENT, 1);
    return NIP_ERROR_INVALID_ARGUMENT;
  }
  
  retval = nip_total_marginalise(c->p, r, index);
  if(retval != NIP_NO_ERROR)
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_GENERAL, 1);

  return(retval);
}


int nip_global_retraction(nip_variable* vars, int nvars, 
			  nip_clique* cliques, int ncliques){
  int i, index;
  int retval;
  nip_variable v;
  nip_clique c;

  for(index = 0; index < ncliques; index++)
    nip_unmark_clique(cliques[index]);

  /* Reset all the potentials back to the original.
   * NOTE: this excludes the priors. */
  nip_join_tree_dfs(cliques[0], nip_retract_clique, nip_retract_sepset, NULL);

  /* Enter evidence back to the join tree.
   * Does not enter the priors... */
  for(i = 0; i < nvars; i++){
    v = vars[i];
    c = nip_find_family(cliques, ncliques, v);
    index = nip_clique_var_index(c, v);

    retval = nip_update_evidence(v->likelihood, NULL, c->p, index);
    if(retval != NIP_NO_ERROR){
      nip_report_error(__FILE__, __LINE__, NIP_ERROR_GENERAL, 1);
      return NIP_ERROR_GENERAL;
    }
  }
  return NIP_NO_ERROR;
}


int nip_enter_observation(nip_variable* vars, int nvars, 
			  nip_clique* cliques, int ncliques, 
			  nip_variable v, char *state){
  int index = nip_variable_state_index(v, state);
  if(index < 0)
    return NIP_NO_ERROR;
  return nip_enter_index_observation(vars, nvars, 
				     cliques, ncliques, 
				     v, index);
}


int nip_enter_index_observation(nip_variable* vars, int nvars, 
				nip_clique* cliques, int ncliques, 
				nip_variable v, int index){
  int i, retval;
  double *evidence;

  if(index < 0)
    return NIP_NO_ERROR;

  evidence = (double *) calloc(NIP_CARDINALITY(v), sizeof(double));
  if(!evidence){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
    return NIP_ERROR_OUTOFMEMORY;
  }

  for(i = 0; i < NIP_CARDINALITY(v); i++)
    if(i == index)
      evidence[i] = 1;
    else
      evidence[i] = 0;

  retval = nip_enter_evidence(vars, nvars, cliques, ncliques, v, evidence);
  free(evidence);
  return retval;
}


int nip_enter_evidence(nip_variable* vars, int nvars, 
		       nip_clique* cliques, int ncliques, 
		       nip_variable v, double evidence[]){
  int index, i;
  int retraction = 0;
  int retval;
  nip_clique c;

  if(v == NULL || evidence == NULL){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_NULLPOINTER, 1);
    return NIP_ERROR_NULLPOINTER;
  }

  c = nip_find_family(cliques, ncliques, v);    
  if(!c){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_INVALID_ARGUMENT, 0);
    return NIP_ERROR_INVALID_ARGUMENT;
  }

  index = nip_clique_var_index(c, v);

  for(i = 0; i < NIP_CARDINALITY(v); i++)
    if((v->likelihood)[i] == 0 && evidence[i] != 0)
      retraction = 1; /* Must do global retraction! */
  
  /*
   * Here is the update of clique potential.
   * MUST be done before update_likelihood.
   */
  if(!retraction){
    retval = nip_update_evidence(evidence, v->likelihood, c->p, index);
    if(retval != NIP_NO_ERROR){
      nip_report_error(__FILE__, __LINE__, NIP_ERROR_GENERAL, 1);
      return NIP_ERROR_GENERAL;
    }
  }

  /* Update likelihood. Check the return value. */
  retval = nip_update_likelihood(v, evidence);
  if(retval != NIP_NO_ERROR){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_GENERAL, 1);
    return NIP_ERROR_GENERAL;
  }

  if(retraction){
    retval = nip_global_retraction(vars, nvars, cliques, ncliques);
    if(retval != NIP_NO_ERROR)
      nip_report_error(__FILE__, __LINE__, NIP_ERROR_GENERAL, 1);
    return(retval);
  }

  return NIP_NO_ERROR;
}


int nip_enter_prior(nip_variable* vars, int nvars, 
		    nip_clique* cliques, int ncliques, 
		    nip_variable v, double prior[]){
  int index, i;
  int e;
  nip_clique c;

  if(v == NULL || prior == NULL){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_NULLPOINTER, 1);
    return NIP_ERROR_NULLPOINTER;
  }

  /* printf("Entering prior for variable %s:\n", v->symbol); DEBUG */

  c = nip_find_family(cliques, ncliques, v);    

  e = 1;
  for(i = 0; i < NIP_CARDINALITY(v); i++){
    /* printf("%g ", prior[i]); DEBUG */
    if(prior[i] > 0)
      e = 0; /* Not a zero vector... */
  }
  /* printf("\n"); DEBUG */

  if(!c || e){ /* v not in any clique or prior is a zero vector */
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_INVALID_ARGUMENT, 0);
    return NIP_ERROR_INVALID_ARGUMENT;
  }

  index = nip_clique_var_index(c, v);

  /*
   * Here is the update of clique potential: simply a multiplication, 
   * effects of which may not disappear if it gets multiplied again
   * when the variable is observed and actual evidence entered.
   */
  e = nip_update_evidence(prior, NULL, c->p, index);
  if(e != NIP_NO_ERROR){
    nip_report_error(__FILE__, __LINE__, e, 1);
    return e;
  }

  /* Don't update the likelihood... */

  return NIP_NO_ERROR;
}


/*
 * Method for checking if variable v is part of clique c.
 * Returns -1 if not, else the index of v among the variables in c.
 */
static int nip_clique_var_index(nip_clique c, nip_variable v){
  int var = 0;

  if(!(c && v)){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_NULLPOINTER, 1);
    return -1;
  }

  while(!nip_equal_variables(v, c->variables[var])){
    var++;
    if(var == c->p->num_of_vars)
      /* variable not in this clique => -1 */
      return -1;
  }
  return var;
}


nip_clique nip_find_family(nip_clique *cliques, int ncliques, 
			   nip_variable var){
  int i, n;
  nip_variable* family = NULL;
  nip_clique found;

  /* NOTE: uses memoization for finding families */
  if(var->family_clique != NULL)
    return (nip_clique)(var->family_clique);

  n = nip_number_of_parents(var);
  family = (nip_variable*) calloc(n + 1, sizeof(nip_variable));
  if(!family){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
    return NULL;
  }

  if(n > 0) /* A useful check or not? */
    for(i = 0; i < n; i++)
      family[i] = var->parents[i];
  family[n] = var;

  found = nip_find_clique(cliques, ncliques, family, n+1);
  var->family_clique = found; /* MEMOIZE! */
  free(family);
  return found;
}


int* nip_find_family_mapping(nip_clique family, nip_variable child){
  int i, j, n, p;
  int *result = NULL;

  if(child->family_mapping == NULL){ /* if not found yet */
    n = child->num_of_parents + 1;
    result = (int *) calloc(n, sizeof(int));
    if(!result){
      nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
      return NULL;
    }

    /* NOTE: Child must always be the first in the target potential! */
    for(i=0; i < family->p->num_of_vars; i++)
      if(nip_equal_variables((family->variables)[i], child)){
	result[0] = i;
	break;
      }

    if(n > 1){ /* if child actually has any parents */
      p = 0;
      n--;
      for(i=0; i < family->p->num_of_vars; i++){
	if(p == n)
	  break; /* all pointers found */
	for(j=0; j < n; j++)
	  if(nip_equal_variables((family->variables)[i], child->parents[j])){
	    result[j+1] = i;
	    p++;
	    break;
	  }
      }
    }
    child->family_mapping = result; /* MEMOIZE */
  }
  return child->family_mapping;
}


nip_clique nip_find_clique(nip_clique *cliques, int ncliques, 
			   nip_variable *variables, int nvars){
  int i, j, k;
  int ok;

  for(i = 0; i < ncliques; i++){
    ok = 0;
    for(j = 0; j < nvars; j++){
      for(k = 0; k < cliques[i]->p->num_of_vars; k++){
	if(nip_equal_variables(variables[j], cliques[i]->variables[k])){
	  ok++;
	  break; /* found the variable in the clique */
	}
      }
    }
    /* All variables found in the clique */
    if(ok == nvars)
      return cliques[i];
  }

  return NULL;
}


/* Finds out if two cliques are in the same tree.
 * Returns 1 if they are, 0 if not.
 * cliques must be unmarked before calling this.
 */
int nip_cliques_connected(nip_clique one, nip_clique two){
  nip_sepset_link l;
  nip_sepset s;

  if(one == NULL || two == NULL) /* normal situation or an error? */
    return 0; /* as in FALSE */

  l = one->sepsets;

  /* mark */
  one->mark = NIP_MARK_ON;

  /* NOTE: this defines the equality of cliques. */
  if(one == two)
    return 1; /* TRUE (end of recursion) */

  /* call neighbouring cliques */
  while (l != NULL){
    s = (nip_sepset)(l->data);
    if(!nip_clique_marked(s->first_neighbour)){
      if(nip_cliques_connected(s->first_neighbour, two))
	return 1; /* TRUE */
    }
    else if(!nip_clique_marked(s->second_neighbour)){
      if(nip_cliques_connected(s->second_neighbour, two))
	return 1; /* TRUE */
    }
    l = l->fwd;
  }

  return 0; /* FALSE */
}


void nip_fprintf_clique(FILE* stream, nip_clique c){
  int i;
  fprintf(stream, "clique ");
  /*fprintf(stream, "( ");*/
  for(i = 0; i < c->p->num_of_vars; i++)
    fprintf(stream, "%s ", c->variables[i]->symbol);
  /*fprintf(stream, ")");*/
  fprintf(stream, "\n");
}


void nip_fprintf_sepset(FILE* stream, nip_sepset s){
  int i;
  fprintf(stream, "sepset ");
  /*fprintf(stream, "[ ");*/
  for(i = 0; i < s->old->num_of_vars; i++)
    fprintf(stream, "%s ", s->variables[i]->symbol);
  /*fprintf(stream, "]");*/
  fprintf(stream, "\n");
}


static void nip_retract_clique(nip_clique c, double* ptr){
  int i;
  for(i = 0; i < c->p->size_of_data; i++)
    c->p->data[i] = c->original_p->data[i]; /* TODO: nip_copy_potential? */
}


static void nip_retract_sepset(nip_sepset s, double* ptr){
  nip_uniform_potential(s->old, 1);
  nip_uniform_potential(s->new, 1);
}


/*
 * A generic function for traversing the join tree. 
 * cliques must be unmarked before calling this.
 * Parameters:
 * - a clique where the DFS starts
 * - a function pointer to the function to be used for every clique on the way
 * - a function pointer to the function to be used for every sepset on the way
 * - a double pointer where a return value can be written
 */
static void nip_join_tree_dfs(nip_clique start, 
			      void (*cFuncPointer)(nip_clique, double*),
			      void (*sFuncPointer)(nip_sepset, double*),
			      double* ptr) {

  /* a lot of copy-paste from collect/distribute_evidence and clique_search */
  nip_sepset_link l = start->sepsets;
  nip_sepset s;
  
  if(start == NULL) /* error? */
    return;

  /* mark */
  start->mark = NIP_MARK_ON;

  if(cFuncPointer)
    cFuncPointer(start, ptr); /* do it now or after the children ??? */

  /* call neighboring cliques */
  while (l != NULL){
    s = (nip_sepset)(l->data);
    if(!nip_clique_marked(s->first_neighbour)){
      if(sFuncPointer)
	sFuncPointer(s, ptr);
      nip_join_tree_dfs(s->first_neighbour, cFuncPointer, sFuncPointer, ptr);
    }
    else if(!nip_clique_marked(s->second_neighbour)){
      if(sFuncPointer)
	sFuncPointer(s, ptr);
      nip_join_tree_dfs(s->second_neighbour, cFuncPointer, sFuncPointer, ptr);
    }
    l = l->fwd;
  }
  return;
}


static void nip_clique_mass(nip_clique c, double* ptr){
  int i;
  double m = 0;
  for(i = 0; i < c->p->size_of_data; i++)
    m += c->p->data[i]; /* TODO: nip_potential_mass? */
  *ptr += m;
  return;
}

static void nip_neg_sepset_mass(nip_sepset s, double* ptr){
  int i;
  double m = 0;
  for(i = 0; i < s->new->size_of_data; i++)
    m += s->new->data[i];
  *ptr -= m;
  return;
}

double nip_probability_mass(nip_clique* cliques, int ncliques){
  int i;
  double ret;

  /* unmark all cliques */
  for(i = 0; i < ncliques; i++)
    nip_unmark_clique(cliques[i]);

  /* init */
  ret = 0;

  /* sum of the probability mass in cliques minus the mass in sepsets */
  nip_join_tree_dfs(cliques[0], nip_clique_mass, nip_neg_sepset_mass, &ret);
  return ret;
}


/** TODO: Currently this computes potentials in EVERY node of join tree 
 ** even if the answer can be found in a single node. It could try to 
 ** remember the found variables and prune the DFS after all necessary 
 ** variables have been encountered.
 **/
nip_potential nip_gather_joint_probability(nip_clique start, 
					   nip_variable *vars, int n_vars,
					   nip_variable *isect, int n_isect){  
  /* a lot of copy-paste from jtree_dfs */
  int i, j, k, m, n;
  int retval;
  int *mapping = NULL;
  int *cardinality = NULL;
  nip_potential product = NULL;
  nip_potential sum = NULL;
  nip_potential rest = NULL;
  nip_variable *union_vars = NULL; 
  int nuv;
  nip_variable *rest_vars = NULL; /* for recursion */
  int nrv;
  nip_variable *temp = NULL;
  int nt;
  nip_sepset_link l;
  nip_sepset s;
  nip_clique c;
  
  /* error? */
  if(start == NULL || n_vars < 0){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_INVALID_ARGUMENT, 1);
    return NULL;
  }
  if(n_vars == 0)
    return nip_new_potential(NULL, 0, NULL); /* NOTE: verify correctness!? */
  if(vars == NULL){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_INVALID_ARGUMENT, 1);
    return NULL;
  }

  /* List of neigboring sepsets */
  l = start->sepsets;

  /* Mark the clique */
  start->mark = NIP_MARK_ON;


  /*** 1. Reserve space ***/

  /* 1.1 Form the union of given variables and clique variables */
  nuv = 0; /* size of intersection */
  for(i = 0; i < n_vars; i++){
    for(j = 0; j < start->p->num_of_vars; j++){
      /* (variables in isect are a subset of clique variables) */
      if(nip_equal_variables(vars[i], start->variables[j])){
	nuv++;
	break;
      }
    }
  }
  nuv = n_vars + start->p->num_of_vars - nuv; /* size of union */
  union_vars = (nip_variable*) calloc(nuv, sizeof(nip_variable));
  if(!union_vars){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
    return NULL;
  }
  /* A certain kind of union */
  for(i = 0; i < n_vars; i++)
    union_vars[i] = vars[i]; /* first the given variables... */
  for(i = 0; i < n_isect; i++)
    union_vars[n_vars + i] = isect[i];
  k = n_vars + n_isect; /* ...then rest of the clique variables */
  for(i = 0; i < start->p->num_of_vars; i++){
    m = 1;
    for(j = 0; j < n_vars; j++){
      if(nip_equal_variables(vars[j], start->variables[i])){
	m = 0; /* don't add duplicates */
	break;
      }
    }
    for(j = 0; j < n_isect; j++){
      if(nip_equal_variables(isect[j], start->variables[i])){
	m = 0; /* don't add duplicates */
	break;
      }
    }
    if(m)
      union_vars[k++] = start->variables[i];
  }

  /* 1.2 Potential for the union of variables */
  cardinality = (int*) calloc(nuv, sizeof(int));
  if(!cardinality){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
    free(union_vars);
    return NULL;
  }
  for(i = 0; i < nuv; i++)
    cardinality[i] = NIP_CARDINALITY(union_vars[i]);

  /* ### possibly HUGE potential array ! ### */
  product = nip_new_potential(cardinality, nuv, NULL); 

  /* free(cardinality);
   * reuse the larger cardinality array: nuv >= n_vars */

  /*** 2. Multiply (<start> clique) ***/
  
  /* 2.1 Form the mapping between potentials */
  /* NOTE: a slightly bigger array allocated for future purposes also. */
  mapping = (int*) calloc(nuv, sizeof(int));
  if(!mapping){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
    free(cardinality);
    free(union_vars);
    nip_free_potential(product);
    return NULL;
  }
  /* perhaps this could have beed done earlier... */
  for(i = 0; i < start->p->num_of_vars; i++){
    for(j = 0; j < nuv; j++){ /* linear search */
      if(nip_equal_variables(start->variables[i], union_vars[j])){
	mapping[i] = j;
	break;
      }
    }
  }

  /* 2.2 Do the multiplication of potentials */
  retval = nip_update_potential(start->p, NULL, product, mapping);
  if(retval != NIP_NO_ERROR){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_GENERAL, 1);
    free(cardinality);
    free(union_vars);
    nip_free_potential(product);
    return NULL;
  }
  /* free(mapping);
   * reuse the mapping array: 
   * the recursive result may require one with <nrv> elements. */

  /** Traverse to the neighboring cliques in the tree **/
  while (l != NULL){
    s = (nip_sepset)(l->data);
    /* try both directions in a neighboring sepset */
    for(i = 0; i < 2; i++){ 
      if (i==0)
	c = s->first_neighbour;
      else
	c = s->second_neighbour;

      if(!nip_clique_marked(c)){
	
	/*** 3. Operations on potentials ***/

	/* 3.1 Mapping between sepset and product potentials */
	for(j = 0; j < s->new->num_of_vars; j++){
	  for(k = 0; k < nuv; k++){ /* linear search */
	    if(nip_equal_variables(s->variables[j], union_vars[k])){
	      mapping[j] = k;
	      break;
	    }
	  }
	}

	/* 3.2 Division with sepset potential */
	retval = nip_update_potential(NULL, s->new, product, mapping);
	if(retval != NIP_NO_ERROR){
	  nip_report_error(__FILE__, __LINE__, NIP_ERROR_GENERAL, 1);
	  free(cardinality);
	  free(union_vars);
	  free(mapping);
	  nip_free_potential(product);
	  return NULL;
	}

	/* 3.3 Decide what kind of potential you need 
	 *     from the rest of the tree */

	/* original <vars> and intersection of cliques */
	/* unfortunately we have to remove duplicates */
	temp = s->variables;
	nt = s->new->num_of_vars;
	nrv = nt;
	for(j = 0; j < nt; j++){
	  for(k = 0; k < n_vars; k++){
	    if(nip_equal_variables(temp[j], vars[k])){
	      nrv--;
	      break;
	    }
	  }
	}
	rest_vars = (nip_variable*) calloc(nrv, sizeof(nip_variable));
	if(!rest_vars){
	  nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
	  free(cardinality);
	  free(union_vars);
	  free(mapping);
	  nip_free_potential(product);
	  return NULL;
	}
	n = 0;
	for(j = 0; j < nt; j++){
	  m = 1;
	  for(k = 0; k < n_vars; k++){
	    if(nip_equal_variables(temp[j], vars[k])){
	      m = 0; /* don't add duplicates */
	      break;
	    }
	  }
	  if(m)
	    rest_vars[n++] = temp[j];
	}

	/* 3.4 Continue DFS */
	rest = nip_gather_joint_probability(c, vars, n_vars, rest_vars, nrv);
	
	/* 3.5 Mapping between product potential and recursive result */
	for(j = 0; j < n_vars; j++)
	  mapping[j] = j; /* the first part is trivial */
	for(j = 0; j < nrv; j++){
	  for(k = n_vars; k < nuv; k++){
	    if(nip_equal_variables(rest_vars[j], union_vars[k])){
	      mapping[n_vars + j] = k;
	      break;
	      /* (Reminder: once this block missed braces...
	       * Had a lot of fun while hunting for the bug... :)*/
	    }
	  }
	}

	/* 3.6 Multiplication with the recursive results */
	retval = nip_update_potential(rest, NULL, product, mapping);
	if(retval != NIP_NO_ERROR){
	  nip_report_error(__FILE__, __LINE__, NIP_ERROR_GENERAL, 1);
	  free(cardinality);
	  free(union_vars);
	  free(mapping);
	  nip_free_potential(product);
	  nip_free_potential(rest);
	  return NULL;
	}
	free(rest_vars);
	nip_free_potential(rest);
      }
    }

    l = l->fwd; /* next neigboring sepset */
  }
  free(union_vars);

  /*** 4. Marginalisation (if any?) ***/

  /* If we already have what we need, no marginalisation needed... */
  if(nuv == n_vars + n_isect){
    free(cardinality);
    sum = product;
  }
  else{
    
    /* 4.1 Reserve space for the result */
    for(i = 0; i < n_vars; i++)
      cardinality[i] = NIP_CARDINALITY(vars[i]);
    for(i = 0; i < n_isect; i++)
      cardinality[n_vars + i] = NIP_CARDINALITY(isect[i]);
    /* possibly LARGE potential array ! */
    sum = nip_new_potential(cardinality, n_vars + n_isect, NULL); 
    free(cardinality);
    
    /* 4.2 Form the mapping between product and sum potentials */
    for(i = 0; i < n_vars + n_isect; i++)
      mapping[i] = i; /* Trivial because of the union operation above */
    
    /* 4.3 Marginalise */
    retval = nip_general_marginalise(product, sum, mapping);
    if(retval != NIP_NO_ERROR){
      nip_report_error(__FILE__, __LINE__, NIP_ERROR_GENERAL, 1);
      free(mapping);
      nip_free_potential(product);
      nip_free_potential(sum);
      return NULL;
    }
    /* The gain of having a join tree in the first place: */
    nip_free_potential(product);
  }
  
  /* 4.4 Normalise (?) */
  /* Q: is this a good idea at this point? */
  /*normalise_potential(sum);*/

  free(mapping);
  return sum;
}

/* TODO: replace with nip_variable_isect() ! */
int nip_clique_intersection(nip_clique cl1, nip_clique cl2, 
			    nip_variable **vars, int *n){

  int i, j;
  int max_vars = cl2->p->num_of_vars;
  int realsize = 0;
  nip_variable *isect;
  nip_variable *shaved_isect;

  if(cl1->p->num_of_vars < max_vars)
    max_vars = cl1->p->num_of_vars;

  isect = (nip_variable *) calloc(max_vars, sizeof(nip_variable));

  if(!isect){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
    return NIP_ERROR_OUTOFMEMORY;
  }

  for(i = 0; i < cl1->p->num_of_vars; i++)
    for(j = 0; j < cl2->p->num_of_vars; j++){

      if(nip_equal_variables(cl1->variables[i], cl2->variables[j]))
	isect[realsize++] = cl1->variables[i];
    }

  if(realsize == 0){
    free(isect);
    *vars = NULL;
    *n = 0;
    return NIP_NO_ERROR;
  }

  /* Intersection is non-empty, realsize > 0 */

  shaved_isect = (nip_variable *) calloc(realsize, sizeof(nip_variable));

  if(!shaved_isect){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
    free(isect);
    return NIP_ERROR_OUTOFMEMORY;
  }

  for(i = 0; i < realsize; i++)
    shaved_isect[i] = isect[i];
  
  free(isect);
  *vars = shaved_isect;
  *n = realsize;

  return NIP_NO_ERROR;
}


nip_potential_list nip_new_potential_list(){
  nip_potential_list pl = (nip_potential_list) 
    malloc(sizeof(nip_potential_list_struct));
  /* Q: What if NULL was returned? */
  pl->length = 0;
  pl->first  = NULL;
  pl->last   = NULL;
  return pl;
}


int nip_append_potential(nip_potential_list l, nip_potential p, 
			 nip_variable child, nip_variable* parents){
  nip_potential_link new = (nip_potential_link) 
    malloc(sizeof(nip_potential_link_struct));

  if(!l || !p){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_INVALID_ARGUMENT, 1);
    return NIP_ERROR_INVALID_ARGUMENT;
  }

  if(!new){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
    return NIP_ERROR_OUTOFMEMORY;
  }

  new->data = p;
  new->child = child;
  /* The "ownership" of the parents[] array changes! */
  new->parents = parents;
  new->fwd = NULL;
  new->bwd = l->last;
  if(l->first == NULL)
    l->first = new;
  else
    l->last->fwd = new;

  l->last = new;
  l->length++;
  return NIP_NO_ERROR;
}


int nip_prepend_potential(nip_potential_list l, nip_potential p, 
			  nip_variable child, nip_variable* parents){
  nip_potential_link new = (nip_potential_link) 
    malloc(sizeof(nip_potential_link_struct));

  if(!l || !p){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_INVALID_ARGUMENT, 1);
    return NIP_ERROR_INVALID_ARGUMENT;
  }

  if(!new){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
    return NIP_ERROR_OUTOFMEMORY;
  }

  new->data = p;
  new->child = child;
  new->parents = parents;
  new->bwd = NULL;
  new->fwd = l->first;
  if(l->last == NULL)
    l->last = new;
  else
    l->first->bwd = new;

  l->first = new;
  l->length++;
  return NIP_NO_ERROR;
}


void nip_free_potential_list(nip_potential_list l){
  nip_potential_link ln;

  if(!l)
    return;
  
  ln = l->last;

  l->last = NULL;
  while(ln != NULL){
    if(ln->fwd != NULL){
      nip_free_potential(ln->fwd->data);
      free(ln->fwd->parents);
      free(ln->fwd);
    }
    ln = ln->bwd;
  }
  if(l->first != NULL){
    nip_free_potential(l->first->data);
    free(l->first->parents);
    free(l->first);
    l->first = NULL;
  }
  l->length = 0;

  free(l);
  l = NULL;
  return;
}

/* More operations for nip_potential_list? */