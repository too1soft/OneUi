package main

import (
	"encoding/json"
	"os"
	"path/filepath"
	"testing"
)

func TestPluginConcurrentAddsPreserveBothRoots(t *testing.T) {
	project, cache := t.TempDir(), t.TempDir()
	errors := make(chan error, 2)
	for _, id := range []string{"example.first", "example.second"} {
		source := t.TempDir()
		data, _ := json.Marshal(testPluginManifest(id, "1.0.0"))
		if err := os.WriteFile(filepath.Join(source, "oneui-plugin.json"), data, 0600); err != nil {
			t.Fatal(err)
		}
		go func(id, source string) {
			errors <- runPlugin([]string{"add", "--project", project, "--id", id, "--version", "^1.0.0", "--path", source, "--target", "windows-modern-x86_64", "--cache", cache, "--offline"})
		}(id, source)
	}
	for i := 0; i < 2; i++ {
		if err := <-errors; err != nil {
			t.Fatal(err)
		}
	}
	declaration, _, err := readPluginProject(project)
	if err != nil || len(declaration.Dependencies) != 2 {
		t.Fatal("concurrent command lost a dependency", err)
	}
	lock, err := readPluginLock(project)
	if err != nil || len(lock.Plugins["windows-modern-x86_64-shared"].Packages) != 2 {
		t.Fatal("concurrent command lost a locked package", err)
	}
}
