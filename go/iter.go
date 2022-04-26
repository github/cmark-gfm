package cmark

/*
#cgo CFLAGS: -std=gnu99 -I. -I../src/
#include <stdlib.h>
#include "cmark-gfm.h"
*/
import "C"
import (
	"runtime"
)

type CMarkEvent int

const (
	CMARK_EVENT_NONE CMarkEvent = iota
	CMARK_EVENT_DONE
	CMARK_EVENT_ENTER
	CMARK_EVENT_EXIT
)

//Wraps a cmark_iter
type CMarkIter struct {
	iter *C.cmark_iter
}

//Creates a new iterator starting with the given node.
func NewCMarkIter(node *CMarkNode) *CMarkIter {
	iter := &CMarkIter{
		iter: C.cmark_iter_new(node.node),
	}
	runtime.SetFinalizer(iter, (*CMarkIter).Free)
	return iter
}

//Returns the event type for the next node
func (iter *CMarkIter) Next() CMarkEvent {
	ne := C.cmark_iter_next(iter.iter)
	return CMarkEvent(ne)
}

//Returns the next node in the sequence
func (iter *CMarkIter) GetNode() *CMarkNode {
	return &CMarkNode{
		node: C.cmark_iter_get_node(iter.iter),
	}

}

//Reset the iterator so the current node is 'current' and the
//event type is 'event'.  Use this to resume after
//desctructively modifying the tree structure
func (iter *CMarkIter) Reset(current *CMarkNode, event CMarkEvent) {
	C.cmark_iter_reset(iter.iter, current.node, C.cmark_event_type(event))
}

//Frees an iterator
func (iter *CMarkIter) Free() {
	if iter.iter != nil {
		C.cmark_iter_free(iter.iter)
	}
	iter.iter = nil
}
