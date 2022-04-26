package cmark

/*
#cgo CFLAGS: -std=gnu99 -I. -I../src/
#include <stdlib.h>
#include <string.h>
#include "cmark-gfm.h"
*/
import "C"
import (
	"unsafe"
)

//Maps to a cmark_node_type enum in cmark.h
type NodeType int

const (
	//Error Status
	CMARK_NODE_NONE NodeType = iota

	//Block
	CMARK_NODE_DOCUMENT
	CMARK_NODE_BLOCK_QUOTE
	CMARK_NODE_LIST
	CMARK_NODE_ITEM
	CMARK_NODE_CODE_BLOCK
	CMARK_NODE_HTML_BLOCK
	CMARK_NODE_CUSTOM_BLOCK
	CMARK_NODE_PARAGRAPH
	CMARK_NODE_HEADING
	CMARK_NODE_THEMATIC_BREAK

	//Inline
	CMARK_NODE_TEXT
	CMARK_NODE_SOFTBREAK
	CMARK_NODE_LINEBREAK
	CMARK_NODE_CODE
	CMARK_NODE_HTML_INLINE
	CMARK_NODE_CUSTOM_INLINE
	CMARK_NODE_EMPH
	CMARK_NODE_STRONG
	CMARK_NODE_LINK
	CMARK_NODE_IMAGE
	//Block
	CMARK_NODE_FIRST_BLOCK = CMARK_NODE_DOCUMENT
	CMARK_NODE_LAST_BLOCK  = CMARK_NODE_THEMATIC_BREAK
	//Inline
	CMARK_NODE_FIRST_INLINE = CMARK_NODE_TEXT
	CMARK_NODE_LAST_INLINE  = CMARK_NODE_IMAGE
)

//Maps to a cmark_list_type in cmark.h
type ListType int

const (
	CMARK_NO_LIST ListType = iota
	CMARK_BULLET_LIST
	CMARK_ORDERED_LIST
)

type DelimType int

const (
	CMARK_NO_DELIM = iota
	CMARK_PERIOD_DELIM
	CMARK_PAREN_DELIM
)

//CMark writer options for render functions
const CMARK_OPT_DEFAULT = 0
const CMARK_OPT_SOURCEPOS = 1
const CMARK_OPT_HARDBREAKS = 2
const CMARK_OPT_NORMALIZE = 4
const CMARK_OPT_SMART = 8
const CMARK_OPT_VALIDATE_UTF8 = 16
const CMARK_OPT_SAFE = 32

//converts C int return codes to True/False :)
func success(code C.int) bool {
	if int(code) > 0 {
		return true
	} else {
		return false
	}
}

//Wraps the cmark_node.
//CommonMark nodes are represented as Trees in memory.
type CMarkNode struct {
	node   *C.struct_cmark_node
	parent *CMarkNode
}

//Creates a new node of the specified type
func NewCMarkNode(nt NodeType) *CMarkNode {
	n := &CMarkNode{
		node: C.cmark_node_new(C.cmark_node_type(nt)),
	}
	return n
}

// print structure as XML
func (node *CMarkNode) RenderXML(options int) string {
	result := C.cmark_render_xml(node.node, C.int(options))
	defer C.free(unsafe.Pointer(result))
	return C.GoString(result)
}

// Renders the document as HTML.
// Returns an HTML string.
func (node *CMarkNode) RenderHtml(options int) string {
	result := C.cmark_render_html(node.node, C.int(options), nil)
	defer C.free(unsafe.Pointer(result))
	return C.GoString(result)
}

// Renders the document as a groff man page,
// without the header
func (node *CMarkNode) RenderMan(options int, width int) string {
	result := C.cmark_render_man(node.node, C.int(options), C.int(width))
	defer C.free(unsafe.Pointer(result))
	return C.GoString(result)
}

// Renders node tree as commonmark text.
func (node *CMarkNode) RenderCMark(options int, width int) string {
	result := C.cmark_render_commonmark(node.node, C.int(options), C.int(width))
	defer C.free(unsafe.Pointer(result))
	return C.GoString(result)
}

// Renders node tree as a LaTeX document
func (node *CMarkNode) RenderLatex(options int, width int) string {
	result := C.cmark_render_latex(node.node, C.int(options), C.int(width))
	defer C.free(unsafe.Pointer(result))
	return C.GoString(result)
}

