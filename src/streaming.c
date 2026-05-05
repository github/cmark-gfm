// Streaming AST support for cmark-gfm.
//
// Provides incremental snapshots of the parse tree while feed() is in
// progress, plus a stream of change events for in-place rewrites (paragraph -> setext,
// paragraph -> table, ref-link resolution, etc.). See cmark-gfm.h for the
// public contract and project_streaming_ast.md for design notes.

#include <stdlib.h>
#include <string.h>

#include "cmark-gfm.h"
#include "inlines.h"
#include "map.h"
#include "node.h"
#include "parser.h"
#include "streaming.h"
#include "syntax_extension.h"

// The opaque iterator type announced in the public header.
struct cmark_change_iter {
  cmark_mem *mem;
  struct cmark_change_record *head;    // still-to-yield records
  struct cmark_change_record *to_free; // full chain head, freed on _free
};

// Thread-local active-parser pointer. Lets node mutation primitives (which
// have public, parser-less signatures) emit change events when running
// inside cmark_parser_feed / cmark_parser_finish. Falls back to plain static
// if neither C11 threads nor a compiler thread-local extension is available
// (single-threaded build).
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && \
    !defined(__STDC_NO_THREADS__)
  static _Thread_local cmark_parser *s_active_parser;
#elif defined(__GNUC__) || defined(__clang__)
  static __thread cmark_parser *s_active_parser;
#else
  static cmark_parser *s_active_parser;
#endif

void cmark_streaming_set_active_parser(cmark_parser *p) {
  s_active_parser = p;
}

cmark_parser *cmark_streaming_active_parser(void) {
  return s_active_parser;
}

void cmark_streaming_state_init(struct cmark_parser_streaming_state *s) {
  memset(s, 0, sizeof(*s));
}

static void free_event_chain(cmark_mem *mem,
                              struct cmark_change_record *head) {
  while (head) {
    struct cmark_change_record *next = head->next;
    mem->free(head);
    head = next;
  }
}

static void free_pending_refs(cmark_mem *mem,
                              struct cmark_pending_ref_user *head) {
  while (head) {
    struct cmark_pending_ref_user *next = head->next;
    cmark_chunk_free(mem, &head->label);
    mem->free(head);
    head = next;
  }
}

void cmark_streaming_state_free(cmark_mem *mem,
                                struct cmark_parser_streaming_state *s) {
  free_event_chain(mem, s->events_head);
  free_pending_refs(mem, s->pending_ref_users_head);
  if (s->line_start_bytes) {
    mem->free(s->line_start_bytes);
  }
  // The dirty-blocks list is intrusive into the node tree; clearing it does
  // not require freeing per-node memory — the nodes themselves are owned by
  // the document tree and freed via cmark_node_free.
  memset(s, 0, sizeof(*s));
}

void cmark_streaming_record_line_start(cmark_parser *parser,
                                       size_t byte_offset) {
  struct cmark_parser_streaming_state *s = &parser->streaming;
  if (s->line_start_bytes_len == s->line_start_bytes_cap) {
    size_t new_cap = s->line_start_bytes_cap ? s->line_start_bytes_cap * 2 : 64;
    size_t *resized = (size_t *)parser->mem->realloc(
        s->line_start_bytes, new_cap * sizeof(size_t));
    s->line_start_bytes = resized;
    s->line_start_bytes_cap = new_cap;
  }
  s->line_start_bytes[s->line_start_bytes_len++] = byte_offset;
}

size_t cmark_streaming_byte_after_line(cmark_parser *parser, int line_number) {
  struct cmark_parser_streaming_state *s = &parser->streaming;
  if (line_number <= 0)
    return 0;
  // line k starts at line_start_bytes[k-1]; line k+1 starts at
  // line_start_bytes[k]. So "byte after line k" = line_start_bytes[k].
  if ((size_t)line_number < s->line_start_bytes_len) {
    return s->line_start_bytes[line_number];
  }
  // Line k+1 has not been recorded yet. Either we're mid-line on line k+1
  // (current_line_start_byte points to its start), or no further input has
  // arrived. current_line_start_byte tracks the start of the line currently
  // being assembled in linebuf, which is the next un-processed line.
  if (s->current_line_start_byte > 0)
    return s->current_line_start_byte;
  return s->total_consumed_bytes;
}

