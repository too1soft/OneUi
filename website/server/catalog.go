package main

import (
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"os"
	"regexp"
	"strings"
	"time"
)

type Catalog struct {
	SchemaVersion int       `json:"schemaVersion"`
	Releases      []Release `json:"releases"`
	root          *os.Root
	files         map[string]download
}

type Release struct {
	Version      string     `json:"version"`
	Channel      string     `json:"channel"`
	ABI          int        `json:"abi"`
	PublishedAt  string     `json:"publishedAt"`
	DocsVersion  string     `json:"docsVersion"`
	SourceCommit string     `json:"sourceCommit"`
	SourceDirty  bool       `json:"sourceDirty"`
	Notes        []string   `json:"notes"`
	Artifacts    []Artifact `json:"artifacts"`
}

type Artifact struct {
	ID                 string           `json:"id"`
	Platform           string           `json:"platform"`
	Architecture       string           `json:"architecture"`
	Profile            string           `json:"profile"`
	Linkage            string           `json:"linkage"`
	File               string           `json:"file"`
	Size               int64            `json:"size"`
	SHA256             string           `json:"sha256"`
	ManifestSHA256     string           `json:"manifestSha256"`
	MinimumOS          string           `json:"minimumOS"`
	Toolchain          string           `json:"toolchain"`
	Signature          string           `json:"signature"`
	Validation         string           `json:"validation"`
	Limitations        []string         `json:"limitations"`
	SystemDependencies []string         `json:"systemDependencies"`
	Runtime            *RuntimeDownload `json:"runtime,omitempty"`
}

// RuntimeDownload is an optional standalone DLL from the matching SDK archive.
// Older SDK clients ignore this field and continue to consume the ZIP unchanged.
type RuntimeDownload struct {
	File   string `json:"file"`
	Size   int64  `json:"size"`
	SHA256 string `json:"sha256"`
}

type download struct {
	file     string
	sha256   string
	mimeType string
	info     os.FileInfo
}

var safeID = regexp.MustCompile(`^[a-zA-Z0-9][a-zA-Z0-9._-]{0,180}$`)
var shaPattern = regexp.MustCompile(`^[0-9a-f]{64}$`)

func loadCatalog(directory string) (*Catalog, error) {
	root, err := os.OpenRoot(directory)
	if err != nil {
		return nil, err
	}
	success := false
	defer func() {
		if !success {
			root.Close()
		}
	}()
	file, err := root.Open("releases.json")
	if err != nil {
		return nil, fmt.Errorf("releases.json: %w", err)
	}
	defer file.Close()
	var catalog Catalog
	decoder := json.NewDecoder(io.LimitReader(file, 4<<20))
	decoder.DisallowUnknownFields()
	if err := decoder.Decode(&catalog); err != nil {
		return nil, fmt.Errorf("invalid releases.json: %w", err)
	}
	if decoder.Decode(new(any)) != io.EOF {
		return nil, fmt.Errorf("unexpected data after catalog")
	}
	if err := catalog.validate(); err != nil {
		return nil, err
	}
	catalog.root, catalog.files = root, make(map[string]download)
	for _, release := range catalog.Releases {
		for _, artifact := range release.Artifacts {
			if err := catalog.addDownload(artifact.File, artifact.Size, artifact.SHA256, "application/zip"); err != nil {
				return nil, err
			}
			if runtime := artifact.Runtime; runtime != nil {
				if err := catalog.addDownload(runtime.File, runtime.Size, runtime.SHA256, "application/octet-stream"); err != nil {
					return nil, err
				}
			}
		}
	}
	success = true
	return &catalog, nil
}

func (c *Catalog) addDownload(name string, size int64, expectedHash, mimeType string) error {
	info, err := c.root.Lstat(name)
	if err != nil || !info.Mode().IsRegular() {
		return fmt.Errorf("download must be a regular file: %s", name)
	}
	f, err := c.root.Open(name)
	if err != nil {
		return err
	}
	defer f.Close()
	hash := sha256.New()
	n, err := io.Copy(hash, f)
	if err != nil || n != size || hex.EncodeToString(hash.Sum(nil)) != expectedHash {
		return fmt.Errorf("download size or SHA256 mismatch: %s", name)
	}
	c.files[name] = download{name, expectedHash, mimeType, info}
	return nil
}

func (c *Catalog) validate() error {
	if c.SchemaVersion != 1 {
		return fmt.Errorf("unsupported catalog schema")
	}
	versions, files := map[string]bool{}, map[string]bool{}
	for _, r := range c.Releases {
		if !safeID.MatchString(r.Version) || versions[r.Version] || r.ABI <= 0 || r.DocsVersion == "" {
			return fmt.Errorf("invalid or duplicate release: %s", r.Version)
		}
		versions[r.Version] = true
		if r.Channel != "stable" && r.Channel != "preview" && r.Channel != "experimental" {
			return fmt.Errorf("invalid release channel")
		}
		if _, err := time.Parse("2006-01-02", r.PublishedAt); err != nil {
			return fmt.Errorf("invalid publication date")
		}
		ids := map[string]bool{}
		for _, a := range r.Artifacts {
			if !safeID.MatchString(a.ID) || ids[a.ID] || !safeID.MatchString(a.File) || files[strings.ToLower(a.File)] || !strings.HasSuffix(a.File, ".zip") {
				return fmt.Errorf("invalid or duplicate artifact: %s", a.File)
			}
			ids[a.ID], files[strings.ToLower(a.File)] = true, true
			if a.Size <= 0 || a.Size > 2<<30 || !shaPattern.MatchString(a.SHA256) || !shaPattern.MatchString(a.ManifestSHA256) {
				return fmt.Errorf("invalid artifact integrity fields")
			}
			if a.Platform != "windows" && a.Platform != "linux" && a.Platform != "macos" {
				return fmt.Errorf("invalid platform")
			}
			if a.Architecture != "x86" && a.Architecture != "x86_64" && a.Architecture != "arm64" {
				return fmt.Errorf("invalid architecture")
			}
			if a.Linkage != "shared" && a.Linkage != "static" {
				return fmt.Errorf("invalid linkage")
			}
			if a.MinimumOS == "" || a.Toolchain == "" || a.Validation == "" || a.Signature == "" {
				return fmt.Errorf("artifact provenance is incomplete")
			}
			if runtime := a.Runtime; runtime != nil {
				if a.Platform != "windows" || a.Linkage != "shared" || !safeID.MatchString(runtime.File) ||
					!strings.HasSuffix(runtime.File, ".dll") || files[strings.ToLower(runtime.File)] ||
					runtime.Size <= 0 || runtime.Size > 2<<30 || !shaPattern.MatchString(runtime.SHA256) {
					return fmt.Errorf("invalid or duplicate DLL download: %s", runtime.File)
				}
				files[strings.ToLower(runtime.File)] = true
			}
		}
	}
	return nil
}