// Cleanup a node, including any children.
// Unlinks a node from the tree and frees it.
func (node *CMarkNode) Free() {
	if node.node != nil {
		C.cmark_node_free(node.node)
	}
	node.node = nil
}

//Node traversal functions

//Get next node
func (node *CMarkNode) Next() *CMarkNode {
	return &CMarkNode{
		node: C.cmark_node_next(node.node),
	}
}

//Get previous node
func (node *CMarkNode) Previous() *CMarkNode {
	return &CMarkNode{
		node: C.cmark_node_previous(node.node),
	}
}

//Get parent node
func (node *CMarkNode) Parent() *CMarkNode {
	return &CMarkNode{
		node: C.cmark_node_parent(node.node),
	}
}

//Get first child node
func (node *CMarkNode) FirstChild() *CMarkNode {
	return &CMarkNode{
		node: C.cmark_node_first_child(node.node),
	}
}

//Get last child node
func (node *CMarkNode) LastChild() *CMarkNode {
	return &CMarkNode{
		node: C.cmark_node_last_child(node.node),
	}
}

//Accessor functions

//Sets arbitrary user data for node
func (node *CMarkNode) SetNodeUserData(userData string) bool {
	cstr := C.CString(userData)
	res := C.cmark_node_set_user_data(node.node, unsafe.Pointer(&cstr))
	return success(res)
}

//Returns the user data of the node as an
//unsafe.Pointer.  Hope you know what you're doing.
func (node *CMarkNode) GetNodeUserData() string {
	data := C.cmark_node_get_user_data(node.node)
	return C.GoString((*C.char)(*(*unsafe.Pointer)(data)))
}

//Get the node type
func (node *CMarkNode) GetNodeType() NodeType {
	nt := C.cmark_node_get_type(node.node)
	return NodeType(nt)
}

//Get the node type as a string
func (node *CMarkNode) GetNodeTypeString() string {
	cstr := C.cmark_node_get_type_string(node.node)
	return C.GoString(cstr)
}

//Get the node's string content
func (node *CMarkNode) GetLiteral() string {
	cstr := C.cmark_node_get_literal(node.node)
	return C.GoString(cstr)
}

//Set the node's string content
func (node *CMarkNode) SetLiteral(content string) bool {
	cstr := C.CString(content)
	defer C.free(unsafe.Pointer(cstr))
	return success(C.cmark_node_set_literal(node.node, cstr))
}

//Get a Header node's level
func (node *CMarkNode) GetHeaderLevel() int {
	level := C.cmark_node_get_header_level(node.node)
	return int(level)
}

//Set a Header node's level (1,2, etc.)
func (node *CMarkNode) SetHeaderLevel(level int) bool {
	return success(C.cmark_node_set_header_level(node.node, C.int(level)))
}

//Get a List node's list type
func (node *CMarkNode) GetListType() ListType {
	lt := C.cmark_node_get_list_type(node.node)
	return ListType(lt)
}

//Set a List node's list type
func (node *CMarkNode) SetListType(lt ListType) bool {
	return success(C.cmark_node_set_list_type(node.node, C.cmark_list_type(lt)))
}

//Returns the list delimiter type of node, or CMARK_NO_DELIM if node is not a list
func (node *CMarkNode) GetListDelim() DelimType {
	dt := C.cmark_node_get_list_delim(node.node)
	return DelimType(dt)
}

//Sets the list delimeter type of the node, returns true on success
func (node *CMarkNode) SetListDelim(dt DelimType) bool {
	return success(C.cmark_node_set_list_delim(node.node, C.cmark_delim_type(dt)))
}

//Get a list's start
func (node *CMarkNode) GetListStart() int {
	ls := C.cmark_node_get_list_start(node.node)
	return int(ls)
}

//Set a list's start
func (node *CMarkNode) SetListStart(start int) bool {
	return success(C.cmark_node_set_list_start(node.node, C.int(start)))
}

//Get list 'tight'
func (node *CMarkNode) GetListTight() bool {
	return success(C.cmark_node_get_list_tight(node.node))
}

//Set list 'tight'
func (node *CMarkNode) SetListTight(isTight bool) bool {
	ti := 0
	if isTight == true {
		ti = 1
	}
	return success(C.cmark_node_set_list_tight(node.node, C.int(ti)))
}

//Get Fence info
func (node *CMarkNode) GetFenceInfo() string {
	cstr := C.cmark_node_get_fence_info(node.node)
	return C.GoString(cstr)
}

