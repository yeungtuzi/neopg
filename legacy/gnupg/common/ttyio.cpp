/* ttyio.c -  tty i/O functions
 * Copyright (C) 1998,1999,2000,2001,2002,2003,2004,2006,2007,
 *               2009, 2010 Free Software Foundation, Inc.
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

#include <config.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(HAVE_W32_SYSTEM)
#define USE_W32_CONSOLE 1
#endif

#ifdef HAVE_TCGETATTR
#include <termios.h>
#else
#ifdef HAVE_TERMIO_H
/* simulate termios with termio */
#include <termio.h>
#define termios termio
#define tcsetattr ioctl
#define TCSAFLUSH TCSETAF
#define tcgetattr(A, B) ioctl(A, TCGETA, B)
#define HAVE_TCGETATTR
#endif
#endif
#ifdef USE_W32_CONSOLE
#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#endif
#include <windows.h>
#ifdef HAVE_TCGETATTR
#error mingw32 and termios
#endif
#endif
#include <ctype.h>
#include <errno.h>

#include "common-defs.h"
#include "ttyio.h"
#include "util.h"

#define CONTROL_D ('D' - 'A' + 1)

#ifdef USE_W32_CONSOLE
static struct { HANDLE in, out; } con;
#define DEF_INPMODE \
  (ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT)
#define HID_INPMODE (ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT)
#define DEF_OUTMODE (ENABLE_WRAP_AT_EOL_OUTPUT | ENABLE_PROCESSED_OUTPUT)

#else /* yeah, we have a real OS */
static FILE *ttyfp = NULL;
#endif

static int initialized;
static int last_prompt_len;
static int batchmode;
static int no_terminal;

#ifdef HAVE_TCGETATTR
static struct termios termsave;
static int restore_termios;
#endif

/* Hooks set by gpgrlhelp.c if required. */
static void (*my_rl_set_completer)(rl_completion_func_t *);
static void (*my_rl_inhibit_completion)(int);
static void (*my_rl_cleanup_after_signal)(void);
static void (*my_rl_init_stream)(FILE *);
static char *(*my_rl_readline)(const char *);
static void (*my_rl_add_history)(const char *);

/* This is a wrapper around ttyname so that we can use it even when
   the standard streams are redirected.  It figures the name out the
   first time and returns it in a statically allocated buffer. */
const char *tty_get_ttyname(void) {
  static char *name;

/* On a GNU system ctermid() always return /dev/tty, so this does
   not make much sense - however if it is ever changed we do the
   Right Thing now. */
#ifdef HAVE_CTERMID
  static int got_name;

  if (!got_name) {
    const char *s;
/* Note that despite our checks for these macros the function is
   not necessarily thread save.  We mainly do this for
   portability reasons, in case L_ctermid is not defined. */
#if defined(_POSIX_THREAD_SAFE_FUNCTIONS) || defined(_POSIX_TRHEADS)
    char buffer[L_ctermid];
    s = ctermid(buffer);
#else
    s = ctermid(NULL);
#endif
    if (s) name = strdup(s);
    got_name = 1;
  }
#endif /*HAVE_CTERMID*/
  /* Assume the standard tty on memory error or when there is no
     ctermid. */
  return name ? name : "/dev/tty";
}

#ifdef HAVE_TCGETATTR
static void cleanup(void) {
  if (restore_termios) {
    restore_termios = 0; /* do it prios in case it is interrupted again */
    if (tcsetattr(fileno(ttyfp), TCSAFLUSH, &termsave))
      log_error("tcsetattr() failed: %s\n", strerror(errno));
  }
}
#endif

static void init_ttyfp(void) {
  if (initialized) return;

#if defined(USE_W32_CONSOLE)
  {
    SECURITY_ATTRIBUTES sa;

    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    con.out = CreateFileA("CONOUT$", GENERIC_READ | GENERIC_WRITE,
                          FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                          OPEN_EXISTING, 0, 0);
    if (con.out == INVALID_HANDLE_VALUE)
      log_fatal("open(CONOUT$) failed: rc=%d", (int)GetLastError());
    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    con.in = CreateFileA("CONIN$", GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING,
                         0, 0);
    if (con.in == INVALID_HANDLE_VALUE)
      log_fatal("open(CONIN$) failed: rc=%d", (int)GetLastError());
  }
  SetConsoleMode(con.in, DEF_INPMODE);
  SetConsoleMode(con.out, DEF_OUTMODE);

#elif defined(__EMX__)
  ttyfp = stdout; /* Fixme: replace by the real functions: see wklib */
  if (my_rl_init_stream) my_rl_init_stream(ttyfp);
#else
  ttyfp = batchmode ? stderr : fopen(tty_get_ttyname(), "r+");
  if (!ttyfp) {
    log_error("cannot open '%s': %s\n", tty_get_ttyname(), strerror(errno));
    exit(2);
  }
  if (my_rl_init_stream) my_rl_init_stream(ttyfp);
#endif

#ifdef HAVE_TCGETATTR
  atexit(cleanup);
#endif
  initialized = 1;
}

