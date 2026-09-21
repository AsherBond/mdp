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
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>
#include <string.h>

#include "parser.h"

// forward declarations for helpers only used within this file
static int markdown_analyse(cstring_t *text, int prev);
static void adjust_line_length(line_t *line);
static void expand_character_entities(line_t *line);
static int next_nontilde(cstring_t *text, int i);
static int next_nonbacktick(cstring_t *text, int i);

// char entry translation table
static struct named_character_entity {
    wchar_t        ucs;
    const wchar_t *name;
} named_character_entities[] = {
   { L'\x0022', L"quot" },
   { L'\x0026', L"amp" },
   { L'\x0027', L"apos" },
   { L'\x003C', L"lt" },
   { L'\x003E', L"gt" },
   { L'\x00A2', L"cent" },
   { L'\x00A3', L"pound" },
   { L'\x00A5', L"yen" },
   { L'\x00A7', L"sect" },
   { L'\x00A9', L"copy" },
   { L'\x00AA', L"laquo" },
   { L'\x00AE', L"reg" },
   { L'\x00B0', L"deg" },
   { L'\x00B1', L"plusmn" },
   { L'\x00B2', L"sup2" },
   { L'\x00B3', L"sup3" },
   { L'\x00B6', L"para" },
   { L'\x00B9', L"sup1" },
   { L'\x00BB', L"raquo" },
   { L'\x00BC', L"frac14" },
   { L'\x00BD', L"frac12" },
   { L'\x00BE', L"frac34" },
   { L'\x00D7', L"times" },
   { L'\x00F7', L"divide" },
   { L'\x2018', L"lsquo" },
   { L'\x2019', L"rsquo" },
   { L'\x201C', L"ldquo" },
   { L'\x201D', L"rdquo" },
   { L'\x2020', L"dagger" },
   { L'\x2021', L"Dagger" },
   { L'\x2022', L"bull" },
   { L'\x2026', L"hellip" },
   { L'\x2030', L"permil" },
   { L'\x2032', L"prime" },
   { L'\x2033', L"Prime" },
   { L'\x2039', L"lsaquo" },
   { L'\x203A', L"rsaquo" },
   { L'\x20AC', L"euro" },
   { L'\x2122', L"trade" },
   { L'\x2190', L"larr" },
   { L'\x2191', L"uarr" },
   { L'\x2192', L"rarr" },
   { L'\x2193', L"darr" },
   { L'\x2194', L"harr" },
   { L'\x21B5', L"crarr" },
   { L'\x21D0', L"lArr" },
   { L'\x21D1', L"uArr" },
   { L'\x21D2', L"rArr" },
   { L'\x21D3', L"dArr" },
   { L'\x21D4', L"hArr" },
   { L'\x221E', L"infin" },
   { L'\x2261', L"equiv" },
   { L'\x2308', L"lceil" },
   { L'\x2309', L"rceil" },
   { L'\x230A', L"lfloor" },
   { L'\x230B', L"rfloor" },
   { L'\x25CA', L"loz" },
   { L'\x2660', L"spades" },
   { L'\x2663', L"clubs" },
   { L'\x2665', L"hearts" },
   { L'\x2666', L"diams" },
   { L'\0', NULL },
};

static void remove_line_from_slide(slide_t *slide, line_t *line) {
   if(!slide || !line)
       return;

   if(line->prev)
       line->prev->next = line->next;
   else
       slide->line = line->next;

   if(line->next)
       line->next->prev = line->prev;

   slide->lines -= 1;
   line->prev = line->next = NULL;

   if(line->text)
       (line->text->delete)(line->text);
   free(line);
}

// strip HTML comment delimiters ("<!--" and "-->") from a line while
// keeping the enclosed text intact - used while scanning top-of-file
// metadata lines, where the text between the markers is the actual
// value to capture (e.g. "<!-- title: Demo -->" -> "title: Demo")
static cstring_t *strip_comment_markers(cstring_t *text) {
   cstring_t *result = cstring_init();
   int i = 0;

   if(!text || !text->value) {
       return result;
   }

   while(i < (int)text->size) {
       if(wcsncmp(&text->value[i], L"<!--", 4) == 0) {
           i += 4;
           continue;
       }

       if(wcsncmp(&text->value[i], L"-->", 3) == 0) {
           i += 3;
           continue;
       }

       (result->expand)(result, text->value[i]);
       i++;
   }

   return result;
}