//Set Fence info
func (node *CMarkNode) SetFenceInfo(fenceInfo string) bool {
	cstr := C.CString(fenceInfo)
	defer C.free(unsafe.Pointer(cstr))
	return success(C.cmark_node_set_fence_info(node.node, cstr))
}

//Get a node's url
func (node *CMarkNode) GetUrl() string {
	cstr := C.cmark_node_get_url(node.node)
	return C.GoString(cstr)
}

//Set a node's url
func (node *CMarkNode) SetUrl(url string) bool {
	cstr := C.CString(url)
	defer C.free(unsafe.Pointer(cstr))
	return success(C.cmark_node_set_url(node.node, cstr))
}

//Set a node's title
func (node *CMarkNode) SetTitle(title string) bool {
	cstr := C.CString(title)
	defer C.free(unsafe.Pointer(cstr))
	return success(C.cmark_node_set_title(node.node, cstr))
}

//Get a node's title
func (node *CMarkNode) GetTitle() string {
	cstr := C.cmark_node_get_title(node.node)
	return C.GoString(cstr)
}

//Returns the literal "on enter" text for a custom node, or an empty
//string if no on_enter is set
func (node *CMarkNode) GetOnEnter() string {
	cstr := C.cmark_node_get_on_enter(node.node)
	return C.GoString(cstr)
}

//Sets the literal text to render "on enter" for a custom node.
//Any children of the node will be rendered after this text.
//Returns true on success and false on failure
func (node *CMarkNode) SetOnEnter(onEnter string) bool {
	cstr := C.CString(onEnter)
	defer C.free(unsafe.Pointer(cstr))
	return success(C.cmark_node_set_on_enter(node.node, cstr))
}

//Returns the literal "on exit" text for a custom node, or an empty
//string if no on_exit is set
func (node *CMarkNode) GetOnExit() string {
	cstr := C.cmark_node_get_on_exit(node.node)
	return C.GoString(cstr)
}

//Sets the literal text to render "on exit" for a custom node.
//Any children of the node will be rendered before this text.
//Returns true on success and false on failure
func (node *CMarkNode) SetOnExit(onExit string) bool {
	cstr := C.CString(onExit)
	defer C.free(unsafe.Pointer(cstr))
	return success(C.cmark_node_set_on_exit(node.node, cstr))
}

//Returns the line on which 'node' begins
func (node *CMarkNode) GetStartLine() int {
	return int(C.cmark_node_get_start_line(node.node))
}

//Returns the column at which 'node' begins
func (node *CMarkNode) GetStartColumn() int {
	return int(C.cmark_node_get_start_column(node.node))
}

//Returns the line on which 'node' ends
func (node *CMarkNode) GetEndLine() int {
	return int(C.cmark_node_get_end_line(node.node))
}

//Returns the column at which 'node' ends
func (node *CMarkNode) GetEndColumn() int {
	return int(C.cmark_node_get_end_column(node.node))
}

// Tree manipulation functions

//Unlink a node from the tree
func (node *CMarkNode) Unlink() {
	C.cmark_node_unlink(node.node)
}

// InsertBefore can cause a panic quite readily :)
// Hint: Both nodes had better already be in the 'tree'
// Insert a node before another 'sibling' node
func (node *CMarkNode) InsertBefore(sibling *CMarkNode) bool {
	return success(C.cmark_node_insert_before(node.node, sibling.node))
}

// InsertAfter can cause a panic quite readily :)
// Hint: Both nodes had better already be in the 'tree'
//Insert a node after another 'sibling' node
func (node *CMarkNode) InsertAfter(sibling *CMarkNode) bool {
	return success(C.cmark_node_insert_after(node.node, sibling.node))
}

// Replaces 'oldNode' with 'newNode' and unlinks 'oldnode' (but does
// not free its memory).
// Returns true on success, false on failure.
func (newNode *CMarkNode) Replace(oldNode *CMarkNode) bool {
	return success(C.cmark_node_replace(oldNode.node, newNode.node))
}

//Prepend a child node
func (node *CMarkNode) PrependChild(child *CMarkNode) bool {
	return success(C.cmark_node_prepend_child(node.node, child.node))
}

//Append a child node
func (node *CMarkNode) AppendChild(child *CMarkNode) bool {
	return success(C.cmark_node_append_child(node.node, child.node))
}

//Consolidates adjacent text nodes.
func (node *CMarkNode) ConsolidateTextNodes() {
	C.cmark_consolidate_text_nodes(node.node)
}