void cmark_streaming_recompute_frontier(cmark_parser *parser) {
  // Walk the document's first_child chain to find the longest committed-
  // non-provisional prefix. To stay O(advance-since-last-call) instead of
  // O(num-children) per line, we cache the last node we already attributed
  // to the frontier and resume from its successor.
  struct cmark_parser_streaming_state *s = &parser->streaming;
  if (!parser->root) {
    return;
  }

  cmark_node *last_committed = s->frontier_last_committed_child;
  cmark_node *child = last_committed ? last_committed->next
                                      : parser->root->first_child;
  bool advanced = false;
  cmark_node *blocker = NULL;

  while (child) {
    bool committed = !(child->flags & CMARK_NODE__OPEN) &&
                     !(child->flags & CMARK_NODE__PROVISIONAL);
    if (!committed) {
      blocker = child;
      break;
    }
    last_committed = child;
    advanced = true;
    child = child->next;
  }

  size_t new_frontier;
  if (blocker) {
    // Frontier ends just before the blocker's first byte.
    if (blocker->start_line > 0 &&
        (size_t)(blocker->start_line - 1) < s->line_start_bytes_len) {
      new_frontier = s->line_start_bytes[blocker->start_line - 1];
    } else if (last_committed) {
      new_frontier = cmark_streaming_byte_after_line(parser,
                                                     last_committed->end_line);
    } else {
      new_frontier = 0;
    }
  } else if (last_committed) {
    new_frontier = cmark_streaming_byte_after_line(parser,
                                                   last_committed->end_line);
  } else {
    new_frontier = 0;
  }

  if (advanced) {
    s->frontier_last_committed_child = last_committed;
  }
  if (new_frontier > s->commit_frontier) {
    s->commit_frontier = new_frontier;
  }
}

// Mirror of contains_inlines() in blocks.c (which is static). Kept in sync
// by hand — both must accept exactly the same set of block types.
static bool streaming_contains_inlines(cmark_node *node) {
  if (node->extension && node->extension->contains_inlines_func) {
    return node->extension->contains_inlines_func(node->extension, node) != 0;
  }
  return (node->type == CMARK_NODE_PARAGRAPH ||
          node->type == CMARK_NODE_HEADING);
}

static void free_block_children(cmark_node *block) {
  while (block->first_child) {
    // cmark_node_free unlinks from siblings/parent and frees subtree.
    cmark_node_free(block->first_child);
  }
}

void cmark_streaming_run_pending_inlines(cmark_parser *parser) {
  cmark_node *block = cmark_streaming_drain_dirty_blocks(parser);
  if (!block)
    return;

  // The inline parser consults extension special-character registries. We
  // mirror process_inlines() in blocks.c by enabling them across the batch.
  cmark_manage_extensions_special_characters(parser, true);

  while (block) {
    cmark_node *next = block->dirty_next;
    block->dirty_next = NULL;
    block->flags &= ~CMARK_NODE__INLINE_DIRTY;

    // The block may have been freed (paragraph -> empty ref-def case). It
    // may also have been morphed since it was marked dirty, in which case
    // streaming_contains_inlines reflects the *current* type.
    if (block->parent == NULL && block != parser->root) {
      // Detached. Skip.
      block = next;
      continue;
    }

    if (streaming_contains_inlines(block)) {
      free_block_children(block);
      cmark_parse_inlines(parser, block, parser->refmap, parser->options);
      block->inline_parsed_len = block->content.size;
      cmark_streaming_record(parser, CMARK_CHANGE_NODE_INLINES_REPARSED,
                             block);
    } else {
      // Non-inline-bearing block (CODE_BLOCK, HTML_BLOCK, custom literal
      // extensions). Inline reparse is meaningless here — but the consumer
      // still needs a signal that the block's raw content grew between
      // snapshots, otherwise streamed code blocks render stale.
      if (block->content.size > block->inline_parsed_len ||
          block->inline_parsed_len == 0) {
        cmark_streaming_record(parser, CMARK_CHANGE_NODE_CONTENT_UPDATED,
                               block);
      }
      block->inline_parsed_len = block->content.size;
    }

    block = next;
  }

  cmark_manage_extensions_special_characters(parser, false);
}

