//go:build !windows

package main

import (
	"os"
	"runtime"
)

func systemInfo() string                                  { return runtime.GOOS }
func checkJunctionBoundary(_, _ string, _ *os.Root) error { return nil }
