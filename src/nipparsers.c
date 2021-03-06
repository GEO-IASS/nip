/*  NIP - Dynamic Bayesian Network library
    Copyright (C) 2012  Janne Toivola

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, see <http://www.gnu.org/licenses/>.
*/

/* nipparsers.c
 * Author: Janne Toivola
 * Version: $Id: nipparsers.c,v 1.1 2010-12-07 17:23:18 jatoivol Exp $
 */


#include "nipparsers.h"


/* #define DEBUG_PARSER */

/* #define PRINT_TOKENS */

/* #define DEBUG_DATAFILE */

static int nip_null_observation(char* token);

static void nip_free_data_file(nip_data_file f);

nip_data_file nip_open_data_file(char* filename, char separator,
				 int write, int nodenames){

  char last_line[MAX_LINELENGTH];
  char* token;
  int length_of_name = 0;
  int num_of_tokens = 0;
  int* token_bounds;
  int linecounter = 0;
  int tscounter = 0;
  int dividend;
  int i, j, state;
  int empty_lines_read = 0;
  nip_string_list* statenames = NULL;
  nip_data_file f = NULL;

  f = (nip_data_file) malloc(sizeof(nip_data_file_struct));

  if(f == NULL){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
    return NULL;
  }

  f->name = NULL;
  f->separator = separator;
  f->file = NULL;
  f->is_open = 0;
  f->first_line_labels = 0;
  f->current_line = 0;
  f->label_line = -1;
  f->ndatarows = 0;
  f->datarows = NULL;
  f->node_symbols = NULL;
  f->num_of_nodes = 0;
  f->node_states = NULL;
  f->num_of_states = NULL;

  if(write)
    f->file = fopen(filename,"w");
  else
    f->file = fopen(filename,"r");

  if (!f->file){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_IO, 1);
    free(f);
    return NULL; /* fopen(...) failed */
  }
  else
    f->is_open = 1;

  length_of_name = strlen(filename);

  f->name = (char *) calloc(length_of_name + 1, sizeof(char));
  if(!f->name){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
    fclose(f->file);
    free(f);
    return NULL;
  }

  strcpy(f->name, filename);

  /* If the file is opened in read mode, check the contents.
   * This includes names of nodes and their states.
   */
  if(!write){
    linecounter = 0;
    empty_lines_read = 0;
    /* tries to ignore the empty lines in the beginning of file
     * and between the node labels and data */
    state = 1; /* ignores empty lines before data */
    if(nodenames)
      state = 2; /* ignores empty lines before node labels */

    while(fgets(last_line, MAX_LINELENGTH, f->file)){
      /* treat the white space as separators */
      num_of_tokens = nip_count_tokens(last_line, NULL, 0, 
				       &separator, 1, 0, 1);
      (f->current_line)++;

      /* JJT  1.9.2004: A sort of bug fix. Ignore empty lines */
      /* JJT 22.6.2005: Another fix. Ignore only the empty lines 
       * immediately after the node labels... and duplicate empty lines.
       * Otherwise start a new timeseries */
      if(num_of_tokens == 0){
	linecounter = 0;
	empty_lines_read++;
	if(state || empty_lines_read > 1)
	  continue; /* empty lines to be ignored */
      }
      else{
	linecounter++;
	empty_lines_read = 0;
	if(state > 0){
	  if(state > 1){
	    f->label_line = f->current_line;
	    linecounter = 0; /* reset for data lines */
	  }
	  state--; /* stop ignoring single empty lines and the node names */
	}
	if(state == 0 && linecounter == 1)
	  (f->ndatarows)++;
      }
    }

    /* rewind */
    rewind(f->file);
    f->current_line = 0;
    linecounter = 0;
    state = 1;
    if(nodenames)
      state = 2;

    /* allocate f->datarows array */
    f->datarows = (int*) calloc(f->ndatarows, sizeof(int));
    /* NOTE: calloc resets the contents to zero! */
    if(!f->datarows){
      nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
      nip_close_data_file(f);
      return NULL;
    }

    while(fgets(last_line, MAX_LINELENGTH, f->file)){
      /* treat the white space as separators */
      num_of_tokens = nip_count_tokens(last_line, NULL, 0, 
				       &separator, 1, 0, 1);

      if(num_of_tokens > 0){
	token_bounds =
	  nip_tokenise(last_line, num_of_tokens, 0, &separator, 1, 0, 1);
	if(!token_bounds){
	  nip_report_error(__FILE__, __LINE__, NIP_ERROR_GENERAL, 1);
	  nip_close_data_file(f);
	  return NULL;
	}
      }
      (f->current_line)++;
      
      /* JJT  1.9.2004: A sort of bug fix. Ignore empty lines */
      /* JJT 22.6.2005: Another fix. Ignore only the empty lines 
       * immediately after the node labels... and duplicate empty lines.
       * Otherwise start a new timeseries */
      if(num_of_tokens == 0){
	empty_lines_read++;
	continue; /* empty lines to be ignored */
      }
      else{
	if(empty_lines_read){
	  linecounter = 1;
	  if(state < 1)
	    tscounter++;
	}
	else
	  linecounter++;
	empty_lines_read = 0;
	if(state > 0)
	  state--; /* stop ignoring single empty lines */
      }
      
      /* Read node names or make them up. */
      if(state){ 
	/* reading the first non-empty line and expecting node names */
	f->num_of_nodes = num_of_tokens;
	f->node_symbols = (char **) calloc(num_of_tokens, sizeof(char *));

	if(!f->node_symbols){
	  nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
	  nip_close_data_file(f);
	  free(token_bounds);
	  return NULL;
	}

	statenames = 
	  (nip_string_list *) calloc(num_of_tokens, sizeof(nip_string_list));

	if(!statenames){
	  nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
	  nip_close_data_file(f);
	  free(token_bounds);
	  return NULL;
	}

	for(i = 0; i < f->num_of_nodes; i++)
	  statenames[i] = nip_new_string_list();

	f->num_of_states = (int *) calloc(num_of_tokens, sizeof(int));

	if(!f->num_of_states){
	  nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
	  nip_close_data_file(f);
	  /* free statenames[i]? */
	  free(statenames);
	  free(token_bounds);
	  return NULL;
	}

	f->node_states = (char ***) calloc(num_of_tokens, sizeof(char **));

	if(!f->node_states){
	  nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
	  nip_close_data_file(f);
	  /* free statenames[i]? */
	  free(statenames);
	  free(token_bounds);
	  return NULL;
	}

	if(nodenames){
	  f->first_line_labels = 1;

	  for(i = 0; i < num_of_tokens; i++){

	    f->node_symbols[i] =
	      (char *) calloc(token_bounds[2*i+1] - token_bounds[2*i] + 1,
			      sizeof(char));
	    if(!f->node_symbols[i]){
	      nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
	      nip_close_data_file(f);
	      /* free statenames[i]? */
	      free(statenames);
	      free(token_bounds);
	      return NULL;
	    }

	    strncpy(f->node_symbols[i], &(last_line[token_bounds[2*i]]),
		    token_bounds[2*i+1] - token_bounds[2*i]);
	    f->node_symbols[i][token_bounds[2*i+1] - token_bounds[2*i]] = '\0';
	  }

	}
	else{
	  f->first_line_labels = 0;

	  for(i = 0; i < num_of_tokens; i++){

	    /* determine length of the name */
	    dividend = i + 1;
	    length_of_name = 5;
	    while((dividend /= 10) > 1)
	      length_of_name++;

	    f->node_symbols[i] = (char *) calloc(length_of_name, sizeof(char));
	    if(!f->node_symbols[i]){
	      nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
	      nip_close_data_file(f);
	      /* free statenames[i]? */
	      free(statenames);
	      free(token_bounds);
	      return NULL; /* error horror */
	    }

	    sprintf(f->node_symbols[i], "node%d", i + 1);
	  }
	}
      }
      else{
	/* Read observations (just in order to see all the different
	   kinds of observations for each node). */

	f->datarows[tscounter]++;
	
	/* j == min(f->num_of_nodes, num_of_tokens) */
	if(f->num_of_nodes < num_of_tokens)
	  j = f->num_of_nodes;
	else
	  j = num_of_tokens;

	for(i = 0; i < j; i++){
	  token = (char *) calloc(token_bounds[2*i+1] - token_bounds[2*i] + 1,
				  sizeof(char));
	  if(!token){
	    nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
	    nip_close_data_file(f);
	    /* free statenames[i]? */
	    free(statenames);
	    free(token_bounds);
	    return NULL;
	  }

	  strncpy(token, &(last_line[token_bounds[2*i]]),
		  token_bounds[2*i+1] - token_bounds[2*i]);

	  token[token_bounds[2*i+1] - token_bounds[2*i]] = '\0';

	  /* If the string has not yet been observed, add it to a list 
	   * (ownership is passed on, string is not freed here) */
	  if(!nip_null_observation(token) && 
	     !nip_string_list_contains(statenames[i], token)){
	    if(nip_prepend_string(statenames[i], token) != NIP_NO_ERROR){
	      nip_report_error(__FILE__, __LINE__, NIP_ERROR_GENERAL, 1);
	      nip_close_data_file(f);
	      for(i = 0; i < f->num_of_nodes; i++)
		nip_free_string_list(statenames[i]);
	      free(statenames);
	      free(token_bounds);
	      free(token);
	      return NULL;
	    }
	  }
	}
      }
      free(token_bounds);
    }

    /* Count number of states in each variable. */
    for(i = 0; i < f->num_of_nodes; i++){
      f->num_of_states[i] = NIP_LIST_LENGTH(statenames[i]);
    }

    for(i = 0; i < f->num_of_nodes; i++){
      f->node_states[i] = nip_string_list_to_array(statenames[i]);
    }
  }

  /* JJT: Added 13.8.2004 because of possible memory leaks. 
   * JJT: Updated 5.1.2007 with the new stringlist implementation */
  for(i = 0; i < f->num_of_nodes; i++){
    nip_empty_string_list(statenames[i]);
  }
  free(statenames);

  rewind(f->file);
  f->current_line = 0;

  return f;
}


