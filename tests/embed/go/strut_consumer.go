// FFI-9 Go consumer (core): a real cgo application driving Strut through the shared
// embedding library (libstrut_embed). Covers context create/destroy, source load + reload,
// int/string/bytes invocation (bytes with embedded NUL and explicit lengths), structured
// checked errors, retained callback + BUSY destroy, and explicit release/destroy.
// (Borrowed-callback invocation from Go requires a cgo C-trampoline/export bridge and is the
// documented follow-up slice of 9-GO.)
//
// Build (paths come from the environment, not this source):
//   CGO_ENABLED=1 CGO_CFLAGS=-I<include> CGO_LDFLAGS="-L<libdir> -lstrut_embed -ldl" go build
package main

/*
#cgo LDFLAGS: -lstrut_embed -ldl
#include <stdint.h>
#include <stdlib.h>
#include "strut/embed.h"

// `type` is a reserved keyword in Go; expose it via a helper.
static const char* strut_embed_error_type_ptr(const strut_embed_error* e) { return e ? e->type : NULL; }
static void strut_embed_set_int(strut_embed_value* v, int64_t i) { v->kind = 2; v->i = i; }
static void strut_embed_set_blob(strut_embed_value* v, int kind, const void* p, size_t n) { v->kind = kind; v->s.data = (const char*)p; v->s.len = n; }
static const void* strut_embed_get_data(const strut_embed_value* v) { return v ? v->s.data : NULL; }
static size_t strut_embed_get_len(const strut_embed_value* v) { return v ? v->s.len : 0; }
*/
import "C"

import (
	"fmt"
	"os"
	"unsafe"
)

func fail(what string) {
	fmt.Println("GO CONSUMER FAIL:", what)
	os.Exit(1)
}

func errDetail(e *C.strut_embed_error) (int, string, string) {
	if e == nil {
		return 0, "", ""
	}
	cat := int(e.category)
	ty := ""
	if p := C.strut_embed_error_type_ptr(e); p != nil {
		ty = C.GoString(p)
	}
	msg := ""
	if e.message != nil {
		msg = C.GoString(e.message)
	}
	code := int(e.code)
	C.strut_embed_error_release(nil, e)
	_ = code
	return cat, ty, msg
}

func main() {
	ctx := C.strut_embed_context_create()
	if ctx == nil {
		fail("create")
	}

	load := func(src string) {
		cs := C.CString(src)
		defer C.free(unsafe.Pointer(cs))
		var e *C.strut_embed_error
		if C.strut_embed_context_load_source(ctx, cs, C.size_t(len(src)), &e) != 0 {
			_, _, m := errDetail(e)
			fail("load source: " + m)
		}
	}

	load(`export "C" function add(int_32 a, int_32 b) -> int_32 { return a + b; }
export "C" function greet(string name) -> string { return "hi " + name; }
export "C" function echo_bytes(bytes b) -> bytes { return b; }
error EmbedErr { string message; }
export "C" function risky(int_32 x) -> int_32 : EmbedErr { if (x < 0) { throw EmbedErr { message: "boom" }; } return x; }
export "C" function make_rc(int_32 base) -> retained_callback<(int_32)->int_32> { return retained_callback((int_32 x) => x + base); }`)

	// integer invocation
	av := [2]C.strut_embed_value{}
	av[0].kind, av[0].i = 2, 20
	av[1].kind, av[1].i = 2, 22
	out := C.strut_embed_value{}
	var e *C.strut_embed_error
	if C.strut_embed_invoke(ctx, C.CString("add"), &av[0], 2, &out, &e) != 0 {
		_, _, m := errDetail(e)
		fail("add: " + m)
	}
	if out.i != 42 {
		fail("add=42")
	}

	// string round-trip
	sv := C.strut_embed_value{}
	sb := C.CBytes([]byte("stranger"))
	C.strut_embed_set_blob(&sv, 4, unsafe.Pointer(sb), 8)
	if C.strut_embed_invoke(ctx, C.CString("greet"), &sv, 1, &out, &e) != 0 {
		_, _, m := errDetail(e)
		fail("greet: " + m)
	}
	if C.GoBytes(unsafe.Pointer(C.strut_embed_get_data(&out)), C.int(C.strut_embed_get_len(&out))) == nil {
		fail("greet nil")
	}
	got := string(C.GoBytes(unsafe.Pointer(C.strut_embed_get_data(&out)), C.int(C.strut_embed_get_len(&out))))
	if got != "hi stranger" {
		fail("greet result")
	}
	C.strut_embed_value_free(ctx, &out)
	C.free(unsafe.Pointer(sb))

	// bytes with embedded NUL + high bit, explicit length
	bv := C.strut_embed_value{}
	raw := []byte{'a', 0, 'b', 0xff}
	buf := C.CBytes(raw)
	C.strut_embed_set_blob(&bv, 5, unsafe.Pointer(buf), 4)
	if C.strut_embed_invoke(ctx, C.CString("echo_bytes"), &bv, 1, &out, &e) != 0 {
		_, _, m := errDetail(e)
		fail("bytes: " + m)
	}
	if out.s.len != 4 {
		fail("bytes len")
	}
	bgot := C.GoBytes(unsafe.Pointer(C.strut_embed_get_data(&out)), C.int(C.strut_embed_get_len(&out)))
	if bgot[0] != 'a' || bgot[1] != 0 || bgot[2] != 'b' || bgot[3] != 0xff {
		fail("bytes exact")
	}
	C.strut_embed_value_free(ctx, &out)
	C.free(unsafe.Pointer(buf))

	// structured checked error (type via accessor; message; code)
	rv := C.strut_embed_value{}
	rv.kind, rv.i = 2, -1
	if C.strut_embed_invoke(ctx, C.CString("risky"), &rv, 1, &out, &e) == 0 {
		fail("risky should fail")
	}
	cat, ty, msg := errDetail(e)
	if cat != 4 || ty != "EmbedErr" || msg != "boom" {
		fail("checked err identity")
	}

	// retained callback -> reload -> BUSY destroy -> release -> destroy
	rcBase := C.strut_embed_value{}
	rcBase.kind, rcBase.i = 2, 100
	rc := C.strut_embed_value{}
	if C.strut_embed_invoke(ctx, C.CString("make_rc"), &rcBase, 1, &rc, &e) != 0 || rc.retained == nil {
		_, _, m := errDetail(e)
		fail("make_rc: " + m)
	}
	xv := C.strut_embed_value{}
	xv.kind, xv.i = 2, 5
	o2 := C.strut_embed_value{}
	if C.strut_embed_retained_invoke(ctx, &rc, &xv, 1, &o2, &e) != 0 || o2.i != 105 {
		fail("retained invoke before reload")
	}
	load(`export "C" function twice(int_32 x) -> int_32 { return x * 2; }`) // A replaced; lease keeps it loaded
	o3 := C.strut_embed_value{}
	if C.strut_embed_retained_invoke(ctx, &rc, &xv, 1, &o3, &e) != 0 || o3.i != 105 {
		fail("retained invoke after reload")
	}
	if C.strut_embed_context_destroy(ctx) == 0 {
		fail("BUSY destroy expected while retained outstanding")
	}
	C.strut_embed_value_free(ctx, &rc)
	if C.strut_embed_context_destroy(ctx) != 0 {
		fail("destroy after release")
	}

	fmt.Println("go consumer ok")
}