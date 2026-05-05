#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CMARK_NO_SHORT_NAMES
#include "cmark-gfm.h"
#include "node.h"
#include "../extensions/cmark-gfm-core-extensions.h"

#include "harness.h"
#include "cplusplus.h"

#define UTF8_REPL "\xEF\xBF\xBD"

static const cmark_node_type node_types[] = {
    CMARK_NODE_DOCUMENT,  CMARK_NODE_BLOCK_QUOTE, CMARK_NODE_LIST,
    CMARK_NODE_ITEM,      CMARK_NODE_CODE_BLOCK,  CMARK_NODE_HTML_BLOCK,
    CMARK_NODE_PARAGRAPH, CMARK_NODE_HEADING,     CMARK_NODE_THEMATIC_BREAK,
    CMARK_NODE_TEXT,      CMARK_NODE_SOFTBREAK,   CMARK_NODE_LINEBREAK,
    CMARK_NODE_CODE,      CMARK_NODE_HTML_INLINE, CMARK_NODE_EMPH,
    CMARK_NODE_STRONG,    CMARK_NODE_LINK,        CMARK_NODE_IMAGE};
static const int num_node_types = sizeof(node_types) / sizeof(*node_types);

static void test_md_to_html(test_batch_runner *runner, const char *markdown,
                            const char *expected_html, const char *msg);

static void test_content(test_batch_runner *runner, cmark_node_type type,
                         unsigned int *allowed_content);

static void test_char(test_batch_runner *runner, int valid, const char *utf8,
                      const char *msg);

static void test_incomplete_char(test_batch_runner *runner, const char *utf8,
                                 const char *msg);

static void test_continuation_byte(test_batch_runner *runner, const char *utf8);

static void version(test_batch_runner *runner) {
  INT_EQ(runner, cmark_version(), CMARK_GFM_VERSION, "cmark_version");
  STR_EQ(runner, cmark_version_string(), CMARK_GFM_VERSION_STRING,
         "cmark_version_string");
}

static void constructor(test_batch_runner *runner) {
  for (int i = 0; i < num_node_types; ++i) {
    cmark_node_type type = node_types[i];
    cmark_node *node = cmark_node_new(type);
    OK(runner, node != NULL, "new type %d", type);
    INT_EQ(runner, cmark_node_get_type(node), type, "get_type %d", type);

    switch (node->type) {
    case CMARK_NODE_HEADING:
      INT_EQ(runner, cmark_node_get_heading_level(node), 1,
             "default heading level is 1");
      node->as.heading.level = 1;
      break;

    case CMARK_NODE_LIST:
      INT_EQ(runner, cmark_node_get_list_type(node), CMARK_BULLET_LIST,
             "default is list type is bullet");
      INT_EQ(runner, cmark_node_get_list_delim(node), CMARK_NO_DELIM,
             "default is list delim is NO_DELIM");
      INT_EQ(runner, cmark_node_get_list_start(node), 0,
             "default is list start is 0");
      INT_EQ(runner, cmark_node_get_list_tight(node), 0,
             "default is list is loose");
      break;

    default:
      break;
    }

    cmark_node_free(node);
  }
}

static void accessors(test_batch_runner *runner) {
  static const char markdown[] = "## Header\n"
                                 "\n"
                                 "* Item 1\n"
                                 "* Item 2\n"
                                 "\n"
                                 "2. Item 1\n"
                                 "\n"
                                 "3. Item 2\n"
                                 "\n"
                                 "``` lang\n"
                                 "fenced\n"
                                 "```\n"
                                 "    code\n"
                                 "\n"
                                 "<div>html</div>\n"
                                 "\n"
                                 "[link](url 'title')\n";

  cmark_node *doc =
      cmark_parse_document(markdown, sizeof(markdown) - 1, CMARK_OPT_DEFAULT);

  // Getters

  cmark_node *heading = cmark_node_first_child(doc);
  INT_EQ(runner, cmark_node_get_heading_level(heading), 2, "get_heading_level");

  cmark_node *bullet_list = cmark_node_next(heading);
  INT_EQ(runner, cmark_node_get_list_type(bullet_list), CMARK_BULLET_LIST,
         "get_list_type bullet");
  INT_EQ(runner, cmark_node_get_list_tight(bullet_list), 1,
         "get_list_tight tight");

  cmark_node *ordered_list = cmark_node_next(bullet_list);
  INT_EQ(runner, cmark_node_get_list_type(ordered_list), CMARK_ORDERED_LIST,
         "get_list_type ordered");
  INT_EQ(runner, cmark_node_get_list_delim(ordered_list), CMARK_PERIOD_DELIM,
         "get_list_delim ordered");
  INT_EQ(runner, cmark_node_get_list_start(ordered_list), 2, "get_list_start");
  INT_EQ(runner, cmark_node_get_list_tight(ordered_list), 0,
         "get_list_tight loose");

  cmark_node *fenced = cmark_node_next(ordered_list);
  STR_EQ(runner, cmark_node_get_literal(fenced), "fenced\n",
         "get_literal fenced code");
  STR_EQ(runner, cmark_node_get_fence_info(fenced), "lang", "get_fence_info");

  cmark_node *code = cmark_node_next(fenced);
  STR_EQ(runner, cmark_node_get_literal(code), "code\n",
         "get_literal indented code");

  cmark_node *html = cmark_node_next(code);
  STR_EQ(runner, cmark_node_get_literal(html), "<div>html</div>\n",
         "get_literal html");

  cmark_node *paragraph = cmark_node_next(html);
  INT_EQ(runner, cmark_node_get_start_line(paragraph), 17, "get_start_line");
  INT_EQ(runner, cmark_node_get_start_column(paragraph), 1, "get_start_column");
  INT_EQ(runner, cmark_node_get_end_line(paragraph), 17, "get_end_line");

  cmark_node *link = cmark_node_first_child(paragraph);
  STR_EQ(runner, cmark_node_get_url(link), "url", "get_url");
  STR_EQ(runner, cmark_node_get_title(link), "title", "get_title");

  cmark_node *string = cmark_node_first_child(link);
  STR_EQ(runner, cmark_node_get_literal(string), "link", "get_literal string");

  // Setters

  OK(runner, cmark_node_set_heading_level(heading, 3), "set_heading_level");

  OK(runner, cmark_node_set_list_type(bullet_list, CMARK_ORDERED_LIST),
     "set_list_type ordered");
  OK(runner, cmark_node_set_list_delim(bullet_list, CMARK_PAREN_DELIM),
     "set_list_delim paren");
  OK(runner, cmark_node_set_list_start(bullet_list, 3), "set_list_start");
  OK(runner, cmark_node_set_list_tight(bullet_list, 0), "set_list_tight loose");

  OK(runner, cmark_node_set_list_type(ordered_list, CMARK_BULLET_LIST),
     "set_list_type bullet");
  OK(runner, cmark_node_set_list_tight(ordered_list, 1),
     "set_list_tight tight");

  OK(runner, cmark_node_set_literal(code, "CODE\n"),
     "set_literal indented code");

  OK(runner, cmark_node_set_literal(fenced, "FENCED\n"),
     "set_literal fenced code");
  OK(runner, cmark_node_set_fence_info(fenced, "LANG"), "set_fence_info");

  OK(runner, cmark_node_set_literal(html, "<div>HTML</div>\n"),
     "set_literal html");

  OK(runner, cmark_node_set_url(link, "URL"), "set_url");
  OK(runner, cmark_node_set_title(link, "TITLE"), "set_title");

  OK(runner, cmark_node_set_literal(string, "prefix-LINK"),
     "set_literal string");

  // Set literal to suffix of itself (issue #139).
  const char *literal = cmark_node_get_literal(string);
  OK(runner, cmark_node_set_literal(string, literal + sizeof("prefix")),
     "set_literal suffix");

  char *rendered_html = cmark_render_html(doc, CMARK_OPT_DEFAULT | CMARK_OPT_UNSAFE, NULL);
  static const char expected_html[] =
      "<h3>Header</h3>\n"
      "<ol start=\"3\">\n"
      "<li>\n"
      "<p>Item 1</p>\n"
      "</li>\n"
      "<li>\n"
      "<p>Item 2</p>\n"
      "</li>\n"
      "</ol>\n"
      "<ul>\n"
      "<li>Item 1</li>\n"
      "<li>Item 2</li>\n"
      "</ul>\n"
      "<pre><code class=\"language-LANG\">FENCED\n"
      "</code></pre>\n"
      "<pre><code>CODE\n"
      "</code></pre>\n"
      "<div>HTML</div>\n"
      "<p><a href=\"URL\" title=\"TITLE\">LINK</a></p>\n";
  STR_EQ(runner, rendered_html, expected_html, "setters work");
  free(rendered_html);

  // Getter errors

  INT_EQ(runner, cmark_node_get_heading_level(bullet_list), 0,
         "get_heading_level error");
  INT_EQ(runner, cmark_node_get_list_type(heading), CMARK_NO_LIST,
         "get_list_type error");
  INT_EQ(runner, cmark_node_get_list_start(code), 0, "get_list_start error");
  INT_EQ(runner, cmark_node_get_list_tight(fenced), 0, "get_list_tight error");
  OK(runner, cmark_node_get_literal(ordered_list) == NULL, "get_literal error");
  OK(runner, cmark_node_get_fence_info(paragraph) == NULL,
     "get_fence_info error");
  OK(runner, cmark_node_get_url(html) == NULL, "get_url error");
  OK(runner, cmark_node_get_title(heading) == NULL, "get_title error");

  // Setter errors

  OK(runner, !cmark_node_set_heading_level(bullet_list, 3),
     "set_heading_level error");
  OK(runner, !cmark_node_set_list_type(heading, CMARK_ORDERED_LIST),
     "set_list_type error");
  OK(runner, !cmark_node_set_list_start(code, 3), "set_list_start error");
  OK(runner, !cmark_node_set_list_tight(fenced, 0), "set_list_tight error");
  OK(runner, !cmark_node_set_literal(ordered_list, "content\n"),
     "set_literal error");
  OK(runner, !cmark_node_set_fence_info(paragraph, "lang"),
     "set_fence_info error");
  OK(runner, !cmark_node_set_url(html, "url"), "set_url error");
  OK(runner, !cmark_node_set_title(heading, "title"), "set_title error");

  OK(runner, !cmark_node_set_heading_level(heading, 0),
     "set_heading_level too small");
  OK(runner, !cmark_node_set_heading_level(heading, 7),
     "set_heading_level too large");
  OK(runner, !cmark_node_set_list_type(bullet_list, CMARK_NO_LIST),
     "set_list_type invalid");
  OK(runner, !cmark_node_set_list_start(bullet_list, -1),
     "set_list_start negative");

  cmark_node_free(doc);
}