void cmark_streaming_record(cmark_parser *parser, cmark_change_event event,
                            cmark_node *node) {
  struct cmark_change_record *rec =
      (struct cmark_change_record *)parser->mem->calloc(
          1, sizeof(struct cmark_change_record));
  rec->event = event;
  rec->node = node;
  rec->next = NULL;

  if (parser->streaming.events_tail) {
    parser->streaming.events_tail->next = rec;
  } else {
    parser->streaming.events_head = rec;
  }
  parser->streaming.events_tail = rec;
}

void cmark_streaming_advance_consumed(cmark_parser *parser, size_t delta) {
  parser->streaming.total_consumed_bytes += delta;
}

void cmark_streaming_mark_inline_dirty(cmark_parser *parser, cmark_node *block) {
  if (!block || (block->flags & CMARK_NODE__INLINE_DIRTY))
    return;
  block->flags |= CMARK_NODE__INLINE_DIRTY;
  block->dirty_next = parser->streaming.dirty_blocks_head;
  parser->streaming.dirty_blocks_head = block;
}

cmark_node *cmark_streaming_drain_dirty_blocks(cmark_parser *parser) {
  cmark_node *head = parser->streaming.dirty_blocks_head;
  parser->streaming.dirty_blocks_head = NULL;
  return head;
}

void cmark_streaming_set_provisional(cmark_parser *parser, cmark_node *node,
                                     bool provisional) {
  if (!node)
    return;
  bool was = (node->flags & CMARK_NODE__PROVISIONAL) != 0;
  if (was == provisional)
    return;
  if (provisional) {
    node->flags |= CMARK_NODE__PROVISIONAL;
  } else {
    node->flags &= ~CMARK_NODE__PROVISIONAL;
    cmark_streaming_record(parser, CMARK_CHANGE_NODE_FINALIZED, node);
  }
}

void cmark_node_morph(cmark_parser *parser, cmark_node *node,
                      cmark_node_type new_type, bool clear_inline_dirty) {
  if (!node || node->type == (uint16_t)new_type)
    return;
  // Route through cmark_node_set_type so the union is freed correctly and a
  // RETYPED record is emitted (cmark_node_set_type consults the active
  // parser via cmark_streaming_active_parser). We temporarily ensure
  // `parser` is active in case the caller provided one we don't currently
  // see — the typical caller is parsing-internal and this is already true,
  // but doing it here makes the primitive safe to call from any context.
  cmark_parser *prev = cmark_streaming_active_parser();
  if (prev != parser)
    cmark_streaming_set_active_parser(parser);
  if (cmark_node_set_type(node, new_type) && clear_inline_dirty) {
    node->flags &= ~CMARK_NODE__INLINE_DIRTY;
    node->inline_parsed_len = 0;
  }
  if (prev != parser)
    cmark_streaming_set_active_parser(prev);
}

void cmark_streaming_add_pending_ref(cmark_parser *parser,
                                     cmark_chunk label,
                                     cmark_node *block) {
  struct cmark_pending_ref_user *u =
      (struct cmark_pending_ref_user *)parser->mem->calloc(
          1, sizeof(struct cmark_pending_ref_user));
  // Take ownership of a copy of label so the original storage may be freed.
  u->label.alloc = 1;
  u->label.len = label.len;
  u->label.data = (unsigned char *)parser->mem->calloc(1, (size_t)label.len + 1);
  if (label.len > 0)
    memcpy(u->label.data, label.data, label.len);
  u->user_block = block;
  u->next = parser->streaming.pending_ref_users_head;
  parser->streaming.pending_ref_users_head = u;
}

