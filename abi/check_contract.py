import re
from pathlib import Path

root = Path(__file__).resolve().parent.parent
header = (root / 'abi/include/opal_c.h').read_text()
implementation = (root / 'abi/src/opal_c.cpp').read_text()
native = (root / 'include/opal/opal.h').read_text()
reference = (root / 'docs/c-abi-reference.md').read_text()

pattern = r'OPAL_C_CALL\s+(opal_\w+)\s*\(([^)]*)\)'
def declarations(text):
    return [(name, re.sub(r'\s+', '', args)) for name, args in re.findall(pattern, text)]

public = declarations(header)
defined = declarations(implementation)
assert len(public) == len(set(name for name, _ in public)), 'Duplicate declarations'
assert sorted(public) == sorted(defined), 'C declaration/definition mismatch'
functions = list(re.finditer(pattern, implementation))
for index, function in enumerate(functions):
    name = function[1]
    end = functions[index + 1].start() if index + 1 < len(functions) else len(implementation)
    if name not in {'opal_get_last_error', 'opal_get_abi_version', 'opal_get_backend'}:
        assert 'guard(__func__,' in implementation[function.end():end], f'Unguarded C boundary: {name}'

assert all(name in reference for name, _ in public), 'Undocumented exported function'
assert not re.search(r'\b(?:bool|class|namespace|template)\b|\bstd::', header), 'C++ in C header'
assert '// ' not in implementation and '/*' not in implementation, 'Unexpected comments'

clean = re.sub(r'/\*[\s\S]*?\*/|//[^\n]*', '', native)
methods = set()
for match in re.finditer(r'class\s+(\w+)\s*\{', clean):
    name = match[1]
    start = match.end()
    depth = 1
    end = start
    while depth:
        depth += (clean[end] == '{') - (clean[end] == '}')
        end += 1
    body = clean[start:end - 1]
    for section in re.findall(r'public:([\s\S]*?)(?=private:|protected:|$)', body):
        for method in re.findall(r'\b(\w+)\s*\(', section):
            if method not in {name, 'if', 'return', 'sizeof', 'defined'}:
                methods.add(method)
missing = sorted(method for method in methods if not re.search(r'(?:->|::)' + method + r'\s*\(', implementation))
assert not missing, f'Native public methods missing from wrappers: {missing}'

assertions = re.findall(r'static_assert\(\s*static_cast<int32_t>\(opal::([\w:]+)\)\s*==\s*(OPAL_\w+)\);', implementation)
constants = dict((name, int(value)) for name, value in re.findall(r'(OPAL_\w+)\s*=\s*(\d+)', header))
count = 0
for enum_name, body in re.findall(r'enum class (\w+)\s*\{([^}]+)\}', clean):
    if enum_name == 'Type':
        enum_name = 'Attachment::Type'
    for value, member in enumerate(body.split(',')):
        member = member.strip()
        if not member:
            continue
        matches = [constant for native_member, constant in assertions if native_member == f'{enum_name}::{member}']
        assert len(matches) == 1 and constants[matches[0]] == value, f'Enum drift: {enum_name}::{member}'
        count += 1

symbols = {name for name, _ in public}
for example in (root / 'abi/examples').glob('*.c'):
    calls = set(re.findall(r'\b(opal_\w+)\s*\(', example.read_text()))
    assert calls <= symbols, f'Unknown C example symbols: {calls - symbols}'
for doc in [root / 'docs/c-abi.md', root / 'docs/c-abi-reference.md']:
    for target in re.findall(r'\]\(([^)]+)\)', doc.read_text()):
        if '://' not in target and not target.startswith('#'):
            assert (doc.parent / target.split('#')[0]).exists(), f'Broken documentation link: {target}'
for source in re.findall(r'\bsrc/[\w.]+', (root / 'abi/CMakeLists.txt').read_text()):
    assert (root / 'abi' / source).exists(), f'Missing CMake source: {source}'
print(f'Checked {len(public)} C signatures, {len(methods)} native public method names, {count} enum values, example calls, CMake sources, and manual links. No compilation performed.')
