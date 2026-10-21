//go:build !windows

package wincompat

import "errors"

func Detect() (Identity, error) {
	return Identity{}, errors.New("Windows identity unavailable on this OS")
}
