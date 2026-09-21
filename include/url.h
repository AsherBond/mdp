#if !defined( URL_H )
#define URL_H

/*
 * An object to store all urls of a slide.
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
 * function: url_init to initialize a new url object
 * function: url_add to store a link's name, target and screen position,
 *           returning its index
 * function: url_get_target to look up a stored link's target by index
 * function: url_get_amount to get the number of stored links
 * function: url_purge to free all stored links and reset the url object
 * function: url_count_inline to count pandoc-style [label](url) links in a
 *           line
 * function: url_len_inline to sum the rendered length of a link's target(s)
 *           hidden inside a line, for width calculations
 * function: url_find_closing_bracket to find the ']' matching a link's '['
 * function: url_find_closing_parentheses to find the ')' matching a link's
 *           target '('
 */

typedef struct _url_t {
    wchar_t *link_name;
    wchar_t *target;
    int x;
    int y;
    struct _url_t *next;
} url_t;

void url_init(void);
int url_add(const wchar_t *link_name, int link_name_length, const wchar_t *target, int target_length, int x, int y);
wchar_t* url_get_target(int index);
int url_get_amount(void);
void url_purge(void);
int url_count_inline(const wchar_t *line);
int url_len_inline(const wchar_t *value);
wchar_t* url_find_closing_bracket(const wchar_t *start);
wchar_t *url_find_closing_parentheses(const wchar_t *start);

#endif // !defined( URL_H )
