package main

import (
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"os"
	"path/filepath"
	"strings"
	"time"
)

type SDKLock struct {
	SchemaVersion int                  `json:"schemaVersion"`
	Entries       map[string]LockedSDK `json:"entries"`
}

type LockedSDK struct {
	Version        string   `json:"version"`
	BaseURL        string   `json:"baseURL"`
	Artifact       Artifact `json:"artifact"`
	ManifestSHA256 string   `json:"manifestSha256"`
}

func runSDK(args []string) error {
	if len(args) == 0 || args[0] != "fetch" {
		return fmt.Errorf("usage: oneui-site sdk fetch --url https://your-site --version VERSION --target PLATFORM-PROFILE-ARCH [--linkage shared|static] [--offline]")
	}
	f := flag.NewFlagSet("sdk fetch", flag.ContinueOnError)
	base := f.String("url", "", "OneUI site origin (HTTPS)")
	version := f.String("version", "", "exact release version; defaults to existing project lock")
	target := f.String("target", "", "target, e.g. windows-modern-x86_64")
	linkage := f.String("linkage", "shared", "shared or static")
	lockPath := f.String("lock", "oneui.lock.json", "project lock file (commit this file)")
	cache := f.String("cache", "", "shared cache directory; defaults to user cache/OneUI/sdk")
	offline := f.Bool("offline", false, "use only the verified local cache and project lock")
	update := f.Bool("update", false, "explicitly replace the selected locked version")
	allowHTTP := f.Bool("allow-http", false, "allow unencrypted HTTP for a trusted development server")
	if err := f.Parse(args[1:]); err != nil {
		return err
	}
	if f.NArg() != 0 || !safeID.MatchString(*target) || (*linkage != "shared" && *linkage != "static") {
		return fmt.Errorf("provide a valid --target and --linkage")
	}
	id := *target + "-" + *linkage
	// A shared project lock is updated as one transaction across agent sessions.
	unlock, err := acquireProjectLock(*lockPath)
	if err != nil {
		return err
	}
	defer unlock()
	lock := SDKLock{SchemaVersion: 1, Entries: map[string]LockedSDK{}}
	if data, err := os.ReadFile(*lockPath); err == nil {
		if err = json.Unmarshal(data, &lock); err != nil || lock.SchemaVersion != 1 || lock.Entries == nil {
			return fmt.Errorf("invalid project lock")
		}
	} else if !errors.Is(err, os.ErrNotExist) {
		return err
	}
	entry, exists := lock.Entries[id]
	if exists && !*update {
		if *version != "" && *version != entry.Version {
			return fmt.Errorf("version differs from project lock; pass --update to upgrade intentionally")
		}
		if *base != "" && strings.TrimRight(*base, "/") != entry.BaseURL {
			return fmt.Errorf("URL differs from project lock; pass --update to change it intentionally")
		}
	} else {
		if *offline {
			return fmt.Errorf("offline mode requires an existing lock entry")
		}
		if *base == "" || *version == "" {
			return fmt.Errorf("a new lock requires --url and exact --version")
		}
		client, origin, err := sdkHTTPClient(*base, *allowHTTP)
		if err != nil {
			return err
		}
		response, err := client.Get(origin + "/api/releases")
		if err != nil {
			return err
		}
		defer response.Body.Close()
		if response.StatusCode != 200 {
			return fmt.Errorf("catalog HTTP %d", response.StatusCode)
		}
		var catalog Catalog
		if err = json.NewDecoder(io.LimitReader(response.Body, 4<<20)).Decode(&catalog); err != nil {
			return err
		}
		if err = catalog.validate(); err != nil {
			return err
		}
		found := false
		for _, release := range catalog.Releases {
			if release.Version == *version {
				for _, a := range release.Artifacts {
					if a.ID == id {
						entry = LockedSDK{Version: release.Version, BaseURL: origin, Artifact: a, ManifestSHA256: a.ManifestSHA256}
						found = true
					}
				}
			}
		}
		if !found {
			return fmt.Errorf("release %s has no %s package", *version, id)
		}
	}
	if !shaPattern.MatchString(entry.Artifact.SHA256) || !safeID.MatchString(entry.Artifact.File) || entry.Artifact.ID != id {
		return fmt.Errorf("invalid locked artifact")
	}
	if *cache == "" {
		home, err := os.UserCacheDir()
		if err != nil {
			return err
		}
		*cache = filepath.Join(home, "OneUI", "sdk")
	}
	root, err := filepath.Abs(*cache)
	if err != nil {
		return err
	}
	destination := filepath.Join(root, entry.Artifact.SHA256)
	if entry.ManifestSHA256 != "" {
		if err = verifySDK(destination, entry.ManifestSHA256); err == nil {
			lock.Entries[id] = entry
			data, e := json.MarshalIndent(lock, "", "  ")
			if e != nil {
				return e
			}
			if e = atomicWrite(*lockPath, append(data, '\n')); e != nil {
				return e
			}
			fmt.Println(destination)
			return nil
		}
		if _, statErr := os.Stat(destination); statErr == nil {
			return fmt.Errorf("cached SDK failed verification: %w; remove this corrupt cache entry before downloading again", err)
		}
	}
	if *offline {
		return fmt.Errorf("verified SDK is not available in the local cache")
	}
	client, origin, err := sdkHTTPClient(entry.BaseURL, *allowHTTP)
	if err != nil {
		return err
	}
	manifestHash, err := fetchSDK(client, origin, entry.Artifact, root, destination)
	if err != nil {
		return err
	}
	if entry.ManifestSHA256 != "" && entry.ManifestSHA256 != manifestHash {
		return fmt.Errorf("archive manifest differs from project lock")
	}
	entry.ManifestSHA256 = manifestHash
	lock.Entries[id] = entry
	data, err := json.MarshalIndent(lock, "", "  ")
	if err != nil {
		return err
	}
	if err = atomicWrite(*lockPath, append(data, '\n')); err != nil {
		return err
	}
	fmt.Println(destination)
	return nil
}

