/* usock-buf.h */

/* Copyright (C) 2022 Mohammad-Reza Nabipoor.  */

/* This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef USOCK_BUF_H
#define USOCK_BUF_H

#include <config.h>
#include <stddef.h>
#include <stdint.h>

struct usock_buf;

// Get a pointer to buffer's data which has length LEN
unsigned char *usock_buf_data (struct usock_buf *b, size_t *len);

// Consider the buffer's data as null-terminated string.
// NOTE All buffers in usock_buf are null-terminated.
char *usock_buf_str (struct usock_buf *b);

// Give the TAG associated with buffer. The interpretation is up to the user.
uint64_t usock_buf_tag (struct usock_buf *b);

// Give the next buffer in the chain.
struct usock_buf *usock_buf_next (struct usock_buf *b);

// Free current node of buffer B and give the next buffer in the chain.
struct usock_buf *usock_buf_free (struct usock_buf *b);

// Free the buffer chain starting from the current node.
void usock_buf_free_chain (struct usock_buf *b);

#endif /* ! USOC_BUF_H */
