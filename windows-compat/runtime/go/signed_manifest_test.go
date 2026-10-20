package wincompat

import (
	"crypto/ed25519"
	"crypto/sha256"
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestDetachedManifestBindsApplicationAndEveryPayloadByte(t *testing.T) {
	root := t.TempDir()
	private := ed25519.NewKeyFromSeed(make([]byte, 32))
	public := base64.RawURLEncoding.EncodeToString(private.Public().(ed25519.PublicKey))
	data := []byte("fixture only")
	digest := sha256.Sum256(data)
	m := BuildManifest{SchemaVersion: 1, Product: "wyc", ClientType: "service", Version: "2.4.3", Profile: "legacy", Architecture: "x86", MinimumOS: "6.1.7601", BuildID: strings.Repeat("a", 64), Files: []ManifestFile{{Path: "payload.txt", Size: int64(len(data)), SHA256: hex.EncodeToString(digest[:])}}}
	raw, _ := json.Marshal(m)
	digest = sha256.Sum256(raw)
	hash := hex.EncodeToString(digest[:])
	proof, _ := json.Marshal(ManifestProof{1, "fixture_app", hash, base64.RawURLEncoding.EncodeToString(ed25519.Sign(private, ManifestSignatureMessage("fixture_app", hash)))})
	for name, content := range map[string][]byte{"build-manifest.json": raw, "build-manifest.sig": proof, "payload.txt": data} {
		if err := os.WriteFile(filepath.Join(root, name), content, 0600); err != nil {
			t.Fatal(err)
		}
	}
	if _, err := VerifySignedManifest(root, "fixture_app", public, "wyc", "service"); err != nil {
		t.Fatal(err)
	}
	if _, err := VerifySignedManifest(root, "other_app", public, "wyc", "service"); err == nil {
		t.Fatal("cross-application signature accepted")
	}
	if _, err := VerifySignedManifest(root, "fixture_app", public, "wyc", "gui"); err == nil {
		t.Fatal("wrong client accepted")
	}
	if err := os.WriteFile(filepath.Join(root, "extra.txt"), data, 0600); err != nil {
		t.Fatal(err)
	}
	if _, err := VerifySignedManifest(root, "fixture_app", public, "wyc", "service"); err == nil {
		t.Fatal("extra file accepted")
	}
	if err := os.Remove(filepath.Join(root, "extra.txt")); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(root, "payload.txt"), []byte("changed data"), 0600); err != nil {
		t.Fatal(err)
	}
	if _, err := VerifySignedManifest(root, "fixture_app", public, "wyc", "service"); err == nil {
		t.Fatal("tampering accepted")
	}
}

func TestIdentityAndPackageProfilesAgree(t *testing.T) {
	for _, row := range []struct {
		identity Identity
		system   string
	}{
		{Identity{Major: 6, Minor: 1, Build: 7601, ServicePack: 1, NativeArchitecture: "x86", ProcessArchitecture: "x86", DefaultProfile: "legacy"}, "win7-sp1-x86"},
		{Identity{Major: 6, Minor: 2, Build: 9200, NativeArchitecture: "x86_64", ProcessArchitecture: "x86", DefaultProfile: "legacy"}, "win8-x86_64"},
		{Identity{Major: 10, Build: 10240, NativeArchitecture: "x86", ProcessArchitecture: "x86", DefaultProfile: "modern"}, "win10-x86"},
		{Identity{Major: 10, Build: 22631, NativeArchitecture: "x86_64", ProcessArchitecture: "x86", DefaultProfile: "modern"}, "win11-x86_64"},
	} {
		got, err := row.identity.SystemID()
		if err != nil || got != row.system {
			t.Fatalf("%s %v", got, err)
		}
		m := BuildManifest{Profile: "legacy", Architecture: "x86"}
		if err = m.ValidateHost(row.identity); err != nil {
			t.Fatal(err)
		}
		m.Profile = "modern"
		if err = m.ValidateHost(row.identity); (err != nil) != (row.identity.DefaultProfile == "legacy") {
			t.Fatal("profile mismatch")
		}
	}
	for _, name := range []string{"../secret", "a:b", "NUL.txt", "x/../y", "a\\b", "bad.", "/absolute"} {
		if safeEntry(name) {
			t.Fatal(name)
		}
	}
	if _, err := profile(11, 0, 30000, 0); err == nil {
		t.Fatal("guessed future Windows version")
	}
}
