package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"os"
	"path/filepath"
)

func runPlugin(args []string) error {
	if len(args) == 0 || (args[0] != "add" && args[0] != "remove" && args[0] != "restore" && args[0] != "list") {
		return fmt.Errorf("usage: oneui-site plugin add|remove|restore|list --project DIRECTORY --target PLATFORM-PROFILE-ARCH [--linkage shared|static]")
	}
	flags := flag.NewFlagSet("plugin "+args[0], flag.ContinueOnError)
	directory := flags.String("project", ".", "application project directory")
	id := flags.String("id", "", "plugin ID")
	version := flags.String("version", "", "semantic version constraint")
	target := flags.String("target", "", "SDK target without linkage")
	linkage := flags.String("linkage", "shared", "shared or static")
	base := flags.String("url", "", "plugin registry origin")
	cache := flags.String("cache", "", "shared cache; defaults to user cache/OneUI/plugins")
	path := flags.String("path", "", "local plugin source directory")
	git := flags.String("git", "", "HTTPS Git plugin repository")
	revision := flags.String("revision", "", "exact Git commit")
	offline := flags.Bool("offline", false, "restore only from the project lock and verified cache")
	update := flags.Bool("update", false, "resolve an intentional version or source update")
	allowHTTP := flags.Bool("allow-http", false, "allow trusted local HTTP development registry")
	if err := flags.Parse(args[1:]); err != nil {
		return err
	}
	if flags.NArg() != 0 {
		return fmt.Errorf("unexpected plugin arguments")
	}
	projectDir, err := filepath.Abs(*directory)
	if err != nil {
		return err
	}
	info, err := os.Stat(projectDir)
	if err != nil || !info.IsDir() {
		return fmt.Errorf("project directory must exist")
	}
	unlock, err := acquireProjectLock(filepath.Join(projectDir, "oneui.lock.json"))
	if err != nil {
		return err
	}
	defer unlock()
	project, original, err := readPluginProject(projectDir)
	if err != nil {
		return err
	}
	if args[0] == "list" {
		data, _ := json.MarshalIndent(project, "", "  ")
		fmt.Println(string(data))
		return nil
	}
	if !safeID.MatchString(*target) || (*linkage != "static" && *linkage != "shared") {
		return fmt.Errorf("provide a valid --target and --linkage")
	}
	targetID := *target + "-" + *linkage
	if *cache == "" {
		home, e := os.UserCacheDir()
		if e != nil {
			return e
		}
		*cache = filepath.Join(home, "OneUI", "plugins")
	}
	*cache, err = filepath.Abs(*cache)
	if err != nil {
		return err
	}
	if *base != "" {
		project.Registry = *base
	}
	client, origin, err := sdkHTTPClient(project.Registry, *allowHTTP)
	if err != nil {
		return err
	}
	project.Registry = origin
	lock, err := readPluginLock(projectDir)
	if err != nil {
		return err
	}
	previous, exists := lock.Plugins[targetID]
	if args[0] == "add" {
		if !pluginID.MatchString(*id) || *version == "" {
			return fmt.Errorf("plugin add requires --id and --version")
		}
		project.Dependencies[*id] = PluginRequest{Version: *version, Path: *path, Git: *git, Revision: *revision}
	} else if args[0] == "remove" {
		if _, ok := project.Dependencies[*id]; !ok {
			return fmt.Errorf("plugin is not a direct project dependency")
		}
		delete(project.Dependencies, *id)
	} else if *id != "" || *version != "" || *path != "" || *git != "" || *revision != "" {
		return fmt.Errorf("source options are only supported by plugin add")
	}
	if err = project.validate(); err != nil {
		return err
	}
	var paths map[string]string
	if args[0] == "restore" && exists && !*update {
		paths, err = restorePluginTarget(projectDir, *cache, targetID, project, previous, *offline, *allowHTTP)
	} else {
		plan := pluginInstallPlan{directory: projectDir, cache: *cache, target: targetID, origin: origin, offline: *offline, allowHTTP: *allowHTTP, client: client}
		lock.Plugins[targetID], paths, err = plan.resolve(project, previous, args[0] == "remove")
	}
	if err != nil {
		return err
	}
	// Other targets remain locked to their old declaration until restored with
	// --update. A project hash mismatch prevents silent cross-target drift.
	generated, err := generatePluginCMake(projectDir, targetID, project, lock.Plugins[targetID], paths)
	if err != nil {
		return err
	}
	if err = savePluginProject(projectDir, project, original, lock); err != nil {
		return err
	}
	fmt.Println(generated)
	return nil
}