/*
 * Tells if the given token indicates a missing value, a "null observation".
 * The token must be null terminated.
 */
static int nip_null_observation(char *token){

#ifdef DEBUG_DATAFILE
  printf("nip_null_observation called\n");
#endif

  if(token == NULL){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_NULLPOINTER, 1);
    return 0;
  }

  /* compare to all known labels for missing data! */
  else if((strcmp("N/A", token) == 0) ||
	  (strcmp("null", token) == 0) ||
	  (strcmp("<null>", token) == 0)){
    return 1;
  }
  else{

    return 0;
  }
}


void nip_close_data_file(nip_data_file file){
  if(!file){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_NULLPOINTER, 1);
    return;
  }
  if(file->is_open){
    fclose(file->file);
    file->is_open = 0;
  }
  /* Release the memory of file struct. */
  nip_free_data_file(file);
}


/* Frees the memory used by (possibly partially allocated) datafile f. */
static void nip_free_data_file(nip_data_file f){
  int i,j;

  if(!f)
    return;
  free(f->name);
  if(f->node_symbols){
    for(j = 0; j < f->num_of_nodes; j++)
      free(f->node_symbols[j]);
    free(f->node_symbols);
  }
  if(f->node_states){
    for(i = 0; i < f->num_of_nodes; i++){
      for(j = 0; j < f->num_of_states[i]; j++)
	free(f->node_states[i][j]);
      free(f->node_states[i]);
    }
    free(f->node_states);
  }
  free(f->num_of_states);
  free(f->datarows);
  free(f);
}