static void node_check(test_batch_runner *runner) {
  // Construct an incomplete tree.
  cmark_node *doc = cmark_node_new(CMARK_NODE_DOCUMENT);
  cmark_node *p1 = cmark_node_new(CMARK_NODE_PARAGRAPH);
  cmark_node *p2 = cmark_node_new(CMARK_NODE_PARAGRAPH);
  doc->first_child = p1;
  p1->next = p2;

  INT_EQ(runner, cmark_node_check(doc, NULL), 4, "node_check works");
  INT_EQ(runner, cmark_node_check(doc, NULL), 0, "node_check fixes tree");

  cmark_node_free(doc);
}

static void iterator(test_batch_runner *runner) {
  cmark_node *doc = cmark_parse_document("> a *b*\n\nc", 10, CMARK_OPT_DEFAULT);
  int parnodes = 0;
  cmark_event_type ev_type;
  cmark_iter *iter = cmark_iter_new(doc);
  cmark_node *cur;

  while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
    cur = cmark_iter_get_node(iter);
    if (cur->type == CMARK_NODE_PARAGRAPH && ev_type == CMARK_EVENT_ENTER) {
      parnodes += 1;
    }
  }
  INT_EQ(runner, parnodes, 2, "iterate correctly counts paragraphs");

  cmark_iter_free(iter);
  cmark_node_free(doc);
}

static void iterator_delete(test_batch_runner *runner) {
  static const char md[] = "a *b* c\n"
                           "\n"
                           "* item1\n"
                           "* item2\n"
                           "\n"
                           "a `b` c\n"
                           "\n"
                           "* item1\n"
                           "* item2\n";
  cmark_node *doc = cmark_parse_document(md, sizeof(md) - 1, CMARK_OPT_DEFAULT);
  cmark_iter *iter = cmark_iter_new(doc);
  cmark_event_type ev_type;

  while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
    cmark_node *node = cmark_iter_get_node(iter);
    // Delete list, emph, and code nodes.
    if ((ev_type == CMARK_EVENT_EXIT && node->type == CMARK_NODE_LIST) ||
        (ev_type == CMARK_EVENT_EXIT && node->type == CMARK_NODE_EMPH) ||
        (ev_type == CMARK_EVENT_ENTER && node->type == CMARK_NODE_CODE)) {
      cmark_node_free(node);
    }
  }

  char *html = cmark_render_html(doc, CMARK_OPT_DEFAULT, NULL);
  static const char expected[] = "<p>a  c</p>\n"
                                 "<p>a  c</p>\n";
  STR_EQ(runner, html, expected, "iterate and delete nodes");

  free(html);
  cmark_iter_free(iter);
  cmark_node_free(doc);
}

static void create_tree(test_batch_runner *runner) {
  char *html;
  cmark_node *doc = cmark_node_new(CMARK_NODE_DOCUMENT);

  cmark_node *p = cmark_node_new(CMARK_NODE_PARAGRAPH);
  OK(runner, !cmark_node_insert_before(doc, p), "insert before root fails");
  OK(runner, !cmark_node_insert_after(doc, p), "insert after root fails");
  OK(runner, cmark_node_append_child(doc, p), "append1");
  INT_EQ(runner, cmark_node_check(doc, NULL), 0, "append1 consistent");
  OK(runner, cmark_node_parent(p) == doc, "node_parent");

  cmark_node *emph = cmark_node_new(CMARK_NODE_EMPH);
  OK(runner, cmark_node_prepend_child(p, emph), "prepend1");
  INT_EQ(runner, cmark_node_check(doc, NULL), 0, "prepend1 consistent");

  cmark_node *str1 = cmark_node_new(CMARK_NODE_TEXT);
  cmark_node_set_literal(str1, "Hello, ");
  OK(runner, cmark_node_prepend_child(p, str1), "prepend2");
  INT_EQ(runner, cmark_node_check(doc, NULL), 0, "prepend2 consistent");

  cmark_node *str3 = cmark_node_new(CMARK_NODE_TEXT);
  cmark_node_set_literal(str3, "!");
  OK(runner, cmark_node_append_child(p, str3), "append2");
  INT_EQ(runner, cmark_node_check(doc, NULL), 0, "append2 consistent");

  cmark_node *str2 = cmark_node_new(CMARK_NODE_TEXT);
  cmark_node_set_literal(str2, "world");
  OK(runner, cmark_node_append_child(emph, str2), "append3");
  INT_EQ(runner, cmark_node_check(doc, NULL), 0, "append3 consistent");

  html = cmark_render_html(doc, CMARK_OPT_DEFAULT, NULL);
  STR_EQ(runner, html, "<p>Hello, <em>world</em>!</p>\n", "render_html");
  free(html);

  OK(runner, cmark_node_insert_before(str1, str3), "ins before1");
  INT_EQ(runner, cmark_node_check(doc, NULL), 0, "ins before1 consistent");
  // 31e
  OK(runner, cmark_node_first_child(p) == str3, "ins before1 works");

  OK(runner, cmark_node_insert_before(str1, emph), "ins before2");
  INT_EQ(runner, cmark_node_check(doc, NULL), 0, "ins before2 consistent");
  // 3e1
  OK(runner, cmark_node_last_child(p) == str1, "ins before2 works");

  OK(runner, cmark_node_insert_after(str1, str3), "ins after1");
  INT_EQ(runner, cmark_node_check(doc, NULL), 0, "ins after1 consistent");
  // e13
  OK(runner, cmark_node_next(str1) == str3, "ins after1 works");

  OK(runner, cmark_node_insert_after(str1, emph), "ins after2");
  INT_EQ(runner, cmark_node_check(doc, NULL), 0, "ins after2 consistent");
  // 1e3
  OK(runner, cmark_node_previous(emph) == str1, "ins after2 works");

  cmark_node *str4 = cmark_node_new(CMARK_NODE_TEXT);
  cmark_node_set_literal(str4, "brzz");
  OK(runner, cmark_node_replace(str1, str4), "replace");
  // The replaced node is not freed
  cmark_node_free(str1);

  INT_EQ(runner, cmark_node_check(doc, NULL), 0, "replace consistent");
  OK(runner, cmark_node_previous(emph) == str4, "replace works");
  INT_EQ(runner, cmark_node_replace(p, str4), 0, "replace str for p fails");

  cmark_node_unlink(emph);

  html = cmark_render_html(doc, CMARK_OPT_DEFAULT, NULL);
  STR_EQ(runner, html, "<p>brzz!</p>\n", "render_html after shuffling");
  free(html);

  cmark_node_free(doc);

  // TODO: Test that the contents of an unlinked inline are valid
  // after the parent block was destroyed. This doesn't work so far.
  cmark_node_free(emph);
}

static void custom_nodes(test_batch_runner *runner) {
  char *html;
  char *man;
  cmark_node *doc = cmark_node_new(CMARK_NODE_DOCUMENT);
  cmark_node *p = cmark_node_new(CMARK_NODE_PARAGRAPH);
  cmark_node_append_child(doc, p);
  cmark_node *ci = cmark_node_new(CMARK_NODE_CUSTOM_INLINE);
  cmark_node *str1 = cmark_node_new(CMARK_NODE_TEXT);
  cmark_node_set_literal(str1, "Hello");
  OK(runner, cmark_node_append_child(ci, str1), "append1");
  OK(runner, cmark_node_set_on_enter(ci, "<ON ENTER|"), "set_on_enter");
  OK(runner, cmark_node_set_on_exit(ci, "|ON EXIT>"), "set_on_exit");
  STR_EQ(runner, cmark_node_get_on_enter(ci), "<ON ENTER|", "get_on_enter");
  STR_EQ(runner, cmark_node_get_on_exit(ci), "|ON EXIT>", "get_on_exit");
  cmark_node_append_child(p, ci);
  cmark_node *cb = cmark_node_new(CMARK_NODE_CUSTOM_BLOCK);
  cmark_node_set_on_enter(cb, "<on enter|");
  // leave on_exit unset
  STR_EQ(runner, cmark_node_get_on_exit(cb), "", "get_on_exit (empty)");
  cmark_node_append_child(doc, cb);

  html = cmark_render_html(doc, CMARK_OPT_DEFAULT, NULL);
  STR_EQ(runner, html, "<p><ON ENTER|Hello|ON EXIT></p>\n<on enter|\n",
         "render_html");
  free(html);

  man = cmark_render_man(doc, CMARK_OPT_DEFAULT, 0);
  STR_EQ(runner, man, ".PP\n<ON ENTER|Hello|ON EXIT>\n<on enter|\n",
         "render_man");
  free(man);

  cmark_node_free(doc);
}

