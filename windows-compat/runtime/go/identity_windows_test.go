package wincompat

import (
	"runtime"
	"testing"
)

func TestDetectNativeWindows(t *testing.T) {
	i, err := Detect()
	if err != nil {
		t.Fatal(err)
	}
	want := map[string]string{"386": "x86", "amd64": "x86_64"}[runtime.GOARCH]
	if i.ProcessArchitecture != want || i.NativeArchitecture == "" || i.DefaultProfile == "" {
		t.Fatalf("incomplete identity: %+v", i)
	}
	t.Logf("actual OS=%s SP=%d native=%s process=%s default=%s", i.Version(), i.ServicePack, i.NativeArchitecture, i.ProcessArchitecture, i.DefaultProfile)
}
