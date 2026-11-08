package main

import (
	"fmt"
	"os"
	"regexp"
)

type ToolDownload struct {
	Platform     string `json:"platform"`
	Architecture string `json:"architecture"`
	Version      string `json:"version"`
	File         string `json:"file"`
	Size         int64  `json:"size"`
	SHA256       string `json:"sha256"`
}
type ToolCatalog struct {
	SchemaVersion int            `json:"schemaVersion"`
	Tools         []ToolDownload `json:"tools"`
}

var toolFile = regexp.MustCompile(`^oneui-site-[0-9]+\.[0-9]+\.[0-9]+(?:-[a-zA-Z0-9.]+)?-(windows-amd64\.exe|linux-amd64|darwin-arm64)$`)

func loadToolCatalog(c *Catalog) error {
	c.tools = &ToolCatalog{SchemaVersion: 1, Tools: []ToolDownload{}}
	file, err := c.root.Open("tools.json")
	if os.IsNotExist(err) {
		return nil
	}
	if err != nil {
		return err
	}
	defer file.Close()
	if err = decodePluginJSON(file, c.tools, 1<<20); err != nil {
		return err
	}
	if c.tools.SchemaVersion != 1 || len(c.tools.Tools) > 16 {
		return fmt.Errorf("invalid tool catalog")
	}
	for _, t := range c.tools.Tools {
		targets := map[string]struct{ architecture, suffix string }{
			"windows": {"x86_64", "windows-amd64.exe"},
			"linux":   {"x86_64", "linux-amd64"},
			"macos":   {"arm64", "darwin-arm64"},
		}
		target, supported := targets[t.Platform]
		if !toolFile.MatchString(t.File) || !supported || t.Architecture != target.architecture || t.Size <= 0 || t.Size > 512<<20 || !shaPattern.MatchString(t.SHA256) {
			return fmt.Errorf("invalid tool download")
		}
		if _, err = parsePluginVersion(t.Version); err != nil {
			return err
		}
		if t.File != "oneui-site-"+t.Version+"-"+target.suffix {
			return fmt.Errorf("tool target metadata differs")
		}
		if _, exists := c.files[t.File]; exists {
			return fmt.Errorf("duplicate tool download")
		}
		if err = c.addDownload(t.File, t.Size, t.SHA256, "application/octet-stream"); err != nil {
			return err
		}
	}
	return nil
}