void cmark_streaming_resolve_pending_refs(cmark_parser *parser,
                                          cmark_chunk label) {
  // CommonMark folds case and collapses whitespace when matching link labels.
  // Compare normalized forms so [Foo Bar] resolves the def [foo  bar].
  unsigned char *norm_resolved = normalize_map_label(parser->mem, &label);
  if (norm_resolved == NULL)
    return;
  size_t norm_resolved_len = strlen((const char *)norm_resolved);

  struct cmark_pending_ref_user **pp =
      &parser->streaming.pending_ref_users_head;
  while (*pp) {
    struct cmark_pending_ref_user *u = *pp;
    unsigned char *norm_pending =
        normalize_map_label(parser->mem, &u->label);
    bool match = false;
    if (norm_pending) {
      size_t l = strlen((const char *)norm_pending);
      match = (l == norm_resolved_len) &&
              (l == 0 ||
               memcmp(norm_pending, norm_resolved, l) == 0);
      parser->mem->free(norm_pending);
    }
    if (match) {
      *pp = u->next;
      cmark_streaming_mark_inline_dirty(parser, u->user_block);
      cmark_chunk_free(parser->mem, &u->label);
      parser->mem->free(u);
    } else {
      pp = &u->next;
    }
  }
  parser->mem->free(norm_resolved);
}

// ------------------------------------------------------------------
// Public API
// ------------------------------------------------------------------

int cmark_node_is_provisional(cmark_node *node) {
  if (!node)
    return 0;
  return (node->flags & (CMARK_NODE__PROVISIONAL |
                         CMARK_NODE__INLINE_PROVISIONAL)) ? 1 : 0;
}

cmark_node *cmark_parser_snapshot(cmark_parser *parser) {
  // Drain the dirty-block list and re-parse inlines on each affected block,
  // including those still open. Unclosed delimiters in an open block fall
  // through the existing inline parser's cleanup path (last_delim /
  // last_bracket pop) which converts them to literal text — the conservative
  // interpretation. The block itself remains PROVISIONAL so consumers know
  // the trailing content may yet shift.
  cmark_parser *prev = cmark_streaming_active_parser();
  cmark_streaming_set_active_parser(parser);
  cmark_streaming_run_pending_inlines(parser);
  cmark_streaming_set_active_parser(prev);

  cmark_streaming_recompute_frontier(parser);
  parser->streaming.snapshot_generation++;
  return parser->root;
}

size_t cmark_parser_commit_frontier(cmark_parser *parser) {
  return parser->streaming.commit_frontier;
}

cmark_change_iter *cmark_parser_changes_since_last_snapshot(
    cmark_parser *parser) {
  cmark_change_iter *iter =
      (cmark_change_iter *)parser->mem->calloc(1, sizeof(*iter));
  iter->mem = parser->mem;
  iter->head = parser->streaming.events_head;
  iter->to_free = parser->streaming.events_head;
  // Detach the chain — caller is now responsible for it via _free.
  parser->streaming.events_head = NULL;
  parser->streaming.events_tail = NULL;
  return iter;
}

cmark_change_event cmark_change_iter_next(cmark_change_iter *iter,
                                         cmark_node **out_node) {
  if (!iter || !iter->head) {
    if (out_node)
      *out_node = NULL;
    return CMARK_CHANGE_NONE;
  }
  struct cmark_change_record *r = iter->head;
  iter->head = r->next;
  if (out_node)
    *out_node = r->node;
  return r->event;
}

void cmark_change_iter_free(cmark_change_iter *iter) {
  if (!iter)
    return;
  cmark_mem *mem = iter->mem;
  free_event_chain(mem, iter->to_free);
  mem->free(iter);
}
