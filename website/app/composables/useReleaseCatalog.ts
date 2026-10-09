export interface Artifact {
  id: string; platform: string; architecture: string; profile: string; linkage: string
  file: string; size: number; sha256: string; minimumOS: string; toolchain: string
  signature: string; validation: string; limitations: string[]; systemDependencies: string[]
  runtime?: { file: string; size: number; sha256: string }
}
export interface Release {
  version: string; channel: string; abi: number; publishedAt: string; docsVersion: string
  sourceCommit: string; sourceDirty: boolean; notes: string[]; artifacts: Artifact[]
}
export function useReleaseCatalog() {
  return useFetch<{ schemaVersion: number, releases: Release[] }>('/api/releases', { server: false })
}
export function fileSize(value: number) { return (value / 1048576).toFixed(1) + ' MiB' }
