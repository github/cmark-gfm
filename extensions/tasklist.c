#include "tasklist.h"
#include <parser.h>
#include <render.h>
#include <html.h>
#include "ext_scanners.h"

typedef enum {
  CMARK_TASKLIST_NOCHECKED,
  CMARK_TASKLIST_CHECKED,
} cmark_tasklist_type;

// Local constants
static const char *TYPE_STRING = "tasklist";

static const char *get_type_string(cmark_syntax_extension *extension, cmark_node *node) {
  return TYPE_STRING;
}


// Return 1 if state was set, 0 otherwise
int cmark_gfm_extensions_set_tasklist_item_checked(cmark_node *node, bool is_checked) {
  // The node has to exist, and be an extension, and actually be the right type in order to get the value.
  if (!node || !node->extension || strcmp(cmark_node_get_type_string(node), TYPE_STRING))
    return 0;

  node->as.list.checked = is_checked;
  return 1;
}

bool cmark_gfm_extensions_get_tasklist_item_checked(cmark_node *node) {
  if (!node || !node->extension || strcmp(cmark_node_get_type_string(node), TYPE_STRING))
    return false;

  if (node->as.list.checked) {
    return true;
  }
  else {
    return false;
  }
}

static bool parse_node_item_prefix(cmark_parser *parser, const char *input,
                                   cmark_node *container) {
  bool res = false;

  if (parser->indent >=
      container->as.list.marker_offset + container->as.list.padding) {
    cmark_parser_advance_offset(parser, input, container->as.list.marker_offset +
                                        container->as.list.padding,
                     true);
    res = true;
  } else if (parser->blank && container->first_child != NULL) {
    // if container->first_child is NULL, then the opening line
    // of the list item was blank after the list marker; in this
    // case, we are done with the list item.
    cmark_parser_advance_offset(parser, input, parser->first_nonspace - parser->offset,
                     false);
    res = true;
  }
  return res;
}

static int matches(cmark_syntax_extension *self, cmark_parser *parser,
                   unsigned char *input, int len,
                   cmark_node *parent_container) {
  return parse_node_item_prefix(parser, (const char*)input, parent_container);
}

static int can_contain(cmark_syntax_extension *extension, cmark_node *node,
                       cmark_node_type child_type) {
  return (node->type == CMARK_NODE_ITEM) ? 1 : 0;
}

// The spacechar class of the original tasklist scanner: a line ending after
// the marker does not make a task.
static bool is_tasklist_space(unsigned char c) {
  return c == ' ' || c == '\t' || c == '\v' || c == '\f';
}

static cmark_node *open_tasklist_item(cmark_syntax_extension *self,
                                      int indented, cmark_parser *parser,
                                      cmark_node *parent_container,
                                      unsigned char *input, int len) {
  cmark_node_type node_type = cmark_node_get_type(parent_container);
  // The marker must be on the line that opens the item: a `[ ]` that starts
  // a later line of the item is text.
  if (node_type != CMARK_NODE_ITEM ||
      parent_container->start_line != parser->line_number) {
    return NULL;
  }

  // Look for the marker where the item's content starts, not at the start of
  // the line, so an item inside a block quote (or other container) is found.
  int offset = cmark_parser_get_offset(parser);
  int marker = cmark_parser_get_first_nonspace(parser);
  if (marker + 3 >= len || input[marker] != '[' || input[marker + 2] != ']' ||
      !(input[marker + 1] == ' ' || input[marker + 1] == 'x' ||
        input[marker + 1] == 'X') ||
      !is_tasklist_space(input[marker + 3])) {
    return NULL;
  }

  cmark_node_set_syntax_extension(parent_container, self);
  cmark_parser_advance_offset(parser, (char *)input, marker - offset + 3, false);

  // The checked state comes from this item's marker only. Either an upper or
  // lower case X means the task is completed.
  parent_container->as.list.checked = input[marker + 1] != ' ';

  return NULL;
}

static void commonmark_render(cmark_syntax_extension *extension,
                              cmark_renderer *renderer, cmark_node *node,
                              cmark_event_type ev_type, int options) {
  bool entering = (ev_type == CMARK_EVENT_ENTER);
  if (entering) {
    renderer->cr(renderer);
    if (node->as.list.checked) {
      renderer->out(renderer, node, "- [x] ", false, LITERAL);
    } else {
      renderer->out(renderer, node, "- [ ] ", false, LITERAL);
    }
    cmark_strbuf_puts(renderer->prefix, "  ");
  } else {
    cmark_strbuf_truncate(renderer->prefix, renderer->prefix->size - 2);
    renderer->cr(renderer);
  }
}

static void html_render(cmark_syntax_extension *extension,
                        cmark_html_renderer *renderer, cmark_node *node,
                        cmark_event_type ev_type, int options) {
  bool entering = (ev_type == CMARK_EVENT_ENTER);
  if (entering) {
    cmark_html_render_cr(renderer->html);
    cmark_strbuf_puts(renderer->html, "<li");
    cmark_html_render_sourcepos(node, renderer->html, options);
    cmark_strbuf_putc(renderer->html, '>');
    if (node->as.list.checked) {
      cmark_strbuf_puts(renderer->html, "<input type=\"checkbox\" checked=\"\" disabled=\"\" /> ");
    } else {
      cmark_strbuf_puts(renderer->html, "<input type=\"checkbox\" disabled=\"\" /> ");
    }
  } else {
    cmark_strbuf_puts(renderer->html, "</li>\n");
  }
}

static const char *xml_attr(cmark_syntax_extension *extension,
                            cmark_node *node) {
  if (node->as.list.checked) {
    return " completed=\"true\"";
  } else {
    return " completed=\"false\"";
  }
}

cmark_syntax_extension *create_tasklist_extension(void) {
  cmark_syntax_extension *ext = cmark_syntax_extension_new("tasklist");

  cmark_syntax_extension_set_match_block_func(ext, matches);
  cmark_syntax_extension_set_get_type_string_func(ext, get_type_string);
  cmark_syntax_extension_set_open_block_func(ext, open_tasklist_item);
  cmark_syntax_extension_set_can_contain_func(ext, can_contain);
  cmark_syntax_extension_set_commonmark_render_func(ext, commonmark_render);
  cmark_syntax_extension_set_plaintext_render_func(ext, commonmark_render);
  cmark_syntax_extension_set_html_render_func(ext, html_render);
  cmark_syntax_extension_set_xml_attr_func(ext, xml_attr);

  return ext;
}
