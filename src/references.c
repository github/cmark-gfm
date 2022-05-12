#include "references.h"
#include "chunk.h"
#include "cmark-gfm.h"
#include "inlines.h"
#include "parser.h"

static void reference_free(cmark_map *map, cmark_map_entry *_ref) {
  cmark_reference *ref = (cmark_reference *)_ref;
  cmark_mem *mem = map->mem;
  if (ref != NULL) {
    mem->free(ref->entry.label);
    cmark_chunk_free(mem, &ref->url);
    cmark_chunk_free(mem, &ref->title);
    mem->free(ref);
  }
}

void cmark_reference_create(cmark_map *map, cmark_chunk *label,
                            cmark_chunk *url, cmark_chunk *title,
                            cmark_node *node) {
  cmark_reference *ref;
  unsigned char *reflabel = normalize_map_label(map->mem, label);

  /* empty reference name, or composed from only whitespace */
  if (reflabel == NULL)
    return;

  assert(map->sorted == NULL);

  ref = (cmark_reference *)map->mem->calloc(1, sizeof(*ref));
  ref->entry.label = reflabel;
  ref->url = cmark_clean_url(map->mem, url);
  ref->title = cmark_clean_title(map->mem, title);
  ref->entry.age = map->size;
  ref->entry.next = map->refs;

  if (node) {
    ref->start_line = node->start_line;
    ref->end_line = node->end_line;
    ref->start_column = node->start_column;
    ref->end_column = node->end_column;
  }

  map->refs = (cmark_map_entry *)ref;
  map->size++;
}

cmark_map *cmark_reference_map_new(cmark_mem *mem) {
  return cmark_map_new(mem, reference_free);
}
