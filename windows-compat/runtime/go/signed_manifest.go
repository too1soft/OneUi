package wincompat

import (
	"crypto/ed25519"
	"crypto/sha256"
	"debug/pe"
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strings"
)

// ManifestProof is a detached signature. It is excluded from the payload's
// identity so signing the final manifest cannot introduce a circular hash.
type ManifestProof struct {
	SchemaVersion  int    `json:"schemaVersion"`
	AppKey         string `json:"appKey"`
	ManifestSHA256 string `json:"manifestSHA256"`
	Signature      string `json:"signature"`
}
type ManifestFile struct {
	Path   string `json:"path"`
	Size   int64  `json:"size"`
	SHA256 string `json:"sha256"`
}
type BuildManifest struct {
	SchemaVersion int            `json:"schemaVersion"`
	Product       string         `json:"product"`
	ClientType    string         `json:"clientType"`
	Version       string         `json:"version"`
	Profile       string         `json:"profile"`
	Architecture  string         `json:"architecture"`
	MinimumOS     string         `json:"minimumOS"`
	BuildID       string         `json:"buildId"`
	Files         []ManifestFile `json:"files"`
}

func ManifestSignatureMessage(appKey, manifestHash string) []byte {
	return []byte("oneupdate.windows.manifest.v1\n" + appKey + "\n" + manifestHash)
}
func validDigest(value string) bool {
	raw, err := hex.DecodeString(value)
	return err == nil && len(raw) == 32 && value == strings.ToLower(value)
}
func readLimited(path string, limit int64) ([]byte, error) {
	file, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer file.Close()
	data, err := io.ReadAll(io.LimitReader(file, limit+1))
	if err != nil {
		return nil, err
	}
	if int64(len(data)) > limit {
		return nil, errors.New("manifest exceeds size limit")
	}
	return data, nil
}
func noLinkedPath(path string) error {
	for current := filepath.Clean(path); ; current = filepath.Dir(current) {
		info, err := os.Lstat(current)
		if err != nil {
			return err
		}
		if info.Mode()&os.ModeSymlink != 0 || isReparse(info) {
			return errors.New("linked package path")
		}
		if parent := filepath.Dir(current); parent == current {
			break
		}
	}
	return nil
}
func safeEntry(name string) bool {
	if name == "" || strings.ContainsAny(name, "\\:\x00<>\"|?*") {
		return false
	}
	for _, part := range strings.Split(name, "/") {
		if part == "" || part == "." || part == ".." || strings.HasSuffix(part, ".") || strings.HasSuffix(part, " ") {
			return false
		}
		stem := strings.ToLower(strings.SplitN(part, ".", 2)[0])
		if stem == "con" || stem == "prn" || stem == "aux" || stem == "nul" || (len(stem) == 4 && (strings.HasPrefix(stem, "com") || strings.HasPrefix(stem, "lpt")) && stem[3] >= '0' && stem[3] <= '9') {
			return false
		}
	}
	return true
}

// VerifySignedManifest requires an application-pinned Ed25519 key; a key next
// to the downloaded archive is never a trust source. Nothing is written here.
func VerifySignedManifest(root, appKey, publicKey, product, client string) (BuildManifest, error) {
	return verifyManifest(root, appKey, publicKey, product, client, "")
}

// Installed services keep only their executable and the signed identity next
// to it. Runtime logs, backups and journals are not distribution payload files.
func VerifyInstalledExecutable(binary, appKey, publicKey, product, client string) (BuildManifest, error) {
	return verifyManifest(filepath.Dir(binary), appKey, publicKey, product, client, filepath.Base(binary))
}