func acquireProjectLock(name string) (func(), error) {
	guard := name + ".writing"
	deadline := time.Now().Add(15 * time.Second)
	for {
		file, err := os.OpenFile(guard, os.O_CREATE|os.O_EXCL|os.O_WRONLY, 0600)
		if err == nil {
			fmt.Fprintf(file, "pid=%d\n", os.Getpid())
			file.Close()
			return func() { _ = os.Remove(guard) }, nil
		}
		if !errors.Is(err, os.ErrExist) {
			return nil, err
		}
		if time.Now().After(deadline) {
			return nil, fmt.Errorf("project lock is busy: %s; if the owning process stopped, remove its stale .writing file", guard)
		}
		time.Sleep(100 * time.Millisecond)
	}
}

func sdkHTTPClient(raw string, allowHTTP bool) (*http.Client, string, error) {
	u, err := url.Parse(strings.TrimRight(raw, "/"))
	if err != nil || u.Host == "" || u.User != nil || u.RawQuery != "" || u.Fragment != "" || (u.Path != "" && u.Path != "/") {
		return nil, "", fmt.Errorf("URL must be a site origin without credentials, path or query")
	}
	if u.Scheme != "https" && !(allowHTTP && u.Scheme == "http") {
		return nil, "", fmt.Errorf("HTTPS is required; --allow-http is only for trusted development servers")
	}
	origin := u.Scheme + "://" + u.Host
	client := &http.Client{Timeout: 10 * time.Minute, CheckRedirect: func(req *http.Request, via []*http.Request) error {
		if len(via) > 3 || req.URL.Scheme != u.Scheme || req.URL.Host != u.Host {
			return fmt.Errorf("cross-origin or excessive redirect refused")
		}
		return nil
	}}
	return client, origin, nil
}

func atomicWrite(name string, data []byte) error {
	dir := filepath.Dir(name)
	f, err := os.CreateTemp(dir, ".oneui-lock-*")
	if err != nil {
		return err
	}
	temp := f.Name()
	defer os.Remove(temp)
	if _, err = f.Write(data); err != nil {
		f.Close()
		return err
	}
	if err = f.Sync(); err != nil {
		f.Close()
		return err
	}
	if err = f.Close(); err != nil {
		return err
	}
	return os.Rename(temp, name)
}
