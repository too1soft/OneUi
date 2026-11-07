package main

import (
	"strings"
	"testing"
)

func testPluginManifest(id, version string, deps ...PluginDependency) PluginManifest {
	name := strings.ReplaceAll(id, ".", "_")
	return PluginManifest{SchemaVersion: 1, ID: id, Version: version, PluginABI: 1, Kind: "service", License: "MIT", Entry: "example_descriptor_v1", CMakePackage: "Pkg_" + name, CMakeTarget: "Example::" + name, Dependencies: deps, Platforms: []string{"windows", "linux"}}
}
func testPluginRelease(id, version string, deps ...PluginDependency) PluginRelease {
	m := testPluginManifest(id, version, deps...)
	a := Artifact{ID: "windows-modern-x86_64-shared", Platform: "windows", Architecture: "x86_64", Profile: "modern", Linkage: "shared", File: id + "-" + version + ".zip", Size: 1, SHA256: strings.Repeat("a", 64), ManifestSHA256: strings.Repeat("b", 64), Toolchain: "test"}
	return PluginRelease{Manifest: m, Artifacts: []Artifact{a}}
}

func TestPluginSemverOrderingAndRanges(t *testing.T) {
	ordered := []string{"1.0.0-alpha", "1.0.0-alpha.1", "1.0.0-alpha.beta", "1.0.0-beta", "1.0.0-beta.2", "1.0.0-beta.11", "1.0.0-rc.1", "1.0.0", "1.0.1", "2.0.0"}
	for i := 1; i < len(ordered); i++ {
		a, e := parsePluginVersion(ordered[i-1])
		if e != nil {
			t.Fatal(e)
		}
		b, _ := parsePluginVersion(ordered[i])
		if comparePluginVersions(a, b) >= 0 {
			t.Fatal("SemVer precedence", ordered[i])
		}
	}
	for _, text := range []string{"01.0.0", "1.2", "1.0.0-01", "1.0.0-", "v1.2.3", "4294967296.0.0"} {
		if _, e := parsePluginVersion(text); e == nil {
			t.Fatal("accepted invalid SemVer", text)
		}
	}
	for _, tc := range []struct {
		constraint, version string
		want                bool
	}{
		{"^1.2.3", "1.9.0", true}, {"^1.2.3", "2.0.0", false}, {"^0.2.3", "0.3.0", false}, {"^0.0.3", "0.0.4", false},
		{"~1.2.3", "1.3.0", false}, {">=1.0.0 <2.0.0 || =3.0.0", "3.0.0", true}, {"*", "1.0.0-beta", false},
		{"^1.0.0-beta.1", "1.0.0-beta.2", true}, {"^1.0.0-beta.1", "1.1.0-beta", false}, {"=1.0.0+build.1", "1.0.0+build.2", true},
	} {
		r, e := parsePluginRange(tc.constraint)
		if e != nil {
			t.Fatal(e)
		}
		v, _ := parsePluginVersion(tc.version)
		if r.accepts(v) != tc.want {
			t.Fatalf("%s %s", tc.constraint, tc.version)
		}
	}
	for _, invalid := range []string{"", "^1.0", "1.x", "|| 1.0.0", "1.0.0 ||", ">= 1.0.0"} {
		if _, e := parsePluginRange(invalid); e == nil {
			t.Fatal("accepted ambiguous range", invalid)
		}
	}
}

func TestPluginResolutionBacktracksAndRejectsCycles(t *testing.T) {
	catalog := PluginCatalog{SchemaVersion: 1, Packages: []PluginRelease{
		testPluginRelease("a.app", "2.0.0", PluginDependency{"c.base", "^2.0.0"}),
		testPluginRelease("a.app", "1.0.0", PluginDependency{"c.base", "^1.0.0"}),
		testPluginRelease("b.feature", "1.0.0", PluginDependency{"c.base", "^1.0.0"}),
		testPluginRelease("c.base", "2.0.0"), testPluginRelease("c.base", "1.2.0"), testPluginRelease("c.base", "1.1.0"),
	}}
	resolved, err := resolvePlugins(catalog, map[string]string{"a.app": "*", "b.feature": "*"}, "windows-modern-x86_64-shared")
	if err != nil {
		t.Fatal(err)
	}
	if resolved["a.app"].Manifest.Version != "1.0.0" || resolved["c.base"].Manifest.Version != "1.2.0" {
		t.Fatal("did not backtrack to a compatible graph")
	}
	if _, err = resolvePlugins(catalog, map[string]string{"a.app": "2.0.0", "b.feature": "*"}, "windows-modern-x86_64-shared"); err == nil {
		t.Fatal("version conflict accepted")
	}
	if _, err = resolvePlugins(catalog, map[string]string{"a.app": "*"}, "linux-desktop-x86_64-shared"); err == nil {
		t.Fatal("missing platform package accepted")
	}
	cycle := PluginCatalog{SchemaVersion: 1, Packages: []PluginRelease{testPluginRelease("a.plugin", "1.0.0", PluginDependency{"b.plugin", "*"}), testPluginRelease("b.plugin", "1.0.0", PluginDependency{"a.plugin", "*"})}}
	if _, err = resolvePlugins(cycle, map[string]string{"a.plugin": "*"}, "windows-modern-x86_64-shared"); err == nil || !strings.Contains(err.Error(), "cycle") {
		t.Fatal("cycle not diagnosed", err)
	}
}
