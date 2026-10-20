//go:build !windows

package wincompat

import "os"

func isReparse(_ os.FileInfo) bool { return false }