void hierarchy(test_batch_runner *runner) {
  cmark_node *bquote1 = cmark_node_new(CMARK_NODE_BLOCK_QUOTE);
  cmark_node *bquote2 = cmark_node_new(CMARK_NODE_BLOCK_QUOTE);
  cmark_node *bquote3 = cmark_node_new(CMARK_NODE_BLOCK_QUOTE);

  OK(runner, cmark_node_append_child(bquote1, bquote2), "append bquote2");
  OK(runner, cmark_node_append_child(bquote2, bquote3), "append bquote3");
  OK(runner, !cmark_node_append_child(bquote3, bquote3),
     "adding a node as child of itself fails");
  OK(runner, !cmark_node_append_child(bquote3, bquote1),
     "adding a parent as child fails");

  cmark_node_free(bquote1);

  unsigned int list_item_flag[] = {CMARK_NODE_ITEM, 0};
  unsigned int top_level_blocks[] = {
    CMARK_NODE_BLOCK_QUOTE, CMARK_NODE_LIST,
    CMARK_NODE_CODE_BLOCK, CMARK_NODE_HTML_BLOCK,
    CMARK_NODE_PARAGRAPH, CMARK_NODE_HEADING,
    CMARK_NODE_THEMATIC_BREAK, 0};
  unsigned int all_inlines[] = {
    CMARK_NODE_TEXT, CMARK_NODE_SOFTBREAK,
    CMARK_NODE_LINEBREAK, CMARK_NODE_CODE,
    CMARK_NODE_HTML_INLINE, CMARK_NODE_EMPH,
    CMARK_NODE_STRONG, CMARK_NODE_LINK,
    CMARK_NODE_IMAGE, 0};

  test_content(runner, CMARK_NODE_DOCUMENT, top_level_blocks);
  test_content(runner, CMARK_NODE_BLOCK_QUOTE, top_level_blocks);
  test_content(runner, CMARK_NODE_LIST, list_item_flag);
  test_content(runner, CMARK_NODE_ITEM, top_level_blocks);
  test_content(runner, CMARK_NODE_CODE_BLOCK, 0);
  test_content(runner, CMARK_NODE_HTML_BLOCK, 0);
  test_content(runner, CMARK_NODE_PARAGRAPH, all_inlines);
  test_content(runner, CMARK_NODE_HEADING, all_inlines);
  test_content(runner, CMARK_NODE_THEMATIC_BREAK, 0);
  test_content(runner, CMARK_NODE_TEXT, 0);
  test_content(runner, CMARK_NODE_SOFTBREAK, 0);
  test_content(runner, CMARK_NODE_LINEBREAK, 0);
  test_content(runner, CMARK_NODE_CODE, 0);
  test_content(runner, CMARK_NODE_HTML_INLINE, 0);
  test_content(runner, CMARK_NODE_EMPH, all_inlines);
  test_content(runner, CMARK_NODE_STRONG, all_inlines);
  test_content(runner, CMARK_NODE_LINK, all_inlines);
  test_content(runner, CMARK_NODE_IMAGE, all_inlines);
}

static void test_content(test_batch_runner *runner, cmark_node_type type,
                         unsigned int *allowed_content) {
  cmark_node *node = cmark_node_new(type);

  for (int i = 0; i < num_node_types; ++i) {
    cmark_node_type child_type = node_types[i];
    cmark_node *child = cmark_node_new(child_type);

    int got = cmark_node_append_child(node, child);
    int expected = 0;
    if (allowed_content)
        for (unsigned int *p = allowed_content; *p; ++p)
            expected |= *p == (unsigned int)child_type;

    INT_EQ(runner, got, expected, "add %d as child of %d", child_type, type);

    cmark_node_free(child);
  }

  cmark_node_free(node);
}

static void parser(test_batch_runner *runner) {
  test_md_to_html(runner, "No newline", "<p>No newline</p>\n",
                  "document without trailing newline");
}

static void render_html(test_batch_runner *runner) {
  char *html;

  static const char markdown[] = "foo *bar*\n"
                                 "\n"
                                 "paragraph 2\n";
  cmark_node *doc =
      cmark_parse_document(markdown, sizeof(markdown) - 1, CMARK_OPT_DEFAULT);

  cmark_node *paragraph = cmark_node_first_child(doc);
  html = cmark_render_html(paragraph, CMARK_OPT_DEFAULT, NULL);
  STR_EQ(runner, html, "<p>foo <em>bar</em></p>\n", "render single paragraph");
  free(html);

  cmark_node *string = cmark_node_first_child(paragraph);
  html = cmark_render_html(string, CMARK_OPT_DEFAULT, NULL);
  STR_EQ(runner, html, "foo ", "render single inline");
  free(html);

  cmark_node *emph = cmark_node_next(string);
  html = cmark_render_html(emph, CMARK_OPT_DEFAULT, NULL);
  STR_EQ(runner, html, "<em>bar</em>", "render inline with children");
  free(html);

  cmark_node_free(doc);
}

static void render_xml(test_batch_runner *runner) {
  char *xml;

  static const char markdown[] = "foo *bar*\n"
                                 "\n"
                                 "paragraph 2\n"
                                 "\n"
                                 "```\ncode\n```\n";
  cmark_node *doc =
      cmark_parse_document(markdown, sizeof(markdown) - 1, CMARK_OPT_DEFAULT);

  xml = cmark_render_xml(doc, CMARK_OPT_DEFAULT);
  STR_EQ(runner, xml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
                      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
                      "  <paragraph>\n"
                      "    <text xml:space=\"preserve\">foo </text>\n"
                      "    <emph>\n"
                      "      <text xml:space=\"preserve\">bar</text>\n"
                      "    </emph>\n"
                      "  </paragraph>\n"
                      "  <paragraph>\n"
                      "    <text xml:space=\"preserve\">paragraph 2</text>\n"
                      "  </paragraph>\n"
                      "  <code_block xml:space=\"preserve\">code\n"
                      "</code_block>\n"
                      "</document>\n",
         "render document");
  free(xml);
  cmark_node *paragraph = cmark_node_first_child(doc);
  xml = cmark_render_xml(paragraph, CMARK_OPT_DEFAULT | CMARK_OPT_SOURCEPOS);
  STR_EQ(runner, xml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
                      "<paragraph sourcepos=\"1:1-1:9\">\n"
                      "  <text sourcepos=\"1:1-1:4\" xml:space=\"preserve\">foo </text>\n"
                      "  <emph sourcepos=\"1:5-1:9\">\n"
                      "    <text sourcepos=\"1:6-1:8\" xml:space=\"preserve\">bar</text>\n"
                      "  </emph>\n"
                      "</paragraph>\n",
         "render first paragraph with source pos");
  free(xml);
  cmark_node_free(doc);
}

static void render_man(test_batch_runner *runner) {
  char *man;

  static const char markdown[] = "foo *bar*\n"
                                 "\n"
                                 "- Lorem ipsum dolor sit amet,\n"
                                 "  consectetur adipiscing elit,\n"
                                 "- sed do eiusmod tempor incididunt\n"
                                 "  ut labore et dolore magna aliqua.\n";
  cmark_node *doc =
      cmark_parse_document(markdown, sizeof(markdown) - 1, CMARK_OPT_DEFAULT);

  man = cmark_render_man(doc, CMARK_OPT_DEFAULT, 20);
  STR_EQ(runner, man, ".PP\n"
                      "foo \\f[I]bar\\f[]\n"
                      ".IP \\[bu] 2\n"
                      "Lorem ipsum dolor\n"
                      "sit amet,\n"
                      "consectetur\n"
                      "adipiscing elit,\n"
                      ".IP \\[bu] 2\n"
                      "sed do eiusmod\n"
                      "tempor incididunt ut\n"
                      "labore et dolore\n"
                      "magna aliqua.\n",
         "render document with wrapping");
  free(man);
  man = cmark_render_man(doc, CMARK_OPT_DEFAULT, 0);
  STR_EQ(runner, man, ".PP\n"
                      "foo \\f[I]bar\\f[]\n"
                      ".IP \\[bu] 2\n"
                      "Lorem ipsum dolor sit amet,\n"
                      "consectetur adipiscing elit,\n"
                      ".IP \\[bu] 2\n"
                      "sed do eiusmod tempor incididunt\n"
                      "ut labore et dolore magna aliqua.\n",
         "render document without wrapping");
  free(man);
  cmark_node_free(doc);
}

static void render_latex(test_batch_runner *runner) {
  char *latex;

  static const char markdown[] = "foo *bar* $%\n"
                                 "\n"
                                 "- Lorem ipsum dolor sit amet,\n"
                                 "  consectetur adipiscing elit,\n"
                                 "- sed do eiusmod tempor incididunt\n"
                                 "  ut labore et dolore magna aliqua.\n";
  cmark_node *doc =
      cmark_parse_document(markdown, sizeof(markdown) - 1, CMARK_OPT_DEFAULT);

  latex = cmark_render_latex(doc, CMARK_OPT_DEFAULT, 20);
  STR_EQ(runner, latex, "foo \\emph{bar} \\$\\%\n"
                        "\n"
                        "\\begin{itemize}\n"
                        "\\item Lorem ipsum\n"
                        "dolor sit amet,\n"
                        "consectetur\n"
                        "adipiscing elit,\n"
                        "\n"
                        "\\item sed do eiusmod\n"
                        "tempor incididunt ut\n"
                        "labore et dolore\n"
                        "magna aliqua.\n"
                        "\n"
                        "\\end{itemize}\n",
         "render document with wrapping");
  free(latex);
  latex = cmark_render_latex(doc, CMARK_OPT_DEFAULT, 0);
  STR_EQ(runner, latex, "foo \\emph{bar} \\$\\%\n"
                        "\n"
                        "\\begin{itemize}\n"
                        "\\item Lorem ipsum dolor sit amet,\n"
                        "consectetur adipiscing elit,\n"
                        "\n"
                        "\\item sed do eiusmod tempor incididunt\n"
                        "ut labore et dolore magna aliqua.\n"
                        "\n"
                        "\\end{itemize}\n",
         "render document without wrapping");
  free(latex);
  cmark_node_free(doc);
}

static void render_commonmark(test_batch_runner *runner) {
  char *commonmark;

  static const char markdown[] = "> \\- foo *bar* \\*bar\\*\n"
                                 "\n"
                                 "- Lorem ipsum dolor sit amet,\n"
                                 "  consectetur adipiscing elit,\n"
                                 "- sed do eiusmod tempor incididunt\n"
                                 "  ut labore et dolore magna aliqua.\n";
  cmark_node *doc =
      cmark_parse_document(markdown, sizeof(markdown) - 1, CMARK_OPT_DEFAULT);

  commonmark = cmark_render_commonmark(doc, CMARK_OPT_DEFAULT, 26);
  STR_EQ(runner, commonmark, "> \\- foo *bar* \\*bar\\*\n"
                             "\n"
                             "  - Lorem ipsum dolor sit\n"
                             "    amet, consectetur\n"
                             "    adipiscing elit,\n"
                             "  - sed do eiusmod tempor\n"
                             "    incididunt ut labore\n"
                             "    et dolore magna\n"
                             "    aliqua.\n",
         "render document with wrapping");
  free(commonmark);
  commonmark = cmark_render_commonmark(doc, CMARK_OPT_DEFAULT, 0);
  STR_EQ(runner, commonmark, "> \\- foo *bar* \\*bar\\*\n"
                             "\n"
                             "  - Lorem ipsum dolor sit amet,\n"
                             "    consectetur adipiscing elit,\n"
                             "  - sed do eiusmod tempor incididunt\n"
                             "    ut labore et dolore magna aliqua.\n",
         "render document without wrapping");
  free(commonmark);

  cmark_node *text = cmark_node_new(CMARK_NODE_TEXT);
  cmark_node_set_literal(text, "Hi");
  commonmark = cmark_render_commonmark(text, CMARK_OPT_DEFAULT, 0);
  STR_EQ(runner, commonmark, "Hi\n", "render single inline node");
  free(commonmark);

  cmark_node_free(text);
  cmark_node_free(doc);
}

