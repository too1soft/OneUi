package main

import (
	"archive/zip"
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"testing/fstest"
)

func pluginArchiveFixture(t *testing.T, directory string, manifest PluginManifest) PluginRelease {
	t.Helper()
	encoded, err := json.Marshal(manifest)
	if err != nil {
		t.Fatal(err)
	}
	files := map[string][]byte{"oneui-plugin.json": encoded, "lib/cmake/" + manifest.CMakePackage + "/" + manifest.CMakePackage + "Config.cmake": []byte("add_library(" + manifest.CMakeTarget + " INTERFACE IMPORTED)\n")}
	inventory := []map[string]any{}
	for name, data := range files {
		inventory = append(inventory, map[string]any{"path": name, "size": len(data), "sha256": hashBytes(data)})
	}
	index, _ := json.Marshal(map[string]any{"schemaVersion": 1, "files": inventory})
	files["sdk-manifest.json"] = index
	var buffer bytes.Buffer
	writer := zip.NewWriter(&buffer)
	for name, data := range files {
		out, e := writer.Create(name)
		if e != nil {
			t.Fatal(e)
		}
		if _, e = out.Write(data); e != nil {
			t.Fatal(e)
		}
	}
	if err = writer.Close(); err != nil {
		t.Fatal(err)
	}
	a := testPluginRelease(manifest.ID, manifest.Version).Artifacts[0]
	a.Size = int64(buffer.Len())
	a.SHA256 = hashBytes(buffer.Bytes())
	a.ManifestSHA256 = hashBytes(index)
	if err = os.WriteFile(filepath.Join(directory, a.File), buffer.Bytes(), 0600); err != nil {
		t.Fatal(err)
	}
	return PluginRelease{Manifest: manifest, Description: "Test package", Artifacts: []Artifact{a}}
}
func pluginRegistryFixture(t *testing.T, mutate func(*PluginRelease)) (*httptest.Server, string, Artifact, PluginRelease) {
	t.Helper()
	directory, sdk := fixture(t)
	p := pluginArchiveFixture(t, directory, testPluginManifest("example.plugin", "1.0.0"))
	if mutate != nil {
		mutate(&p)
	}
	encoded, _ := json.Marshal(PluginCatalog{SchemaVersion: 1, Packages: []PluginRelease{p}})
	if e := os.WriteFile(filepath.Join(directory, "plugins.json"), encoded, 0600); e != nil {
		t.Fatal(e)
	}
	c, e := loadCatalog(directory)
	if e != nil {
		t.Fatal(e)
	}
	t.Cleanup(func() { c.root.Close() })
	server := httptest.NewServer(newHandler(c, fstest.MapFS{"index.html": {Data: []byte("home")}}))
	t.Cleanup(server.Close)
	return server, directory, sdk, p
}

