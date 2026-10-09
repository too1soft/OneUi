package main

import (
	"archive/zip"
	"encoding/json"
	"io"
	"io/fs"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"testing"
)

// Opt-in release check exercises the real embedded pages and every download.
// It calls the HTTP handler in process; it does not automate a browser.
func TestPreparedDelivery(t *testing.T) {
	dir := os.Getenv("ONEUI_DELIVERY_DIR")
	if dir == "" {
		t.Skip("set ONEUI_DELIVERY_DIR to verify prepared delivery")
	}
	catalog, err := loadCatalog(dir)
	if err != nil {
		t.Fatal(err)
	}
	defer catalog.root.Close()
	assets, err := fs.Sub(webFiles, "web")
	if err != nil {
		t.Fatal(err)
	}
	handler := newHandler(catalog, assets)
	for url, text := range map[string]string{
		"/": "下载 DLL / SDK", "/downloads": "下载 OneUI",
		"/docs/sdk-quick-start": "oneui.too1.net", "/docs/portal-deployment": "oneui.too1.net",
	} {
		w := httptest.NewRecorder()
		handler.ServeHTTP(w, httptest.NewRequest("GET", url, nil))
		if w.Code != 200 || !strings.Contains(w.Body.String(), text) {
			t.Fatalf("embedded route %s incomplete: %d", url, w.Code)
		}
	}
	for filename, download := range catalog.files {
		w := httptest.NewRecorder()
		handler.ServeHTTP(w, httptest.NewRequest("HEAD", "/files/"+filename, nil))
		if w.Code != 200 || w.Header().Get("Content-Length") != strconv.FormatInt(download.info.Size(), 10) {
			t.Fatalf("HEAD %s failed", filename)
		}
		req := httptest.NewRequest("GET", "/files/"+filename, nil)
		req.Header.Set("Range", "bytes=0-31")
		w = httptest.NewRecorder()
		handler.ServeHTTP(w, req)
		if w.Code != 206 || w.Body.Len() != 32 {
			t.Fatalf("Range %s failed", filename)
		}
	}
	runtimes := 0
	for _, release := range catalog.Releases {
		for _, a := range release.Artifacts {
			if a.Runtime == nil {
				continue
			}
			runtimes++
			archive, err := zip.OpenReader(filepath.Join(dir, a.File))
			if err != nil {
				t.Fatal(err)
			}
			found := false
			for _, file := range archive.File {
				if file.Name != "bin/oneui.dll" {
					continue
				}
				stream, err := file.Open()
				if err != nil {
					t.Fatal(err)
				}
				payload, err := io.ReadAll(stream)
				stream.Close()
				if err != nil || hashBytes(payload) != a.Runtime.SHA256 {
					t.Fatalf("DLL differs from SDK: %s", a.File)
				}
				found = true
			}
			archive.Close()
			if !found {
				t.Fatalf("DLL absent from SDK: %s", a.File)
			}
			w := httptest.NewRecorder()
			handler.ServeHTTP(w, httptest.NewRequest("GET", "/files/"+a.Runtime.File, nil))
			if w.Code != 200 || hashBytes(w.Body.Bytes()) != a.Runtime.SHA256 {
				t.Fatalf("DLL HTTP payload mismatch: %s", a.Runtime.File)
			}
		}
	}
	w := httptest.NewRecorder()
	handler.ServeHTTP(w, httptest.NewRequest("GET", "/healthz", nil))
	var health struct {
		Downloads int `json:"downloads"`
	}
	if err := json.Unmarshal(w.Body.Bytes(), &health); err != nil || health.Downloads != len(catalog.files) {
		t.Fatal("health download count mismatch")
	}
	if runtimes == 0 {
		t.Fatal("delivery has no DLLs")
	}
	t.Logf("verified %d downloads including %d DLLs against immutable SDK archives", len(catalog.files), runtimes)
}
