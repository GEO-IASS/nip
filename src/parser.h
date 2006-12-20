/*
 * Definitions for the bison parser and for other parsers.
 *
 * JJ Comment: Currently the parser is ugly as hell...
 *             Get rid of global variables and ad-hoc data structures!
 *             (add_X(), get_X(), and set_X() are probably the worst)
 *
 * $Id: parser.h,v 1.49 2006-12-20 15:57:29 jatoivol Exp $
 */

#ifndef __PARSER_H__
#define __PARSER_H__

#define MAX_LINELENGTH 10000

/* Comment character in input files.
 * After this, the rest of the line is ignored.
 */
#define COMMENT_CHAR '%'

#include "clique.h"
#include "lists.h"
#include "variable.h"
#include "potential.h"
#include "Graph.h"
#include <stdio.h>


typedef struct {
  char *name;
  char separator;
  FILE *file;
  int is_open;
  int firstline_labels; /* Does the first line contain node labels? */
  int line_now; /* Current position in file 
		 * ([1...N] after nextline_tokens) */
  int label_line;

  /* Number of continuous time series (no empty lines) */
  int ndatarows;
  /* Number of rows in the file for each time series, 
   * excluding the line containing node labels */
  int *datarows;

  char **node_symbols;
  int num_of_nodes;

  char ***node_states; /* 1st index = node, 2nd index = state */
  int *num_of_states;
} datafile;


/* Opens an input file. Returns 0 if file was opened or if some file was
 * already open. Returns ERROR_GENERAL if an error occurred
 * opening the file.
 */
int open_yyparse_infile(const char *filename);


/* Closes the current input file (if there is one).
 */
void close_yyparse_infile();


/*
 * Opens a file by creating a datafile struct. The struct can be used 
 * for reading or writing after opening.
 * The function sets the file-position indicator to the beginning of the file.
 * Parameters:
 * - filename : the name of the file to be opened
 * - separator : the separator character between fields
 * - write : 0 if the file is opened for reading only, 
 *           e.g. 1 if the file is opened for writing only.
 * - nodenames : 1 if the first row contains names of nodes
 */
datafile *open_datafile(char *filename, char separator,
			int write, int nodenames);


/*
 * Closes a file described by the datafile struct.
 * Also releases the memory of the datafile struct.
 */
void close_datafile(datafile *file);


/*
 * Gets the tokens (separated by separator) on the next line of
 * the given file. Allocates memory for an array of strings and its
 * contents. Skips the first line of the file, if it contains node labels.
 * Returns the number of tokens found.
 * Negative number indicates error or end of file.
 */
int nextline_tokens(datafile *f, char separator, char ***tokens);


/* Gets the next token from the hugin .net file.
 * Returns the token. token_length is the length of the token.
 * The token is a NUL terminated string.
 * *token_length doesn't include the NULL character.
 * After the token has been used, PLEASE free the memory allocated for it.
 * If token_length == 0, there are no more tokens.
 */
char *next_token(int *token_length);


/* Saves two positive integers by truncating and taking absolute value. */
void set_parser_node_position(double x, double y);

/* Saves two positive integers by truncating and taking absolute value. */
void set_parser_node_size(double x, double y);

/* Sets variable position (node position by Hugin) according to 
 * the two position values saved by set_parser_node_position */
void set_variable_position(variable v);

/* Sets two values according to the node size values parsed last. */
void get_parser_node_size(int* x, int* y);


/* Adds a potential and the correspondent variable references into a list.
 * The "ownership" of the vars[] array changes! */
int add_initData(potential p, variable child, variable* parents);


/* Adds information about the time relations between "time slices".
 */
int add_time_init(variable var, char* name);


/* Frees some memory after parsing. */
void reset_initData();


/* Frees some memory after information is no longer needed. */
void reset_timeinit();


/* Inserts the parsed variables and their relations into the graph.
 * Returns an error code. (0 is O.K.) */
int parsedVars2Graph(variablelist vl, Graph* g);


/* Adds time relations to variables.
 */
int time2Vars(variablelist vl);


/* Constructs the join tree from the graph (which is hopefully ready)
 * Returns an error code. (0 is O.K.) */
int Graph2JTree();


/* Initialises the join tree (clique array) with parsed potentials. 
 * NOTE: the priors of independent variables are not entered into the 
 * join tree (as evidence), but are stored into the variable though.
 * Returns an error code. (0 is O.K.) */
int parsedPots2JTree();


/* Some debug printing about what was parsed. */
void print_parsed_stuff();


int get_num_of_cliques();

clique **get_cliques_pointer();

void reset_clique_array();

#endif /* __PARSER_H__ */
