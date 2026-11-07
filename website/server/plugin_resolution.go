package main

import "net/http"

// Finite install inputs shared by online resolution and cache-only updates.
// No application/window object or mutable process-global state is exposed.
type pluginInstallPlan struct {
	directory, cache, target, origin string
	offline, allowHTTP               bool
	client                           *http.Client
}

func (plan pluginInstallPlan) resolve(project PluginProject, previous PluginTargetLock, lockedOnly bool) (PluginTargetLock, map[string]string, error) {
	catalog := PluginCatalog{SchemaVersion: 1, Packages: []PluginRelease{}}
	if lockedOnly || plan.offline {
		for _, p := range previous.Packages {
			if p.Artifact != nil {
				catalog.Packages = append(catalog.Packages, PluginRelease{Manifest: p.Manifest, Artifacts: []Artifact{*p.Artifact}})
			}
		}
	} else {
		needsRegistry, err := projectNeedsPluginRegistry(plan.directory, plan.cache, project)
		if err != nil {
			return PluginTargetLock{}, nil, err
		}
		if needsRegistry {
			catalog, err = fetchPluginCatalog(plan.client, plan.origin)
			if err != nil {
				return PluginTargetLock{}, nil, err
			}
		}
	}
	selected, sources, err := preparePluginResolution(plan.directory, plan.cache, plan.target, project, catalog, plan.offline)
	if err != nil {
		return PluginTargetLock{}, nil, err
	}
	next := PluginTargetLock{ProjectSHA256: pluginProjectHash(project), Packages: map[string]LockedPlugin{}}
	paths := map[string]string{}
	for id, p := range selected {
		source := sources[id]
		if source.Version == "" {
			source = PluginRequest{Version: p.Manifest.Version}
		}
		if request, root := project.Dependencies[id]; root {
			source = request
		}
		entry := LockedPlugin{Manifest: p.Manifest, Source: source}
		if p.sourceDirectory == "" {
			entry.BaseURL = plan.origin
			entry.Artifact = pluginArtifact(p, plan.target)
			entry.InventorySHA256 = entry.Artifact.ManifestSHA256
		}
		location, err := installLockedPlugin(plan.directory, plan.cache, entry, plan.offline, plan.allowHTTP)
		if err != nil {
			return PluginTargetLock{}, nil, err
		}
		next.Packages[id] = entry
		paths[id] = location
	}
	return next, paths, nil
}
