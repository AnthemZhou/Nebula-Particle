#!/usr/bin/env python3
"""Real AE integration tests through the locally verified AE 23.6 CLI.

Does not use -reuse, change preferences, terminate GUI AE, or install plugins.
The JSX setup refuses any already-open project. Requires an installed build.
"""
from pathlib import Path
import argparse
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'test-output'
APP = Path('/Applications/Adobe After Effects 2023')

def run(arguments, log_name):
    with (OUT/log_name).open('w') as log:
        result = subprocess.run([str(a) for a in arguments], stdout=log, stderr=subprocess.STDOUT, timeout=240)
    if result.returncode:
        raise RuntimeError(f'Command exited {result.returncode}; inspect {OUT/log_name}')

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--existing', action='store_true', help='Use the most recently prepared test AEPs.')
    args = parser.parse_args()
    OUT.mkdir(exist_ok=True)
    if not args.existing:
        subprocess.run([sys.executable, str(ROOT/'scripts/make-fixtures.py')], check=True)
        print('Preparing a new isolated test project…', flush=True)
        run([APP/'Adobe After Effects 2023.app/Contents/MacOS/After Effects', '--unified', '--noui', '--execute', ROOT/'scripts/cli-smoke-test.jsx'], 'ae-setup-console.log')
    report = (OUT/'ae-render-report.txt').read_text()
    assert 'PASS: Native plugin loaded' in report and 'FAIL:' not in report, report
    project = Path(next(line[8:] for line in report.splitlines() if line.startswith('Project ')))
    assert project.parent == OUT and project.is_file()
    started = time.time()
    for bit_depth, path in [(16, project), (8, OUT/'Particle Atelier Test 8bit.aep')]:
        print(f'Rendering the {bit_depth}-bpc integration queue…', flush=True)
        run([APP/'aerender', '-project', path, '-mfr', 'OFF', '100', '-v', 'ERRORS_AND_PROGRESS'], f'ae-render-{bit_depth}.log')
        text = (OUT/f'ae-render-{bit_depth}.log').read_text()
        assert 'Total Time Elapsed' in text and 'ERROR:' not in text and 'SYNTAX ERROR' not in text, text[-3000:]
    conversions = {
        'ae-16-comp-0_0040.psd':'ae-preview-alpha.png',
        'ae-16-comp-1_0040.psd':'ae-preview-damping.png',
        'ae-16-comp-2_0040.psd':'ae-preview-mask.png',
        'ae-16-repeat_0040.psd':'ae-preview-repeat.png',
        'ae-8-comp-0_0040.psd':'ae-preview-8.png',
        'ae-8-repeat_0040.psd':'ae-preview-8-repeat.png',
        'ae-8-comp-3_0060.psd':'ae-preview-stop.png',
        'ae-8-comp-4_0110.psd':'ae-preview-after-end.png',
    }
    for source, target in conversions.items():
        file = OUT/source
        assert file.exists() and file.stat().st_mtime >= started-2, f'No fresh render: {file}'
        subprocess.run(['sips', '-s', 'format', 'png', str(file), '--out', str(OUT/target)], check=True, stdout=subprocess.DEVNULL)
    subprocess.run([sys.executable, str(ROOT/'scripts/verify-ae-output.py')], check=True)
    (OUT/'latest-project.txt').write_text(str(project)+'\n')
    print(f'PASS. AE project: {project}', flush=True)

if __name__ == '__main__':
    main()
