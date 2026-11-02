from pathlib import Path
import hashlib,json,ast,csv
ROOT=Path(__file__).resolve().parent
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    manifest=json.loads((ROOT/'SHA256.json').read_text(encoding='utf8'))
    bad=[n for n,h in manifest.items() if not (ROOT/n).is_file() or sha(ROOT/n)!=h]
    archived=json.loads((ROOT/'provenance/MANIFEST_SHA256.json').read_text(encoding='utf8'))['files']
    for n,h in archived.items():
        if n.startswith('xiwang_v3/') and (not (ROOT/'experiment'/n).exists() or sha(ROOT/'experiment'/n)!=h): bad.append('archive:'+n)
    for p in ROOT.rglob('*.py'): ast.parse(p.read_text(encoding='utf-8-sig'),filename=str(p))
    controllers=[json.loads(s) for s in (ROOT/'experiment/controllers.jsonl').read_text(encoding='utf8').splitlines() if s]
    for c in controllers:
        if sha(ROOT/'experiment'/c['model_path'])!=c['model_sha256']: bad.append(c['model_id'])
    models=len(list((ROOT/'experiment/models').glob('*.ft')))
    assert models==44
    rows=list(csv.DictReader((ROOT/'analysis/v3cold_vs_fixed_overall.csv').open(encoding='utf-8-sig')))
    expected={'STR':0.7907504889913044,'RDI':0.777741182253902}
    for row in rows:
        assert abs(float(row['median_ratio'])-expected[row['ifun']])<1e-12
        assert int(row['models'])==41
    print(json.dumps({'files_checked':len(manifest),'models':models,'controllers':len(controllers),'failures':bad}))
    raise SystemExit(bool(bad))
if __name__=='__main__': main()
