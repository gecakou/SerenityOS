#!/usr/bin/env python3

import argparse
import json
import multiprocessing
import os
from pathlib import Path
import signal
import subprocess
import sys

# Relative to Userland directory
COMMON_PATHS_TO_SEARCH = [
    'Libraries/LibJS',
    'Libraries/LibMarkdown',
    'Libraries/LibWeb',
    'Services/WebContent',
    'Services/WebWorker',
]
SERENITY_PATHS_TO_SEARCH = COMMON_PATHS_TO_SEARCH + [
    'Applications/Assistant',
    'Applications/Browser',
    'Applications/Spreadsheet',
    'Applications/TextEditor',
    'DevTools/HackStudio',
]

parser = argparse.ArgumentParser('LibJSGCVerifier', description='A Clang tool to validate usage of the LibJS GC')
parser.add_argument('-b', '--build-path', required=True, help='Path to the project Build folder')
parser.add_argument('-l', '--lagom', action='store_true', required=False,
                    help='Use the lagom build instead of the serenity build')
args = parser.parse_args()

build_path = Path(args.build_path).resolve()
userland_path = build_path / '..' / 'Userland'
if args.lagom:
    compile_commands_path = build_path / 'lagom' / 'compile_commands.json'
else:
    compile_commands_path = build_path / 'x86_64clang' / 'compile_commands.json'

if not compile_commands_path.exists():
    print(f'Could not find compile_commands.json in {compile_commands_path.parent}')
    exit(1)

paths = []

if args.lagom:
    paths_to_search = COMMON_PATHS_TO_SEARCH
else:
    paths_to_search = SERENITY_PATHS_TO_SEARCH
for containing_path in paths_to_search:
    for root, dirs, files in os.walk(userland_path / containing_path):
        for file in files:
            if file.endswith('.cpp'):
                paths.append(Path(root) / file)


def thread_init():
    signal.signal(signal.SIGINT, signal.SIG_IGN)


def thread_execute(file_path):
    clang_args = [
        './build/LibJSGCVerifier',
        '--extra-arg',
        '-DUSING_AK_GLOBALLY=1',  # To avoid errors about USING_AK_GLOBALLY not being defined at all
        '--extra-arg',
        '-DNULL=0',  # To avoid errors about NULL not being defined at all
        '-p',
        compile_commands_path,
        file_path
    ]
    proc = subprocess.Popen(clang_args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = proc.communicate()
    print(f'Processed {file_path.resolve()}')
    if stderr:
        print(stderr.decode(), file=sys.stderr)

    results = []
    if stdout:
        for line in stdout.split(b'\n'):
            if line:
                results.append(json.loads(line))
    return results


with multiprocessing.Pool(processes=multiprocessing.cpu_count() - 2, initializer=thread_init) as pool:
    try:
        clang_results = []
        for results in pool.map(thread_execute, paths):
            clang_results.extend(results)
    except KeyboardInterrupt:
        pool.terminate()
        pool.join()

# Process output data
clang_results = {r['name']: r for r in clang_results}
leaf_objects = set(clang_results.keys())
for result in clang_results.values():
    leaf_objects.difference_update(result['parents'])

for key, value in clang_results.items():
    if key == 'JS::HeapBlock::FreelistEntry' or key == 'JS::HeapFunction':
        # These are Heap-related classes and don't need their own allocator
        continue

    if not value['has_cell_allocator'] and (key in leaf_objects or value['has_js_constructor']):
        print(f'Class {key} is missing a JS_DECLARE_ALLOCATOR() declaration in its header file')
