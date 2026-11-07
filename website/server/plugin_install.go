package main

import (
	"fmt"
	"net/http"
	"os"
	"path/filepath"
	"strings"
)

func fetchPluginCatalog(client *http.Client, origin string) (PluginCatalog, error) {
	var catalog PluginCatalog
	response, err := client.Get(origin + "/api/plugins")
	if err != nil {
		return catalog, err
	}
	defer response.Body.Close()
	if response.StatusCode != 200 {
		return catalog, fmt.Errorf("plugin catalog HTTP %d", response.StatusCode)
	}
	if err = decodePluginJSON(response.Body, &catalog, 8<<20); err != nil {
		return catalog, err
	}
	return catalog, catalog.validate()
}

func preparePluginResolution(directory, cache, target string, project PluginProject, catalog PluginCatalog, offline bool) (pluginSelection, map[string]PluginRequest, error) {
	requests := map[string]PluginRequest{}
	roots := map[string]string{}
	platform := strings.Split(target, "-")[0]
	for id, request := range project.Dependencies {
		roots[id] = request.Version
		if request.Path == "" && request.Git == "" {
			continue
		}
		path, err := pluginSource(directory, cache, request, offline)
		if err != nil {
			return nil, nil, err
		}
		manifest, err := readPluginManifest(path)
		if err != nil {
			return nil, nil, err
		}
		if manifest.ID != id || !containsString(manifest.Platforms, platform) {
			return nil, nil, fmt.Errorf("source plugin identity/platform mismatch for %s", id)
		}
		// An explicit source overrides registry releases of the same package.
		kept := catalog.Packages[:0]
		for _, p := range catalog.Packages {
			if p.Manifest.ID != id {
				kept = append(kept, p)
			}
		}
		catalog.Packages = kept
		catalog.Packages = append(catalog.Packages, PluginRelease{Manifest: manifest, sourceDirectory: path})
		requests[id] = request
	}
	selected, err := resolvePlugins(catalog, roots, target)
	return selected, requests, err
}

func installLockedPlugin(directory, cache string, p LockedPlugin, offline, allowHTTP bool) (string, error) {
	if err := p.Manifest.validate(); err != nil {
		return "", err
	}
	declaration := PluginProject{SchemaVersion: 1, Dependencies: map[string]PluginRequest{p.Manifest.ID: p.Source}}
	if err := declaration.validate(); err != nil {
		return "", err
	}
	rangeValue, err := parsePluginRange(p.Source.Version)
	version, _ := parsePluginVersion(p.Manifest.Version)
	if err != nil || !rangeValue.accepts(version) {
		return "", fmt.Errorf("locked plugin version violates its source request")
	}
	var path string
	if p.Source.Path != "" || p.Source.Git != "" {
		if p.Artifact != nil {
			return "", fmt.Errorf("locked source plugin also contains a binary artifact")
		}
		path, err = pluginSource(directory, cache, p.Source, offline)
	} else {
		if p.Artifact == nil || p.InventorySHA256 != p.Artifact.ManifestSHA256 {
			return "", fmt.Errorf("invalid locked plugin artifact")
		}
		catalog := PluginCatalog{SchemaVersion: 1, Packages: []PluginRelease{{Manifest: p.Manifest, Artifacts: []Artifact{*p.Artifact}}}}
		if err = catalog.validate(); err != nil {
			return "", err
		}
		client, origin, e := sdkHTTPClient(p.BaseURL, allowHTTP)
		if e != nil {
			return "", e
		}
		path = filepath.Join(cache, p.Artifact.SHA256)
		if err = verifySDK(path, p.InventorySHA256); err != nil {
			if _, statErr := os.Stat(path); statErr == nil {
				return "", fmt.Errorf("cached plugin failed integrity verification: %w", err)
			}
			if offline {
				return "", fmt.Errorf("plugin %s is not available in the verified local cache", p.Manifest.ID)
			}
			hash, e := fetchSDK(client, origin, *p.Artifact, cache, path)
			if e != nil {
				return "", e
			}
			if hash != p.InventorySHA256 {
				return "", fmt.Errorf("plugin inventory differs from project lock")
			}
			err = nil
		}
	}
	if err != nil {
		return "", err
	}
	actual, err := readPluginManifest(path)
	if err != nil {
		return "", err
	}
	if !samePluginManifest(actual, p.Manifest) {
		return "", fmt.Errorf("plugin %s manifest differs from the catalog/lock; use --update for an intentional source change", p.Manifest.ID)
	}
	return path, nil
}

func restorePluginTarget(directory, cache, target string, project PluginProject, lock PluginTargetLock, offline, allowHTTP bool) (map[string]string, error) {
	if lock.ProjectSHA256 != pluginProjectHash(project) || lock.Packages == nil || len(lock.Packages) > 256 {
		return nil, fmt.Errorf("plugin declaration differs from the lock; restore with --update")
	}
	selection := pluginSelection{}
	for id, p := range lock.Packages {
		if p.Manifest.ID != id {
			return nil, fmt.Errorf("locked plugin identity mismatch")
		}
		if !containsString(p.Manifest.Platforms, strings.Split(target, "-")[0]) {
			return nil, fmt.Errorf("locked plugin platform mismatch")
		}
		if p.Artifact != nil && p.Artifact.ID != target {
			return nil, fmt.Errorf("locked plugin target mismatch")
		}
		if p.Artifact != nil {
			selection[id] = PluginRelease{Manifest: p.Manifest, Artifacts: []Artifact{*p.Artifact}}
		} else {
			selection[id] = PluginRelease{Manifest: p.Manifest, sourceDirectory: "locked"}
		}
	}
	roots := map[string]string{}
	for id, r := range project.Dependencies {
		roots[id] = r.Version
	}
	catalog := PluginCatalog{SchemaVersion: 1}
	for _, p := range selection {
		catalog.Packages = append(catalog.Packages, p)
	}
	resolved, err := resolvePlugins(catalog, roots, target)
	if err != nil {
		return nil, err
	}
	if len(resolved) != len(lock.Packages) {
		return nil, fmt.Errorf("lock contains unreachable plugins; restore with --update")
	}
	paths := map[string]string{}
	for id, p := range lock.Packages {
		if request, root := project.Dependencies[id]; root && request != p.Source {
			return nil, fmt.Errorf("locked source differs from plugin declaration")
		}
		path, err := installLockedPlugin(directory, cache, p, offline, allowHTTP)
		if err != nil {
			return nil, err
		}
		paths[id] = path
	}
	return paths, nil
}
