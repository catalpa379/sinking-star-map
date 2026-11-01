"""Package a prepared offline directory without redundant root notes."""
from pathlib import Path
import argparse
import hashlib
import zipfile

OMIT = {'测试验证记录.txt', '先读我-安装与测试.txt'}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--version', required=True)
    args = parser.parse_args()
    root = args.directory.resolve(strict=True)
    output = args.output.resolve()
    if not root.is_dir() or root == output or root in output.parents:
        parser.error('Output ZIP must be outside the prepared directory')
    if not (root / '安装与卸载.exe').is_file() or not (root / 'files/package-integrity.json').is_file():
        parser.error('Expected a complete prepared offline directory')
    output.parent.mkdir(parents=True, exist_ok=True)
    prefix = 'SinkingMap-' + args.version + '-Offline'
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for path in sorted(root.rglob('*')):
            if path.is_file():
                relative = path.relative_to(root)
                if len(relative.parts) == 1 and path.name in OMIT:
                    continue
                archive.write(path, prefix + '/' + relative.as_posix())
    with zipfile.ZipFile(output) as archive:
        assert archive.testzip() is None
        assert not any(name in archive.namelist() for name in (prefix + '/' + item for item in OMIT))
    digest = hashlib.sha256(output.read_bytes()).hexdigest().upper()
    checksum = output.with_name('Sinking-Star-Map-' + args.version + '-SHA256.txt')
    checksum.write_text(digest + '  ' + output.name + '\n', encoding='utf-8')
    print(f'{output.stat().st_size} bytes; SHA256 {digest}')

if __name__ == '__main__':
    main()
