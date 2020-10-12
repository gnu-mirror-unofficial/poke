/* ios-buffer.h - The buffer for IO devices.  */

/* Copyright (C) 2020 Egeyar Bagcioglu */

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

struct ios_buffer;

struct ios_buffer * ios_buffer_init ();

int ios_buffer_free (struct ios_buffer *buffer);

ios_dev_off
ios_buffer_get_begin_offset (struct ios_buffer *buffer);

ios_dev_off
ios_buffer_get_end_offset (struct ios_buffer *buffer);

struct ios_buffer_chunk*
ios_buffer_get_chunk (struct ios_buffer *buffer, int chunk_no);

int
ios_buffer_allocate_new_chunk (struct ios_buffer *buffer, int final_chunk_no,
			       struct ios_buffer_chunk **final_chunk);

int
ios_buffer_pread (struct ios_buffer *buffer, void *buf, size_t count,
		  ios_dev_off offset);

int
ios_buffer_pwrite (struct ios_buffer *buffer, const void *buf, size_t count,
		   ios_dev_off offset);

int
ios_buffer_forget_till (struct ios_buffer *buffer, ios_dev_off offset);
