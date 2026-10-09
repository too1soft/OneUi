package main

import (
	"archive/zip"
	"bytes"
	"encoding/json"
	"io"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"testing/fstest"
)

func fixture(t *testing.T) (string, Artifact) {
	t.Helper()
	root := t.TempDir()
	content := []byte("real SDK payload")
	manifest := map[string]any{"schemaVersion": 1, "files": []map[string]any{{"path": "lib/example.a", "size": len(content), "sha256": hashBytes(content)}}}
	data, _ := json.Marshal(manifest)
	var buf bytes.Buffer
	zw := zip.NewWriter(&buf)
	for name, body := range map[string][]byte{"sdk-manifest.json": data, "lib/example.a": content} {
		f, e := zw.Create(name)
		if e != nil {
			t.Fatal(e)
		}
		f.Write(body)
	}
	zw.Close()
	a := Artifact{ID: "windows-modern-x86_64-shared", Platform: "windows", Architecture: "x86_64", Profile: "modern",
		Linkage: "shared", File: "oneui-test.zip", Size: int64(buf.Len()), SHA256: hashBytes(buf.Bytes()), ManifestSHA256: hashBytes(data),
		MinimumOS: "Windows 10", Toolchain: "test compiler", Signature: "unsigned test fixture", Validation: "test fixture"}
	os.WriteFile(filepath.Join(root, a.File), buf.Bytes(), 0600)
	c := Catalog{SchemaVersion: 1, Releases: []Release{{Version: "0.1.0-preview.1", Channel: "preview", ABI: 43, PublishedAt: "2026-10-08", DocsVersion: "0.1.0-preview.1", Artifacts: []Artifact{a}}}}
	b, _ := json.Marshal(c)
	os.WriteFile(filepath.Join(root, "releases.json"), b, 0600)
	os.WriteFile(filepath.Join(root, "private.txt"), []byte("must not be served"), 0600)
	return root, a
}

func TestHTTPDownloadsAndIsolation(t *testing.T) {
	dir, a := fixture(t)
	c, err := loadCatalog(dir)
	if err != nil {
		t.Fatal(err)
	}
	defer c.root.Close()
	handler := newHandler(c, fstest.MapFS{"index.html": {Data: []byte("home")}, "docs/test/index.html": {Data: []byte("docs")}, "_nuxt/app.js": {Data: []byte("script")}})
	for _, tc := range []struct {
		method, url string
		status      int
	}{
		{"GET", "/", 200}, {"GET", "/docs/test", 200}, {"GET", "/missing", 404}, {"GET", "/private.txt", 404},
		{"GET", "/files/private.txt", 404}, {"GET", "/files/%2e%2e%2fprivate.txt", 404},
		{"GET", "/api/private", 404}, {"POST", "/api/releases", 405}, {"HEAD", "/files/" + a.File, 200},
	} {
		t.Run(tc.method+tc.url, func(t *testing.T) {
			w := httptest.NewRecorder()
			handler.ServeHTTP(w, httptest.NewRequest(tc.method, tc.url, nil))
			if w.Code != tc.status {
				t.Fatalf("got %d: %s", w.Code, w.Body.String())
			}
		})
	}
	request := httptest.NewRequest("GET", "/files/"+a.File, nil)
	request.Header.Set("Range", "bytes=0-7")
	w := httptest.NewRecorder()
	handler.ServeHTTP(w, request)
	if w.Code != 206 || w.Body.Len() != 8 || !strings.Contains(w.Header().Get("Content-Disposition"), a.File) {
		t.Fatalf("range response: %d %s", w.Code, w.Body.String())
	}
	request = httptest.NewRequest("GET", "/files/"+a.File, nil)
	request.Header.Set("If-None-Match", `"`+a.SHA256+`"`)
	w = httptest.NewRecorder()
	handler.ServeHTTP(w, request)
	if w.Code != 304 {
		t.Fatal("ETag conditional download failed")
	}
	os.WriteFile(filepath.Join(dir, a.File), []byte("tampered"), 0600)
	w = httptest.NewRecorder()
	handler.ServeHTTP(w, httptest.NewRequest("GET", "/files/"+a.File, nil))
	if w.Code != 503 {
		t.Fatal("modified release must not be served")
	}
	if _, err = loadCatalog(dir); err == nil {
		t.Fatal("startup accepted a corrupt release")
	}
}

