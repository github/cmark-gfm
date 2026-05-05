#ifndef CMARK_STREAMING_H
#define CMARK_STREAMING_H

#include "cmark-gfm.h"
#include "node.h"
#include "parser.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Internal change record. Public consumers walk these via cmark_change_iter
 * declared in cmark-gfm.h.
 */
struct cmark_change_record {
  /**
   * Which kind of change this record describes. Determines how the `node`
   * field below should be interpreted.
   */
  cmark_change_event event;
  /**
   * For NODE_ADDED / NODE_RETYPED / NODE_CONTENT_UPDATED /
   * NODE_INLINES_REPARSED / NODE_FINALIZED: the affected node itself.
   * For NODE_REMOVED: the parent of the removed node (the removed node
   * pointer is no longer valid by the time the consumer sees the record).
   */
  cmark_node *node;
  /**
   * Singly-linked list pointer. Records are linked in chronological order
   * of emission; the head is the oldest unread record.
   */
  struct cmark_change_record *next;
};

/**
 * Initialize/free per-parser streaming state. Called by cmark_parser_reset
 * and cmark_parser_dispose.
 */
void cmark_streaming_state_init(struct cmark_parser_streaming_state *s);
void cmark_streaming_state_free(cmark_mem *mem,
                                struct cmark_parser_streaming_state *s);

/**
 * Set/get the parser that is currently consuming input on this thread.
 * Used so that mutation primitives like cmark_node_set_type can emit
 * change events without taking the parser as a parameter (which would
 * change public ABIs). NULL when no parser is active.
 *
 * Implemented as a thread-local: each thread may drive its own parser.
 */
void cmark_streaming_set_active_parser(cmark_parser *p);
cmark_parser *cmark_streaming_active_parser(void);

/**
 * Append `byte_offset` to the parser's line->byte map. Called by
 * S_parser_feed at the moment a line is about to be processed. The k-th
 * call (0-indexed) records the start byte of line k+1.
 */
void cmark_streaming_record_line_start(cmark_parser *parser,
                                       size_t byte_offset);

/**
 * Returns the byte offset just after the end of `line_number` (1-indexed) —
 * i.e. the byte where line (line_number + 1) starts, or the byte offset of
 * the in-progress line if line_number+1 has not yet been processed, or
 * total_consumed_bytes if no further input has been observed.
 */
size_t cmark_streaming_byte_after_line(cmark_parser *parser, int line_number);

/**
 * Recompute parser->streaming.commit_frontier by scanning the document's
 * children for the longest committed-non-provisional prefix. Idempotent.
 */
void cmark_streaming_recompute_frontier(cmark_parser *parser);

/**
 * Drain the dirty-block list, running incremental inline parsing on each
 * block whose `contains_inlines` predicate is true. For each such block:
 *   - existing inline children are freed,
 *   - cmark_parse_inlines is called against the block's content + refmap,
 *   - INLINE_DIRTY is cleared, inline_parsed_len is updated to content size,
 *   - a CMARK_CHANGE_NODE_INLINES_REPARSED record is emitted.
 *
 * Called by cmark_parser_snapshot. The same machinery also handles the
 * finalize-time pass — process_inlines() in blocks.c routes through here so
 * a tree that has already been incrementally parsed is not re-parsed.
 */
void cmark_streaming_run_pending_inlines(cmark_parser *parser);

/**
 * Record an event in the streaming event stream. node may be NULL for
 * NODE_REMOVED if no parent is meaningful (top-level removal). Allocates
 * from parser->mem.
 */
void cmark_streaming_record(cmark_parser *parser, cmark_change_event event,
                            cmark_node *node);

/**
 * Advance total_consumed_bytes by `delta`. Called from S_parser_feed.
 */
void cmark_streaming_advance_consumed(cmark_parser *parser, size_t delta);

/**
 * Mark a block's inline content as dirty, queuing it for re-parse on the
 * next snapshot. Idempotent.
 */
void cmark_streaming_mark_inline_dirty(cmark_parser *parser, cmark_node *block);

/**
 * Drain the dirty-block list. Returns the head; caller must walk via
 * dirty_next and reset each node's CMARK_NODE__INLINE_DIRTY flag and
 * dirty_next pointer.
 */
cmark_node *cmark_streaming_drain_dirty_blocks(cmark_parser *parser);

/**
 * Set/clear the provisional flag on a node, emitting a NODE_FINALIZED event
 * when clearing.
 */
void cmark_streaming_set_provisional(cmark_parser *parser, cmark_node *node,
                                     bool provisional);

/**
 * In-place node rewrite. Replaces `node->type` (and optionally its `as`
 * payload) without changing the node's pointer identity, parent, or sibling
 * links. Children may be edited by the caller before/after; this primitive
 * does not touch them. The node's flags are preserved except as noted by
 * `clear_inline_dirty` (set it to true to drop any pending inline state when
 * the new type does not contain inlines).
 *
 * Emits a CMARK_CHANGE_NODE_RETYPED record.
 */
void cmark_node_morph(cmark_parser *parser, cmark_node *node,
                      cmark_node_type new_type, bool clear_inline_dirty);

/**
 * Add a pending [ref] user — when `block` contains an unresolved reference
 * of `label`, the block must be re-inline-parsed once the definition arrives.
 */
void cmark_streaming_add_pending_ref(cmark_parser *parser,
                                     cmark_chunk label,
                                     cmark_node *block);
/**
 * Notify that a definition for `label` has arrived. All pending users are
 * removed and marked inline-dirty.
 */
void cmark_streaming_resolve_pending_refs(cmark_parser *parser,
                                          cmark_chunk label);

#ifdef __cplusplus
}
#endif

#endif