static void render_plaintext(test_batch_runner *runner) {
  char *plaintext;

  static const char markdown[] = "> \\- foo *bar* \\*bar\\*\n"
                                 "\n"
                                 "- Lorem ipsum dolor sit amet,\n"
                                 "  consectetur adipiscing elit,\n"
                                 "- sed do eiusmod tempor incididunt\n"
                                 "  ut labore et dolore magna aliqua.\n";
  cmark_node *doc =
      cmark_parse_document(markdown, sizeof(markdown) - 1, CMARK_OPT_DEFAULT);

  plaintext = cmark_render_plaintext(doc, CMARK_OPT_DEFAULT, 26);
  STR_EQ(runner, plaintext, "- foo bar *bar*\n"
                             "\n"
                             "  - Lorem ipsum dolor sit\n"
                             "    amet, consectetur\n"
                             "    adipiscing elit,\n"
                             "  - sed do eiusmod tempor\n"
                             "    incididunt ut labore\n"
                             "    et dolore magna\n"
                             "    aliqua.\n",
         "render document with wrapping");
  free(plaintext);
  plaintext = cmark_render_plaintext(doc, CMARK_OPT_DEFAULT, 0);
  STR_EQ(runner, plaintext, "- foo bar *bar*\n"
                             "\n"
                             "  - Lorem ipsum dolor sit amet,\n"
                             "    consectetur adipiscing elit,\n"
                             "  - sed do eiusmod tempor incididunt\n"
                             "    ut labore et dolore magna aliqua.\n",
         "render document without wrapping");
  free(plaintext);

  cmark_node *text = cmark_node_new(CMARK_NODE_TEXT);
  cmark_node_set_literal(text, "Hi");
  plaintext = cmark_render_plaintext(text, CMARK_OPT_DEFAULT, 0);
  STR_EQ(runner, plaintext, "Hi\n", "render single inline node");
  free(plaintext);

  cmark_node_free(text);
  cmark_node_free(doc);
}

static void utf8(test_batch_runner *runner) {
  // Ranges
  test_char(runner, 1, "\x01", "valid utf8 01");
  test_char(runner, 1, "\x7F", "valid utf8 7F");
  test_char(runner, 0, "\x80", "invalid utf8 80");
  test_char(runner, 0, "\xBF", "invalid utf8 BF");
  test_char(runner, 0, "\xC0\x80", "invalid utf8 C080");
  test_char(runner, 0, "\xC1\xBF", "invalid utf8 C1BF");
  test_char(runner, 1, "\xC2\x80", "valid utf8 C280");
  test_char(runner, 1, "\xDF\xBF", "valid utf8 DFBF");
  test_char(runner, 0, "\xE0\x80\x80", "invalid utf8 E08080");
  test_char(runner, 0, "\xE0\x9F\xBF", "invalid utf8 E09FBF");
  test_char(runner, 1, "\xE0\xA0\x80", "valid utf8 E0A080");
  test_char(runner, 1, "\xED\x9F\xBF", "valid utf8 ED9FBF");
  test_char(runner, 0, "\xED\xA0\x80", "invalid utf8 EDA080");
  test_char(runner, 0, "\xED\xBF\xBF", "invalid utf8 EDBFBF");
  test_char(runner, 0, "\xF0\x80\x80\x80", "invalid utf8 F0808080");
  test_char(runner, 0, "\xF0\x8F\xBF\xBF", "invalid utf8 F08FBFBF");
  test_char(runner, 1, "\xF0\x90\x80\x80", "valid utf8 F0908080");
  test_char(runner, 1, "\xF4\x8F\xBF\xBF", "valid utf8 F48FBFBF");
  test_char(runner, 0, "\xF4\x90\x80\x80", "invalid utf8 F4908080");
  test_char(runner, 0, "\xF7\xBF\xBF\xBF", "invalid utf8 F7BFBFBF");
  test_char(runner, 0, "\xF8", "invalid utf8 F8");
  test_char(runner, 0, "\xFF", "invalid utf8 FF");

  // Incomplete byte sequences at end of input
  test_incomplete_char(runner, "\xE0\xA0", "invalid utf8 E0A0");
  test_incomplete_char(runner, "\xF0\x90\x80", "invalid utf8 F09080");

  // Invalid continuation bytes
  test_continuation_byte(runner, "\xC2\x80");
  test_continuation_byte(runner, "\xE0\xA0\x80");
  test_continuation_byte(runner, "\xF0\x90\x80\x80");

  // Test string containing null character
  static const char string_with_null[] = "((((\0))))";
  char *html = cmark_markdown_to_html(
      string_with_null, sizeof(string_with_null) - 1, CMARK_OPT_DEFAULT);
  STR_EQ(runner, html, "<p>((((" UTF8_REPL "))))</p>\n", "utf8 with U+0000");
  free(html);

  // Test NUL followed by newline
  static const char string_with_nul_lf[] = "```\n\0\n```\n";
  html = cmark_markdown_to_html(
      string_with_nul_lf, sizeof(string_with_nul_lf) - 1, CMARK_OPT_DEFAULT);
  STR_EQ(runner, html, "<pre><code>\xef\xbf\xbd\n</code></pre>\n",
         "utf8 with \\0\\n");
  free(html);

  // Test byte-order marker
  static const char string_with_bom[] = "\xef\xbb\xbf# Hello\n";
  html = cmark_markdown_to_html(
      string_with_bom, sizeof(string_with_bom) - 1, CMARK_OPT_DEFAULT);
  STR_EQ(runner, html, "<h1>Hello</h1>\n", "utf8 with BOM");
  free(html);
}

static void test_char(test_batch_runner *runner, int valid, const char *utf8,
                      const char *msg) {
  char buf[20];
  sprintf(buf, "((((%s))))", utf8);

  if (valid) {
    char expected[30];
    sprintf(expected, "<p>((((%s))))</p>\n", utf8);
    test_md_to_html(runner, buf, expected, msg);
  } else {
    test_md_to_html(runner, buf, "<p>((((" UTF8_REPL "))))</p>\n", msg);
  }
}

static void test_incomplete_char(test_batch_runner *runner, const char *utf8,
                                 const char *msg) {
  char buf[20];
  sprintf(buf, "----%s", utf8);
  test_md_to_html(runner, buf, "<p>----" UTF8_REPL "</p>\n", msg);
}

static void test_continuation_byte(test_batch_runner *runner,
                                   const char *utf8) {
  size_t len = strlen(utf8);

  for (size_t pos = 1; pos < len; ++pos) {
    char buf[20];
    sprintf(buf, "((((%s))))", utf8);
    buf[4 + pos] = '\x20';

    char expected[50];
    strcpy(expected, "<p>((((" UTF8_REPL "\x20");
    for (size_t i = pos + 1; i < len; ++i) {
      strcat(expected, UTF8_REPL);
    }
    strcat(expected, "))))</p>\n");

    char *html =
        cmark_markdown_to_html(buf, strlen(buf), CMARK_OPT_VALIDATE_UTF8);
    STR_EQ(runner, html, expected, "invalid utf8 continuation byte %zu/%zu", pos,
           len);
    free(html);
  }
}

static void line_endings(test_batch_runner *runner) {
  // Test list with different line endings
  static const char list_with_endings[] = "- a\n- b\r\n- c\r- d";
  char *html = cmark_markdown_to_html(
      list_with_endings, sizeof(list_with_endings) - 1, CMARK_OPT_DEFAULT);
  STR_EQ(runner, html,
         "<ul>\n<li>a</li>\n<li>b</li>\n<li>c</li>\n<li>d</li>\n</ul>\n",
         "list with different line endings");
  free(html);

  static const char crlf_lines[] = "line\r\nline\r\n";
  html = cmark_markdown_to_html(crlf_lines, sizeof(crlf_lines) - 1,
                                CMARK_OPT_DEFAULT | CMARK_OPT_HARDBREAKS);
  STR_EQ(runner, html, "<p>line<br />\nline</p>\n",
         "crlf endings with CMARK_OPT_HARDBREAKS");
  free(html);
  html = cmark_markdown_to_html(crlf_lines, sizeof(crlf_lines) - 1,
                                CMARK_OPT_DEFAULT | CMARK_OPT_NOBREAKS);
  STR_EQ(runner, html, "<p>line line</p>\n",
         "crlf endings with CMARK_OPT_NOBREAKS");
  free(html);

  static const char no_line_ending[] = "```\nline\n```";
  html = cmark_markdown_to_html(no_line_ending, sizeof(no_line_ending) - 1,
                                CMARK_OPT_DEFAULT);
  STR_EQ(runner, html, "<pre><code>line\n</code></pre>\n",
         "fenced code block with no final newline");
  free(html);
}

static void numeric_entities(test_batch_runner *runner) {
  test_md_to_html(runner, "&#0;", "<p>" UTF8_REPL "</p>\n",
                  "Invalid numeric entity 0");
  test_md_to_html(runner, "&#55295;", "<p>\xED\x9F\xBF</p>\n",
                  "Valid numeric entity 0xD7FF");
  test_md_to_html(runner, "&#xD800;", "<p>" UTF8_REPL "</p>\n",
                  "Invalid numeric entity 0xD800");
  test_md_to_html(runner, "&#xDFFF;", "<p>" UTF8_REPL "</p>\n",
                  "Invalid numeric entity 0xDFFF");
  test_md_to_html(runner, "&#57344;", "<p>\xEE\x80\x80</p>\n",
                  "Valid numeric entity 0xE000");
  test_md_to_html(runner, "&#x10FFFF;", "<p>\xF4\x8F\xBF\xBF</p>\n",
                  "Valid numeric entity 0x10FFFF");
  test_md_to_html(runner, "&#x110000;", "<p>" UTF8_REPL "</p>\n",
                  "Invalid numeric entity 0x110000");
  test_md_to_html(runner, "&#x80000000;", "<p>" UTF8_REPL "</p>\n",
                  "Invalid numeric entity 0x80000000");
  test_md_to_html(runner, "&#xFFFFFFFF;", "<p>" UTF8_REPL "</p>\n",
                  "Invalid numeric entity 0xFFFFFFFF");
  test_md_to_html(runner, "&#99999999;", "<p>" UTF8_REPL "</p>\n",
                  "Invalid numeric entity 99999999");

  test_md_to_html(runner, "&#;", "<p>&amp;#;</p>\n",
                  "Min decimal entity length");
  test_md_to_html(runner, "&#x;", "<p>&amp;#x;</p>\n",
                  "Min hexadecimal entity length");
  test_md_to_html(runner, "&#999999999;", "<p>&amp;#999999999;</p>\n",
                  "Max decimal entity length");
  test_md_to_html(runner, "&#x000000041;", "<p>&amp;#x000000041;</p>\n",
                  "Max hexadecimal entity length");
}

