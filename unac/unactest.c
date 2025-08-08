/*
 * Copyright (C) 2000, 2001, 2002 Loic Dachary <loic@senga.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/*
 * Generate a machine readable file mapping each UTF-16 character to
 * its unaccented equivalent using the libunac library. The output may
 * then be compared with the output of builder -reference. Both must be
 * equal (ignoring space differences).
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "unac.h"

int main() {
  unsigned int c;
  unsigned short* p;
  int l;
  (void)unac_version();
  for(c = 0; c <= 0xFFFF; c++) {
    unac_char_utf16(c, p, l);
    printf("%04X", c);
    if(l == 0) {
      printf("\n");
    } else {
      int i;
      printf(" => ");
      for(i = 0; i < l; i++) {
    printf("%04X ", p[i]);
      }
      printf("\n");
    }
  }

  return 0;
}
