#ifndef CMARK_PARSER_H
#define CMARK_PARSER_H

#include <stdio.h>
#include "references.h"
#include "node.h"
#include "buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_LINK_LABEL_LENGTH 1000

/* Forward declaration for the streaming event record; defined in streaming.c. */
struct cmark_change_record;

/**
 * Per-parser streaming state. Aggregated here (not allocated separately) to
 * keep cache locality with the rest of the parser. All fields are zero-init
 * compatible — a parser that never calls snapshot() pays only the storage
 * cost.
 */
struct cmark_parser_streaming_state {
  /**
   * Byte offset into the consumed input stream before which the AST is
   * committed (no future input can rewrite it). 0 at construction.
   */
  size_t commit_frontier;
  /* Total bytes accepted by feed() so far. commit_frontier <= total. */
  size_t total_consumed_bytes;
  /**
   * Byte offset where the line currently being assembled (in linebuf) started
   * in the global input stream. Equals total_consumed_bytes when there is no
   * in-progress line.
   */
  size_t current_line_start_byte;
  /**
   * Map from line number to byte offset of that line's start.
   * line_start_bytes[k-1] = byte offset where line k starts (1-indexed lines).
   * Grown lazily by S_parser_feed each time a line is processed.
   */
  size_t *line_start_bytes;
  size_t line_start_bytes_len;
  size_t line_start_bytes_cap;
  /**
   * The last document child whose subtree is fully committed in the frontier
   * computation. Subsequent recompute_frontier calls start from this node's
   * `next` rather than walking the whole prefix every time. NULL when no
   * children are yet committed. Cleared on parser_reset.
   */
  struct cmark_node *frontier_last_committed_child;
  /* Singly-linked stream of change events since last snapshot consumption. */
  struct cmark_change_record *events_head;
  struct cmark_change_record *events_tail;
  /**
   * Head of the intrusive dirty-block list (linked via cmark_node.dirty_next).
   * Blocks are appended when their content changes and drained on snapshot.
   */
  struct cmark_node *dirty_blocks_head;
  /**
   * Reverse index: link reference labels that have been seen as `[ref]`-style
   * uses but whose definitions are not yet in refmap. Linked list of
   * {label_chunk, user_block} pairs. Drained when a definition arrives, marking
   * the user block inline-dirty for re-parse.
   */
  struct cmark_pending_ref_user *pending_ref_users_head;
  /**
   * Generation counter incremented on each snapshot — used for cheap "is this
   * event record stale" checks if the consumer skips snapshots.
   */
  uint32_t snapshot_generation;
};

struct cmark_pending_ref_user {
  cmark_chunk label;
  struct cmark_node *user_block;
  struct cmark_pending_ref_user *next;
};

struct cmark_parser {
  struct cmark_mem *mem;
  /* A hashtable of urls in the current document for cross-references */
  struct cmark_map *refmap;
  /* The root node of the parser, always a CMARK_NODE_DOCUMENT */
  struct cmark_node *root;
  /* The last open block after a line is fully processed */
  struct cmark_node *current;
  /* See the documentation for cmark_parser_get_line_number() in cmark.h */
  int line_number;
  /* See the documentation for cmark_parser_get_offset() in cmark.h */
  bufsize_t offset;
  /* See the documentation for cmark_parser_get_column() in cmark.h */
  bufsize_t column;
  /* See the documentation for cmark_parser_get_first_nonspace() in cmark.h */
  bufsize_t first_nonspace;
  /* See the documentation for cmark_parser_get_first_nonspace_column() in cmark.h */
  bufsize_t first_nonspace_column;
  bufsize_t thematic_break_kill_pos;
  /* See the documentation for cmark_parser_get_indent() in cmark.h */
  int indent;
  /* See the documentation for cmark_parser_is_blank() in cmark.h */
  bool blank;
  /* See the documentation for cmark_parser_has_partially_consumed_tab() in cmark.h */
  bool partially_consumed_tab;
  /* Contains the currently processed line */
  cmark_strbuf curline;
  /* See the documentation for cmark_parser_get_last_line_length() in cmark.h */
  bufsize_t last_line_length;
  /* FIXME: not sure about the difference with curline */
  cmark_strbuf linebuf;
  /* Options set by the user, see the Options section in cmark.h */
  int options;
  bool last_buffer_ended_with_cr;
  size_t total_size;
  cmark_llist *syntax_extensions;
  cmark_llist *inline_syntax_extensions;
  cmark_ispunct_func backslash_ispunct;
  /* Streaming AST state. Always present; quiescent unless snapshot() is used. */
  struct cmark_parser_streaming_state streaming;
};

#ifdef __cplusplus
}
#endif

#endif