static void test_safe(test_batch_runner *runner) {
  // Test safe mode
  static const char raw_html[] = "<div>\nhi\n</div>\n\n<a>hi</"
                                 "a>\n[link](JAVAscript:alert('hi'))\n![image]("
                                 "file:my.js)\n";
  char *html = cmark_markdown_to_html(raw_html, sizeof(raw_html) - 1,
                                      CMARK_OPT_DEFAULT);
  STR_EQ(runner, html, "<!-- raw HTML omitted -->\n<p><!-- raw HTML omitted "
                       "-->hi<!-- raw HTML omitted -->\n<a "
                       "href=\"\">link</a>\n<img src=\"\" alt=\"image\" "
                       "/></p>\n",
         "input with raw HTML and dangerous links");
  free(html);
}

static void test_md_to_html(test_batch_runner *runner, const char *markdown,
                            const char *expected_html, const char *msg) {
  char *html = cmark_markdown_to_html(markdown, strlen(markdown),
                                      CMARK_OPT_VALIDATE_UTF8);
  STR_EQ(runner, html, expected_html, msg);
  free(html);
}

static void test_feed_across_line_ending(test_batch_runner *runner) {
  // See #117
  cmark_parser *parser = cmark_parser_new(CMARK_OPT_DEFAULT);
  cmark_parser_feed(parser, "line1\r", 6);
  cmark_parser_feed(parser, "\nline2\r\n", 8);
  cmark_node *document = cmark_parser_finish(parser);
  OK(runner, document->first_child->next == NULL, "document has one paragraph");
  cmark_parser_free(parser);
  cmark_node_free(document);
}

