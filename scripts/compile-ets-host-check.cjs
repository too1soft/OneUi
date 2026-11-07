// Check portable .ets policy with TypeScript. This is not an ArkTS SDK check.
const fs = require('node:fs');
const path = require('node:path');
const [source, output, compilerRoot] = process.argv.slice(2);
const ts = require(require.resolve('typescript', {paths:[compilerRoot]}));
const virtual = path.resolve(path.dirname(output), path.basename(source).replace(/\.ets$/, '.ts'));
const options = {strict:true,target:ts.ScriptTarget.ES2020,module:ts.ModuleKind.CommonJS,noEmitOnError:true,skipLibCheck:true};
const host = ts.createCompilerHost(options);
const originalSource = host.getSourceFile.bind(host), originalExists = host.fileExists.bind(host);
host.getSourceFile = (name, language, onError, create) => path.resolve(name) === virtual
  ? ts.createSourceFile(name, fs.readFileSync(source,'utf8'), language, true) : originalSource(name, language, onError, create);
host.fileExists = name => path.resolve(name) === virtual || originalExists(name);
host.writeFile = (name, data) => {
  if (path.resolve(name) !== virtual.replace(/\.ts$/, '.js')) throw Error('Unexpected test output');
  fs.writeFileSync(output, data);
};
const program = ts.createProgram([virtual], options, host);
const diagnostics = ts.getPreEmitDiagnostics(program);
if (diagnostics.length) {
  console.error(ts.formatDiagnosticsWithColorAndContext(diagnostics, {getCanonicalFileName:n=>n,getCurrentDirectory:()=>process.cwd(),getNewLine:()=> '\n'}));
  process.exitCode = 1;
} else program.emit();
