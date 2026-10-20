package main

import (
	"bytes"
	"crypto/rand"
	"crypto/sha256"
	"crypto/tls"
	"crypto/x509"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"net"
	"net/http"
	"net/http/httptest"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"sync"
	"time"
)

type result struct {
	Name   string `json:"name"`
	Passed bool   `json:"passed"`
	Error  string `json:"error,omitempty"`
}

func main() {
	if len(os.Args) > 1 && os.Args[1] == "child" {
		fmt.Print("child-ok")
		return
	}
	results := []result{}
	check := func(name string, f func() error) {
		err := f()
		item := result{Name: name, Passed: err == nil}
		if err != nil {
			item.Error = err.Error()
		}
		results = append(results, item)
	}
	check("crypto-random", func() error {
		a, b := make([]byte, 65536), make([]byte, 65536)
		if _, err := rand.Read(a); err != nil {
			return err
		}
		if _, err := rand.Read(b); err != nil {
			return err
		}
		if bytes.Equal(a, b) || bytes.Equal(a, make([]byte, len(a))) {
			return fmt.Errorf("non-random output")
		}
		return nil
	})
	check("process-and-console", func() error {
		path, err := os.Executable()
		if err != nil {
			return err
		}
		out, err := exec.Command(path, "child").Output()
		if err != nil {
			return err
		}
		if string(out) != "child-ok" {
			return fmt.Errorf("unexpected child output")
		}
		return nil
	})
	check("unicode-files-and-root-boundary", func() error {
		dir, err := os.MkdirTemp("", "网云穿兼容-")
		if err != nil {
			return err
		}
		defer os.RemoveAll(dir)
		sub := filepath.Join(dir, "共享目录")
		if err = os.Mkdir(sub, 0700); err != nil {
			return err
		}
		if err = os.WriteFile(filepath.Join(dir, "outside.txt"), []byte("keep"), 0600); err != nil {
			return err
		}
		root, err := os.OpenRoot(sub)
		if err != nil {
			return err
		}
		defer root.Close()
		f, err := root.Create("中文 #+.txt")
		if err != nil {
			return err
		}
		if _, err = f.Write([]byte("你好")); err != nil {
			f.Close()
			return err
		}
		if err = f.Close(); err != nil {
			return err
		}
		if escaped, err := root.Open("../outside.txt"); err == nil {
			escaped.Close()
			return fmt.Errorf("Root escaped its boundary")
		}
		if err = root.Mkdir("nested", 0700); err != nil {
			return err
		}
		child, err := root.Create("nested/data")
		if err != nil {
			return err
		}
		child.Close()
		if err = root.RemoveAll("nested"); err != nil {
			return err
		}
		if err = root.Remove("中文 #+.txt"); err != nil {
			return err
		}
		if err = checkJunctionBoundary(dir, sub, root); err != nil {
			return err
		}
		return nil
	})
	check("http-tls-and-certificate-rejection", func() error {
		server := httptest.NewTLSServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) { io.WriteString(w, "tls-ok") }))
		defer server.Close()
		roots := x509.NewCertPool()
		roots.AddCert(server.Certificate())
		transport := &http.Transport{TLSClientConfig: &tls.Config{RootCAs: roots, MinVersion: tls.VersionTLS12}}
		defer transport.CloseIdleConnections()
		client := &http.Client{Transport: transport, Timeout: 5 * time.Second}
		response, err := client.Get(server.URL)
		if err != nil {
			return err
		}
		data, err := io.ReadAll(response.Body)
		response.Body.Close()
		if err != nil {
			return err
		}
		if string(data) != "tls-ok" {
			return fmt.Errorf("TLS response mismatch")
		}
		badTransport := &http.Transport{TLSClientConfig: &tls.Config{RootCAs: x509.NewCertPool(), MinVersion: tls.VersionTLS12}}
		defer badTransport.CloseIdleConnections()
		badClient := &http.Client{Transport: badTransport, Timeout: 5 * time.Second}
		if response, err := badClient.Get(server.URL); err == nil {
			response.Body.Close()
			return fmt.Errorf("untrusted certificate accepted")
		}
		return nil
	})
	check("tcp-socket", func() error {
		listener, err := net.Listen("tcp", "127.0.0.1:0")
		if err != nil {
			return err
		}
		defer listener.Close()
		done := make(chan error, 1)
		go func() {
			connection, err := listener.Accept()
			if err == nil {
				connection.SetDeadline(time.Now().Add(5 * time.Second))
				_, err = io.CopyN(connection, connection, 4)
				connection.Close()
			}
			done <- err
		}()
		connection, err := net.DialTimeout("tcp", listener.Addr().String(), 5*time.Second)
		if err != nil {
			return err
		}
		defer connection.Close()
		connection.SetDeadline(time.Now().Add(5 * time.Second))
		if _, err = connection.Write([]byte("ping")); err != nil {
			return err
		}
		buffer := make([]byte, 4)
		if _, err = io.ReadFull(connection, buffer); err != nil {
			return err
		}
		if string(buffer) != "ping" {
			return fmt.Errorf("echo mismatch")
		}
		return <-done
	})
	check("scheduler-timers", func() error {
		var group sync.WaitGroup
		for i := 0; i < 100; i++ {
			group.Add(1)
			go func() {
				defer group.Done()
				for j := 0; j < 10; j++ {
					time.Sleep(time.Millisecond)
					runtime.Gosched()
				}
			}()
		}
		group.Wait()
		return nil
	})
	executable, _ := os.Executable()
	data, _ := os.ReadFile(executable)
	hash := sha256.Sum256(data)
	json.NewEncoder(os.Stdout).Encode(map[string]any{"goVersion": runtime.Version(), "processArchitecture": runtime.GOARCH, "system": systemInfo(), "sha256": hex.EncodeToString(hash[:]), "results": results, "scope": "runtime-smoke-not-product-or-public-signature-certification"})
	for _, item := range results {
		if !item.Passed {
			os.Exit(1)
		}
	}
}
