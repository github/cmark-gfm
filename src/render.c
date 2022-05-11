#include "render.h"
#include "buffer.h"
#include "chunk.h"
#include "cmark-gfm.h"
#include "node.h"
#include "syntax_extension.h"
#include "utf8.h"
#include <stdlib.h>
#include <wchar.h>

static CMARK_INLINE void S_cr(cmark_renderer *renderer) {
  if (renderer->need_cr < 1) {
    renderer->need_cr = 1;
  }
}

static CMARK_INLINE void S_blankline(cmark_renderer *renderer) {
  if (renderer->need_cr < 2) {
    renderer->need_cr = 2;
  }
}

static void S_out(cmark_renderer *renderer, cmark_node *node,
                  const char *source, bool wrap, cmark_escaping escape) {
  int length = (int)strlen(source);
  unsigned char nextc;
  int32_t c;
  int i = 0;
  int last_nonspace;
  int len;
  cmark_chunk remainder = cmark_chunk_literal("");
  int k = renderer->buffer->size - 1;

  cmark_syntax_extension *ext = NULL;
  cmark_node *n = node;
  while (n && !ext) {
    ext = n->extension;
    if (!ext)
      n = n->parent;
  }
  if (ext && !ext->commonmark_escape_func)
    ext = NULL;

  wrap = wrap && !renderer->no_linebreaks;

  if (renderer->in_tight_list_item && renderer->need_cr > 1) {
    renderer->need_cr = 1;
  }
  while (renderer->need_cr) {
    if (k < 0 || renderer->buffer->ptr[k] == '\n') {
      k -= 1;
    } else {
      cmark_strbuf_putc(renderer->buffer, '\n');
      if (renderer->need_cr > 1) {
        cmark_strbuf_put(renderer->buffer, renderer->prefix->ptr,
                         renderer->prefix->size);
      }
    }
    renderer->column = 0;
    renderer->last_breakable = 0;
    renderer->begin_line = true;
    renderer->begin_content = true;
    renderer->need_cr -= 1;
  }

  while (i < length) {
    if (renderer->begin_line) {
      cmark_strbuf_put(renderer->buffer, renderer->prefix->ptr,
                       renderer->prefix->size);
      // note: this assumes prefix is ascii:
      renderer->column = renderer->prefix->size;
    }

    len = cmark_utf8proc_iterate((const uint8_t *)source + i, length - i, &c);
    if (len == -1) { // error condition
      return;        // return without rendering rest of string
    }

    if (ext && ext->commonmark_escape_func(ext, node, c))
      cmark_strbuf_putc(renderer->buffer, '\\');

    nextc = source[i + len];
    if (c == 32 && wrap) {
      if (!renderer->begin_line) {
        last_nonspace = renderer->buffer->size;
        cmark_strbuf_putc(renderer->buffer, ' ');
        renderer->column += 1;
        renderer->begin_line = false;
        renderer->begin_content = false;
        // skip following spaces
        while (source[i + 1] == ' ') {
          i++;
        }
        // We don't allow breaks that make a digit the first character
        // because this causes problems with commonmark output.
        if (!cmark_isdigit(source[i + 1])) {
          renderer->last_breakable = last_nonspace;
        }
      }
    } else if (escape == LITERAL) {
      if (c == 10) {
        cmark_strbuf_putc(renderer->buffer, '\n');
        renderer->column = 0;
        renderer->begin_line = true;
        renderer->begin_content = true;
        renderer->last_breakable = 0;
      } else {
        cmark_render_code_point(renderer, c);
        renderer->begin_line = false;
        // we don't set 'begin_content' to false til we've
        // finished parsing a digit.  Reason:  in commonmark
        // we need to escape a potential list marker after
        // a digit:
        renderer->begin_content =
            renderer->begin_content && cmark_isdigit((char)c) == 1;
      }
    } else {
      (renderer->outc)(renderer, node, escape, c, nextc);
      renderer->begin_line = false;
      renderer->begin_content =
          renderer->begin_content && cmark_isdigit((char)c) == 1;
    }

    // If adding the character went beyond width, look for an
    // earlier place where the line could be broken:
    if (renderer->width > 0 && renderer->column > renderer->width &&
        !renderer->begin_line && renderer->last_breakable > 0) {

      // copy from last_breakable to remainder
      cmark_chunk_set_cstr(renderer->mem, &remainder,
                           (char *)renderer->buffer->ptr +
                               renderer->last_breakable + 1);
      // truncate at last_breakable
      if (*(renderer->buffer->ptr + renderer->last_breakable) == ' ') {
        cmark_strbuf_truncate(renderer->buffer, renderer->last_breakable);
      } else {
        cmark_strbuf_truncate(renderer->buffer, renderer->last_breakable + 1);
      }
      // add newline, prefix, and remainder
      cmark_strbuf_putc(renderer->buffer, '\n');
      cmark_strbuf_put(renderer->buffer, renderer->prefix->ptr,
                       renderer->prefix->size);
      cmark_strbuf_put(renderer->buffer, remainder.data, remainder.len);
      renderer->column = renderer->prefix->size + remainder.len;
      cmark_chunk_free(renderer->mem, &remainder);
      renderer->last_breakable = 0;
      renderer->begin_line = false;
      renderer->begin_content = false;
    }

    i += len;
  }
}

