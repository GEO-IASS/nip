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

/* nipstring.h 
 * Authors: Janne Toivola, Mikko Korpela
 * Version: $Id: nipstring.h,v 1.3 2010-11-26 17:06:02 jatoivol Exp $
 */

#ifndef __NIPSTRING_H__
#define __NIPSTRING_H__

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "niperrorhandler.h"

/*
 * Counts the number of words in string s. Words are separated by white space.
 * String s must be null terminated. If chars is not a null pointer,
 * the number of characters in the string is placed there.
 */
int nip_count_words(const char *s, int *chars);

/*
 * Counts the number of tokens in a null terminated string.
 * Parameters:
 * - s:
 *     the input string
 * - *chars:
 *     the number of characters in s is stored here
 * - q_strings:
 *     if != 0, strings enclosed in "" are considered one token
 * - separators:
 *     Each character in this string acts as a separator between tokens.
 *     White space is a separator, too.
 * - n_separators:
 *     the number of characters in *separators
 * - sep_tokens:
 *     if != 0, separators are themselves considered to be tokens
 * - wspace_sep:
 *     if != 0, whitespace is a separator of tokens,
 *     but is not considered a token itself
 */
int nip_count_tokens(const char *s, int *chars, int q_strings,
		     char *separators, int n_separators, int sep_tokens,
		     int wspace_sep);

/*
 * Extracts n first words or tokens of string s. Returns an array of indices
 * (length 2*n) in ascending order. The indices indicate the location of
 * the first and (last+1) characters of each word.
 * The string must be null terminated.
 * Returns NULL if the string contains less than n words.
 * See the parameters of count_tokens.
 */
int* nip_tokenise(const char s[], int n, int q_strings,
		  char *separators, int n_separators, int sep_tokens,
		  int wspace_sep);

/*
 * Splits string s into n null terminated tokens.
 * - indices: an index array (see tokenise(...))
 */
char** nip_split(const char s[], int indices[], int n);

#endif /* __NIPSTRING_H__ */
