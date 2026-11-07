package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"sort"
)

type PluginRequest struct {
	Version  string `json:"version"`
	Path     string `json:"path,omitempty"`
	Git      string `json:"git,omitempty"`
	Revision string `json:"revision,omitempty"`
}
type PluginProject struct {
	SchemaVersion int                      `json:"schemaVersion"`
	Registry      string                   `json:"registry"`
	Dependencies  map[string]PluginRequest `json:"dependencies"`
}
type LockedPlugin struct {
	Manifest        PluginManifest `json:"manifest"`
	Source          PluginRequest  `json:"source"`
	BaseURL         string         `json:"baseURL,omitempty"`
	Artifact        *Artifact      `json:"artifact,omitempty"`
	InventorySHA256 string         `json:"inventorySha256,omitempty"`
}
type PluginTargetLock struct {
	ProjectSHA256 string                  `json:"projectSha256"`
	Packages      map[string]LockedPlugin `json:"packages"`
}

func readPluginProject(directory string) (PluginProject, []byte, error) {
	project := PluginProject{SchemaVersion: 1, Registry: "https://oneui.too1.net", Dependencies: map[string]PluginRequest{}}
	data, err := os.ReadFile(filepath.Join(directory, "oneui.plugins.json"))
	if errors.Is(err, os.ErrNotExist) {
		return project, nil, nil
	}
	if err != nil {
		return project, nil, err
	}
	if err = decodePluginJSON(bytes.NewReader(data), &project, 1<<20); err != nil {
		return project, nil, err
	}
	if err = project.validate(); err != nil {
		return project, nil, err
	}
	return project, data, nil
}
func (p PluginProject) validate() error {
	if p.SchemaVersion != 1 || p.Dependencies == nil || len(p.Dependencies) > 256 {
		return fmt.Errorf("invalid plugin project")
	}
	for id, request := range p.Dependencies {
		if !pluginID.MatchString(id) {
			return fmt.Errorf("invalid plugin ID")
		}
		if _, err := parsePluginRange(request.Version); err != nil {
			return err
		}
		if request.Path != "" && request.Git != "" {
			return fmt.Errorf("plugin %s has two source locations", id)
		}
		if (request.Git == "") != (request.Revision == "") || (request.Git != "" && !gitCommit.MatchString(request.Revision)) {
			return fmt.Errorf("Git plugins require an exact 40-character commit")
		}
	}
	return nil
}
func pluginProjectHash(p PluginProject) string { data, _ := json.Marshal(p); return hashBytes(data) }

func readPluginLock(directory string) (SDKLock, error) {
	lock := SDKLock{SchemaVersion: 1, Entries: map[string]LockedSDK{}, Plugins: map[string]PluginTargetLock{}}
	data, err := os.ReadFile(filepath.Join(directory, "oneui.lock.json"))
	if errors.Is(err, os.ErrNotExist) {
		return lock, nil
	}
	if err != nil {
		return lock, err
	}
	if len(data) > 16<<20 {
		return lock, fmt.Errorf("project lock exceeds size limit")
	}
	if err = json.Unmarshal(data, &lock); err != nil || lock.SchemaVersion != 1 || lock.Entries == nil {
		return lock, fmt.Errorf("invalid project lock")
	}
	if lock.Plugins == nil {
		lock.Plugins = map[string]PluginTargetLock{}
	}
	return lock, nil
}

func savePluginProject(directory string, project PluginProject, original []byte, lock SDKLock) error {
	path := filepath.Join(directory, "oneui.plugins.json")
	current, err := os.ReadFile(path)
	if err != nil && !errors.Is(err, os.ErrNotExist) {
		return err
	}
	if !bytes.Equal(current, original) {
		return fmt.Errorf("plugin project changed during installation; run the command again")
	}
	data, err := json.MarshalIndent(project, "", "  ")
	if err != nil {
		return err
	}
	encoded, err := json.MarshalIndent(lock, "", "  ")
	if err != nil {
		return err
	}
	if err = atomicWrite(path, append(data, '\n')); err != nil {
		return err
	}
	if err = atomicWrite(filepath.Join(directory, "oneui.lock.json"), append(encoded, '\n')); err != nil {
		var rollback error
		if original == nil {
			rollback = os.Remove(path)
		} else {
			rollback = atomicWrite(path, original)
		}
		if rollback != nil {
			return fmt.Errorf("lock update failed (%v); project rollback failed (%v)", err, rollback)
		}
		return err
	}
	return nil
}

func pluginOrder(packages map[string]LockedPlugin) ([]string, error) {
	marks := map[string]int{}
	var result []string
	var visit func(string) error
	visit = func(id string) error {
		if marks[id] == 2 {
			return nil
		}
		if marks[id] == 1 {
			return fmt.Errorf("plugin dependency cycle at %s", id)
		}
		p, ok := packages[id]
		if !ok {
			return fmt.Errorf("missing locked plugin %s", id)
		}
		marks[id] = 1
		for _, dep := range p.Manifest.Dependencies {
			if err := visit(dep.ID); err != nil {
				return err
			}
		}
		marks[id] = 2
		result = append(result, id)
		return nil
	}
	ids := make([]string, 0, len(packages))
	for id := range packages {
		ids = append(ids, id)
	}
	sort.Strings(ids)
	for _, id := range ids {
		if err := visit(id); err != nil {
			return nil, err
		}
	}
	return result, nil
}