int nip_next_line_tokens(nip_data_file f, char separator, char ***tokens){

  char line[MAX_LINELENGTH];
  char *token;
  int num_of_tokens;
  int *token_bounds;
  int i, j;

  if(!f || !tokens){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_NULLPOINTER, 1);
    return -1;
  }

  if(!(f->is_open)){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_GENERAL, 1);
    return -1;
  }

  /* seek the first line of data (starting from the current line) */
  num_of_tokens = 0;
  do{
    if(fgets(line, MAX_LINELENGTH, f->file) == NULL)
      break;
    else
      f->current_line++;
    
    /* treat the white space as separators */
    num_of_tokens = nip_count_tokens(line, NULL, 0, &separator, 1, 0, 1);
    
    /* Skip the first line if it contains node labels. */
    if((f->current_line == f->label_line)  &&  f->first_line_labels)
      num_of_tokens = 0;

  }while(num_of_tokens < 1);

  if(num_of_tokens == 0)
    return 0;

  /* treat the white space as separators */
  token_bounds = nip_tokenise(line, num_of_tokens, 0, &separator, 1, 0, 1);
  
  if(!token_bounds){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_GENERAL, 1);
    return -1;
  }

  *tokens = (char **)
    calloc(f->num_of_nodes<num_of_tokens?f->num_of_nodes:num_of_tokens,
	   sizeof(char *));

  if(!tokens){
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
    free(token_bounds);
    return -1;
  }


  for(i = 0;
      i < (f->num_of_nodes<num_of_tokens?f->num_of_nodes:num_of_tokens);
      i++){

    token = (char *) calloc(token_bounds[2*i+1] - token_bounds[2*i] + 1,
			    sizeof(char));

    if(!token){
      nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
      nip_free_data_file(f);
      for(j = 0; j < i; j++)
	free(*tokens[j]);
      free(token_bounds);
      return -1;
    }

    strncpy(token, &(line[token_bounds[2*i]]),
	    token_bounds[2*i+1] - token_bounds[2*i]);

    token[token_bounds[2*i+1] - token_bounds[2*i]] = '\0';

    /* Children, remember what papa said: always use parentheses... */
    (*tokens)[i] = token;
  }

  free(token_bounds);

  /* Return the number of acquired tokens. */
  return f->num_of_nodes<num_of_tokens?f->num_of_nodes:num_of_tokens;
}