// Streaming AST tests — exercise snapshot, change events, and node-morph
// pointer stability across paragraph -> setext-heading rewrites.
static void test_streaming_ast(test_batch_runner *runner) {
  // Snapshot returns the live root and is non-NULL even before finish.
  {
    cmark_parser *p = cmark_parser_new(CMARK_OPT_DEFAULT);
    cmark_parser_feed(p, "Hello", 5);
    cmark_node *snap = cmark_parser_snapshot(p);
    OK(runner, snap != NULL, "snapshot returns non-null root before finish");
    INT_EQ(runner, (int)cmark_node_get_type(snap), (int)CMARK_NODE_DOCUMENT,
           "snapshot root is document");
    cmark_parser_free(p);
  }

  // commit_frontier starts at zero and is monotonically non-decreasing.
  // After "abc\n\ndef\n" the first paragraph is finalized (the blank line
  // closed it and rules out setext/table promotion), so the frontier
  // advances to at least the byte immediately after that paragraph (= 4,
  // the start of the blank line). The second paragraph is still open and
  // provisional, so the frontier does not include its bytes.
  {
    cmark_parser *p = cmark_parser_new(CMARK_OPT_DEFAULT);
    INT_EQ(runner, (int)cmark_parser_commit_frontier(p), 0,
           "commit_frontier starts at 0");
    cmark_parser_feed(p, "abc\n\ndef\n", 9);
    size_t f = cmark_parser_commit_frontier(p);
    OK(runner, f >= 4 && f <= 5,
       "commit_frontier advanced past committed first paragraph");
    // Monotonicity: feeding the close of the second paragraph extends it
    // but never moves frontier backwards.
    cmark_parser_feed(p, "\n", 1);
    size_t f2 = cmark_parser_commit_frontier(p);
    OK(runner, f2 >= f, "commit_frontier is monotonically non-decreasing");
    cmark_parser_free(p);
  }

  // The setext-rewrite path emits a RETYPED event, and the
  // affected node retains its pointer identity across the morph.
  {
    cmark_parser *p = cmark_parser_new(CMARK_OPT_DEFAULT);
    cmark_parser_feed(p, "Hello\n", 6);
    cmark_node *snap = cmark_parser_snapshot(p);
    cmark_node *para = cmark_node_first_child(snap);
    OK(runner, para != NULL, "paragraph node exists after first line");
    INT_EQ(runner, (int)cmark_node_get_type(para),
           (int)CMARK_NODE_PARAGRAPH, "first child is paragraph");

    // Discard initial change records (paragraph creation, etc.).
    cmark_change_iter *it0 =
        cmark_parser_changes_since_last_snapshot(p);
    cmark_node *n0;
    while (cmark_change_iter_next(it0, &n0) != CMARK_CHANGE_NONE) { }
    cmark_change_iter_free(it0);

    // The setext underline arrives, rewriting the paragraph in place.
    cmark_parser_feed(p, "=====\n", 6);

    cmark_change_iter *it = cmark_parser_changes_since_last_snapshot(p);
    int retyped_seen = 0;
    int retyped_was_para = 0;
    int retyped_node_is_same = 0;
    cmark_node *n;
    cmark_change_event k;
    while ((k = cmark_change_iter_next(it, &n)) != CMARK_CHANGE_NONE) {
      if (k == CMARK_CHANGE_NODE_RETYPED) {
        retyped_seen++;
        if (n == para)
          retyped_node_is_same = 1;
        if (cmark_node_get_type(n) == CMARK_NODE_HEADING)
          retyped_was_para = 1;
      }
    }
    cmark_change_iter_free(it);

    OK(runner, retyped_seen == 1,
       "exactly one RETYPED record for paragraph->setext");
    OK(runner, retyped_node_is_same,
       "RETYPED node pointer matches original paragraph (in-place morph)");
    OK(runner, retyped_was_para,
       "RETYPED node is now a heading");
    INT_EQ(runner, (int)cmark_node_get_type(para),
           (int)CMARK_NODE_HEADING,
           "in-place morph: paragraph pointer is now a heading");

    cmark_node *doc = cmark_parser_finish(p);
    cmark_parser_free(p);
    cmark_node_free(doc);
  }

  // Paragraph -> GFM table head also reaches us via cmark_node_set_type
  // (extensions/table.c calls it when promoting the paragraph). The
  // RETYPED record should fire for that path too.
  {
    cmark_gfm_core_extensions_ensure_registered();
    cmark_parser *p = cmark_parser_new(CMARK_OPT_DEFAULT);
    cmark_syntax_extension *table_ext =
        cmark_find_syntax_extension("table");
    OK(runner, table_ext != NULL, "found table extension");
    cmark_parser_attach_syntax_extension(p, table_ext);

    cmark_parser_feed(p, "Header 1 | Header 2\n", 20);
    cmark_node *snap = cmark_parser_snapshot(p);
    cmark_node *para = cmark_node_first_child(snap);
    OK(runner, para != NULL && cmark_node_get_type(para) == CMARK_NODE_PARAGRAPH,
       "first child is paragraph before delimiter row");

    cmark_change_iter *it0 =
        cmark_parser_changes_since_last_snapshot(p);
    cmark_node *n0;
    while (cmark_change_iter_next(it0, &n0) != CMARK_CHANGE_NONE) { }
    cmark_change_iter_free(it0);

    cmark_parser_feed(p, "--- | ---\n", 10);

    cmark_change_iter *it = cmark_parser_changes_since_last_snapshot(p);
    int retyped_seen = 0;
    int retyped_node_is_same = 0;
    cmark_node *n;
    cmark_change_event k;
    while ((k = cmark_change_iter_next(it, &n)) != CMARK_CHANGE_NONE) {
      if (k == CMARK_CHANGE_NODE_RETYPED && n == para) {
        retyped_seen++;
        retyped_node_is_same = 1;
      }
    }
    cmark_change_iter_free(it);

    OK(runner, retyped_seen >= 1,
       "RETYPED record emitted for paragraph->table morph");
    OK(runner, retyped_node_is_same,
       "RETYPED node pointer matches original paragraph (table morph)");

    cmark_node *doc = cmark_parser_finish(p);
    cmark_parser_free(p);
    cmark_node_free(doc);
  }

  // P2: open blocks carry CMARK_NODE__PROVISIONAL. Closed blocks lose it
  // and emit a FINALIZED record. After cmark_parser_finish nothing is
  // provisional.
  {
    cmark_parser *p = cmark_parser_new(CMARK_OPT_DEFAULT);
    // Need at least one terminated line before a block exists in the AST.
    // (cmark_parser_feed buffers partial lines until \n.)
    cmark_parser_feed(p, "Hello\n", 6);
    cmark_node *snap = cmark_parser_snapshot(p);
    cmark_node *para = cmark_node_first_child(snap);
    OK(runner, para != NULL, "open paragraph exists");
    OK(runner, cmark_node_is_provisional(para),
       "open paragraph is provisional");
    OK(runner, !cmark_node_is_provisional(snap),
       "document root is never provisional");

    // Drain change records before the close.
    cmark_change_iter *it0 =
        cmark_parser_changes_since_last_snapshot(p);
    cmark_node *n0; while (cmark_change_iter_next(it0, &n0)
                          != CMARK_CHANGE_NONE) { }
    cmark_change_iter_free(it0);

    // Blank line closes the paragraph and rules out setext/table.
    cmark_parser_feed(p, "\n", 1);
    cmark_change_iter *it = cmark_parser_changes_since_last_snapshot(p);
    int finalized_seen = 0;
    int finalized_was_para = 0;
    cmark_node *n; cmark_change_event k;
    while ((k = cmark_change_iter_next(it, &n)) != CMARK_CHANGE_NONE) {
      if (k == CMARK_CHANGE_NODE_FINALIZED) {
        finalized_seen++;
        if (n == para) finalized_was_para = 1;
      }
    }
    cmark_change_iter_free(it);
    OK(runner, finalized_seen >= 1,
       "at least one FINALIZED record on paragraph close");
    OK(runner, finalized_was_para,
       "FINALIZED record references the paragraph node by identity");
    OK(runner, !cmark_node_is_provisional(para),
       "paragraph is no longer provisional after close");

    cmark_node *doc = cmark_parser_finish(p);
    // Walk the tree — no node should remain provisional.
    {
      cmark_iter *iter = cmark_iter_new(doc);
      cmark_event_type ev;
      int leftover_provisional = 0;
      while ((ev = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        if (ev == CMARK_EVENT_ENTER &&
            cmark_node_is_provisional(cmark_iter_get_node(iter))) {
          leftover_provisional++;
        }
      }
      cmark_iter_free(iter);
      INT_EQ(runner, leftover_provisional, 0,
             "no provisional nodes after finish");
    }
    cmark_parser_free(p);
    cmark_node_free(doc);
  }

  // P3: snapshot returns a tree with inline children populated. A closed
  // emphasis becomes EMPH; an unclosed delimiter falls back to literal text.
  {
    cmark_parser *p = cmark_parser_new(CMARK_OPT_DEFAULT);
    cmark_parser_feed(p, "Hello *world*\n", 14);
    cmark_node *snap = cmark_parser_snapshot(p);
    cmark_node *para = cmark_node_first_child(snap);
    OK(runner, para != NULL && cmark_node_get_type(para) == CMARK_NODE_PARAGRAPH,
       "first child is a paragraph");
    // Walk inline children: expect TEXT, EMPH(TEXT).
    cmark_node *first_inline = cmark_node_first_child(para);
    OK(runner, first_inline != NULL,
       "snapshot populated inline children");
    int saw_emph = 0;
    for (cmark_node *c = first_inline; c; c = cmark_node_next(c)) {
      if (cmark_node_get_type(c) == CMARK_NODE_EMPH)
        saw_emph = 1;
    }
    OK(runner, saw_emph,
       "closed *...* surfaces as CMARK_NODE_EMPH after snapshot");
    cmark_node *doc = cmark_parser_finish(p);
    cmark_parser_free(p);
    cmark_node_free(doc);
  }

  // P3: an open paragraph with an unclosed `*` snapshots as literal text;
  // once the closer arrives, a re-parse yields EMPH.
  {
    cmark_parser *p = cmark_parser_new(CMARK_OPT_DEFAULT);
    cmark_parser_feed(p, "Hello *world\n", 13);
    cmark_node *snap1 = cmark_parser_snapshot(p);
    cmark_node *para = cmark_node_first_child(snap1);
    int saw_emph_before = 0;
    for (cmark_node *c = cmark_node_first_child(para); c;
         c = cmark_node_next(c)) {
      if (cmark_node_get_type(c) == CMARK_NODE_EMPH) saw_emph_before = 1;
    }
    OK(runner, !saw_emph_before,
       "unclosed `*` is literal text in interim snapshot");

    // Append the closer on a continuation line; paragraph re-parses.
    cmark_parser_feed(p, "again*\n", 7);
    cmark_node *snap2 = cmark_parser_snapshot(p);
    cmark_node *para2 = cmark_node_first_child(snap2);
    OK(runner, para2 == para, "paragraph pointer identity preserved");
    int saw_emph_after = 0;
    for (cmark_node *c = cmark_node_first_child(para2); c;
         c = cmark_node_next(c)) {
      if (cmark_node_get_type(c) == CMARK_NODE_EMPH) saw_emph_after = 1;
    }
    OK(runner, saw_emph_after,
       "closer arrival turns the run into EMPH on next snapshot");
    cmark_node *doc = cmark_parser_finish(p);
    cmark_parser_free(p);
    cmark_node_free(doc);
  }

  // Direction 1: a code block's content grows across snapshots; consumers
  // must see CONTENT_UPDATED records (INLINES_REPARSED would be wrong since
  // code blocks have no inlines).
  {
    cmark_parser *p = cmark_parser_new(CMARK_OPT_DEFAULT);
    cmark_parser_feed(p, "```\nfirst\n", 10);
    cmark_node *snap1 = cmark_parser_snapshot(p);
    // Drain all change records so far.
    cmark_change_iter *it0 = cmark_parser_changes_since_last_snapshot(p);
    cmark_node *n0;
    while (cmark_change_iter_next(it0, &n0) != CMARK_CHANGE_NONE) { }
    cmark_change_iter_free(it0);

    cmark_parser_feed(p, "second\n", 7);
    cmark_parser_snapshot(p);
    cmark_change_iter *it = cmark_parser_changes_since_last_snapshot(p);
    int content_updated = 0;
    int inlines_reparsed = 0;
    cmark_node *n; cmark_change_event k;
    while ((k = cmark_change_iter_next(it, &n)) != CMARK_CHANGE_NONE) {
      if (k == CMARK_CHANGE_NODE_CONTENT_UPDATED) content_updated++;
      if (k == CMARK_CHANGE_NODE_INLINES_REPARSED) inlines_reparsed++;
    }
    cmark_change_iter_free(it);
    OK(runner, content_updated >= 1,
       "code block emits CONTENT_UPDATED when its content grows");
    INT_EQ(runner, inlines_reparsed, 0,
           "code block does not emit INLINES_REPARSED (no inlines)");
    (void)snap1;
    cmark_node *doc = cmark_parser_finish(p);
    cmark_parser_free(p);
    cmark_node_free(doc);
  }

  // Direction 2: an unmatched `*` in an open paragraph carries
  // INLINE_PROVISIONAL on its TEXT node, distinguishing "tentatively
  // unclosed emphasis" from "literal asterisk". When a closer arrives, the
  // flag should disappear (the run becomes EMPH instead of TEXT).
  {
    cmark_parser *p = cmark_parser_new(CMARK_OPT_DEFAULT);
    cmark_parser_feed(p, "Hello *partial\n", 15);
    cmark_node *snap1 = cmark_parser_snapshot(p);
    cmark_node *para = cmark_node_first_child(snap1);
    int saw_inline_provisional = 0;
    for (cmark_node *c = cmark_node_first_child(para); c;
         c = cmark_node_next(c)) {
      if (cmark_node_is_provisional(c) &&
          cmark_node_get_type(c) == CMARK_NODE_TEXT) {
        saw_inline_provisional = 1;
      }
    }
    OK(runner, saw_inline_provisional,
       "unmatched `*` in open block is INLINE_PROVISIONAL on its TEXT");
    // Provide the closer; the EMPH that materializes should NOT be
    // INLINE_PROVISIONAL.
    cmark_parser_feed(p, "more*\n", 6);
    cmark_parser_snapshot(p);
    int still_provisional_text = 0;
    int saw_emph_clean = 0;
    for (cmark_node *c = cmark_node_first_child(para); c;
         c = cmark_node_next(c)) {
      if (cmark_node_get_type(c) == CMARK_NODE_TEXT &&
          (c->flags & CMARK_NODE__INLINE_PROVISIONAL)) {
        still_provisional_text = 1;
      }
      if (cmark_node_get_type(c) == CMARK_NODE_EMPH &&
          !(c->flags & CMARK_NODE__INLINE_PROVISIONAL)) {
        saw_emph_clean = 1;
      }
    }
    OK(runner, !still_provisional_text,
       "after closer arrives, no TEXT child carries INLINE_PROVISIONAL");
    OK(runner, saw_emph_clean,
       "the resolved EMPH is not INLINE_PROVISIONAL");
    cmark_node *doc = cmark_parser_finish(p);
    cmark_parser_free(p);
    cmark_node_free(doc);
  }

  // Direction 3: eager ref-def extraction. Refs resolve as soon as the def
  // is followed by a non-whitespace byte that proves it is bounded — no
  // trailing blank line needed.
  {
    cmark_parser *p = cmark_parser_new(CMARK_OPT_DEFAULT);
    cmark_parser_feed(p, "Click [foo] now.\n\n", 18);
    // Para 1 finalized; pending [foo] registered.
    cmark_parser_feed(p, "[foo]: http://x.example\n", 24);
    // Para 2 still open; eager-extract sees parsed_len == content.size, defers.
    cmark_node *snap1 = cmark_parser_snapshot(p);
    cmark_node *para1 = cmark_node_first_child(snap1);
    int link_before = 0;
    for (cmark_node *c = cmark_node_first_child(para1); c;
         c = cmark_node_next(c)) {
      if (cmark_node_get_type(c) == CMARK_NODE_LINK) link_before = 1;
    }
    OK(runner, !link_before,
       "before bounding evidence arrives, [foo] is unresolved");

    // A non-whitespace next-line proves the def is bounded.
    cmark_parser_feed(p, "Aftertext\n", 10);
    cmark_node *snap2 = cmark_parser_snapshot(p);
    cmark_node *para1_again = cmark_node_first_child(snap2);
    OK(runner, para1_again == para1,
       "para1 pointer identity preserved across eager-extract round");
    int link_after = 0;
    const char *url = NULL;
    for (cmark_node *c = cmark_node_first_child(para1_again); c;
         c = cmark_node_next(c)) {
      if (cmark_node_get_type(c) == CMARK_NODE_LINK) {
        link_after = 1;
        url = cmark_node_get_url(c);
      }
    }
    OK(runner, link_after,
       "[foo] resolves to LINK after eager extraction (no blank line needed)");
    OK(runner, url && strcmp(url, "http://x.example") == 0,
       "eagerly-extracted def carries correct URL");
    cmark_node *doc = cmark_parser_finish(p);
    cmark_parser_free(p);
    cmark_node_free(doc);
  }

  // Direction 3: safety — when content following a def is whitespace, we
  // MUST defer (could be a title-continuation). One-shot must agree with
  // streamed result.
  {
    const char *input = "[foo]: http://x.example\n   \"my title\"\n\nUse [foo].\n";
    size_t len = strlen(input);
    cmark_node *one = cmark_parse_document(input, len, CMARK_OPT_DEFAULT);
    char *html_one = cmark_render_html(one, CMARK_OPT_DEFAULT, NULL);

    cmark_parser *p = cmark_parser_new(CMARK_OPT_DEFAULT);
    for (size_t i = 0; i < len; ++i) {
      cmark_parser_feed(p, input + i, 1);
      cmark_parser_snapshot(p);
    }
    cmark_node *streamed = cmark_parser_finish(p);
    char *html_stream = cmark_render_html(streamed, CMARK_OPT_DEFAULT, NULL);
    STR_EQ(runner, html_stream, html_one,
           "title-continuation safety: streamed HTML matches one-shot");
    free(html_one); free(html_stream);
    cmark_node_free(one); cmark_node_free(streamed);
    cmark_parser_free(p);
  }

  // P5: when a [foo] reference is parsed before its definition arrives, the
  // containing paragraph gets re-parsed once the definition is added.
  {
    cmark_parser *p = cmark_parser_new(CMARK_OPT_DEFAULT);
    // Paragraph 1 references [foo] which has no def yet.
    cmark_parser_feed(p, "See [foo] for details\n\n", 23);
    cmark_node *snap1 = cmark_parser_snapshot(p);
    cmark_node *para = cmark_node_first_child(snap1);
    OK(runner, para != NULL, "paragraph created");
    // Walk inline children — should contain no LINK before def arrives.
    int link_before = 0;
    for (cmark_node *c = cmark_node_first_child(para); c;
         c = cmark_node_next(c)) {
      if (cmark_node_get_type(c) == CMARK_NODE_LINK) link_before = 1;
    }
    OK(runner, !link_before,
       "[foo] is plain text before definition arrives");

    // Now feed the definition. The blank line closes the def-bearing
    // paragraph, which is the trigger for resolve_reference_link_definitions
    // to populate the refmap (CommonMark requirement).
    cmark_parser_feed(p, "[foo]: http://example.com\n\n", 27);
    cmark_node *snap2 = cmark_parser_snapshot(p);
    // Paragraph 1's pointer should be unchanged; [foo] should now be a LINK.
    cmark_node *para2 = cmark_node_first_child(snap2);
    OK(runner, para2 == para,
       "ref-resolved paragraph keeps pointer identity");
    int link_after = 0;
    const char *link_url = NULL;
    for (cmark_node *c = cmark_node_first_child(para2); c;
         c = cmark_node_next(c)) {
      if (cmark_node_get_type(c) == CMARK_NODE_LINK) {
        link_after = 1;
        link_url = cmark_node_get_url(c);
      }
    }
    OK(runner, link_after,
       "[foo] re-parses as CMARK_NODE_LINK after definition arrives");
    OK(runner, link_url && strcmp(link_url, "http://example.com") == 0,
       "resolved link carries the correct URL");
    cmark_node *doc = cmark_parser_finish(p);
    cmark_parser_free(p);
    cmark_node_free(doc);
  }

  // P2: a paragraph that is just a reference definition gets removed at
  // finalize; we should see a NODE_REMOVED record (and no FINALIZED for it).
  {
    cmark_parser *p = cmark_parser_new(CMARK_OPT_DEFAULT);
    cmark_parser_feed(p, "[foo]: http://example.com\n", 26);
    // Drain ADDED/etc.
    cmark_change_iter *it0 =
        cmark_parser_changes_since_last_snapshot(p);
    cmark_node *n0; while (cmark_change_iter_next(it0, &n0)
                          != CMARK_CHANGE_NONE) { }
    cmark_change_iter_free(it0);
    cmark_parser_feed(p, "\n", 1); // close the paragraph
    cmark_change_iter *it =
        cmark_parser_changes_since_last_snapshot(p);
    int removed_seen = 0;
    cmark_node *n; cmark_change_event k;
    while ((k = cmark_change_iter_next(it, &n)) != CMARK_CHANGE_NONE) {
      if (k == CMARK_CHANGE_NODE_REMOVED) removed_seen++;
    }
    cmark_change_iter_free(it);
    OK(runner, removed_seen == 1,
       "ref-def-only paragraph emits exactly one NODE_REMOVED");
    cmark_node *doc = cmark_parser_finish(p);
    cmark_parser_free(p);
    cmark_node_free(doc);
  }
}

// Differential validation: for a representative corpus of markdown inputs,
// confirm that streaming parse with snapshots-between-every-byte produces
// the same final HTML as a one-shot parse. Also confirms monotonicity of
// commit_frontier and that no node remains provisional after finish.
static void test_streaming_convergence(test_batch_runner *runner) {
  static const char *corpus[] = {
    "Hello world\n",
    "Hello\n=====\n",
    "# Heading\n\nParagraph with *emphasis* and **strong**.\n",
    "- item one\n- item two\n- item three\n",
    "1. one\n2. two\n\n3. three (loose)\n",
    "> blockquote line one\n> line two\n",
    "```c\nint main(void){return 0;}\n```\n",
    "    indented code\n    second line\n",
    "Header 1 | Header 2\n--- | ---\nA | B\nC | D\n",
    "See [foo] for more.\n\n[foo]: http://example.com \"title\"\n",
    "First paragraph.\n\nSecond with `code` and a [link](http://x.com).\n",
    "<p>raw html</p>\n\nThen prose.\n",
    "Paragraph one.\nLine two of paragraph one.\n\nParagraph two.\n",
    "Mix: **bold *nested italic* still bold** plain\n",
    // Direction 3 edge cases:
    //  - title-on-continuation-line. Eager extract MUST defer until full
    //    title arrives, otherwise streamed-vs-oneshot diverges.
    "[foo]: http://x.example\n   \"title here\"\n\nUse [foo].\n",
    //  - multiple back-to-back defs. The first is provably bounded by the
    //    second `[`, so eager extract should fire.
    "[a]: http://a.example\n[b]: http://b.example\n\nSee [a] and [b].\n",
    //  - def followed by non-title non-whitespace prose.
    "[a]: http://a.example\nNot a title line.\n\nUsing [a].\n",
    //  - unclosed emphasis spanning multiple lines, then closed.
    "Open *star\nstill open\nnow closed*\n",
  };
  size_t n = sizeof(corpus) / sizeof(*corpus);

  for (size_t i = 0; i < n; ++i) {
    const char *input = corpus[i];
    size_t input_len = strlen(input);

    // Reference: one-shot parse + render.
    cmark_node *one = cmark_parse_document(input, input_len, CMARK_OPT_DEFAULT);
    char *html_one = cmark_render_html(one, CMARK_OPT_DEFAULT, NULL);

    // Streamed: feed byte-by-byte, snapshot after each, then finish.
    cmark_parser *p = cmark_parser_new(CMARK_OPT_DEFAULT);
    size_t prev_frontier = 0;
    int monotonic = 1;
    for (size_t k = 0; k < input_len; ++k) {
      cmark_parser_feed(p, input + k, 1);
      cmark_parser_snapshot(p);
      size_t f = cmark_parser_commit_frontier(p);
      if (f < prev_frontier) monotonic = 0;
      prev_frontier = f;
    }
    cmark_node *streamed = cmark_parser_finish(p);
    char *html_stream = cmark_render_html(streamed, CMARK_OPT_DEFAULT, NULL);

    // No node should remain provisional after finish.
    int leftover = 0;
    cmark_iter *iter = cmark_iter_new(streamed);
    cmark_event_type ev;
    while ((ev = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
      if (ev == CMARK_EVENT_ENTER &&
          cmark_node_is_provisional(cmark_iter_get_node(iter))) {
        leftover++;
      }
    }
    cmark_iter_free(iter);

    OK(runner, monotonic, "convergence[%zu]: commit_frontier monotonic", i);
    INT_EQ(runner, leftover, 0,
           "convergence[%zu]: no provisional nodes after finish", i);
    STR_EQ(runner, html_stream, html_one,
           "convergence[%zu]: streaming HTML == one-shot HTML", i);

    free(html_one);
    free(html_stream);
    cmark_node_free(one);
    cmark_node_free(streamed);
    cmark_parser_free(p);
  }

  // Same convergence guarantee with GFM extensions enabled — exercises the
  // paragraph -> table morph and the strikethrough/autolink inline paths.
  cmark_gfm_core_extensions_ensure_registered();
  static const char *gfm_corpus[] = {
    "Header 1 | Header 2\n--- | ---\nA | B\nC | D\n",
    "Mix ~strike~ and **bold** text.\n",
    "Visit https://example.com today.\n",
    "Tasks:\n- [ ] todo\n- [x] done\n",
  };
  size_t gn = sizeof(gfm_corpus) / sizeof(*gfm_corpus);
  for (size_t i = 0; i < gn; ++i) {
    const char *input = gfm_corpus[i];
    size_t input_len = strlen(input);

    // Helper: build a parser with GFM core extensions attached.
    #define ATTACH_GFM_EXTS(p) do { \
      const char *exts[] = {"table","strikethrough","autolink","tasklist"}; \
      for (size_t e = 0; e < sizeof(exts)/sizeof(*exts); ++e) { \
        cmark_syntax_extension *x = cmark_find_syntax_extension(exts[e]); \
        if (x) cmark_parser_attach_syntax_extension((p), x); \
      } \
    } while (0)

    cmark_parser *one_p = cmark_parser_new(CMARK_OPT_DEFAULT);
    ATTACH_GFM_EXTS(one_p);
    cmark_parser_feed(one_p, input, input_len);
    cmark_node *one = cmark_parser_finish(one_p);
    char *html_one = cmark_render_html(one, CMARK_OPT_DEFAULT,
                                       cmark_parser_get_syntax_extensions(one_p));
    cmark_parser_free(one_p);

    cmark_parser *p = cmark_parser_new(CMARK_OPT_DEFAULT);
    ATTACH_GFM_EXTS(p);
    for (size_t k = 0; k < input_len; ++k) {
      cmark_parser_feed(p, input + k, 1);
      cmark_parser_snapshot(p);
    }
    cmark_node *streamed = cmark_parser_finish(p);
    char *html_stream = cmark_render_html(streamed, CMARK_OPT_DEFAULT,
                                          cmark_parser_get_syntax_extensions(p));

    STR_EQ(runner, html_stream, html_one,
           "gfm-convergence[%zu]: streaming HTML == one-shot HTML", i);

    free(html_one);
    free(html_stream);
    cmark_node_free(one);
    cmark_node_free(streamed);
    cmark_parser_free(p);
    #undef ATTACH_GFM_EXTS
  }
}

#if !defined(_WIN32) || defined(__CYGWIN__)
#  include <sys/time.h>
static struct timeval _before, _after;
static int _timing;
#  define START_TIMING() \
       gettimeofday(&_before, NULL)

#  define END_TIMING() \
        do { \
          gettimeofday(&_after, NULL); \
          _timing = (_after.tv_sec - _before.tv_sec) * 1000 + (_after.tv_usec - _before.tv_usec) / 1000; \
        } while (0)

#  define TIMING _timing
#else
#  define START_TIMING()
#  define END_TIMING()
#  define TIMING 0
#endif

static void test_pathological_regressions(test_batch_runner *runner) {
  {
    // I don't care what the output is, so long as it doesn't take too long.
    char path[] = "[a](b";
    char *input = (char *)calloc(1, (sizeof(path) - 1) * 50000);
    for (int i = 0; i < 50000; ++i)
      memcpy(input + i * (sizeof(path) - 1), path, sizeof(path) - 1);

    START_TIMING();
    char *html = cmark_markdown_to_html(input, (sizeof(path) - 1) * 50000,
                                        CMARK_OPT_VALIDATE_UTF8);
    END_TIMING();
    free(html);
    free(input);

    OK(runner, TIMING < 1000, "takes less than 1000ms to run");
  }

  {
    char path[] = "[a](<b";
    char *input = (char *)calloc(1, (sizeof(path) - 1) * 50000);
    for (int i = 0; i < 50000; ++i)
      memcpy(input + i * (sizeof(path) - 1), path, sizeof(path) - 1);

    START_TIMING();
    char *html = cmark_markdown_to_html(input, (sizeof(path) - 1) * 50000,
                                        CMARK_OPT_VALIDATE_UTF8);
    END_TIMING();
    free(html);
    free(input);

    OK(runner, TIMING < 1000, "takes less than 1000ms to run");
  }
}

static void source_pos(test_batch_runner *runner) {
  static const char markdown[] =
    "# Hi *there*.\n"
    "\n"
    "Hello &ldquo; <http://www.google.com>\n"
    "there `hi` -- [okay](www.google.com (ok)).\n"
    "\n"
    "> 1. Okay.\n"
    ">    Sure.\n"
    ">\n"
    "> 2. Yes, okay.\n"
    ">    ![ok](hi \"yes\")\n";

  cmark_node *doc = cmark_parse_document(markdown, sizeof(markdown) - 1, CMARK_OPT_DEFAULT);
  char *xml = cmark_render_xml(doc, CMARK_OPT_DEFAULT | CMARK_OPT_SOURCEPOS);
  STR_EQ(runner, xml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
                      "<document sourcepos=\"1:1-10:20\" xmlns=\"http://commonmark.org/xml/1.0\">\n"
                      "  <heading sourcepos=\"1:1-1:13\" level=\"1\">\n"
                      "    <text sourcepos=\"1:3-1:5\" xml:space=\"preserve\">Hi </text>\n"
                      "    <emph sourcepos=\"1:6-1:12\">\n"
                      "      <text sourcepos=\"1:7-1:11\" xml:space=\"preserve\">there</text>\n"
                      "    </emph>\n"
                      "    <text sourcepos=\"1:13-1:13\" xml:space=\"preserve\">.</text>\n"
                      "  </heading>\n"
                      "  <paragraph sourcepos=\"3:1-4:42\">\n"
                      "    <text sourcepos=\"3:1-3:14\" xml:space=\"preserve\">Hello \xe2\x80\x9c </text>\n"
                      "    <link sourcepos=\"3:15-3:37\" destination=\"http://www.google.com\" title=\"\">\n"
                      "      <text sourcepos=\"3:16-3:36\" xml:space=\"preserve\">http://www.google.com</text>\n"
                      "    </link>\n"
                      "    <softbreak />\n"
                      "    <text sourcepos=\"4:1-4:6\" xml:space=\"preserve\">there </text>\n"
                      "    <code sourcepos=\"4:8-4:9\" xml:space=\"preserve\">hi</code>\n"
                      "    <text sourcepos=\"4:11-4:14\" xml:space=\"preserve\"> -- </text>\n"
                      "    <link sourcepos=\"4:15-4:41\" destination=\"www.google.com\" title=\"ok\">\n"
                      "      <text sourcepos=\"4:16-4:19\" xml:space=\"preserve\">okay</text>\n"
                      "    </link>\n"
                      "    <text sourcepos=\"4:42-4:42\" xml:space=\"preserve\">.</text>\n"
                      "  </paragraph>\n"
                      "  <block_quote sourcepos=\"6:1-10:20\">\n"
                      "    <list sourcepos=\"6:3-10:20\" type=\"ordered\" start=\"1\" delim=\"period\" tight=\"false\">\n"
                      "      <item sourcepos=\"6:3-8:1\">\n"
                      "        <paragraph sourcepos=\"6:6-7:10\">\n"
                      "          <text sourcepos=\"6:6-6:10\" xml:space=\"preserve\">Okay.</text>\n"
                      "          <softbreak />\n"
                      "          <text sourcepos=\"7:6-7:10\" xml:space=\"preserve\">Sure.</text>\n"
                      "        </paragraph>\n"
                      "      </item>\n"
                      "      <item sourcepos=\"9:3-10:20\">\n"
                      "        <paragraph sourcepos=\"9:6-10:20\">\n"
                      "          <text sourcepos=\"9:6-9:15\" xml:space=\"preserve\">Yes, okay.</text>\n"
                      "          <softbreak />\n"
                      "          <image sourcepos=\"10:6-10:20\" destination=\"hi\" title=\"yes\">\n"
                      "            <text sourcepos=\"10:8-10:9\" xml:space=\"preserve\">ok</text>\n"
                      "          </image>\n"
                      "        </paragraph>\n"
                      "      </item>\n"
                      "    </list>\n"
                      "  </block_quote>\n"
                      "</document>\n",
         "sourcepos are as expected");
  free(xml);
  cmark_node_free(doc);
}

static void source_pos_inlines(test_batch_runner *runner) {
  {
    static const char markdown[] =
      "*first*\n"
      "second\n";

    cmark_node *doc = cmark_parse_document(markdown, sizeof(markdown) - 1, CMARK_OPT_DEFAULT);
    char *xml = cmark_render_xml(doc, CMARK_OPT_DEFAULT | CMARK_OPT_SOURCEPOS);
    STR_EQ(runner, xml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                        "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
                        "<document sourcepos=\"1:1-2:6\" xmlns=\"http://commonmark.org/xml/1.0\">\n"
                        "  <paragraph sourcepos=\"1:1-2:6\">\n"
                        "    <emph sourcepos=\"1:1-1:7\">\n"
                        "      <text sourcepos=\"1:2-1:6\" xml:space=\"preserve\">first</text>\n"
                        "    </emph>\n"
                        "    <softbreak />\n"
                        "    <text sourcepos=\"2:1-2:6\" xml:space=\"preserve\">second</text>\n"
                        "  </paragraph>\n"
                        "</document>\n",
                        "sourcepos are as expected");
    free(xml);
    cmark_node_free(doc);
  }
  {
    static const char markdown[] =
      "*first\n"
      "second*\n";

    cmark_node *doc = cmark_parse_document(markdown, sizeof(markdown) - 1, CMARK_OPT_DEFAULT);
    char *xml = cmark_render_xml(doc, CMARK_OPT_DEFAULT | CMARK_OPT_SOURCEPOS);
    STR_EQ(runner, xml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                        "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
                        "<document sourcepos=\"1:1-2:7\" xmlns=\"http://commonmark.org/xml/1.0\">\n"
                        "  <paragraph sourcepos=\"1:1-2:7\">\n"
                        "    <emph sourcepos=\"1:1-2:7\">\n"
                        "      <text sourcepos=\"1:2-1:6\" xml:space=\"preserve\">first</text>\n"
                        "      <softbreak />\n"
                        "      <text sourcepos=\"2:1-2:6\" xml:space=\"preserve\">second</text>\n"
                        "    </emph>\n"
                        "  </paragraph>\n"
                        "</document>\n",
                        "sourcepos are as expected");
    free(xml);
    cmark_node_free(doc);
  }
}

static void ref_source_pos(test_batch_runner *runner) {
  static const char markdown[] =
    "Let's try [reference] links.\n"
    "\n"
    "[reference]: https://github.com (GitHub)\n";

  cmark_node *doc = cmark_parse_document(markdown, sizeof(markdown) - 1, CMARK_OPT_DEFAULT);
  char *xml = cmark_render_xml(doc, CMARK_OPT_DEFAULT | CMARK_OPT_SOURCEPOS);
  STR_EQ(runner, xml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
                      "<document sourcepos=\"1:1-3:40\" xmlns=\"http://commonmark.org/xml/1.0\">\n"
                      "  <paragraph sourcepos=\"1:1-1:28\">\n"
                      "    <text sourcepos=\"1:1-1:10\" xml:space=\"preserve\">Let's try </text>\n"
                      "    <link sourcepos=\"1:11-1:21\" destination=\"https://github.com\" title=\"GitHub\">\n"
                      "      <text sourcepos=\"1:12-1:20\" xml:space=\"preserve\">reference</text>\n"
                      "    </link>\n"
                      "    <text sourcepos=\"1:22-1:28\" xml:space=\"preserve\"> links.</text>\n"
                      "  </paragraph>\n"
                      "</document>\n",
         "sourcepos are as expected");
  free(xml);
  cmark_node_free(doc);
}

int main() {
  int retval;
  test_batch_runner *runner = test_batch_runner_new();

  cmark_enable_safety_checks(true);
  version(runner);
  constructor(runner);
  accessors(runner);
  node_check(runner);
  iterator(runner);
  iterator_delete(runner);
  create_tree(runner);
  custom_nodes(runner);
  hierarchy(runner);
  parser(runner);
  render_html(runner);
  render_xml(runner);
  render_man(runner);
  render_latex(runner);
  render_commonmark(runner);
  render_plaintext(runner);
  utf8(runner);
  line_endings(runner);
  numeric_entities(runner);
  test_cplusplus(runner);
  test_safe(runner);
  test_feed_across_line_ending(runner);
  test_streaming_ast(runner);
  test_streaming_convergence(runner);
  test_pathological_regressions(runner);
  source_pos(runner);
  source_pos_inlines(runner);
  ref_source_pos(runner);

  test_print_summary(runner);
  retval = test_ok(runner) ? 0 : 1;
  free(runner);

  return retval;
}
