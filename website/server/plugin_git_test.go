package main

import (
	"encoding/json"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"
)

func TestPinnedGitPluginOfflineCacheRejectsChangedOrIgnoredFiles(t *testing.T) {
	if _, err := exec.LookPath("git"); err != nil {
		t.Skip("Git not installed")
	}
	repo, cache := t.TempDir(), t.TempDir()
	command := func(args ...string) string {
		t.Helper()
		out, err := exec.Command("git", args...).CombinedOutput()
		if err != nil {
			t.Fatalf("Git fixture: %v", err)
		}
		return strings.TrimSpace(string(out))
	}
	command("init", "--quiet", repo)
	manifest, _ := json.Marshal(testPluginManifest("example.git", "1.0.0"))
	if err := os.WriteFile(filepath.Join(repo, "oneui-plugin.json"), manifest, 0600); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(repo, ".gitignore"), []byte("ignored.txt\n"), 0600); err != nil {
		t.Fatal(err)
	}
	command("-C", repo, "add", ".")
	command("-C", repo, "-c", "user.name=Plugin Test", "-c", "user.email=plugins@example.invalid", "-c", "commit.gpgsign=false", "commit", "--quiet", "-m", "fixture")
	commit := command("-C", repo, "rev-parse", "HEAD")
	request := PluginRequest{Version: "^1.0.0", Git: "https://example.invalid/plugin.git", Revision: commit}
	destination := filepath.Join(cache, "git-"+hashBytes([]byte(request.Git+"\n"+request.Revision)))
	command("clone", "--quiet", repo, destination)
	if _, err := pluginSource(t.TempDir(), cache, request, true); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(destination, "ignored.txt"), []byte("unexpected source"), 0600); err != nil {
		t.Fatal(err)
	}
	if _, err := pluginSource(t.TempDir(), cache, request, true); err == nil {
		t.Fatal("ignored cache file accepted")
	}
	request.Git = "https://user:password@example.invalid/plugin.git"
	if _, err := pluginSource(t.TempDir(), cache, request, true); err == nil {
		t.Fatal("credential-bearing URL accepted")
	}
}