// match title/author/date metadata with or without the legacy % prefix
static int matches_metadata_keyword(const wchar_t *text, const wchar_t *keyword) {
   size_t keyword_len = wcslen(keyword);
   int i = 0;

   while(text[i] != L'\0' && iswspace(text[i])) {
       i++;
   }

   if(text[i] == L'%') {
       i++;
   }

   if(wcsncmp(&text[i], keyword, keyword_len) != 0) {
       return 0;
   }

   i += keyword_len;
   while(text[i] != L'\0' && iswspace(text[i])) {
       i++;
   }

   return text[i] == L':';
}

// normalize whitespace so top-of-file metadata lines are easy to match
static cstring_t *trim_whitespace(cstring_t *text) {
   cstring_t *result = cstring_init();
   int start = 0;
   int end = 0;

   if(!text || !text->value) {
       return result;
   }

   while(start < (int)text->size && iswspace(text->value[start])) {
       start++;
   }

   end = (int)text->size;
   while(end > start && iswspace(text->value[end - 1])) {
       end--;
   }

   for(; start < end; start++) {
       (result->expand)(result, text->value[start]);
   }

   return result;
}

// read the metadata value after the key, or treat the whole line as the value when implicit
static cstring_t *metadata_value_for(cstring_t *text, const wchar_t *keyword, int allow_implicit_value) {
   wchar_t *cursor = NULL;
   cstring_t *value = cstring_init();
   int i = 0;

   if(!text || !text->value) {
       return value;
   }

   while(text->value[i] != L'\0' && iswspace(text->value[i])) {
       i++;
   }

   if(text->value[i] == L'%') {
       i++;
   }

   if(wcsncmp(&text->value[i], keyword, wcslen(keyword)) == 0) {
       i += (int)wcslen(keyword);
       while(text->value[i] != L'\0' && iswspace(text->value[i])) {
           i++;
       }
       if(text->value[i] == L':') {
           i++;
           while(text->value[i] != L'\0' && iswspace(text->value[i])) {
               i++;
           }
           cursor = &text->value[i];
           while(*cursor != L'\0') {
               (value->expand)(value, *cursor);
               cursor++;
           }
           return value;
       }
   }

   if(allow_implicit_value) {
       cursor = &text->value[i];
       while(*cursor != L'\0') {
           (value->expand)(value, *cursor);
           cursor++;
       }
       return value;
   }

   return value;
}

// reformat captured metadata as title: footer: footer: entries
static cstring_t *normalize_metadata_line(cstring_t *text, int field_index) {
   const wchar_t *keys[] = { L"title", L"footer", L"footer", NULL };
   const wchar_t *key = keys[field_index % 3];
   cstring_t *value = NULL;
   cstring_t *result = cstring_init();
   int i = 0;

   if(!text || !text->value) {
       return result;
   }

   for(i = 0; keys[i] != NULL; i++) {
       if(matches_metadata_keyword(text->value, keys[i])) {
           key = keys[i];
           value = metadata_value_for(text, keys[i], 0);
           break;
       }
   }

   if(!value || value->size == 0) {
       if(matches_metadata_keyword(text->value, L"footer")) {
           key = keys[(field_index == 0) ? 1 : (field_index % 3)];
           value = metadata_value_for(text, L"footer", 0);
       } else if(matches_metadata_keyword(text->value, L"author")) {
           key = keys[1];
           value = metadata_value_for(text, L"author", 0);
       } else if(matches_metadata_keyword(text->value, L"date")) {
           key = keys[2];
           value = metadata_value_for(text, L"date", 0);
       }
   }

   if(!value || value->size == 0) {
       value = metadata_value_for(text, key, 1);
   }

   if(value->size == 0) {
       (value->delete)(value);
       return result;
   }

   for(i = 0; key[i] != L'\0'; i++) {
       (result->expand)(result, key[i]);
   }
   (result->expand)(result, L':');
   (result->expand)(result, L' ');
   for(i = 0; value->value[i] != L'\0'; i++) {
       (result->expand)(result, value->value[i]);
   }

   (value->delete)(value);
   return result;
}

// remove comment markers and hidden fragments from slide content, tracking
// comment state across multiple lines so a block like "<!--\nhidden\n-->"
// is fully discarded, not just the marker lines themselves
static void strip_comment_markup_from_slide(slide_t *slide) {
   line_t *line = slide ? slide->line : NULL;
   int in_comment_block = 0;

   while(line) {
       line_t *next = line->next;
       cstring_t *filtered = cstring_init();
       int i = 0;

       if(!line->text || !line->text->value) {
           line = next;
           continue;
       }

       while(i < (int)line->text->size) {
           if(in_comment_block) {
               if(wcsncmp(&line->text->value[i], L"-->", 3) == 0) {
                   i += 3;
                   in_comment_block = 0;
                   continue;
               }
               i++;
               continue;
           }

           if(wcsncmp(&line->text->value[i], L"<!--", 4) == 0) {
               // drop a trailing space left behind by the removed comment
               if(filtered->size > 0 && iswspace(filtered->value[filtered->size - 1])) {
                   filtered->strip(filtered, filtered->size - 1, 1);
               }
               in_comment_block = 1;
               i += 4;
               continue;
           }

           (filtered->expand)(filtered, line->text->value[i]);
           i++;
       }

       if(filtered->size == 0) {
           (filtered->delete)(filtered);
           remove_line_from_slide(slide, line);
       } else {
           (line->text->delete)(line->text);
           line->text = filtered;
           line->offset = next_nonblank(line->text, 0);
           if(line->text->value)
               adjust_line_length(line);
       }

       line = next;
   }
}