func TestCatalogRejectsUnsafeAndDuplicateFiles(t *testing.T) {
	dir, a := fixture(t)
	for _, name := range []string{"../private.zip", "/absolute.zip", "C:private.zip", "a/b.zip", "a\b.zip"} {
		a.File = name
		c := Catalog{SchemaVersion: 1, Releases: []Release{{Version: "test", Channel: "preview", ABI: 43, PublishedAt: "2026-10-08", DocsVersion: "test", Artifacts: []Artifact{a}}}}
		if c.validate() == nil {
			t.Fatalf("accepted %q", name)
		}
	}
	dir, a = fixture(t)
	old := filepath.Join(dir, a.File)
	os.Remove(old)
	if err := os.Symlink(filepath.Join(dir, "private.txt"), old); err != nil {
		t.Skip("symlink privilege unavailable")
	}
	if _, err := loadCatalog(dir); err == nil {
		t.Fatal("accepted symlink download")
	}
}

func TestSDKFetchCacheOfflineAndIntegrity(t *testing.T) {
	dir, a := fixture(t)
	catalog, err := loadCatalog(dir)
	if err != nil {
		t.Fatal(err)
	}
	defer catalog.root.Close()
	requests := 0
	handler := newHandler(catalog, fstest.MapFS{"index.html": {Data: []byte("home")}})
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if strings.HasPrefix(r.URL.Path, "/files/") {
			requests++
		}
		handler.ServeHTTP(w, r)
	}))
	defer server.Close()
	cache := t.TempDir()
	lock := filepath.Join(t.TempDir(), "oneui.lock.json")
	args := []string{"fetch", "--url", server.URL, "--version", "0.1.0-preview.1", "--target", "windows-modern-x86_64", "--cache", cache, "--lock", lock, "--allow-http"}
	if err = runSDK(args); err != nil {
		t.Fatal(err)
	}
	second := append([]string{}, args...)
	for i := range second {
		if second[i] == "--lock" {
			second[i+1] = filepath.Join(t.TempDir(), "second.lock.json")
			break
		}
	}
	if err = runSDK(second); err != nil {
		t.Fatal(err)
	}
	if requests != 1 {
		t.Fatalf("shared cache downloaded %d times", requests)
	}
	server.Close()
	offline := []string{"fetch", "--target", "windows-modern-x86_64", "--cache", cache, "--lock", lock, "--offline"}
	if err = runSDK(offline); err != nil {
		t.Fatal(err)
	}
	os.WriteFile(filepath.Join(cache, a.SHA256, "lib/example.a"), []byte("corrupt"), 0600)
	if err = runSDK(offline); err == nil {
		t.Fatal("offline cache corruption was accepted")
	}
}

func TestArchiveRejectsTraversalDuplicatesAndLinks(t *testing.T) {
	for _, names := range [][]string{{"../escape"}, {"a", "A"}, {"C:evil"}, {"lib\\evil"}} {
		var data bytes.Buffer
		writer := zip.NewWriter(&data)
		for _, name := range names {
			f, _ := writer.Create(name)
			io.WriteString(f, "x")
		}
		writer.Close()
		reader, _ := zip.NewReader(bytes.NewReader(data.Bytes()), int64(data.Len()))
		if extractSDK(reader, t.TempDir()) == nil {
			t.Fatalf("accepted unsafe entries %v", names)
		}
	}
}

func TestClientRequiresTrustedOrigin(t *testing.T) {
	for _, raw := range []string{"http://example.com", "https://user:pass@example.com", "https://example.com/path", "file:///secret"} {
		if _, _, err := sdkHTTPClient(raw, false); err == nil {
			t.Fatalf("accepted %s", raw)
		}
	}
	client, _, err := sdkHTTPClient("https://example.com", false)
	if err != nil {
		t.Fatal(err)
	}
	req, _ := http.NewRequest("GET", "https://elsewhere.example/file", nil)
	if client.CheckRedirect(req, nil) == nil {
		t.Fatal("cross-origin redirect accepted")
	}
}
