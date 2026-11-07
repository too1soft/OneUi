package main

import (
	"fmt"
	"sort"
)

type pluginRequirement struct {
	rangeValue pluginRange
	source     string
}
type pluginSelection map[string]PluginRelease

func resolvePlugins(catalog PluginCatalog, roots map[string]string, target string) (pluginSelection, error) {
	if err := catalog.validate(); err != nil {
		return nil, err
	}
	if len(roots) > 256 || !safeID.MatchString(target) {
		return nil, fmt.Errorf("invalid plugin roots or target")
	}
	available := map[string][]PluginRelease{}
	for _, p := range catalog.Packages {
		if pluginArtifact(p, target) == nil && p.sourceDirectory == "" {
			continue
		}
		available[p.Manifest.ID] = append(available[p.Manifest.ID], p)
	}
	for id := range available {
		sort.Slice(available[id], func(i, j int) bool {
			a, b := available[id][i].Manifest.Version, available[id][j].Manifest.Version
			va, _ := parsePluginVersion(a)
			vb, _ := parsePluginVersion(b)
			if cmp := comparePluginVersions(va, vb); cmp != 0 {
				return cmp > 0
			}
			return a > b
		})
	}
	requirements := map[string][]pluginRequirement{}
	for id, constraint := range roots {
		if !pluginID.MatchString(id) {
			return nil, fmt.Errorf("invalid plugin ID: %s", id)
		}
		r, err := parsePluginRange(constraint)
		if err != nil {
			return nil, fmt.Errorf("%s: %w", id, err)
		}
		requirements[id] = []pluginRequirement{{r, "application"}}
	}
	steps := 0
	var last string
	var search func(pluginSelection, map[string][]pluginRequirement) (pluginSelection, bool)
	search = func(selected pluginSelection, requirements map[string][]pluginRequirement) (pluginSelection, bool) {
		steps++
		if steps > 10000 {
			last = "dependency resolution exceeded 10000 decisions"
			return nil, false
		}
		if len(requirements) > 256 {
			last = "dependency graph exceeds 256 plugins"
			return nil, false
		}
		ids := make([]string, 0, len(requirements))
		for id := range requirements {
			ids = append(ids, id)
		}
		sort.Strings(ids)
		for _, id := range ids {
			if p, ok := selected[id]; ok {
				if !satisfiesPlugin(p, requirements[id]) {
					last = "version conflict for " + id
					return nil, false
				}
			}
		}
		missing := ""
		for _, id := range ids {
			if _, ok := selected[id]; !ok {
				missing = id
				break
			}
		}
		if missing == "" {
			if err := validatePluginCycles(selected); err != nil {
				last = err.Error()
				return nil, false
			}
			if err := validatePluginExports(selected); err != nil {
				last = err.Error()
				return nil, false
			}
			return selected, true
		}
		if len(available[missing]) == 0 {
			last = "plugin " + missing + " has no package for " + target
			return nil, false
		}
		for _, candidate := range available[missing] {
			if !satisfiesPlugin(candidate, requirements[missing]) {
				continue
			}
			next := make(pluginSelection, len(selected)+1)
			for id, p := range selected {
				next[id] = p
			}
			next[missing] = candidate
			rules := make(map[string][]pluginRequirement, len(requirements))
			for id, r := range requirements {
				rules[id] = append([]pluginRequirement(nil), r...)
			}
			for _, dep := range candidate.Manifest.Dependencies {
				r, _ := parsePluginRange(dep.Version)
				rules[dep.ID] = append(rules[dep.ID], pluginRequirement{r, missing})
			}
			if result, ok := search(next, rules); ok {
				return result, true
			}
			if steps > 10000 {
				break
			}
		}
		if last == "" {
			last = "no compatible version for " + missing
		}
		return nil, false
	}
	if result, ok := search(pluginSelection{}, requirements); ok {
		return result, nil
	}
	return nil, fmt.Errorf("cannot resolve plugins: %s", last)
}

func validatePluginExports(selected pluginSelection) error {
	targets, packages, services := map[string]string{}, map[string]string{}, map[string]string{}
	components := map[string]string{}
	for id, p := range selected {
		for _, entry := range []struct {
			key   string
			table map[string]string
		}{{p.Manifest.CMakeTarget, targets}, {p.Manifest.CMakePackage, packages}} {
			if previous, exists := entry.table[entry.key]; exists {
				return fmt.Errorf("plugin build export conflict between %s and %s: %s", previous, id, entry.key)
			}
			entry.table[entry.key] = id
		}
		for _, service := range p.Manifest.Services {
			key := fmt.Sprintf("%s/%d", service.ID, service.ABI)
			if previous, exists := services[key]; exists {
				return fmt.Errorf("service provider conflict between %s and %s: %s", previous, id, key)
			}
			services[key] = id
		}
		for _, component := range p.Manifest.Components {
			if previous, exists := components[component.Name]; exists {
				return fmt.Errorf("component name conflict between %s and %s: %s", previous, id, component.Name)
			}
			components[component.Name] = id
		}
	}
	return nil
}

func pluginArtifact(release PluginRelease, target string) *Artifact {
	for i := range release.Artifacts {
		if release.Artifacts[i].ID == target {
			a := release.Artifacts[i]
			return &a
		}
	}
	return nil
}
func satisfiesPlugin(p PluginRelease, rules []pluginRequirement) bool {
	v, _ := parsePluginVersion(p.Manifest.Version)
	for _, r := range rules {
		if !r.rangeValue.accepts(v) {
			return false
		}
	}
	return true
}

func validatePluginCycles(selected pluginSelection) error {
	marks := map[string]int{}
	var visit func(string) error
	visit = func(id string) error {
		if marks[id] == 1 {
			return fmt.Errorf("plugin dependency cycle at %s", id)
		}
		if marks[id] == 2 {
			return nil
		}
		p, exists := selected[id]
		if !exists {
			return fmt.Errorf("missing plugin dependency %s", id)
		}
		marks[id] = 1
		for _, dep := range p.Manifest.Dependencies {
			if err := visit(dep.ID); err != nil {
				return err
			}
		}
		marks[id] = 2
		return nil
	}
	ids := make([]string, 0, len(selected))
	for id := range selected {
		ids = append(ids, id)
	}
	sort.Strings(ids)
	for _, id := range ids {
		if err := visit(id); err != nil {
			return err
		}
	}
	return nil
}