int tty_batchmode(int onoff) {
  int old = batchmode;
  if (onoff != -1) batchmode = onoff;
  return old;
}

int tty_no_terminal(int onoff) {
  int old = no_terminal;
  no_terminal = onoff ? 1 : 0;
  return old;
}

void tty_printf(const char *fmt, ...) {
  va_list arg_ptr;

  if (no_terminal) return;

  if (!initialized) init_ttyfp();

  va_start(arg_ptr, fmt);
#ifdef USE_W32_CONSOLE
  {
    char *buf = NULL;
    int n;
    DWORD nwritten;

    n = vasprintf(&buf, fmt, arg_ptr);
    if (!buf) log_bug("vasprintf() failed\n");

    if (!WriteConsoleA(con.out, buf, n, &nwritten, NULL))
      log_fatal("WriteConsole failed: rc=%d", (int)GetLastError());
    if (n != nwritten)
      log_fatal("WriteConsole failed: %d != %d\n", n, (int)nwritten);
    last_prompt_len += n;
    xfree(buf);
  }
#else
  last_prompt_len += vfprintf(ttyfp, fmt, arg_ptr);
  fflush(ttyfp);
#endif
  va_end(arg_ptr);
}

/* Same as tty_printf but if FP is not NULL, behave like a regular
   fprintf. */
void tty_fprintf(estream_t fp, const char *fmt, ...) {
  va_list arg_ptr;

  if (fp) {
    va_start(arg_ptr, fmt);
    es_vfprintf(fp, fmt, arg_ptr);
    va_end(arg_ptr);
    return;
  }

  if (no_terminal) return;

  if (!initialized) init_ttyfp();

  va_start(arg_ptr, fmt);
#ifdef USE_W32_CONSOLE
  {
    char *buf = NULL;
    int n;
    DWORD nwritten;

    n = vasprintf(&buf, fmt, arg_ptr);
    if (!buf) log_bug("vasprintf() failed\n");

    if (!WriteConsoleA(con.out, buf, n, &nwritten, NULL))
      log_fatal("WriteConsole failed: rc=%d", (int)GetLastError());
    if (n != nwritten)
      log_fatal("WriteConsole failed: %d != %d\n", n, (int)nwritten);
    last_prompt_len += n;
    xfree(buf);
  }
#else
  last_prompt_len += vfprintf(ttyfp, fmt, arg_ptr);
  fflush(ttyfp);
#endif
  va_end(arg_ptr);
}

/* Print a string, but filter all control characters out.  If FP is
 * not NULL print to that stream instead to the tty.  */
static void do_print_string(estream_t fp, const byte *p, size_t n) {
  if (no_terminal && !fp) return;

  if (!initialized && !fp) init_ttyfp();

  if (fp) {
    print_utf8_buffer(fp, p, n);
    return;
  }

#ifdef USE_W32_CONSOLE
  /* Not so effective, change it if you want */
  for (; n; n--, p++) {
    if (iscntrl(*p)) {
      if (*p == '\n')
        tty_printf("\\n");
      else if (!*p)
        tty_printf("\\0");
      else
        tty_printf("\\x%02x", *p);
    } else
      tty_printf("%c", *p);
  }
#else
  for (; n; n--, p++) {
    if (iscntrl(*p)) {
      putc('\\', ttyfp);
      if (*p == '\n')
        putc('n', ttyfp);
      else if (!*p)
        putc('0', ttyfp);
      else
        fprintf(ttyfp, "x%02x", *p);
    } else
      putc(*p, ttyfp);
  }
#endif
}

void tty_print_utf8_string2(estream_t fp, const byte *p, size_t n,
                            size_t max_n) {
  size_t i;

  if (no_terminal && !fp) return;

  /* we can handle plain ascii simpler, so check for it first */
  for (i = 0; i < n; i++) {
    if (p[i] & 0x80) break;
  }
  if (i < n) {
    std::string buf = utf8_to_native((const char *)p, n, 0);
    if (max_n && (strlen(buf.c_str()) > max_n)) {
      buf[max_n] = 0;
    }
    /*(utf8 conversion already does the control character quoting)*/
    tty_fprintf(fp, "%s", buf.c_str());
  } else {
    if (max_n && (n > max_n)) {
      n = max_n;
    }
    do_print_string(fp, p, n);
  }
}

