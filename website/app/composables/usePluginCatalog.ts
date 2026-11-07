import type { Artifact } from './useReleaseCatalog'
export interface PluginRelease {
  manifest: { id: string; version: string; kind: string; license: string; platforms: string[]; services: {id: string, abi: number}[]; components?: {name: string, version: number}[]; dependencies: {id: string, version: string}[] }
  description: string; sourceCommit: string; artifacts: Artifact[]
}
export function usePluginCatalog() {
  return useFetch<{schemaVersion: number, packages: PluginRelease[]}>('/api/plugins', {server: false})
}
