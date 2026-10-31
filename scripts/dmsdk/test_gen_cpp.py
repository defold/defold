import json
from pathlib import Path
from types import SimpleNamespace
import unittest

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


if __name__ == '__main__':
    unittest.main()
