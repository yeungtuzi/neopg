/* http.h  -  HTTP protocol handler
 * Copyright (C) 1999, 2000, 2001, 2003, 2006,
 *               2010 Free Software Foundation, Inc.
 * Copyright (C) 2015  g10 Code GmbH
 *
 * This file is part of GnuPG.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of either
 *
 *   - the GNU Lesser General Public License as published by the Free
 *     Software Foundation; either version 3 of the License, or (at
 *     your option) any later version.
 *
 * or
 *
 *   - the GNU General Public License as published by the Free
 *     Software Foundation; either version 2 of the License, or (at
 *     your option) any later version.
 *
 * or both in parallel, as here.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */
#ifndef GNUPG_COMMON_HTTP_H
#define GNUPG_COMMON_HTTP_H

#include <string>

#include <gpg-error.h>

struct uri_tuple_s {
  struct uri_tuple_s *next;
  const char *name; /* A pointer into name. */
  char *value;      /* A pointer to value (a Nul is always appended). */
  size_t valuelen;  /* The real length of the value; we need it
                       because the value may contain embedded Nuls. */
  int no_value;     /* True if no value has been given in the URL. */
};
typedef struct uri_tuple_s *uri_tuple_t;

struct parsed_uri_s {
  /* All these pointers point into BUFFER; most stuff is not escaped. */
  char *scheme; /* Pointer to the scheme string (always lowercase). */
  unsigned int is_http : 1;       /* This is a HTTP style URI.   */
  unsigned int use_tls : 1;       /* Whether TLS should be used. */
  unsigned int opaque : 1;        /* Unknown scheme; PATH has the rest.  */
  unsigned int v6lit : 1;         /* Host was given as a literal v6 address.  */
  unsigned int onion : 1;         /* .onion address given.  */
  unsigned int explicit_port : 1; /* The port was explicitly specified.  */
  char *auth;                     /* username/password for basic auth.  */
  char *host;                     /* Host (converted to lowercase). */
  unsigned short port;            /* Port (always set if the host is set). */
  char *path;                     /* Path. */
  uri_tuple_t params;             /* ";xxxxx" */
  uri_tuple_t query;              /* "?xxx=yyy" */
  char buffer[1]; /* Buffer which holds a (modified) copy of the URI. */
};
typedef struct parsed_uri_s *parsed_uri_t;

gpg_error_t http_parse_uri(parsed_uri_t *ret_uri, const char *uri,
                           int no_scheme_check);

void http_release_parsed_uri(parsed_uri_t uri);

std::string http_escape_string(const char *string, const char *specials);

#endif /*GNUPG_COMMON_HTTP_H*/
