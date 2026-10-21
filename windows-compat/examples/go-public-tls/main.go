// Native HTTPS trust probe. Only performs TLS handshakes: no account, login,
// mutation or application HTTP requests. Never disables certificate checks.
package main

import (
	"crypto/tls"
	"encoding/json"
	"net"
	"os"
	"runtime"
	"time"
)

func main() {
	passed:=true
	for _,host:=range os.Args[1:] {
		started:=time.Now()
		connection,err:=tls.DialWithDialer(&net.Dialer{Timeout:12*time.Second},"tcp",net.JoinHostPort(host,"443"),&tls.Config{ServerName:host,MinVersion:tls.VersionTLS12})
		result:=map[string]any{"host":host,"os":runtime.GOOS,"arch":runtime.GOARCH,"go":runtime.Version(),"elapsedMs":time.Since(started).Milliseconds(),"time":started.UTC().Format(time.RFC3339),"scope":"public-tls-handshake-only","passed":err==nil}
		if err!=nil {passed=false;result["error"]=err.Error()}else{state:=connection.ConnectionState();result["verifiedChains"]=len(state.VerifiedChains);result["protocol"]=state.Version;result["issuer"]=state.PeerCertificates[0].Issuer.String();connection.Close()}
		json.NewEncoder(os.Stdout).Encode(result)
	}
	if len(os.Args)<2 || !passed {os.Exit(1)}
}