char* nip_next_hugin_token(int* token_length, FILE* f){

  /* The last line read from the file */
  static char last_line[MAX_LINELENGTH];

  /* How many tokens left in the current line of input? */
  static int tokens_left;

  /* Pointer to the index array of token boundaries */
  static int* indexarray = NULL;

  /* Pointer to the index array of token boundaries
   * (not incremented, we need this for free() ) */
  static int* indexarray_original = NULL;

  /* Should a new line be read in when the next token is requested? */
  static int nip_read_line = 1; 
  /* FIXME: this should be a part of the given file struct? */

  /* The token we return */
  char* token;

  /* Return if some evil mastermind gives us a NULL pointer */
  if(!token_length){
    if(indexarray_original){
      free(indexarray_original);
      indexarray = NULL;
      indexarray_original = NULL;
    }

    return NULL;
  }

  /* Return if input file is not open */
  if(f == NULL){
    if(indexarray_original){
      free(indexarray_original);
      indexarray = NULL;
      indexarray_original = NULL;
    }

    return NULL;
  }

  /* Read new line if needed and do other magic... */
  while(nip_read_line){
    /* Read the line and check for EOF */
    if(!(fgets(last_line, MAX_LINELENGTH, f))){

      if(indexarray_original){
	free(indexarray_original);
	indexarray = NULL;
	indexarray_original = NULL;
      }

      *token_length = 0;
      return NULL;
    }
    /* how many tokens in the line? */
    tokens_left = nip_count_tokens(last_line, NULL, 1, "(){}=,;", 7, 1, 1);

    /* Check whether the line has any tokens. If not, read a new line
     * (go to beginning of loop) */
    if(tokens_left > 0){
      /* Adjust pointer to the beginning of token boundary index array */
      if(indexarray_original){
	free(indexarray_original);
	indexarray = NULL;
	indexarray_original = NULL;
      }

      indexarray = nip_tokenise(last_line, tokens_left, 1, "(){}=,;", 7, 1, 1);
      indexarray_original = indexarray;

      /* Check if tokenise failed. If it failed, we have no other option
       * than to stop: return NULL, *token_length = 0.
       */
      if(!indexarray){
	nip_report_error(__FILE__, __LINE__, NIP_ERROR_GENERAL, 1);
	*token_length = 0;

	return NULL;
      }

      /* Ignore lines that have COMMENT_CHAR as first non-whitespace char */
      if(last_line[indexarray[0]] == NIP_COMMENT_CHAR)
	nip_read_line = 1;
      else
	nip_read_line = 0;
    }
  }

  *token_length = indexarray[1] - indexarray[0];
  token = (char *) calloc(*token_length + 1, sizeof(char));
  if(!token){
    if(indexarray_original){
      free(indexarray_original);
      indexarray = NULL;
      indexarray_original = NULL;
    }
    nip_report_error(__FILE__, __LINE__, NIP_ERROR_OUTOFMEMORY, 1);
    *token_length = -1;
    return NULL;
  }

  /* Copy the token */
  strncpy(token, &(last_line[indexarray[0]]), *token_length);

  /* NULL terminate the token. */
  token[*token_length] = '\0';

  indexarray += 2;
  
  /* If all the tokens have been handled, read a new line next time */
  if(--tokens_left == 0)
    nip_read_line = 1;

  /* Still some tokens left. Check for COMMENT_CHAR. */
  else if(last_line[indexarray[0]] == NIP_COMMENT_CHAR)
    nip_read_line = 1;

#ifdef PRINT_TOKENS
  printf("%s\n", token);
#endif

  return token;
}