func TestPluginInstallOfflineIntegrityRemovalAndSDKLockCoexistence(t *testing.T) {
	server, _, _, release := pluginRegistryFixture(t, nil)
	project, cache, sdkCache := t.TempDir(), t.TempDir(), t.TempDir()
	lockPath := filepath.Join(project, "oneui.lock.json")
	if err := runSDK([]string{"fetch", "--url", server.URL, "--version", "0.1.0-preview.1", "--target", "windows-modern-x86_64", "--cache", sdkCache, "--lock", lockPath, "--allow-http"}); err != nil {
		t.Fatal(err)
	}
	base := []string{"--project", project, "--target", "windows-modern-x86_64", "--cache", cache, "--allow-http"}
	add := append([]string{"add", "--id", "example.plugin", "--version", "^1.0.0", "--url", server.URL}, base...)
	if err := runPlugin(add); err != nil {
		t.Fatal(err)
	}
	lock, err := readPluginLock(project)
	if err != nil || len(lock.Entries) != 1 || len(lock.Plugins) != 1 {
		t.Fatal("SDK lock lost", err)
	}
	if err = runSDK([]string{"fetch", "--target", "windows-modern-x86_64", "--cache", sdkCache, "--lock", lockPath, "--offline"}); err != nil {
		t.Fatal(err)
	}
	lock, err = readPluginLock(project)
	if err != nil || len(lock.Plugins) != 1 {
		t.Fatal("SDK fetch dropped plugin lock", err)
	}
	response, err := http.Get(server.URL + "/api/plugins")
	if err != nil {
		t.Fatal(err)
	}
	defer response.Body.Close()
	var catalog PluginCatalog
	if err = decodePluginJSON(response.Body, &catalog, 8<<20); err != nil || len(catalog.Packages) != 1 {
		t.Fatal("plugin directory API", err)
	}
	server.Close()
	if err = runPlugin(append([]string{"restore", "--offline"}, base...)); err != nil {
		t.Fatal("offline restore attempted network or failed integrity", err)
	}
	generated := filepath.Join(project, ".oneui/plugins/windows-modern-x86_64-shared/OneUIProjectPlugins.cmake")
	data, err := os.ReadFile(generated)
	if err != nil || !bytes.Contains(data, []byte("find_package(Pkg_example_plugin CONFIG REQUIRED)")) || !bytes.Contains(data, []byte("SHA256")) {
		t.Fatal("missing guarded CMake output", err)
	}
	before, _ := os.ReadFile(lockPath)
	corrupt := filepath.Join(cache, release.Artifacts[0].SHA256, "unlisted.cmake")
	if err = os.WriteFile(corrupt, []byte("unexpected build script"), 0600); err != nil {
		t.Fatal(err)
	}
	if err = runPlugin(append([]string{"restore", "--offline"}, base...)); err == nil {
		t.Fatal("unlisted cached file accepted")
	}
	after, _ := os.ReadFile(lockPath)
	if !bytes.Equal(before, after) {
		t.Fatal("failed restore mutated lock")
	}
	if err = runPlugin(append([]string{"remove", "--id", "example.plugin", "--offline"}, base...)); err != nil {
		t.Fatal(err)
	}
	lock, err = readPluginLock(project)
	if err != nil || len(lock.Entries) != 1 || len(lock.Plugins["windows-modern-x86_64-shared"].Packages) != 0 {
		t.Fatal("remove damaged SDK or retained unused graph", err)
	}
	if _, err = os.Stat(corrupt); err != nil {
		t.Fatal("remove deleted a shared cache entry")
	}
}

func TestPluginMetadataMismatchDoesNotChangeProject(t *testing.T) {
	server, _, _, _ := pluginRegistryFixture(t, func(p *PluginRelease) { p.Manifest.Version = "1.1.0" })
	project := t.TempDir()
	err := runPlugin([]string{"add", "--project", project, "--id", "example.plugin", "--version", "^1.0.0", "--target", "windows-modern-x86_64", "--cache", t.TempDir(), "--url", server.URL, "--allow-http"})
	if err == nil || !strings.Contains(err.Error(), "manifest differs") {
		t.Fatal("archive/catalog mismatch accepted", err)
	}
	for _, name := range []string{"oneui.plugins.json", "oneui.lock.json"} {
		if _, err = os.Stat(filepath.Join(project, name)); !os.IsNotExist(err) {
			t.Fatal("failed install changed project", name)
		}
	}
}

func TestPluginLocalSourceWorksWithoutRegistryAndChecksDrift(t *testing.T) {
	project, source := t.TempDir(), t.TempDir()
	m := testPluginManifest("example.local", "1.0.0")
	write := func() {
		data, _ := json.Marshal(m)
		if err := os.WriteFile(filepath.Join(source, "oneui-plugin.json"), data, 0600); err != nil {
			t.Fatal(err)
		}
	}
	write()
	base := []string{"--project", project, "--target", "windows-modern-x86_64", "--cache", t.TempDir()}
	if err := runPlugin(append([]string{"add", "--id", m.ID, "--version", "^1.0.0", "--path", source, "--url", "https://registry.invalid"}, base...)); err != nil {
		t.Fatal("local plugin unnecessarily contacted a registry", err)
	}
	if err := runPlugin(append([]string{"restore", "--offline"}, base...)); err != nil {
		t.Fatal(err)
	}
	m.Version = "1.1.0"
	write()
	if err := runPlugin(append([]string{"restore", "--offline"}, base...)); err == nil {
		t.Fatal("local metadata drift accepted without update")
	}
	if err := runPlugin(append([]string{"restore", "--offline", "--update"}, base...)); err != nil {
		t.Fatal("offline source update failed", err)
	}
}
