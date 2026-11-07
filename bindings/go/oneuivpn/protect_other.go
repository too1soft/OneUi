//go:build !android && !ios && !openharmony && !ohos && !oneui_harmonyos

package oneuivpn

// Desktop tests do not own a mobile system tunnel. Mobile runtimes use their
// dedicated, mandatory native socket and interface hooks instead.
func installSocketProtector(func(int) error, func() ([]byte, error)) (func(), error) {
	return func() {}, nil
}

func refreshNativeNetwork() error { return nil }
