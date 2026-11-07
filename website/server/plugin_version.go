package main

import (
	"fmt"
	"regexp"
	"strconv"
	"strings"
)

type pluginVersion struct {
	major, minor, patch uint64
	pre                 string
}

var pluginSemver = regexp.MustCompile(`^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?$`)

func parsePluginVersion(text string) (pluginVersion, error) {
	m := pluginSemver.FindStringSubmatch(text)
	if m == nil || len(text) > 128 {
		return pluginVersion{}, fmt.Errorf("invalid semantic version %q", text)
	}
	numbers := [3]uint64{}
	for i := range numbers {
		n, err := strconv.ParseUint(m[i+1], 10, 32)
		if err != nil {
			return pluginVersion{}, fmt.Errorf("version component exceeds uint32")
		}
		numbers[i] = n
	}
	for _, part := range strings.Split(m[4], ".") {
		if numericIdentifier(part) && len(part) > 1 && part[0] == '0' {
			return pluginVersion{}, fmt.Errorf("numeric prerelease identifier has a leading zero")
		}
	}
	return pluginVersion{numbers[0], numbers[1], numbers[2], m[4]}, nil
}

func numericIdentifier(text string) bool {
	if text == "" {
		return false
	}
	for _, c := range text {
		if c < '0' || c > '9' {
			return false
		}
	}
	return true
}

func comparePluginVersions(a, b pluginVersion) int {
	left, right := [3]uint64{a.major, a.minor, a.patch}, [3]uint64{b.major, b.minor, b.patch}
	for i := range left {
		if left[i] < right[i] {
			return -1
		}
		if left[i] > right[i] {
			return 1
		}
	}
	if a.pre == b.pre {
		return 0
	}
	if a.pre == "" {
		return 1
	}
	if b.pre == "" {
		return -1
	}
	ax, bx := strings.Split(a.pre, "."), strings.Split(b.pre, ".")
	for i := 0; i < len(ax) && i < len(bx); i++ {
		if ax[i] == bx[i] {
			continue
		}
		an, bn := numericIdentifier(ax[i]), numericIdentifier(bx[i])
		if an != bn {
			if an {
				return -1
			}
			return 1
		}
		if an && len(ax[i]) != len(bx[i]) {
			if len(ax[i]) < len(bx[i]) {
				return -1
			}
			return 1
		}
		if ax[i] < bx[i] {
			return -1
		}
		return 1
	}
	if len(ax) < len(bx) {
		return -1
	}
	return 1
}

type pluginComparator struct {
	op      string
	version pluginVersion
}
type pluginRange [][]pluginComparator

// Deliberately explicit grammar: full SemVer, *, ^, ~, comparisons joined by
// spaces, and ||. Invalid or ambiguous shorthand never silently widens a range.
func parsePluginRange(text string) (pluginRange, error) {
	if len(text) == 0 || len(text) > 512 {
		return nil, fmt.Errorf("invalid version range length")
	}
	groups := strings.Split(text, "||")
	if len(groups) > 16 {
		return nil, fmt.Errorf("too many range alternatives")
	}
	var result pluginRange
	for _, group := range groups {
		terms := strings.Fields(group)
		if len(terms) == 0 || len(terms) > 16 {
			return nil, fmt.Errorf("empty or excessive version range")
		}
		var comparisons []pluginComparator
		for _, term := range terms {
			if term == "*" {
				continue
			}
			op, versionText := "=", term
			for _, candidate := range []string{">=", "<=", "^", "~", ">", "<", "="} {
				if strings.HasPrefix(term, candidate) {
					op, versionText = candidate, strings.TrimPrefix(term, candidate)
					break
				}
			}
			version, err := parsePluginVersion(versionText)
			if err != nil {
				return nil, err
			}
			if op == "^" || op == "~" {
				upper := pluginVersion{major: version.major, minor: version.minor, patch: version.patch}
				if op == "~" {
					upper.minor++
					upper.patch = 0
				} else if version.major > 0 {
					upper.major++
					upper.minor, upper.patch = 0, 0
				} else if version.minor > 0 {
					upper.minor++
					upper.patch = 0
				} else {
					upper.patch++
				}
				comparisons = append(comparisons, pluginComparator{">=", version}, pluginComparator{"<", upper})
			} else {
				comparisons = append(comparisons, pluginComparator{op, version})
			}
		}
		result = append(result, comparisons)
	}
	return result, nil
}

func (r pluginRange) accepts(v pluginVersion) bool {
	for _, group := range r {
		accepted, prerelease := true, v.pre == ""
		for _, c := range group {
			comparison := comparePluginVersions(v, c.version)
			if c.version.pre != "" && v.major == c.version.major && v.minor == c.version.minor && v.patch == c.version.patch {
				prerelease = true
			}
			if (c.op == "=" && comparison != 0) || (c.op == ">" && comparison <= 0) || (c.op == ">=" && comparison < 0) ||
				(c.op == "<" && comparison >= 0) || (c.op == "<=" && comparison > 0) {
				accepted = false
				break
			}
		}
		if accepted && prerelease {
			return true
		}
	}
	return false
}
