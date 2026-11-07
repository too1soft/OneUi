package main

import (
	"context"
	"encoding/json"
	"fmt"
	"net/url"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strings"
	"time"
)

var gitCommit = regexp.MustCompile(`^[0-9a-f]{40}$`)

func readPluginManifest(directory string) (PluginManifest, error) {
	var m PluginManifest
	root, err := os.OpenRoot(directory)
	if err != nil {
		return m, err
	}
	defer root.Close()
	info, err := root.Lstat("oneui-plugin.json")
	if err != nil || !info.Mode().IsRegular() || info.Size() > 1<<20 {
		return m, fmt.Errorf("plugin manifest must be a regular file below 1 MiB")
	}
	f, err := root.Open("oneui-plugin.json")
	if err != nil {
		return m, err
	}
	defer f.Close()
	if err = decodePluginJSON(f, &m, 1<<20); err != nil {
		return m, err
	}
	return m, m.validate()
}
func samePluginManifest(a, b PluginManifest) bool {
	// Empty and omitted optional lists carry identical semantics.
	normalize := func(m PluginManifest) []byte {
		if len(m.Dependencies) == 0 {
			m.Dependencies = nil
		}
		if len(m.Services) == 0 {
			m.Services = nil
		}
		if len(m.NativeDependencies) == 0 {
			m.NativeDependencies = nil
		}
		data, _ := json.Marshal(m)
		return data
	}
	return string(normalize(a)) == string(normalize(b))
}

func pluginSource(directory, cache string, source PluginRequest, offline bool) (string, error) {
	if source.Path != "" {
		path := source.Path
		if !filepath.IsAbs(path) {
			path = filepath.Join(directory, path)
		}
		return filepath.Abs(path)
	}
	if source.Git == "" {
		return "", fmt.Errorf("plugin has no source location")
	}
	u, err := url.Parse(source.Git)
	if err != nil || u.Scheme != "https" || u.Host == "" || u.User != nil || u.RawQuery != "" || u.Fragment != "" || !gitCommit.MatchString(source.Revision) {
		return "", fmt.Errorf("Git plugin requires an HTTPS URL without credentials/query and an exact commit")
	}
	destination := filepath.Join(cache, "git-"+hashBytes([]byte(source.Git+"\n"+source.Revision)))
	if info, err := os.Stat(destination); err == nil && info.IsDir() {
		return destination, verifyGitPlugin(destination, source.Revision)
	}
	if offline {
		return "", fmt.Errorf("pinned Git plugin is not in the local cache")
	}
	if err = os.MkdirAll(cache, 0755); err != nil {
		return "", err
	}
	stage, err := os.MkdirTemp(cache, ".oneui-plugin-git-*")
	if err != nil {
		return "", err
	}
	defer removePluginStage(stage, cache)
	commands := [][]string{{"init", "--quiet", stage}, {"-C", stage, "fetch", "--quiet", "--depth=1", source.Git, source.Revision}, {"-C", stage, "-c", "core.hooksPath=", "checkout", "--quiet", "--detach", "FETCH_HEAD"}}
	for _, args := range commands {
		if _, err = runPluginGit(args...); err != nil {
			return "", fmt.Errorf("cannot fetch pinned Git plugin: %w", err)
		}
	}
	if err = verifyGitPlugin(stage, source.Revision); err != nil {
		return "", err
	}
	if err = os.Rename(stage, destination); err != nil {
		if verifyGitPlugin(destination, source.Revision) != nil {
			return "", err
		}
	}
	return destination, nil
}
func runPluginGit(args ...string) ([]byte, error) {
	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Minute)
	defer cancel()
	command := exec.CommandContext(ctx, "git", args...)
	command.Env = append(os.Environ(), "GIT_TERMINAL_PROMPT=0", "GIT_CONFIG_NOSYSTEM=1")
	// Repository URLs never contain credentials. Avoid returning raw subprocess
	// output, which may include private helpers or proxy configuration.
	return command.Output()
}

func projectNeedsPluginRegistry(directory, cache string, project PluginProject) (bool, error) {
	for id, request := range project.Dependencies {
		if request.Path == "" && request.Git == "" {
			return true, nil
		}
		path, err := pluginSource(directory, cache, request, false)
		if err != nil {
			return false, err
		}
		manifest, err := readPluginManifest(path)
		if err != nil {
			return false, err
		}
		if manifest.ID != id {
			return false, fmt.Errorf("source plugin ID differs from --id")
		}
		for _, dep := range manifest.Dependencies {
			r, exists := project.Dependencies[dep.ID]
			if !exists || (r.Path == "" && r.Git == "") {
				return true, nil
			}
		}
	}
	return false, nil
}
func verifyGitPlugin(directory, commit string) error {
	head, err := runPluginGit("-C", directory, "rev-parse", "HEAD")
	if err != nil || strings.TrimSpace(string(head)) != commit {
		return fmt.Errorf("cached Git commit differs from project lock")
	}
	changed, err := runPluginGit("-C", directory, "status", "--porcelain", "--untracked-files=all")
	if err != nil || len(changed) != 0 {
		return fmt.Errorf("cached Git plugin was modified")
	}
	ignored, err := runPluginGit("-C", directory, "ls-files", "--others", "--ignored", "--exclude-standard")
	if err != nil || len(ignored) != 0 {
		return fmt.Errorf("cached Git plugin contains untracked ignored files")
	}
	return nil
}
func removePluginStage(stage, cache string) {
	resolved, err := filepath.Abs(stage)
	if err != nil {
		return
	}
	boundary, err := filepath.Abs(cache)
	if err != nil {
		return
	}
	rel, err := filepath.Rel(boundary, resolved)
	if err == nil && rel != "." && !strings.HasPrefix(rel, ".."+string(filepath.Separator)) && rel != ".." && strings.HasPrefix(filepath.Base(resolved), ".oneui-plugin-git-") {
		_ = os.RemoveAll(resolved)
	}
}
