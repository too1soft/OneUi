package main

import (
	"archive/zip"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"path"
	"path/filepath"
	"strings"
)

type SDKManifest struct {
	SchemaVersion int `json:"schemaVersion"`
	Files         []struct {
		Path   string `json:"path"`
		Size   int64  `json:"size"`
		SHA256 string `json:"sha256"`
	} `json:"files"`
}

func hashBytes(data []byte) string { sum := sha256.Sum256(data); return hex.EncodeToString(sum[:]) }

func safeArchivePath(name string) bool {
	return name != "" && !strings.ContainsAny(name, "\\:\x00") && !strings.HasPrefix(name, "/") && path.Clean(name) == name && name != ".." && !strings.HasPrefix(name, "../")
}

func verifySDK(directory, expected string) error {
	root, err := os.OpenRoot(directory)
	if err != nil {
		return err
	}
	defer root.Close()
	data, err := root.ReadFile("sdk-manifest.json")
	if err != nil {
		return err
	}
	if hashBytes(data) != expected {
		return fmt.Errorf("SDK manifest hash mismatch")
	}
	var manifest SDKManifest
	if err = json.Unmarshal(data, &manifest); err != nil {
		return err
	}
	if manifest.SchemaVersion != 1 || len(manifest.Files) == 0 {
		return fmt.Errorf("missing SDK file inventory")
	}
	for _, entry := range manifest.Files {
		if !safeArchivePath(entry.Path) || !shaPattern.MatchString(entry.SHA256) {
			return fmt.Errorf("invalid SDK file inventory")
		}
		info, err := root.Lstat(entry.Path)
		if err != nil || !info.Mode().IsRegular() {
			return fmt.Errorf("invalid SDK file %s", entry.Path)
		}
		f, err := root.Open(entry.Path)
		if err != nil {
			return err
		}
		h := sha256.New()
		size, e := io.Copy(h, f)
		f.Close()
		if e != nil || size != entry.Size || hex.EncodeToString(h.Sum(nil)) != entry.SHA256 {
			return fmt.Errorf("SDK file mismatch: %s", entry.Path)
		}
	}
	return nil
}

func fetchSDK(client *http.Client, origin string, artifact Artifact, cache, destination string) (string, error) {
	if artifact.Size <= 0 || artifact.Size > 2<<30 {
		return "", fmt.Errorf("invalid archive size")
	}
	if err := os.MkdirAll(cache, 0755); err != nil {
		return "", err
	}
	stage, err := os.MkdirTemp(cache, ".oneui-fetch-*")
	if err != nil {
		return "", err
	}
	defer os.RemoveAll(stage)
	response, err := client.Get(origin + "/files/" + artifact.File)
	if err != nil {
		return "", err
	}
	defer response.Body.Close()
	if response.StatusCode != 200 {
		return "", fmt.Errorf("download HTTP %d", response.StatusCode)
	}
	f, err := os.Create(filepath.Join(stage, "download.zip"))
	if err != nil {
		return "", err
	}
	h := sha256.New()
	n, err := io.Copy(io.MultiWriter(f, h), io.LimitReader(response.Body, artifact.Size+1))
	f.Close()
	if err != nil {
		return "", err
	}
	if n != artifact.Size || hex.EncodeToString(h.Sum(nil)) != artifact.SHA256 {
		return "", fmt.Errorf("download size or SHA256 mismatch")
	}
	reader, err := zip.OpenReader(filepath.Join(stage, "download.zip"))
	if err != nil {
		return "", err
	}
	defer reader.Close()
	extracted := filepath.Join(stage, "sdk")
	if err = os.Mkdir(extracted, 0755); err != nil {
		return "", err
	}
	if err = extractSDK(&reader.Reader, extracted); err != nil {
		return "", err
	}
	data, err := os.ReadFile(filepath.Join(extracted, "sdk-manifest.json"))
	if err != nil {
		return "", err
	}
	manifestHash := hashBytes(data)
	if err = verifySDK(extracted, manifestHash); err != nil {
		return "", err
	}
	if err = os.Rename(extracted, destination); err != nil {
		// Another session may have installed the identical immutable package.
		if verifySDK(destination, manifestHash) != nil {
			return "", err
		}
	}
	return manifestHash, nil
}

func extractSDK(reader *zip.Reader, directory string) error {
	if len(reader.File) > 10000 {
		return fmt.Errorf("too many archive entries")
	}
	var total uint64
	seen := map[string]bool{}
	for _, entry := range reader.File {
		name := strings.TrimSuffix(entry.Name, "/")
		key := strings.ToLower(name)
		if !safeArchivePath(name) || seen[key] || entry.Mode()&os.ModeSymlink != 0 {
			return fmt.Errorf("unsafe archive entry: %s", entry.Name)
		}
		seen[key] = true
		if entry.UncompressedSize64 > 1<<30 || total+entry.UncompressedSize64 > 1<<30 {
			return fmt.Errorf("unpacked SDK exceeds 1 GiB")
		}
		total += entry.UncompressedSize64
		destination := filepath.Join(directory, filepath.FromSlash(name))
		if entry.FileInfo().IsDir() {
			if err := os.MkdirAll(destination, 0755); err != nil {
				return err
			}
			continue
		}
		if !entry.Mode().IsRegular() {
			return fmt.Errorf("non-regular archive entry")
		}
		if err := os.MkdirAll(filepath.Dir(destination), 0755); err != nil {
			return err
		}
		input, err := entry.Open()
		if err != nil {
			return err
		}
		mode := os.FileMode(0644)
		if entry.Mode()&0111 != 0 {
			mode = 0755
		}
		output, err := os.OpenFile(destination, os.O_WRONLY|os.O_CREATE|os.O_EXCL, mode)
		if err != nil {
			input.Close()
			return err
		}
		_, err = io.Copy(output, io.LimitReader(input, int64(entry.UncompressedSize64)+1))
		closeErr := output.Close()
		input.Close()
		if err != nil {
			return err
		}
		if closeErr != nil {
			return closeErr
		}
	}
	return nil
}
