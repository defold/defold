import json
import os
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import gen_cpp
import gen_doc


class CppDocumentationTests(unittest.TestCase):
    def setUp(self):
        self.root = Path(__file__).resolve().parents[2]
        extension = self.root / 'engine/extension'
        self.docs = gen_doc.parse_docs(str(extension / 'src/dmsdk/extension/extension.h'))
        self.info = json.loads((extension / 'sdk_gen.json').read_text())['files'][0]['languages']['cpp']

    # Header regeneration must keep C++ enum names and types without changing the appended C docs.
    def test_header_renames_cpp_docs_and_preserves_c_docs(self):
        enum = SimpleNamespace(name='ExtensionParam', values=[{'name': 'EXTENSION_PARAM_DT'}])
        function = {
            'name': 'ExtensionParamsGetFloat',
            'type': {'qualType': 'bool (const ExtensionParams *, ExtensionParam, float *)'},
            'inner': [
                {'kind': 'ParmVarDecl', 'name': name, 'type': {'qualType': typ}}
                for name, typ in (
                    ('params', 'const ExtensionParams *'),
                    ('param', 'ExtensionParam'),
                    ('out_value', 'float *'),
                )
            ],
        }
        state = SimpleNamespace(
            enum_types=[(enum, None)],
            struct_typedefs=[],
            function_typedefs=[],
            functions=[(function, None)],
        )
        header = gen_cpp.gen_cpp_header(
            str(self.root),
            'dmsdk/extension/extension.h',
            str(self.root / 'engine/extension/src/dmsdk/extension/extension_gen.hpp'),
            self.info, None, state, [], self.docs)
        cpp_docs, c_docs = header.split('} // namespace dmExtension', 1)

        self.assertRegex(cpp_docs, r'@member\s+PARAM_DT\b')
        self.assertIn('@param params [type:const Params*]', cpp_docs)
        self.assertIn('@param param [type:Param]', cpp_docs)
        self.assertIn('`PARAM_DT`', cpp_docs)
        self.assertIn('Param param', cpp_docs)
        self.assertNotIn('EXTENSION_PARAM_DT', cpp_docs)

        self.assertRegex(c_docs, r'@member\s+EXTENSION_PARAM_DT\b')
        self.assertIn('@param params [type:const ExtensionParams*]', c_docs)
        self.assertIn('@param param [type:ExtensionParam]', c_docs)

    # Declaration-only namespace removal must not break already-qualified C++ types and references.
    def test_doc_renames_preserve_cpp_namespace(self):
        name = 'ExtensionAppParamsGetAppExitCode'
        renames = self.info['rename']
        doc = gen_cpp.gen_doc(self.docs, name, gen_cpp.rename_symbols(renames, name), renames=renames)

        self.assertIn('@param app_params [type:dmExtension::AppParams*]', doc)
        self.assertIn('dmExtension::AppInitialize', doc)
        self.assertIn('@return code [type:AppExitCode]', doc)

    # Const opaque record pointers must retain their C++ aliases and read-only C type.
    def test_const_opaque_action_handles(self):
        with patch.dict(os.environ, {'DYNAMO_HOME': str(self.root / 'tmp/dynamo_home')}):
            import gen_sdk
        config = json.loads((self.root / 'engine/input/sdk_gen.json').read_text())['files'][0]['languages']['cpp']
        docs = gen_doc.parse_docs(str(self.root / 'engine/input/src/dmsdk/input/input.h'))
        declarations = []
        for name in ['HInputAction', 'HInputActionBatch']:
            node = {'kind': 'RecordType'}
            for kind in ['ElaboratedType', 'QualType', 'PointerType', 'TypedefDecl']:
                node = {'kind': kind, 'inner': [node]}
            node['name'] = name
            self.assertTrue(gen_sdk.is_struct_typedef(node))
            declarations.append((node, None))
        state = SimpleNamespace(enum_types=[], struct_typedefs=declarations, function_typedefs=[], functions=[])
        header = gen_cpp.gen_cpp_header(str(self.root), 'dmsdk/input/input.h',
            str(self.root / 'engine/input/src/dmsdk/input/input_gen.hpp'), config, None, state, [], docs)
        self.assertIn('typedef HInputAction HAction;', header)
        self.assertIn('typedef HInputActionBatch HActionBatch;', header)

    # Runtime hash macros need typed C++ aliases and matching documentation after regeneration.
    def test_hash_constant_aliases(self):
        engine = self.root / 'engine/engine'
        docs = gen_doc.parse_docs(str(engine / 'src/dmsdk/engine/engine_event.h'))
        info = json.loads((engine / 'sdk_gen.json').read_text())['files'][0]['languages']['cpp']
        state = SimpleNamespace(enum_types=[], struct_typedefs=[], function_typedefs=[], functions=[])
        header = gen_cpp.gen_cpp_header(
            str(self.root), 'dmsdk/engine/engine_event.h',
            str(engine / 'src/dmsdk/engine/engine_event_gen.hpp'),
            info, None, state, [], docs)
        cpp_docs, c_docs = header.split('} // namespace dmEngine', 1)
        aliases = info['constant_aliases']
        self.assertEqual(8, len(aliases))
        self.assertIn('| `EVENT_WINDOW_RESIZED` |', cpp_docs)
        for name in aliases:
            renamed = gen_cpp.rename_symbols(info['rename'], name)
            self.assertIn('static const dmhash_t %s = %s;' % (renamed, name), cpp_docs)
            self.assertIn('@name ' + renamed, cpp_docs)
            self.assertIn('@name ' + name, c_docs)


if __name__ == '__main__':
    unittest.main()
