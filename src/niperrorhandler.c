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

/* Unified error handler stuff! */
/*http://www.opengroup.org/onlinepubs/007904975/functions/xsh_chap02_03.html*/
/* #include <errno.h> ??? */

#include "niperrorhandler.h"

/* A variable for counting errors */
static int NIP_ERROR_COUNTER = 0;

/* Errorcode of the last error */
static nip_error_code NIP_ERROR_CODE = NIP_NO_ERROR; 

void nip_report_error(char *srcFile, int line, nip_error_code e, int verbose){
  NIP_ERROR_CODE = e;
  NIP_ERROR_COUNTER++;
  if(verbose){
    fprintf(stderr, "In %s (%d): ", srcFile, line);
    switch (e) {
    case NIP_NO_ERROR : 
      fprintf(stderr, "O.K.\n"); break;
    case NIP_ERROR_NULLPOINTER : 
      fprintf(stderr, "Nullpointer given.\n"); break;
    case NIP_ERROR_DIVBYZERO :
      fprintf(stderr, "Division by zero.\n"); break;
    case NIP_ERROR_INVALID_ARGUMENT :
      fprintf(stderr, "Invalid argument given.\n"); break;
    case NIP_ERROR_OUTOFMEMORY :
      fprintf(stderr, "Malloc or calloc failed.\n"); break;
    case NIP_ERROR_IO :
      fprintf(stderr, "I/O failure.\n"); break;
    case NIP_ERROR_GENERAL :
      fprintf(stderr, "An error encountered.\n"); break;
    case NIP_ERROR_FILENOTFOUND :
      fprintf(stderr, "Requested file not found.\n"); break;
    case NIP_ERROR_BAD_LUCK :
      fprintf(stderr, "Random model was born dead.\n"); break;
    default : fprintf(stderr, "Something went wrong.\n");
    }
  }
}

void nip_reset_error_handler(){
  NIP_ERROR_CODE = NIP_NO_ERROR;
  NIP_ERROR_COUNTER = 0;
}

nip_error_code nip_check_error_type(){
  return NIP_ERROR_CODE;
}

int nip_check_error_counter(){
  return NIP_ERROR_COUNTER;
}