void tty_print_utf8_string(const byte *p, size_t n) {
  tty_print_utf8_string2(NULL, p, n, 0);
}

static char *do_get(const char *prompt) {
  char *buf;
  byte cbuf[1];
  int c, n, i;

  if (batchmode) {
    log_error("Sorry, we are in batchmode - can't get input\n");
    exit(2);
  }

  if (no_terminal) {
    log_error("Sorry, no terminal at all requested - can't get input\n");
    exit(2);
  }

  if (!initialized) init_ttyfp();

  last_prompt_len = 0;
  tty_printf("%s", prompt);
  buf = (char *)xmalloc((n = 50));
  i = 0;

#ifdef USE_W32_CONSOLE
  for (;;) {
    DWORD nread;

    if (!ReadConsoleA(con.in, cbuf, 1, &nread, NULL))
      log_fatal("ReadConsole failed: rc=%d", (int)GetLastError());
    if (!nread) continue;
    if (*cbuf == '\n') break;

    last_prompt_len++;
    c = *cbuf;
    if (c == '\t')
      c = ' ';
    else if (c > 0xa0)
      ; /* we don't allow 0xa0, as this is a protected blank which may
         * confuse the user */
    else if (iscntrl(c))
      continue;
    if (!(i < n - 1)) {
      n += 50;
      buf = xrealloc(buf, n);
    }
    buf[i++] = c;
  }

#else /* Other systems. */

  /* fixme: How can we avoid that the \n is echoed w/o disabling
   * canonical mode - w/o this kill_prompt can't work */
  while (read(fileno(ttyfp), cbuf, 1) == 1 && *cbuf != '\n') {
    last_prompt_len++;
    c = *cbuf;
    if (c == CONTROL_D) log_info("control d found\n");
    if (c == '\t')
      c = ' ';
    else if (c > 0xa0)
      ; /* we don't allow 0xa0, as this is a protected blank which may
         * confuse the user */
    else if (iscntrl(c))
      continue;
    if (!(i < n - 1)) {
      n += 50;
      buf = (char *)xrealloc(buf, n);
    }
    buf[i++] = c;
  }
  if (*cbuf != '\n') {
    buf[0] = CONTROL_D;
    i = 1;
  }

#endif /* end unix version */
  buf[i] = 0;
  return buf;
}

static Botan::secure_vector<char> *do_get_hidden(const char *prompt) {
  Botan::secure_vector<char> *buf = nullptr;
  byte cbuf[1];
  int c, n, i;

  if (batchmode) {
    log_error("Sorry, we are in batchmode - can't get input\n");
    exit(2);
  }

  if (no_terminal) {
    log_error("Sorry, no terminal at all requested - can't get input\n");
    exit(2);
  }

  if (!initialized) init_ttyfp();

  last_prompt_len = 0;
  tty_printf("%s", prompt);
  n = 50;
  buf = new Botan::secure_vector<char>(n);
  i = 0;

#ifdef USE_W32_CONSOLE
  SetConsoleMode(con.in, HID_INPMODE);

  for (;;) {
    DWORD nread;

    if (!ReadConsoleA(con.in, cbuf, 1, &nread, NULL))
      log_fatal("ReadConsole failed: rc=%d", (int)GetLastError());
    if (!nread) continue;
    if (*cbuf == '\n') break;

    c = *cbuf;
    if (c == '\t')
      c = ' ';
    else if (c > 0xa0)
      ; /* we don't allow 0xa0, as this is a protected blank which may
         * confuse the user */
    else if (iscntrl(c))
      continue;
    if (!(i < n - 1)) {
      n += 50;
      buf->reserve(n);
    }
    (*buf)[i++] = c;
  }

  SetConsoleMode(con.in, DEF_INPMODE);

#else /* Other systems. */
  {
#ifdef HAVE_TCGETATTR
    struct termios term;

    if (tcgetattr(fileno(ttyfp), &termsave))
      log_fatal("tcgetattr() failed: %s\n", strerror(errno));
    restore_termios = 1;
    term = termsave;
    term.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
    if (tcsetattr(fileno(ttyfp), TCSAFLUSH, &term))
      log_fatal("tcsetattr() failed: %s\n", strerror(errno));
#endif
  }

  /* fixme: How can we avoid that the \n is echoed w/o disabling
   * canonical mode - w/o this kill_prompt can't work */
  while (read(fileno(ttyfp), cbuf, 1) == 1 && *cbuf != '\n') {
    c = *cbuf;
    if (c == CONTROL_D) log_info("control d found\n");
    if (c == '\t')
      c = ' ';
    else if (c > 0xa0)
      ; /* we don't allow 0xa0, as this is a protected blank which may
         * confuse the user */
    else if (iscntrl(c))
      continue;
    if (!(i < n - 1)) {
      n += 50;
      buf->reserve(n);
    }
    (*buf)[i++] = c;
  }
  if (*cbuf != '\n') {
    (*buf)[0] = CONTROL_D;
    i = 1;
  }

  {
#ifdef HAVE_TCGETATTR
    if (tcsetattr(fileno(ttyfp), TCSAFLUSH, &termsave))
      log_error("tcsetattr() failed: %s\n", strerror(errno));
    restore_termios = 0;
#endif
  }
#endif /* end unix version */
  (*buf)[i] = 0;
  return buf;
}

