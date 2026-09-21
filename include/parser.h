#if !defined( PARSER_H )
#define PARSER_H

/*
 * Functions necessary to parse a file and transform its content into
 * a deck of slides containing lines. All based on markdown formating
 * rules.
 * Copyright (C) 2026 Michael Goehler
 *
 * This file is part of mdp.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * function: markdown_load is the main function which reads a file handle,
 *           and initializes deck, slides and lines
 * function: markdown_debug to print a report of the generated data structure
 * function: next_nonblank, prev_blank, next_blank, next_word to calculate
 *           string offset's
 *
 */

#include "common.h"
#include "markdown.h"
#include "cstack.h"

#if defined( CYGWIN )
#undef WEOF
#define WEOF (0xffff)
#endif // defined( CYGWIN )

#define EXPAND_TABS 4
#define CODE_INDENT 4
#define UNORDERED_LIST_MAX_LEVEL 3

deck_t *markdown_load(FILE *input, int noexpand);
void markdown_debug(deck_t *deck, int debug);
int next_nonblank(cstring_t *text, int i);
int prev_blank(cstring_t *text, int i);
int next_blank(cstring_t *text, int i);
int next_word(cstring_t *text, int i);

#endif // !defined( PARSER_H )
