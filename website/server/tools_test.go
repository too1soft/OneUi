package main

import (
	"encoding/json"
	"net/http/httptest"
	"os"
	"path/filepath"
	"testing"
	"testing/fstest"
)

func TestToolDownloadsVerifyAndKeepOtherFilesPrivate(t *testing.T) {
	directory, _ := fixture(t)
	data := []byte("cli binary fixture")
	tool := ToolDownload{Platform: "windows", Architecture: "x86_64", Version: "0.1.0-preview.4", File: "oneui-site-0.1.0-preview.4-windows-amd64.exe", Size: int64(len(data)), SHA256: hashBytes(data)}
	path := filepath.Join(directory, tool.File)
	if err := os.WriteFile(path, data, 0600); err != nil {
		t.Fatal(err)
	}
	encoded, _ := json.Marshal(ToolCatalog{SchemaVersion: 1, Tools: []ToolDownload{tool}})
	if err := os.WriteFile(filepath.Join(directory, "tools.json"), encoded, 0600); err != nil {
		t.Fatal(err)
	}
	catalog, err := loadCatalog(directory)
	if err != nil {
		t.Fatal(err)
	}
	defer catalog.root.Close()
	handler := newHandler(catalog, fstest.MapFS{})
	for _, path := range []string{"/api/tools", "/files/" + tool.File} {
		response := httptest.NewRecorder()
		handler.ServeHTTP(response, httptest.NewRequest("GET", path, nil))
		if response.Code != 200 {
			t.Fatal(path, response.Code)
		}
	}
	response := httptest.NewRecorder()
	handler.ServeHTTP(response, httptest.NewRequest("GET", "/files/private.txt", nil))
	if response.Code != 404 {
		t.Fatal("private file exposed")
	}
	if err := os.WriteFile(path, []byte("corrupt"), 0600); err != nil {
		t.Fatal(err)
	}
	if bad, err := loadCatalog(directory); err == nil {
		bad.root.Close()
		t.Fatal("bad CLI hash accepted")
	}
}

func TestComponentPackagesRejectNamespaceConflicts(t *testing.T) {
	first, second := testPluginRelease("a.component", "1.0.0"), testPluginRelease("b.component", "1.0.0")
	for _, p := range []*PluginRelease{&first, &second} {
		p.Manifest.Kind = "component"
		p.Manifest.Components = []PluginComponent{{"Acme_Status", 1}}
		if err := p.Manifest.validate(); err != nil {
			t.Fatal(err)
		}
	}
	if err := validatePluginExports(pluginSelection{"a.component": first, "b.component": second}); err == nil {
		t.Fatal("component collision accepted")
	}
	first.Manifest.Components[0].Name = "NoNamespace"
	if err := first.Manifest.validate(); err == nil {
		t.Fatal("missing component namespace accepted")
	}
}
