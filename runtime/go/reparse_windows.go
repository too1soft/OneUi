package wincompat

import (
	"os"
	"syscall"
)

func isReparse(info os.FileInfo) bool {
	data, ok := info.Sys().(*syscall.Win32FileAttributeData)
	return ok && data.FileAttributes&0x400 != 0
}
