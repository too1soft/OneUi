package main

import (
	"context"
	"embed"
	"flag"
	"fmt"
	"io/fs"
	"log"
	"net/http"
	"os"
	"os/signal"
	"path/filepath"
	"runtime"
	"syscall"
	"time"
)

// The build script generates this directory from the checked-in Nuxt website.
//
//go:embed all:web
var webFiles embed.FS

var buildVersion = "development"

func main() {
	if err := run(os.Args[1:]); err != nil {
		log.Print(err)
		os.Exit(1)
	}
}

func run(args []string) error {
	if len(args) > 0 && args[0] == "sdk" {
		return runSDK(args[1:])
	}
	executable, err := os.Executable()
	if err != nil {
		return err
	}
	if real, e := filepath.EvalSymlinks(executable); e == nil {
		executable = real
	}
	flags := flag.NewFlagSet("oneui-site", flag.ContinueOnError)
	defaultListen := "127.0.0.1:8080"
	if runtime.GOOS == "linux" {
		defaultListen = "0.0.0.0:23187"
	}
	listen := flags.String("listen", defaultListen, "HTTP listen address (host:port)")
	directory := flags.String("data", filepath.Dir(executable), "directory containing releases.json and SDK archives")
	check := flags.Bool("check", false, "verify catalog and every download, then exit")
	version := flags.Bool("version", false, "print build version")
	if err := flags.Parse(args); err != nil {
		return err
	}
	if flags.NArg() != 0 {
		return fmt.Errorf("unexpected arguments: %v", flags.Args())
	}
	if *version {
		fmt.Println(buildVersion)
		return nil
	}
	catalog, err := loadCatalog(*directory)
	if err != nil {
		return err
	}
	defer catalog.root.Close()
	assets, err := fs.Sub(webFiles, "web")
	if err != nil {
		return err
	}
	if _, err := fs.Stat(assets, "index.html"); err != nil {
		return fmt.Errorf("embedded website missing: %w", err)
	}
	if *check {
		fmt.Printf("OK: %d releases, %d verified downloads\n", len(catalog.Releases), len(catalog.files))
		return nil
	}
	server := &http.Server{Addr: *listen, Handler: newHandler(catalog, assets),
		ReadHeaderTimeout: 10 * time.Second, IdleTimeout: 60 * time.Second, MaxHeaderBytes: 32 << 10}
	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()
	go func() {
		<-ctx.Done()
		shutdown, cancel := context.WithTimeout(context.Background(), 10*time.Second)
		defer cancel()
		_ = server.Shutdown(shutdown)
	}()
	log.Printf("OneUI %s: http://%s (%d verified downloads)", buildVersion, *listen, len(catalog.files))
	if err := server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
		return err
	}
	return nil
}
