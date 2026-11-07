package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"os"
	"regexp"
)

type PluginDependency struct {
	ID      string `json:"id"`
	Version string `json:"version"`
}
type PluginService struct {
	ID  string `json:"id"`
	ABI int    `json:"abi"`
}
type PluginManifest struct {
	SchemaVersion      int                `json:"schema_version"`
	ID                 string             `json:"id"`
	Version            string             `json:"version"`
	PluginABI          int                `json:"plugin_abi"`
	Kind               string             `json:"kind"`
	License            string             `json:"license"`
	Entry              string             `json:"entry"`
	CMakePackage       string             `json:"cmake_package"`
	CMakeTarget        string             `json:"cmake_target"`
	Services           []PluginService    `json:"services"`
	Dependencies       []PluginDependency `json:"dependencies"`
	NativeDependencies []string           `json:"native_dependencies"`
	Platforms          []string           `json:"platforms"`
}
type PluginRelease struct {
	Manifest        PluginManifest `json:"manifest"`
	Description     string         `json:"description"`
	SourceCommit    string         `json:"sourceCommit"`
	Artifacts       []Artifact     `json:"artifacts"`
	sourceDirectory string
}
type PluginCatalog struct {
	SchemaVersion int             `json:"schemaVersion"`
	Packages      []PluginRelease `json:"packages"`
}

var pluginID = regexp.MustCompile(`^[a-z][a-z0-9_.-]{0,127}$`)
var cIdentifier = regexp.MustCompile(`^[A-Za-z_][A-Za-z0-9_]*$`)
var cmakeTarget = regexp.MustCompile(`^[A-Za-z_][A-Za-z0-9_]*::[A-Za-z_][A-Za-z0-9_.-]*$`)
var pluginPlatforms = map[string]bool{"windows": true, "linux": true, "macos": true, "android": true, "ios": true, "harmonyos": true}

func (m PluginManifest) validate() error {
	if m.SchemaVersion != 1 || m.PluginABI != 1 || !pluginID.MatchString(m.ID) {
		return fmt.Errorf("invalid plugin identity or unsupported manifest/ABI")
	}
	if _, err := parsePluginVersion(m.Version); err != nil {
		return err
	}
	if m.Kind != "service" && m.Kind != "component" && m.Kind != "sdk" {
		return fmt.Errorf("invalid plugin kind")
	}
	if (m.Kind != "sdk" && !cIdentifier.MatchString(m.Entry)) || (m.Kind == "sdk" && m.Entry != "") || !cIdentifier.MatchString(m.CMakePackage) || !cmakeTarget.MatchString(m.CMakeTarget) || m.License == "" || len(m.License) > 128 {
		return fmt.Errorf("invalid plugin entry, CMake metadata or license")
	}
	if len(m.Platforms) == 0 || len(m.Platforms) > len(pluginPlatforms) || len(m.Dependencies) > 64 || len(m.Services) > 256 || len(m.NativeDependencies) > 64 {
		return fmt.Errorf("invalid plugin metadata limits")
	}
	seen := map[string]bool{}
	for _, platform := range m.Platforms {
		if !pluginPlatforms[platform] || seen[platform] {
			return fmt.Errorf("invalid/duplicate plugin platform")
		}
		seen[platform] = true
	}
	seen = map[string]bool{}
	for _, dep := range m.Dependencies {
		if !pluginID.MatchString(dep.ID) || dep.ID == m.ID || seen[dep.ID] {
			return fmt.Errorf("invalid/duplicate plugin dependency")
		}
		if _, err := parsePluginRange(dep.Version); err != nil {
			return fmt.Errorf("dependency %s: %w", dep.ID, err)
		}
		seen[dep.ID] = true
	}
	seen = map[string]bool{}
	for _, service := range m.Services {
		key := fmt.Sprintf("%s/%d", service.ID, service.ABI)
		if !pluginID.MatchString(service.ID) || service.ABI < 1 || service.ABI > 65535 || seen[key] {
			return fmt.Errorf("invalid/duplicate plugin service")
		}
		seen[key] = true
	}
	seen = map[string]bool{}
	for _, name := range m.NativeDependencies {
		if !cIdentifier.MatchString(name) || seen[name] {
			return fmt.Errorf("invalid native package dependency")
		}
		seen[name] = true
	}
	return nil
}