// collect the leading metadata comments and pull them out of the first slide
static void capture_top_metadata_comments(deck_t *deck) {
   line_t *line = deck && deck->slide ? deck->slide->line : NULL;
   line_t *header = NULL;
   line_t *header_tail = NULL;
   int hc = 0;
   int field_index = 0;
   int in_comment_block = 0;
   int started = 0;

   while(line) {
       line_t *next = line->next;
       cstring_t *candidate = NULL;
       cstring_t *trimmed = NULL;
       cstring_t *header_text = NULL;
       cstring_t *raw_trimmed = NULL;
       int opens_comment_block = 0;
       int closes_comment_block = 0;

       // once 3 fields are collected, only keep draining lines while a
       // comment block is still open (to remove its trailing "-->"),
       // otherwise stop and leave the rest of the slide untouched
       if(field_index >= 3 && !in_comment_block) {
           break;
       }

       if(!line->text || !line->text->value || line->text->size == 0) {
           if(!started) {
               break;
           }
           remove_line_from_slide(deck->slide, line);
           line = next;
           continue;
       }

       // only a line that *starts* with the comment marker opens a
       // metadata block; a line with other text before "<!--" (e.g.
       // "abc <!-- def -->") is regular slide content, not metadata
       raw_trimmed = trim_whitespace(line->text);
       opens_comment_block = raw_trimmed->size >= 4 &&
           wcsncmp(raw_trimmed->value, L"<!--", 4) == 0;
       (raw_trimmed->delete)(raw_trimmed);
       closes_comment_block = wcsstr(line->text->value, L"-->") != NULL;

       if(opens_comment_block) {
           in_comment_block = 1;
           started = 1;
       }

       // draining trailing lines of an already-closed metadata block
       if(field_index >= 3) {
           remove_line_from_slide(deck->slide, line);
           line = next;
           if(closes_comment_block) {
               in_comment_block = 0;
           }
           continue;
       }

       candidate = strip_comment_markers(line->text);
       trimmed = trim_whitespace(candidate);
       (candidate->delete)(candidate);

       if(trimmed->size == 0) {
           if(!started) {
               (trimmed->delete)(trimmed);
               break;
           }
           (trimmed->delete)(trimmed);
           remove_line_from_slide(deck->slide, line);
           line = next;
           if(closes_comment_block) {
               in_comment_block = 0;
           }
           continue;
       }

       if(in_comment_block ||
          opens_comment_block ||
          matches_metadata_keyword(trimmed->value, L"title") ||
          matches_metadata_keyword(trimmed->value, L"footer") ||
          matches_metadata_keyword(trimmed->value, L"author") ||
          matches_metadata_keyword(trimmed->value, L"date")) {
           started = 1;
           header_text = normalize_metadata_line(trimmed, field_index);
           if(header_text->size == 0) {
               (trimmed->delete)(trimmed);
               remove_line_from_slide(deck->slide, line);
               line = next;
               if(closes_comment_block) {
                   in_comment_block = 0;
               }
               continue;
           }

           line_t *header_line = new_line();
           header_line->text = header_text;
           header_line->offset = next_nonblank(header_text, 0);
           if(!header) {
               header = header_line;
           } else {
               header_tail->next = header_line;
               header_line->prev = header_tail;
           }
           header_tail = header_line;
           hc++;
           field_index++;
           remove_line_from_slide(deck->slide, line);
           line = next;
           (trimmed->delete)(trimmed);
           if(closes_comment_block) {
               in_comment_block = 0;
           }
           continue;
       }

       if(closes_comment_block) {
           in_comment_block = 0;
       }

       (trimmed->delete)(trimmed);
       break;
   }

   if(header) {
       deck->header = header;
       deck->headers = hc;
   }
}

