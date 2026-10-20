package wincompat

import "testing"

func TestProfiles(t *testing.T) {
	for _, c := range []struct {
		major, minor, build uint32
		sp                  uint16
		want                string
	}{
		{6, 1, 7600, 0, ""}, {6, 1, 7601, 0, ""}, {6, 1, 7601, 1, "legacy"},
		{6, 2, 9200, 0, "legacy"}, {6, 3, 9600, 0, "legacy"}, {10, 0, 22631, 0, "modern"}, {6, 0, 6002, 2, ""},
	} {
		got, err := profile(c.major, c.minor, c.build, c.sp)
		if got != c.want || (err != nil) != (c.want == "") {
			t.Fatalf("%+v: %s %v", c, got, err)
		}
	}
}