func decodePluginJSON(reader io.Reader, out any, limit int64) error {
	data, err := io.ReadAll(io.LimitReader(reader, limit+1))
	if err != nil {
		return err
	}
	if int64(len(data)) > limit {
		return fmt.Errorf("plugin JSON exceeds size limit")
	}
	decoder := json.NewDecoder(bytes.NewReader(data))
	decoder.DisallowUnknownFields()
	if err := decoder.Decode(out); err != nil {
		return err
	}
	if err := decoder.Decode(new(any)); err != io.EOF {
		return fmt.Errorf("unexpected data after plugin JSON")
	}
	return nil
}

func (c PluginCatalog) validate() error {
	if c.SchemaVersion != 1 || len(c.Packages) > 10000 {
		return fmt.Errorf("unsupported/excessive plugin catalog")
	}
	seen := map[string]bool{}
	for _, release := range c.Packages {
		m := release.Manifest
		if err := m.validate(); err != nil {
			return fmt.Errorf("plugin %s: %w", m.ID, err)
		}
		key := m.ID + "@" + m.Version
		if seen[key] || len(release.Description) > 4096 || len(release.Artifacts) > 64 {
			return fmt.Errorf("duplicate/excessive plugin release")
		}
		seen[key] = true
		artifacts := map[string]bool{}
		for _, a := range release.Artifacts {
			if !safeID.MatchString(a.Profile) || (a.Architecture != "x86_64" && a.Architecture != "arm64" && a.Architecture != "x86" && a.Architecture != "armv7") || a.ID != a.Platform+"-"+a.Profile+"-"+a.Architecture+"-"+a.Linkage {
				return fmt.Errorf("plugin artifact target fields disagree: %s", key)
			}
			if !safeID.MatchString(a.ID) || !safeID.MatchString(a.File) || !shaPattern.MatchString(a.SHA256) || !shaPattern.MatchString(a.ManifestSHA256) || a.Size <= 0 || a.Size > 2<<30 || !pluginPlatforms[a.Platform] || (a.Linkage != "static" && a.Linkage != "shared") || artifacts[a.ID] {
				return fmt.Errorf("invalid/duplicate plugin artifact: %s", key)
			}
			if !containsString(m.Platforms, a.Platform) || a.Toolchain == "" || len(a.Toolchain) > 256 || a.Runtime != nil {
				return fmt.Errorf("incompatible plugin artifact metadata: %s", key)
			}
			artifacts[a.ID] = true
		}
	}
	return nil
}

func containsString(items []string, text string) bool {
	for _, item := range items {
		if item == text {
			return true
		}
	}
	return false
}

func loadPluginCatalog(c *Catalog) error {
	c.plugins = &PluginCatalog{SchemaVersion: 1, Packages: []PluginRelease{}}
	f, err := c.root.Open("plugins.json")
	if os.IsNotExist(err) {
		return nil
	}
	if err != nil {
		return err
	}
	defer f.Close()
	if err = decodePluginJSON(f, c.plugins, 8<<20); err != nil {
		return fmt.Errorf("plugins.json: %w", err)
	}
	if err = c.plugins.validate(); err != nil {
		return err
	}
	for _, release := range c.plugins.Packages {
		for _, a := range release.Artifacts {
			if _, exists := c.files[a.File]; exists {
				return fmt.Errorf("plugin download filename collides: %s", a.File)
			}
			if err = c.addDownload(a.File, a.Size, a.SHA256, "application/zip"); err != nil {
				return err
			}
		}
	}
	return nil
}