func verifyManifest(root, appKey, publicKey, product, client, installedFile string) (BuildManifest, error) {
	var manifest BuildManifest
	root, err := filepath.Abs(root)
	if err != nil {
		return manifest, err
	}
	if err = noLinkedPath(root); err != nil {
		return manifest, err
	}
	raw, err := readLimited(filepath.Join(root, "build-manifest.json"), 8<<20)
	if err != nil {
		return manifest, err
	}
	proofRaw, err := readLimited(filepath.Join(root, "build-manifest.sig"), 8192)
	if err != nil {
		return manifest, err
	}
	var proof ManifestProof
	if err = json.Unmarshal(proofRaw, &proof); err != nil {
		return manifest, err
	}
	key, err := base64.RawURLEncoding.DecodeString(publicKey)
	if err != nil || len(key) != ed25519.PublicKeySize {
		return manifest, errors.New("invalid pinned release key")
	}
	signature, err := base64.RawURLEncoding.DecodeString(proof.Signature)
	digest := sha256.Sum256(raw)
	if err != nil || proof.SchemaVersion != 1 || proof.AppKey != appKey || proof.ManifestSHA256 != hex.EncodeToString(digest[:]) || !ed25519.Verify(key, ManifestSignatureMessage(appKey, proof.ManifestSHA256), signature) {
		return manifest, errors.New("release manifest signature is invalid")
	}
	if err = json.Unmarshal(raw, &manifest); err != nil {
		return manifest, err
	}
	if manifest.SchemaVersion != 1 || manifest.Product != product || manifest.ClientType != client || manifest.Version == "" || !validDigest(manifest.BuildID) || len(manifest.Files) == 0 || len(manifest.Files) > 65536 {
		return manifest, errors.New("invalid signed product identity")
	}
	if manifest.Profile != "legacy" && manifest.Profile != "modern" || manifest.Architecture != "x86" && manifest.Architecture != "x86_64" {
		return manifest, errors.New("unsupported build variant")
	}
	minimum := map[string]string{"legacy": "6.1.7601", "modern": "10.0.0"}[manifest.Profile]
	if manifest.MinimumOS != minimum {
		return manifest, errors.New("system baseline does not match profile")
	}
	expected := map[string]bool{}
	matchedExecutable := false
	for _, entry := range manifest.Files {
		name := strings.ToLower(entry.Path)
		if !safeEntry(entry.Path) || expected[name] || name == "build-manifest.json" || name == "build-manifest.sig" || entry.Size < 0 || !validDigest(entry.SHA256) {
			return manifest, errors.New("invalid payload inventory")
		}
		expected[name] = true
		if installedFile != "" && entry.Path != installedFile {
			continue
		}
		matchedExecutable = true
		path := filepath.Join(root, filepath.FromSlash(entry.Path))
		if err = noLinkedPath(path); err != nil {
			return manifest, err
		}
		file, openErr := os.Open(path)
		if openErr != nil {
			return manifest, openErr
		}
		info, statErr := file.Stat()
		hash := sha256.New()
		_, hashErr := io.Copy(hash, file)
		file.Close()
		if statErr != nil || !info.Mode().IsRegular() || info.Size() != entry.Size || hashErr != nil || hex.EncodeToString(hash.Sum(nil)) != entry.SHA256 {
			return manifest, fmt.Errorf("payload changed: %s", entry.Path)
		}
		if ext := strings.ToLower(filepath.Ext(path)); ext == ".exe" || ext == ".dll" {
			binary, peErr := pe.Open(path)
			if peErr != nil {
				return manifest, peErr
			}
			machine := binary.Machine
			binary.Close()
			want := uint16(pe.IMAGE_FILE_MACHINE_I386)
			if manifest.Architecture == "x86_64" {
				want = pe.IMAGE_FILE_MACHINE_AMD64
			}
			if machine != want {
				return manifest, errors.New("payload mixes architectures")
			}
		}
	}
	if installedFile != "" {
		if !matchedExecutable {
			return manifest, errors.New("installed executable is not in signed inventory")
		}
		return manifest, nil
	}
	actual := map[string]bool{}
	err = filepath.WalkDir(root, func(path string, entry os.DirEntry, walkErr error) error {
		if walkErr != nil {
			return walkErr
		}
		if err := noLinkedPath(path); err != nil {
			return err
		}
		if entry.IsDir() {
			return nil
		}
		name, _ := filepath.Rel(root, path)
		name = strings.ToLower(filepath.ToSlash(name))
		if actual[name] {
			return errors.New("case-colliding package entries")
		}
		actual[name] = true
		if name != "build-manifest.json" && name != "build-manifest.sig" && !expected[name] {
			return fmt.Errorf("unlisted package file: %s", name)
		}
		return nil
	})
	return manifest, err
}

func (m BuildManifest) ValidateHost(host Identity) error {
	if _, err := host.SystemID(); err != nil {
		return err
	}
	if m.Architecture != host.ProcessArchitecture {
		return errors.New("program architecture differs from package")
	}
	if m.Architecture == "x86_64" && host.NativeArchitecture != "x86_64" {
		return errors.New("64-bit package requires 64-bit Windows")
	}
	if m.Profile == "modern" && host.DefaultProfile != "modern" {
		return errors.New("this package requires Windows 10 or later")
	}
	return nil
}
