package wincompat

import "fmt"

type Identity struct {
	Major, Minor, Build                     uint32
	ServicePack                             uint16
	NativeArchitecture, ProcessArchitecture string
	DefaultProfile                          string
}

func (i Identity) Version() string { return fmt.Sprintf("%d.%d.%d", i.Major, i.Minor, i.Build) }

func profile(major, minor, build uint32, servicePack uint16) (string, error) {
	if major == 10 && minor == 0 && build >= 10240 {
		return "modern", nil
	}
	if major == 6 && ((minor == 1 && build >= 7601 && servicePack >= 1) || (minor == 2 && build >= 9200) || (minor == 3 && build >= 9600)) {
		return "legacy", nil
	}
	return "", fmt.Errorf("unsupported Windows %d.%d.%d SP%d", major, minor, build, servicePack)
}

func (i Identity) SystemID() (string, error) {
	if _, err := profile(i.Major, i.Minor, i.Build, i.ServicePack); err != nil {
		return "", err
	}
	if i.NativeArchitecture != "x86" && i.NativeArchitecture != "x86_64" {
		return "", fmt.Errorf("unsupported native architecture")
	}
	var system string
	switch {
	case i.Major == 6 && i.Minor == 1:
		system = "win7-sp1"
	case i.Major == 6 && i.Minor == 2:
		system = "win8"
	case i.Major == 6 && i.Minor == 3:
		system = "win81"
	case i.Major == 10 && i.Build < 22000:
		system = "win10"
	case i.Major == 10 && i.NativeArchitecture == "x86_64":
		system = "win11"
	default:
		return "", fmt.Errorf("unknown Windows system")
	}
	return system + "-" + i.NativeArchitecture, nil
}