// Assumes no newlines, assumes ascii content:
void cmark_render_ascii(cmark_renderer *renderer, const char *s) {
  int origsize = renderer->buffer->size;
  cmark_strbuf_puts(renderer->buffer, s);
  renderer->column += renderer->buffer->size - origsize;
}

bool S_allow_in_start_of_line(uint32_t c) {
  static const uint32_t data[] = {
      33,    34,    37,    39,    41,    44,    46,    58,    59,    63,
      93,    125,   162,   176,   183,   187,   8208,  8211,  8212,  8224,
      8225,  8226,  8250,  8252,  8263,  8264,  8265,  8451,  8758,  12289,
      12290, 12291, 12293, 12294, 12297, 12299, 12301, 12303, 12305, 12309,
      12311, 12313, 12316, 12318, 12319, 12347, 12353, 12355, 12357, 12359,
      12361, 12387, 12419, 12421, 12423, 12430, 12437, 12438, 12448, 12449,
      12451, 12453, 12455, 12457, 12483, 12515, 12517, 12519, 12526, 12533,
      12534, 12539, 12540, 12541, 12542, 12784, 12785, 12786, 12787, 12788,
      12789, 12790, 12791, 12792, 12793, 12794, 12795, 12796, 12797, 12798,
      12799, 65072, 65073, 65074, 65075, 65078, 65080, 65082, 65084, 65086,
      65088, 65090, 65104, 65105, 65106, 65108, 65109, 65110, 65111, 65112,
      65114, 65116, 65281, 65282, 65285, 65287, 65289, 65292, 65294, 65306,
      65307, 65311, 65341, 65372, 65373, 65374, 65376, 65380,
  };
  for (size_t i = 0; i < sizeof(data) / sizeof(data[0]); i++) {
    if (data[i] == c) {
      return false;
    }
  }
  return true;
}

bool S_allow_in_end_of_line(uint32_t c) {
  static const uint32_t data[] = {
      34,    35,    36,    39,    40,    91,    92,    123,   163,   165,
      171,   183,   8245,  12293, 12295, 12296, 12297, 12298, 12299, 12300,
      12301, 12302, 12304, 12308, 12310, 12312, 12317, 65076, 65077, 65079,
      65081, 65083, 65085, 65087, 65089, 65091, 65103, 65113, 65115, 65284,
      65288, 65294, 65339, 65371, 65375, 65376, 65505, 65509, 65510,
  };
  for (size_t i = 0; i < sizeof(data) / sizeof(data[0]); i++) {
    if (data[i] == c) {
      return false;
    }
  }
  return true;
}

void cmark_render_code_point(cmark_renderer *renderer, uint32_t c) {
  extern int wcwidth(wchar_t);
  int width = wcwidth(c);
  renderer->column += width;
  if (width == 2 && renderer->last_is_breakable &&
      S_allow_in_start_of_line(c)) {
    renderer->last_breakable = renderer->buffer->size - 1;
  }
  renderer->last_is_breakable = S_allow_in_end_of_line(c);
  cmark_utf8proc_encode_char(c, renderer->buffer);
}

char *cmark_render(cmark_mem *mem, cmark_node *root, int options, int width,
                   void (*outc)(cmark_renderer *, cmark_node *, cmark_escaping,
                                int32_t, unsigned char),
                   int (*render_node)(cmark_renderer *renderer,
                                      cmark_node *node,
                                      cmark_event_type ev_type, int options)) {
  cmark_strbuf pref = CMARK_BUF_INIT(mem);
  cmark_strbuf buf = CMARK_BUF_INIT(mem);
  cmark_node *cur;
  cmark_event_type ev_type;
  char *result;
  cmark_iter *iter = cmark_iter_new(root);

  cmark_renderer renderer = {mem,  &buf, &pref,       0,     width, 0,
                             0,    true, true,        false, false, false,
                             outc, S_cr, S_blankline, S_out, 0};

  while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
    cur = cmark_iter_get_node(iter);
    if (!render_node(&renderer, cur, ev_type, options)) {
      // a false value causes us to skip processing
      // the node's contents.  this is used for
      // autolinks.
      cmark_iter_reset(iter, cur, CMARK_EVENT_EXIT);
    }
  }

  // ensure final newline
  if (renderer.buffer->size == 0 ||
      renderer.buffer->ptr[renderer.buffer->size - 1] != '\n') {
    cmark_strbuf_putc(renderer.buffer, '\n');
  }

  result = (char *)cmark_strbuf_detach(renderer.buffer);

  cmark_iter_free(iter);
  cmark_strbuf_free(renderer.prefix);
  cmark_strbuf_free(renderer.buffer);

  return result;
}
