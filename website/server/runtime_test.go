package main

import (
	"bytes"
	"encoding/json"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"testing/fstest"
)

func runtimeFixture(t *testing.T) (string, *RuntimeDownload, []byte) {
	t.Helper()
	dir, _ := fixture(t)
	data, err := os.ReadFile(filepath.Join(dir, "releases.json"))
	if err != nil {
		t.Fatal(err)
	}
	var catalog Catalog
	if err := json.Unmarshal(data, &catalog); err != nil {
		t.Fatal(err)
	}
	payload := []byte("MZ-test-only-runtime-payload")
	runtime := &RuntimeDownload{File: "oneui-test-windows-x64.dll", Size: int64(len(payload)), SHA256: hashBytes(payload)}
	catalog.Releases[0].Artifacts[0].Runtime = runtime
	data, err = json.Marshal(catalog)
	if err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(dir, "releases.json"), data, 0600); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(dir, runtime.File), payload, 0600); err != nil {
		t.Fatal(err)
	}
	return dir, runtime, payload
}

func TestRuntimeDownload(t *testing.T) {
	dir, runtime, payload := runtimeFixture(t)
	catalog, err := loadCatalog(dir)
	if err != nil {
		t.Fatal(err)
	}
	defer catalog.root.Close()
	handler := newHandler(catalog, fstest.MapFS{"index.html": {Data: []byte("home")}})
	for _, method := range []string{"GET", "HEAD"} {
		w := httptest.NewRecorder()
		handler.ServeHTTP(w, httptest.NewRequest(method, "/files/"+runtime.File, nil))
		if w.Code != 200 || w.Header().Get("Content-Type") != "application/octet-stream" ||
			!strings.Contains(w.Header().Get("Content-Disposition"), runtime.File) || w.Header().Get("ETag") != `"`+runtime.SHA256+`"` {
			t.Fatalf("invalid %s runtime response: %d %v", method, w.Code, w.Header())
		}
		if method == "GET" && !bytes.Equal(w.Body.Bytes(), payload) {
			t.Fatal("DLL payload changed")
		}
		if method == "HEAD" && w.Body.Len() != 0 {
			t.Fatal("HEAD returned body")
		}
	}
	for _, tc := range []struct {
		header, value  string
		status, length int
	}{
		{"Range", "bytes=2-8", 206, 7},
		{"Range", "bytes=9000-", 416, -1},
		{"If-None-Match", `"` + runtime.SHA256 + `"`, 304, 0},
	} {
		request := httptest.NewRequest("GET", "/files/"+runtime.File, nil)
		request.Header.Set(tc.header, tc.value)
		w := httptest.NewRecorder()
		handler.ServeHTTP(w, request)
		if w.Code != tc.status || (tc.length >= 0 && w.Body.Len() != tc.length) {
			t.Fatalf("%s: %d %s", tc.header, w.Code, w.Body.String())
		}
	}
	w := httptest.NewRecorder()
	handler.ServeHTTP(w, httptest.NewRequest("GET", "/api/releases", nil))
	var public Catalog
	if err := json.Unmarshal(w.Body.Bytes(), &public); err != nil || public.Releases[0].Artifacts[0].Runtime.SHA256 != runtime.SHA256 {
		t.Fatal("runtime missing from public catalog")
	}
	if err := os.WriteFile(filepath.Join(dir, runtime.File), []byte("corrupt"), 0600); err != nil {
		t.Fatal(err)
	}
	w = httptest.NewRecorder()
	handler.ServeHTTP(w, httptest.NewRequest("GET", "/files/"+runtime.File, nil))
	if w.Code != 503 {
		t.Fatal("served modified DLL")
	}
	if c, err := loadCatalog(dir); err == nil {
		c.root.Close()
		t.Fatal("accepted corrupt DLL at startup")
	}
}

func TestRuntimeValidation(t *testing.T) {
	dir, _, _ := runtimeFixture(t)
	data, _ := os.ReadFile(filepath.Join(dir, "releases.json"))
	for _, change := range []func(*Artifact){
		func(a *Artifact) { a.Runtime.File = "../escape.dll" },
		func(a *Artifact) { a.Runtime.File = "private.txt" },
		func(a *Artifact) { a.Runtime.Size = 0 },
		func(a *Artifact) { a.Runtime.SHA256 = "invalid" },
		func(a *Artifact) { a.Linkage = "static" },
		func(a *Artifact) { a.Platform = "linux" },
	} {
		var c Catalog
		json.Unmarshal(data, &c)
		change(&c.Releases[0].Artifacts[0])
		if c.validate() == nil {
			t.Fatal("accepted invalid runtime metadata")
		}
	}
	var c Catalog
	json.Unmarshal(data, &c)
	other := c.Releases[0].Artifacts[0]
	other.ID = "different"
	other.File = "different.zip"
	c.Releases[0].Artifacts = append(c.Releases[0].Artifacts, other)
	if c.validate() == nil {
		t.Fatal("accepted duplicate DLL filename")
	}
	if err := os.Remove(filepath.Join(dir, other.Runtime.File)); err != nil {
		t.Fatal(err)
	}
	if c, err := loadCatalog(dir); err == nil {
		c.root.Close()
		t.Fatal("accepted missing DLL")
	}
}