char *tty_get(const char *prompt) {
  if (!batchmode && !no_terminal && my_rl_readline && my_rl_add_history) {
    char *line;
    char *buf;

    if (!initialized) init_ttyfp();

    last_prompt_len = 0;

    line = my_rl_readline(prompt ? prompt : "");

    /* We need to copy it to memory controlled by our malloc
       implementations; further we need to convert an EOF to our
       convention. */
    buf = (char *)xmalloc(line ? strlen(line) + 1 : 2);
    if (line) {
      strcpy(buf, line);
      trim_spaces(buf);
      if (strlen(buf) > 2)
        my_rl_add_history(line); /* Note that we test BUF but add LINE. */
      free(line);
    } else {
      buf[0] = CONTROL_D;
      buf[1] = 0;
    }
    return buf;
  } else
    return do_get(prompt);
}

/* Variable argument version of tty_get.  The prompt is actually a
   format string with arguments.  */
char *tty_getf(const char *promptfmt, ...) {
  va_list arg_ptr;
  char *prompt;
  char *answer;

  va_start(arg_ptr, promptfmt);
  if (gpgrt_vasprintf(&prompt, promptfmt, arg_ptr) < 0)
    log_fatal("estream_vasprintf failed: %s\n", strerror(errno));
  va_end(arg_ptr);
  answer = tty_get(prompt);
  xfree(prompt);
  return answer;
}

Botan::secure_vector<char> *tty_get_hidden(const char *prompt) {
  return do_get_hidden(prompt);
}

void tty_kill_prompt() {
  if (no_terminal) return;

  if (!initialized) init_ttyfp();

  if (batchmode) last_prompt_len = 0;
  if (!last_prompt_len) return;
#ifdef USE_W32_CONSOLE
  tty_printf("\r%*s\r", last_prompt_len, "");
#else
  {
    int i;
    putc('\r', ttyfp);
    for (i = 0; i < last_prompt_len; i++) putc(' ', ttyfp);
    putc('\r', ttyfp);
    fflush(ttyfp);
  }
#endif
  last_prompt_len = 0;
}

int tty_get_answer_is_yes(const char *prompt) {
  int yes;
  char *p = tty_get(prompt);
  tty_kill_prompt();
  yes = answer_is_yes(p);
  xfree(p);
  return yes;
}

/* Called by gnupg_rl_initialize to setup the readline support. */
void tty_private_set_rl_hooks(void (*init_stream)(FILE *),
                              void (*set_completer)(rl_completion_func_t *),
                              void (*inhibit_completion)(int),
                              void (*cleanup_after_signal)(void),
                              char *(*readline_fun)(const char *),
                              void (*add_history_fun)(const char *)) {
  my_rl_init_stream = init_stream;
  my_rl_set_completer = set_completer;
  my_rl_inhibit_completion = inhibit_completion;
  my_rl_cleanup_after_signal = cleanup_after_signal;
  my_rl_readline = readline_fun;
  my_rl_add_history = add_history_fun;
}

#ifdef HAVE_LIBREADLINE
void tty_enable_completion(rl_completion_func_t *completer) {
  if (no_terminal || !my_rl_set_completer) return;

  if (!initialized) init_ttyfp();

  my_rl_set_completer(completer);
}

void tty_disable_completion(void) {
  if (no_terminal || !my_rl_inhibit_completion) return;

  if (!initialized) init_ttyfp();

  my_rl_inhibit_completion(1);
}
#endif

void tty_cleanup_after_signal(void) {
#ifdef HAVE_TCGETATTR
  cleanup();
#endif
}

void tty_cleanup_rl_after_signal(void) {
  if (my_rl_cleanup_after_signal) my_rl_cleanup_after_signal();
}