deck_t *markdown_load(FILE *input, int noexpand) {

    wchar_t c = L'\0';    // char
    int i = 0;    // increment
    int hc = 0;   // header count
    int lc = 0;   // line count
    int sc = 1;   // slide count
    int bits = 0; // markdown bits
    int prev = 0; // markdown bits of previous line

    deck_t *deck = new_deck();
    slide_t *slide = deck->slide;
    line_t *line = NULL;
    line_t *tmp = NULL;
    cstring_t *text = cstring_init();

    // initialize bits as empty line
    SET_BIT(bits, IS_EMPTY);

    for (;;) {
        c = fgetwc(input);
        if (ferror(input)) {
            fprintf(stderr, "markdown_load() failed to read input: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        if(c == L'\n' || c == WEOF) {

            // markdown analyse
            prev = bits;
            bits = markdown_analyse(text, prev);

            // if first line in file is markdown hr
            if(!line && CHECK_BIT(bits, IS_HR)) {

                // clear text
                (text->reset)(text);

            } else if(line && CHECK_BIT(bits, IS_STOP)) {

                // set stop bit on last line
                SET_BIT(line->bits, IS_STOP);

                // clear text
                (text->reset)(text);

            // if text is markdown hr
            } else if(CHECK_BIT(bits, IS_HR) &&
                      CHECK_BIT(line->bits, IS_EMPTY)) {

                slide->lines = lc;

                // clear text
                (text->reset)(text);

                // create next slide
                slide = next_slide(slide);
                sc++;

            } else if((CHECK_BIT(bits, IS_TILDE_CODE) ||
		       CHECK_BIT(bits, IS_GFM_CODE)) &&
                      CHECK_BIT(bits, IS_EMPTY)) {
                // remove tilde code markers
                (text->reset)(text);

            } else {

                // if slide ! has line
                if(!slide->line || !line) {

                    // create new line
                    line = new_line();
                    slide->line = line;
                    lc = 1;

                } else {

                    // create next line
                    line = next_line(line);
                    lc++;

                }

                // add text to line
                line->text = text;

                // add bits to line
                line->bits = bits;

                // calc offset
                line->offset = next_nonblank(text, 0);

                // expand character entities if enabled
                if(line->text->value &&
                   !noexpand &&
                   !CHECK_BIT(line->bits, IS_CODE))
                    expand_character_entities(line);

                // adjust line length dynamicaly - excluding markup
                if(line->text->value)
                    adjust_line_length(line);

                // new text
                text = cstring_init();
            }

        } else if(c == L'\t') {

            // expand tab to spaces
            for (i = 0;  i < EXPAND_TABS;  i++) {
                (text->expand)(text, L' ');
            }

        } else if(c == L'\\') {

            // add char to line
            (text->expand)(text, c);

            // if !IS_CODE add next char to line
            // and do not increase line count
            if(next_nonblank(text, 0) < CODE_INDENT) {

                c = fgetwc(input);
                (text->expand)(text, c);
            }

        } else if(iswprint(c) || iswspace(c)) {

            // add char to line
            (text->expand)(text, c);
        }

        if (c == WEOF) {
            break;
        }
    }
    (text->delete)(text);

    slide->lines = lc;
    deck->slides = sc;

    capture_top_metadata_comments(deck);

    slide = deck->slide;
    while(slide) {
        strip_comment_markup_from_slide(slide);
        slide = slide->next;
    }

    // detect legacy header lines
    line = deck->slide ? deck->slide->line : NULL;
    if(!deck->header &&
       line && line->text && line->text->size > 0 &&
       line->text->value[next_nonblank(line->text, 0)] == L'%') {

        // assign header to deck
        deck->header = line;

        // find first non-header line
        while(line && line->text && line->text->size > 0 &&
              line->text->value[next_nonblank(line->text, 0)] == L'%') {
            hc++;
            line = line->next;
        }

        // only split header if any non-header line is found
        if(line) {

            // split linked list
            line->prev->next = NULL;
            line->prev = NULL;

            // remove header lines from slide
            deck->slide->line = line;

            // adjust counts
            deck->headers += hc;
            deck->slide->lines -= hc;
        } else {

            // remove header from deck
            deck->header = NULL;
        }
    }

    slide = deck->slide;
    while(slide) {
        line = slide->line;

        // ignore mdpress format attributes
        if(line &&
           slide->lines > 1 &&
           !CHECK_BIT(line->bits, IS_EMPTY) &&
           line->text->value[line->offset] == L'=' &&
           line->text->value[line->offset + 1] == L' ') {

            // remove line from linked list
            slide->line = line->next;
            line->next->prev = NULL;

            // maintain loop condition
            tmp = line;
            line = line->next;

            // adjust line count
            slide->lines -= 1;

            // delete line
            (tmp->text->delete)(tmp->text);
            free(tmp);
        }

        while(line) {
            // combine underlined H1/H2 in single line
            if((CHECK_BIT(line->bits, IS_H1) ||
                CHECK_BIT(line->bits, IS_H2)) &&
               CHECK_BIT(line->bits, IS_EMPTY) &&
               line->prev &&
               !CHECK_BIT(line->prev->bits, IS_EMPTY)) {


                // remove line from linked list
                line->prev->next = line->next;
                if(line->next)
                    line->next->prev = line->prev;

                // set bits on previous line
                if(CHECK_BIT(line->bits, IS_H1)) {
                    SET_BIT(line->prev->bits, IS_H1);
                } else {
                    SET_BIT(line->prev->bits, IS_H2);
                }

                // adjust line count
                slide->lines -= 1;

                // maintain loop condition
                tmp = line;
                line = line->prev;

                // delete line
                (tmp->text->delete)(tmp->text);
                free(tmp);

            // pass enclosing flag IS_UNORDERED_LIST_3
            // to nested levels for unordered lists
            } else if(CHECK_BIT(line->bits, IS_UNORDERED_LIST_3)) {
                tmp = line->next;
                line_t *list_last_level_3 = line;

                while(tmp &&
                      CHECK_BIT(tmp->bits, IS_UNORDERED_LIST_3)) {
                    if(CHECK_BIT(tmp->bits, IS_UNORDERED_LIST_3)) {
                        list_last_level_3 = tmp;
                    }
                    tmp = tmp->next;
                }

                for(tmp = line; tmp != list_last_level_3; tmp = tmp->next) {
                    SET_BIT(tmp->bits, IS_UNORDERED_LIST_3);
                }

            // pass enclosing flag IS_UNORDERED_LIST_2
            // to nested levels for unordered lists
            } else if(CHECK_BIT(line->bits, IS_UNORDERED_LIST_2)) {
                tmp = line->next;
                line_t *list_last_level_2 = line;

                while(tmp &&
                      (CHECK_BIT(tmp->bits, IS_UNORDERED_LIST_2) ||
                       CHECK_BIT(tmp->bits, IS_UNORDERED_LIST_3))) {
                    if(CHECK_BIT(tmp->bits, IS_UNORDERED_LIST_2)) {
                        list_last_level_2 = tmp;
                    }
                    tmp = tmp->next;
                }

                for(tmp = line; tmp != list_last_level_2; tmp = tmp->next) {
                    SET_BIT(tmp->bits, IS_UNORDERED_LIST_2);
                }

            // pass enclosing flag IS_UNORDERED_LIST_1
            // to nested levels for unordered lists
            } else if(CHECK_BIT(line->bits, IS_UNORDERED_LIST_1)) {
                tmp = line->next;
                line_t *list_last_level_1 = line;

                while(tmp &&
                      (CHECK_BIT(tmp->bits, IS_UNORDERED_LIST_1) ||
                       CHECK_BIT(tmp->bits, IS_UNORDERED_LIST_2) ||
                       CHECK_BIT(tmp->bits, IS_UNORDERED_LIST_3))) {
                    if(CHECK_BIT(tmp->bits, IS_UNORDERED_LIST_1)) {
                        list_last_level_1 = tmp;
                    }
                    tmp = tmp->next;
                }

                for(tmp = line; tmp != list_last_level_1; tmp = tmp->next) {
                    SET_BIT(tmp->bits, IS_UNORDERED_LIST_1);
                }
            }

            line = line->next;
        }
        slide = slide->next;
    }

    return deck;
}

static int markdown_analyse(cstring_t *text, int prev) {

    // static variables can not be redeclaired, but changed outside of a declaration
    // the program remembers their value on every function calls
    static int unordered_list_level = 0;
    static int unordered_list_level_offset[] = {-1, -1, -1, -1};
    static int num_tilde_characters = 0;
    static int num_backticks = 0;

    int i = 0;      // increment
    int bits = 0;   // markdown bits
    int offset = 0; // text offset
    int eol    = 0; // end of line

    int equals = 0, hashes = 0,
        stars  = 0, minus  = 0,
        spaces = 0, other  = 0; // special character counts

    const int unordered_list_offset = unordered_list_level_offset[unordered_list_level];

    // return IS_EMPTY on null pointers
    if(!text || !text->value) {
        SET_BIT(bits, IS_EMPTY);

        // continue fenced code blocks across empty lines
        if(num_tilde_characters > 0)
            SET_BIT(bits, IS_CODE);

        return bits;
    }

    // count leading spaces
    offset = next_nonblank(text, 0);

    // IS_TILDE_CODE
    if (wcsncmp(text->value, L"~~~", 3) == 0) {
        int tildes_in_line = next_nontilde(text, 0);
        if (tildes_in_line >= num_tilde_characters) {
            if (num_tilde_characters > 0) {
                num_tilde_characters = 0;
            } else {
                num_tilde_characters = tildes_in_line;
            }
            SET_BIT(bits, IS_EMPTY);
            SET_BIT(bits, IS_TILDE_CODE);
            return bits;
        }
    }

    if (num_tilde_characters > 0) {
        SET_BIT(bits, IS_CODE);
        SET_BIT(bits, IS_TILDE_CODE);
        return bits;
    }

    // IS_GFM_CODE
    if (wcsncmp(text->value, L"```", 3) == 0) {
        int backticks_in_line = next_nonbacktick(text, 0);
        if (backticks_in_line >= num_backticks) {
            if (num_backticks > 0) {
                num_backticks = 0;
            } else {
                num_backticks = backticks_in_line;
            }
            SET_BIT(bits, IS_EMPTY);
            SET_BIT(bits, IS_GFM_CODE);
            return bits;
        }
    }

    if (num_backticks > 0) {
        SET_BIT(bits, IS_CODE);
        SET_BIT(bits, IS_GFM_CODE);
        return bits;
    }

    // IS_STOP
    if((offset < CODE_INDENT || !CHECK_BIT(prev, IS_CODE)) &&
       (!wcsncmp(&text->value[offset], L"<br>", 4) ||
        !wcsncmp(&text->value[offset], L"<BR>", 4) ||
        !wcsncmp(&text->value[offset], L"^", 1))) {
        SET_BIT(bits, IS_STOP);
        return bits;
    }

    // strip trailing spaces
    for(eol = text->size; eol > offset && iswspace(text->value[eol - 1]); eol--);
    text->size = eol;

    // IS_UNORDERED_LIST_#
    if(text->size >= offset + 2 &&
       (text->value[offset] == L'*' || text->value[offset] == L'-') &&
       iswspace(text->value[offset + 1])) {

        // if different from last lines offset
        if(offset != unordered_list_offset) {

            // test if offset matches a lower indent level
            for(i = unordered_list_level; i >= 0; i--) {
                if(unordered_list_level_offset[i] == offset) {
                    unordered_list_level = i;
                    break;
                }
            }
            // if offset doesn't match any previously stored indent level
            if(i != unordered_list_level) {
                unordered_list_level = MIN(unordered_list_level + 1, UNORDERED_LIST_MAX_LEVEL);
                // memorize the offset as next bigger indent level
                unordered_list_level_offset[unordered_list_level] = offset;
            }
        }

        // if no previous indent level matches, this must be the first line of the list
        if(unordered_list_level == 0) {
            unordered_list_level = 1;
            unordered_list_level_offset[1] = offset;
        }

        switch(unordered_list_level) {
            case 1: SET_BIT(bits, IS_UNORDERED_LIST_1); break;
            case 2: SET_BIT(bits, IS_UNORDERED_LIST_2); break;
            case 3: SET_BIT(bits, IS_UNORDERED_LIST_3); break;
            default: break;
        }
    }

    if(!CHECK_BIT(bits, IS_UNORDERED_LIST_1) &&
       !CHECK_BIT(bits, IS_UNORDERED_LIST_2) &&
       !CHECK_BIT(bits, IS_UNORDERED_LIST_3)) {

        // continue list if indent level is still the same as in previous line
        if ((CHECK_BIT(prev, IS_UNORDERED_LIST_1) ||
             CHECK_BIT(prev, IS_UNORDERED_LIST_2) ||
             CHECK_BIT(prev, IS_UNORDERED_LIST_3)) &&
            offset >= unordered_list_offset) {

            switch(unordered_list_level) {
                case 1: SET_BIT(bits, IS_UNORDERED_LIST_1); break;
                case 2: SET_BIT(bits, IS_UNORDERED_LIST_2); break;
                case 3: SET_BIT(bits, IS_UNORDERED_LIST_3); break;
                default: break;
            }

            // this line extends the previous list item
            SET_BIT(bits, IS_UNORDERED_LIST_EXT);

        // or reset indent level
        } else {
            unordered_list_level = 0;
        }
    }

    if(!CHECK_BIT(bits, IS_UNORDERED_LIST_1) &&
       !CHECK_BIT(bits, IS_UNORDERED_LIST_2) &&
       !CHECK_BIT(bits, IS_UNORDERED_LIST_3)) {

        // IS_CODE
        if(offset >= CODE_INDENT &&
           (CHECK_BIT(prev, IS_EMPTY) ||
            CHECK_BIT(prev, IS_CODE)  ||
            CHECK_BIT(prev, IS_STOP))) {
            SET_BIT(bits, IS_CODE);

        } else {

            // IS_QUOTE
            if(text->value[offset] == L'>') {
                SET_BIT(bits, IS_QUOTE);
            }

            // IS_CENTER
            if(text->size >= offset + 3 &&
               text->value[offset] == L'-' &&
               text->value[offset + 1] == L'>' &&
               iswspace(text->value[offset + 2])) {
                SET_BIT(bits, IS_CENTER);

                // remove start tag
                (text->strip)(text, offset, 3);
                eol -= 3;

                if(text->size >= offset + 3 &&
                   text->value[eol - 1] == L'-' &&
                   text->value[eol - 2] == L'<' &&
                   iswspace(text->value[eol - 3])) {

                    // remove end tags
                    (text->strip)(text, eol - 3, 3);

                    // adjust end of line
                    for(eol = text->size; eol > offset && iswspace(text->value[eol - 1]); eol--);

                }
            }

            for(i = offset; i < eol; i++) {

                if(iswspace(text->value[i])) {
                    spaces++;

                } else {
                    switch(text->value[i]) {
                        case L'=': equals++;  break;
                        case L'#': hashes++;  break;
                        case L'*': stars++;   break;
                        case L'-': minus++;   break;
                        case L'\\': other++; i++; break;
                        default:  other++;   break;
                    }
                }
            }

            // IS_H1
            if(equals > 0 &&
               hashes + stars + minus + spaces + other == 0) {
                SET_BIT(bits, IS_H1);
            }
            if(text->value[offset] == L'#' &&
               iswspace(text->value[offset+1])) {
                SET_BIT(bits, IS_H1);
                SET_BIT(bits, IS_H1_ATX);
            }

            // IS_H2
            if(minus > 0 &&
               equals + hashes + stars + spaces + other == 0) {
                SET_BIT(bits, IS_H2);
            }
            if(text->value[offset] == L'#' &&
               text->value[offset+1] == L'#' &&
               iswspace(text->value[offset+2])) {
                SET_BIT(bits, IS_H2);
                SET_BIT(bits, IS_H2_ATX);
            }

            // IS_H3..IS_H6
            if(text->value[offset] == L'#' &&
               text->value[offset+1] == L'#' &&
               text->value[offset+2] == L'#' &&
               iswspace(text->value[offset+3])) {
                SET_BIT(bits, IS_H3_ATX);
            }
            if(text->value[offset] == L'#' &&
               text->value[offset+1] == L'#' &&
               text->value[offset+2] == L'#' &&
               text->value[offset+3] == L'#' &&
               iswspace(text->value[offset+4])) {
                SET_BIT(bits, IS_H4_ATX);
            }
            if(text->value[offset] == L'#' &&
               text->value[offset+1] == L'#' &&
               text->value[offset+2] == L'#' &&
               text->value[offset+3] == L'#' &&
               text->value[offset+4] == L'#' &&
               iswspace(text->value[offset+5])) {
                SET_BIT(bits, IS_H5_ATX);
            }
            if(text->value[offset] == L'#' &&
               text->value[offset+1] == L'#' &&
               text->value[offset+2] == L'#' &&
               text->value[offset+3] == L'#' &&
               text->value[offset+4] == L'#' &&
               text->value[offset+5] == L'#' &&
               iswspace(text->value[offset+6])) {
                SET_BIT(bits, IS_H6_ATX);
            }

            // IS_HR
            if((minus >= 3 && equals + hashes + stars + other == 0) ||
               (stars >= 3 && equals + hashes + minus + other == 0)) {

                SET_BIT(bits, IS_HR);
            }

            // IS_EMPTY
            if(other == 0) {
                SET_BIT(bits, IS_EMPTY);
            }
        }
    }

    return bits;
}

void markdown_debug(deck_t *deck, int debug) {

    int sc = 0; // slide count
    int lc = 0; // line count

    int offset;
    line_t *header;

    if(debug == 1) {
        fwprintf(stderr, L"headers: %i\nslides: %i\n", deck->headers, deck->slides);

    } else if(debug > 1) {

        // print header to STDERR
        if(deck->header) {
            header = deck->header;
            while(header &&
                header->length > 0 &&
                header->text->value[0] == L'%') {

                // skip descriptor word (e.g. %title:)
                offset = next_blank(header->text, 0) + 1;

                fwprintf(stderr, L"header: %S\n", &header->text->value[offset]);
                header = header->next;
            }
        }
    }

    slide_t *slide = deck->slide;
    line_t *line;

    // print slide/line count to STDERR
    while(slide) {
        sc++;

        if(debug == 1) {
            fwprintf(stderr, L"  slide %i: %i lines\n", sc, slide->lines);

        } else if(debug > 1) {

            // also print bits and line length
            fwprintf(stderr, L"  slide %i:\n", sc);
            line = slide->line;
            lc = 0;
            while(line) {
                lc++;
                fwprintf(stderr, L"    line %i: bits = %i, length = %i\n", lc, line->bits, line->length);
                line = line->next;
            }
        }

        slide = slide->next;
    }
}

static void expand_character_entities(line_t *line)
{
    wchar_t *ampersand;
    wchar_t *prev, *curr;

    ampersand = NULL;
    curr = &line->text->value[0];

    // for each char in line
    for(prev = NULL; *curr; prev = curr++) {
        if (*curr == L'&' && (prev == NULL || *prev != L'\\')) {
            ampersand = curr;
            continue;
        }
        if (ampersand == NULL) {
            continue;
        }
        if (*curr == L'#') {
            if (prev == ampersand)
                continue;
            ampersand = NULL;
            continue;
        }
        if (iswalpha(*curr) || iswxdigit(*curr)) {
            continue;
        }
        if (*curr == L';') {
            int cnt;
            wchar_t ucs = L'\0';
            if (ampersand + 1 >= curr || ampersand + 16 < curr) { // what is a good limit?
                ampersand = NULL;
                continue;
            }
            if (ampersand[1] == L'#') { // &#nnnn; or &#xhhhh;
                if (ampersand + 2 >= curr) {
                    ampersand = NULL;
                    continue;
                }
                if (ampersand[2] != L'x') { // &#nnnn;
                    cnt = wcsspn(&ampersand[2], L"0123456789");
                    if (ampersand + 2 + cnt != curr) {
                        ampersand = NULL;
                        continue;
                    }
                    ucs = wcstoul(&ampersand[2], NULL, 10);
                } else { // &#xhhhh;
                    if (ampersand + 3 >= curr) {
                        ampersand = NULL;
                        continue;
                    }
                    cnt = wcsspn(&ampersand[3], L"0123456789abcdefABCDEF");
                    if (ampersand + 3 + cnt != curr) {
                        ampersand = NULL;
                        continue;
                    }
                    ucs = wcstoul(&ampersand[3], NULL, 16);
                }
            } else { // &name;
                for (cnt = 0; cnt < sizeof(named_character_entities)/sizeof(named_character_entities[0]); ++cnt) {
                    if (wcsncmp(named_character_entities[cnt].name, &ampersand[1], curr - ampersand - 1))
                        continue;
                    ucs = named_character_entities[cnt].ucs;
                    break;
                }
                if (ucs == L'\0') {
                    ampersand = NULL;
                    continue;
                }
            }
            *ampersand = ucs;
            cstring_strip(line->text, ampersand + 1 - &line->text->value[0], curr - ampersand);
            curr = ampersand;
            ampersand = NULL;
        }
    }
}

static void adjust_line_length(line_t *line) {
    int l = 0;
    const static wchar_t *special = L"\\*_`"; // list of interpreted chars
    const wchar_t *c = &line->text->value[0];
    cstack_t *stack = cstack_init();

    // for each char in line
    for(; *c; c++) {
        // if char is in special char list
        if(wcschr(special, *c)) {

            // closing special char (or second backslash)
            if((stack->top)(stack, *c)) {
                if(*c == L'\\') l++;
                (stack->pop)(stack);

            // treat special as regular char
            } else if((stack->top)(stack, L'\\')) {
                l++;
                (stack->pop)(stack);

            // opening special char
            } else {
                (stack->push)(stack, *c);
            }

        } else {
            // remove backslash from stack
            if((stack->top)(stack, L'\\'))
                (stack->pop)(stack);
            l++;
        }
    }

    if(CHECK_BIT(line->bits, IS_H1_ATX))
        l -= 2;
    if(CHECK_BIT(line->bits, IS_H2_ATX))
        l -= 3;

    line->length = l;

    (stack->delete)(stack);
}

int next_nonblank(cstring_t *text, int i) {
    while ((i < text->size) && iswspace((text->value)[i]))
        i++;

    return i;
}

int prev_blank(cstring_t *text, int i) {
    while ((i > 0) && !iswspace((text->value)[i]))
        i--;

    return i;
}

int next_blank(cstring_t *text, int i) {
    while ((i < text->size) && !iswspace((text->value)[i]))
        i++;

    return i;
}

int next_word(cstring_t *text, int i) {
    return next_nonblank(text, next_blank(text, i));
}

static int next_nontilde(cstring_t *text, int i) {
    while ((i < text->size) && text->value[i] == L'~')
        i++;

    return i;
}

static int next_nonbacktick(cstring_t *text, int i) {
    while ((i < text->size) && text->value[i] == L'`')
        i++;

    return i;
}
