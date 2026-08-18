# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0

import io
import json
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest import mock

import yaml

import build_docs
import lua_annotations
import script_doc_ddf_pb2


def document(namespace, elements):
    result = script_doc_ddf_pb2.Document()
    result.info.namespace = namespace
    result.info.name = namespace
    result.info.brief = namespace
    result.info.description = "<p>%s docs</p>" % namespace
    result.info.path = "%s.cpp" % namespace
    result.info.file = "%s.cpp" % namespace
    result.info.language = "Lua"
    for element in elements:
        result.elements.add().CopyFrom(element)
    return result


def function(name, parameter_type="string", return_type=None):
    result = script_doc_ddf_pb2.Element()
    result.type = script_doc_ddf_pb2.FUNCTION
    result.name = name
    result.brief = "Brief"
    result.description = "<p>Function <code>docs</code>.</p>"
    parameter = result.parameters.add()
    parameter.name = "value"
    parameter.doc = "Value"
    parameter.types.append(parameter_type)
    if return_type:
        return_value = result.returnvalues.add()
        return_value.name = "result"
        return_value.doc = "Result"
        return_value.types.append(return_type)
    return result


def constant(name, value_type=None):
    result = script_doc_ddf_pb2.Element()
    result.type = script_doc_ddf_pb2.CONSTANT
    result.name = name
    result.brief = name
    result.description = name
    if value_type:
        value = result.parameters.add()
        value.name = "value"
        value.types.append(value_type)
    return result


def message(name, field_type="string"):
    result = script_doc_ddf_pb2.Element()
    result.type = script_doc_ddf_pb2.MESSAGE
    result.name = name
    result.brief = "Message"
    field = result.parameters.add()
    field.name = "value"
    field.types.append(field_type)
    return result


def struct(name, fields):
    result = script_doc_ddf_pb2.Element()
    result.type = script_doc_ddf_pb2.STRUCT
    result.name = name
    result.brief = "Structure"
    result.description = "<p>Structure <code>docs</code>.</p>"
    for field_name, field_type, field_doc in fields:
        field = result.members.add()
        field.name = field_name
        field.type = field_type
        field.doc = field_doc
    return result


def typedef(name, target):
    result = script_doc_ddf_pb2.Element()
    result.type = script_doc_ddf_pb2.TYPEDEF
    result.name = name
    result.brief = "%s type" % name
    parameter = result.parameters.add()
    parameter.name = "value"
    parameter.doc = "Underlying value"
    parameter.types.append(target)
    return result


def enum(name, members=(), value_type=None):
    result = script_doc_ddf_pb2.Element()
    result.type = script_doc_ddf_pb2.ENUM
    result.name = name
    result.brief = "%s values" % name
    if value_type:
        parameter = result.parameters.add()
        parameter.name = "value"
        parameter.doc = "Underlying value"
        parameter.types.append(value_type)
    for member_name in members:
        member = result.members.add()
        member.name = member_name
        member.type = ""
        member.doc = ""
    return result


class TestLuaAnnotations(unittest.TestCase):

    def metadata(
            self,
            directory,
            enums=None,
            generics=None,
            global_symbols=None,
            method_classes=None):
        data = {
            "migration": {
                "source_commit": "test",
                "patch_files": 0,
                "patch_entries": 0,
            },
            "standard_namespaces": ["math"],
            "excluded_functions": ["init"],
            "excluded_function_patterns": ["client:*"],
            "global_symbols": global_symbols or [],
            "method_classes": method_classes or {},
            "allowed_diagnostics": ["lowercase-global", "missing-return", "args-after-dots"],
            "aliases": {"hash": "userdata"},
            "type_replacements": {"bool": "boolean"},
            "generics": generics or {},
            "enums": enums or {},
            "classes": {
                "vector3": {
                    "fields": {"x": "number"},
                    "operators": {"unm": [None, "vector3"]},
                },
                "vector4": {
                    "fields": {"x": "number"},
                },
            },
        }
        path = Path(directory) / "metadata.yaml"
        path.write_text(yaml.safe_dump(data, sort_keys=False), encoding="utf-8")
        return path

    def test_input_list_resolves_relative_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            absolute_input = directory / "nested" / "absolute.apidoc"
            input_list = directory / "inputs.txt"
            input_list.write_text(
                "relative input.apidoc\n\n%s\n" % absolute_input,
                encoding="utf-8")

            self.assertEqual(
                [
                    str((directory / "relative input.apidoc").resolve()),
                    str(absolute_input.resolve()),
                ],
                build_docs.read_input_list(input_list))

    def test_aggregate_namespaces_duplicates_overloads_and_exclusions(self):
        duplicate = function("go.play", "string")
        overload = function("go.play", "number")
        init = function("init")
        go_doc = document("go", [
            constant("go.PLAYBACK_ONCE_FORWARD"),
            duplicate,
            duplicate,
            overload,
            init,
        ])
        editor_doc = document("editor", [function("editor.ui.show", "bool", "hash")])
        math_doc = document("math", [function("math.abs", "number")])

        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(directory, {"go.PLAYBACK": {}})
            output = Path(directory) / "output"
            names = lua_annotations.generate([
                ("go.cpp", go_doc),
                ("editor.cpp", editor_doc),
                ("math.cpp", math_doc),
            ], output, metadata)

            self.assertEqual(["editor.lua", "go.lua", "meta.lua"], names)
            go_lua = (output / "go.lua").read_text(encoding="utf-8")
            self.assertEqual(1, go_lua.count("function go.play("))
            self.assertIn("---@overload fun(value:number)", go_lua)
            self.assertNotIn("function init", go_lua)
            self.assertIn(
                "---@enum defold_enum.go.PLAYBACK: integer",
                go_lua)
            self.assertIn(
                "---@alias go.PLAYBACK defold_enum.go.PLAYBACK",
                go_lua)

            editor_lua = (output / "editor.lua").read_text(encoding="utf-8")
            self.assertIn("editor.ui = {}", editor_lua)
            self.assertIn("---@param value boolean", editor_lua)
            self.assertIn("---@return hash result", editor_lua)
            self.assertNotIn("<code>", editor_lua)

    def test_runtime_and_editor_contexts_do_not_merge_overloads(self):
        runtime_doc = document(
            "http",
            [function("http.request", "string", "number")])
        editor_doc = document(
            "editor",
            [function("http.request", "table<string, any>", "string")])
        documents = [
            ("http.cpp", runtime_doc),
            ("editor.apidoc", editor_doc),
        ]

        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(directory)
            runtime_output = Path(directory) / "runtime"
            editor_output = Path(directory) / "editor"

            runtime_names = lua_annotations.generate(
                documents,
                runtime_output,
                metadata,
                context="runtime")
            editor_names = lua_annotations.generate(
                documents,
                editor_output,
                metadata,
                context="editor")

            self.assertEqual(["http.lua", "meta.lua"], runtime_names)
            runtime_lua = (
                runtime_output / "http.lua").read_text(encoding="utf-8")
            self.assertIn("---@param value string", runtime_lua)
            self.assertIn("---@return number result", runtime_lua)
            self.assertNotIn("---@overload", runtime_lua)

            self.assertEqual(["http.lua", "meta.lua"], editor_names)
            editor_lua = (
                editor_output / "http.lua").read_text(encoding="utf-8")
            self.assertIn("---@param value table<string, any>", editor_lua)
            self.assertIn("---@return string result", editor_lua)
            self.assertNotIn("---@overload", editor_lua)

    def test_global_symbol_is_not_qualified_by_document_namespace(self):
        pprint_function = function("pprint", "any")
        pprint_function.parameters[0].name = "..."
        editor_doc = document("editor", [pprint_function])

        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(directory, global_symbols=["pprint"])
            output = Path(directory) / "editor"

            names = lua_annotations.generate(
                [("editor.apidoc", editor_doc)],
                output,
                metadata,
                context="editor")

            self.assertEqual(["builtins.lua", "meta.lua"], names)
            builtins_lua = (
                output / "builtins.lua").read_text(encoding="utf-8")
            self.assertIn("---@param ... any", builtins_lua)
            self.assertIn("function pprint(...) end", builtins_lua)
            self.assertNotIn("editor.pprint", builtins_lua)

    def test_unknown_annotation_context_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(directory)
            with self.assertRaisesRegex(ValueError, "Unknown Lua annotation context"):
                lua_annotations.generate(
                    [],
                    Path(directory) / "output",
                    metadata,
                    context="unknown")

    def test_metadata_tracks_pinned_migration_source(self):
        metadata = lua_annotations.load_metadata(
            Path(__file__).with_name("lua_annotations.yaml"))
        self.assertEqual(
            "b0731459640bb6aa9f91ea24128bb6584c270160",
            metadata["migration"]["source_commit"])
        self.assertEqual(31, metadata["migration"]["patch_files"])
        self.assertEqual(176, metadata["migration"]["patch_entries"])
        manifest = json.loads(
            Path(__file__).with_name(
                metadata["migration"]["manifest"]).read_text(encoding="utf-8"))
        self.assertEqual(
            metadata["migration"]["source_commit"],
            manifest["attribution"]["source_commit"])
        self.assertEqual(
            metadata["migration"]["patch_files"],
            len(manifest["patches"]))
        self.assertEqual(
            metadata["migration"]["patch_entries"],
            sum(len(entries) for entries in manifest["patches"].values()))
        self.assertNotIn("known_types", metadata)
        self.assertEqual({}, metadata["classes"]["hash"])
        self.assertEqual(
            {"fields": {"[string]": "any"}},
            metadata["classes"]["script_instance"])
        self.assertEqual(
            {
                "editor.command_location",
                "editor.resource_definition",
                "editor.schema",
                "editor.component",
                "editor.transaction_step",
                "editor.tiles",
                "editor.image",
                "editor.command",
                "editor.message",
                "http.response",
                "http.route",
                "http.server.handler",
                "zip.entries",
                "zip.entry",
                "zip.unpack_options",
            },
            set(metadata["aliases"]))
        self.assertEqual(
            set(metadata["aliases"]),
            set(metadata["editor_only_metadata"]["aliases"]))
        self.assertEqual(
            {"editor.command_context", "http.server.request"},
            set(metadata["editor_only_metadata"]["classes"]))
        self.assertEqual(
            {"editor.ui.component"},
            set(metadata["editor_only_metadata"]["generics"]))
        self.assertEqual(
            {"http.server.route", "zip.pack", "zip.unpack"},
            set(metadata["editor_only_metadata"]["function_overloads"]))
        self.assertEqual(
            {
                "b2BodyType",
                "b2ContactEdge",
                "b2MassData",
                "b2Transform",
            },
            set(metadata["type_replacements"]))

    def test_migration_manifest_validates_every_expected_outcome(self):
        go_doc = document("go", [function("go.play", "string")])

        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(directory)
            metadata_data = yaml.safe_load(metadata.read_text(encoding="utf-8"))
            metadata_data["migration"].update({
                "manifest": "migration.json",
                "patch_files": 1,
                "patch_entries": 1,
            })
            metadata.write_text(
                yaml.safe_dump(metadata_data, sort_keys=False),
                encoding="utf-8")
            manifest_path = Path(directory) / "migration.json"
            manifest = {
                "attribution": {"source_commit": "test"},
                "patches": {
                    "go.cpp_doc.lua": [{
                        "path_pattern":
                            "elements.go.play.parameters.value.types.table",
                        "expected": "table",
                        "current": "string",
                    }],
                },
            }
            manifest_path.write_text(
                json.dumps(manifest),
                encoding="utf-8")

            lua_annotations.generate(
                [("go.cpp.apidoc", go_doc)],
                Path(directory) / "output",
                metadata)

            manifest["patches"]["go.cpp_doc.lua"][0]["current"] = "number"
            manifest_path.write_text(
                json.dumps(manifest),
                encoding="utf-8")
            with self.assertRaisesRegex(
                    ValueError,
                    r"go\.cpp_doc\.lua: .* expected 'number', got 'string'"):
                lua_annotations.generate(
                    [("go.cpp.apidoc", go_doc)],
                    Path(directory) / "output",
                    metadata)

    def test_migration_normalizes_callback_self_as_script_instance(self):
        self.assertEqual(
            "fun(self:script_instance, result:boolean)",
            lua_annotations._normalize_migration_type(
                "fun(self, result:boolean)",
                {}))

    def test_generate_accepts_document_iterators(self):
        go_doc = document("go", [function("go.play", "string")])

        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(directory)
            output = Path(directory) / "output"
            documents = iter([("go.cpp", go_doc)])

            lua_annotations.generate(documents, output, metadata)

            go_lua = (output / "go.lua").read_text(encoding="utf-8")
            self.assertIn("function go.play(value) end", go_lua)

    def test_unknown_type_fails_after_aggregation_with_source_and_symbol(self):
        unknown = document("go", [function("go.play", "not_a_real_type")])

        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(directory)
            with self.assertRaisesRegex(
                    ValueError,
                    r"go\.cpp: go\.play parameter value references unknown type 'not_a_real_type'"):
                lua_annotations.generate(
                    [("go.cpp", unknown)],
                    Path(directory) / "output",
                    metadata)

    def test_bare_table_type_fails_with_remediation(self):
        untyped_table = document("go", [function("go.play", "table")])

        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(directory)
            with self.assertRaisesRegex(
                    ValueError,
                    r"go\.cpp: go\.play parameter value uses bare table type .*"
                    r"use an array, a structural record, or table<key, value>"):
                lua_annotations.generate(
                    [("go.cpp", untyped_table)],
                    Path(directory) / "output",
                    metadata)

    def test_bare_table_type_detection_ignores_generics_and_field_names(self):
        self.assertTrue(lua_annotations._contains_bare_table_type("table"))
        self.assertTrue(lua_annotations._contains_bare_table_type("table[]"))
        self.assertTrue(lua_annotations._contains_bare_table_type(
            "fun(value:table):nil"))
        self.assertFalse(lua_annotations._contains_bare_table_type(
            "table<string, any>"))
        self.assertFalse(lua_annotations._contains_bare_table_type(
            "{ table:string }"))

    def test_type_expression_parser(self):
        valid = [
            "string|nil",
            "editor.component|false",
            "(string|hash)[]",
            "table<string, {value:number|nil, items?:vector3[]}>",
            "{[1]:string, [2]?:string, method?:\"stored\"|\"deflated\"}",
            "{callback:fun(value:number, ...:any):(string, nil), enabled?:boolean}",
            "fun(value:T):T",
            '"one"|"many"',
            "number?",
        ]
        for expression in valid:
            with self.subTest(expression=expression):
                self.assertTrue(lua_annotations._is_valid_type_expression(expression))

        invalid = [
            "string|",
            "table<string>",
            "{value number}",
            "fun(value):string",
            "string[",
            "table<string, number>>",
            "{[bad-field]:string}",
        ]
        for expression in invalid:
            with self.subTest(expression=expression):
                self.assertFalse(lua_annotations._is_valid_type_expression(expression))

    def test_boolean_literal_type_is_known(self):
        literal = document("go", [function("go.play", "false")])

        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(directory)
            output = Path(directory) / "output"
            lua_annotations.generate(
                [("go.cpp", literal)],
                output,
                metadata)

            go_lua = (output / "go.lua").read_text(encoding="utf-8")
            self.assertIn("---@param value false", go_lua)

    def test_plain_function_type_remains_broad(self):
        self.assertEqual(
            "function",
            lua_annotations.normalize_type("function", {}))
        self.assertEqual(
            "function|nil",
            lua_annotations.normalize_type("function|nil", {}))
        self.assertEqual(
            "fun(value:any)",
            lua_annotations.normalize_type("function(value)", {}))

    def test_optional_parameter_and_nil_return(self):
        element = function("go.lookup", "string")
        element.parameters[0].is_optional = True
        result = element.returnvalues.add()
        result.name = "result"
        result.types.extend(["number", "nil"])

        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(directory)
            output = Path(directory) / "output"
            lua_annotations.generate(
                [("go.cpp", document("go", [element]))],
                output,
                metadata)
            go_lua = (output / "go.lua").read_text(encoding="utf-8")
            self.assertIn("---@param value? string", go_lua)
            self.assertIn("---@return number|nil result", go_lua)

    def test_overloads_with_different_parameter_optionality_are_preserved(self):
        required = function("go.lookup", "string")
        optional = function("go.lookup", "string")
        optional.parameters[0].is_optional = True

        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(directory)
            output = Path(directory) / "output"
            lua_annotations.generate(
                [("go.cpp", document("go", [required, optional]))],
                output,
                metadata)
            go_lua = (output / "go.lua").read_text(encoding="utf-8")
            self.assertIn("---@overload fun(value?:string)", go_lua)

    def test_metadata_function_overloads_are_rendered_and_validated(self):
        lookup = function("go.lookup", "string", "number")

        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(directory)
            metadata_data = yaml.safe_load(metadata.read_text(encoding="utf-8"))
            metadata_data["function_overloads"] = {
                "go.lookup": [
                    "fun(name:string, options:{ exact?:boolean }):number",
                ],
            }
            metadata.write_text(
                yaml.safe_dump(metadata_data, sort_keys=False),
                encoding="utf-8")
            output = Path(directory) / "output"

            lua_annotations.generate(
                [("go.cpp", document("go", [lookup]))],
                output,
                metadata)

            go_lua = (output / "go.lua").read_text(encoding="utf-8")
            self.assertIn(
                "---@overload fun(name:string, options:{ exact?:boolean }):number",
                go_lua)

            metadata_data["function_overloads"]["go.lookup"] = [
                "fun(options:table):number",
            ]
            metadata.write_text(
                yaml.safe_dump(metadata_data, sort_keys=False),
                encoding="utf-8")
            with self.assertRaisesRegex(
                    ValueError,
                    r"go\.lookup metadata overload uses bare table type"):
                lua_annotations.generate(
                    [("go.cpp", document("go", [lookup]))],
                    output,
                    metadata)

    def test_html_entities_are_preserved_as_text(self):
        self.assertEqual(
            "`headers table<string, string>`",
            lua_annotations.lua_doc_text(
                "<code>headers table&lt;string, string&gt;</code>"))

    def test_balanced_type_tags_are_preserved_as_text(self):
        cases = {
            "[type:number[]]": "`number[]`",
            "[type:table<string, string>]": "`table<string, string>`",
            "[type:table&lt;string, string&gt;]": "`table<string, string>`",
            "[type:{[1]:string, [2]?:string}]":
                "`{[1]:string, [2]?:string}`",
            "[type:table<string, {items:string[], pair:{[1]:string, [2]?:number}}>]":
                "`table<string, {items:string[], pair:{[1]:string, [2]?:number}}>`",
        }
        for source, expected in cases.items():
            with self.subTest(source=source):
                self.assertEqual(expected, lua_annotations.lua_doc_text(source))

    def test_receiver_functions_are_rendered_as_documented_class_methods(self):
        send = function("client:send", "string", "number|nil")
        send.description = "<p>Sends <code>data</code> like [ref:string.sub].</p>"

        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(
                directory,
                method_classes={"client": "socket_client"})
            output = Path(directory) / "output"
            names = lua_annotations.generate(
                [("luasocket.doc_h", document("socket", [send]))],
                output,
                metadata)

            self.assertEqual(["meta.lua"], names)
            meta_lua = (output / "meta.lua").read_text(encoding="utf-8")
            self.assertIn("---@class socket_client", meta_lua)
            self.assertIn(
                "---@field send fun(self:socket_client, value:string):number|nil "
                "Sends `data` like `string.sub`.",
                meta_lua)
            self.assertNotIn("function client:send", meta_lua)
            self.assertIn("---@field x number", meta_lua)

    def test_documented_struct_fields_are_rendered_from_source(self):
        options = struct("resource.options", [
            ("path", "string|hash", "Resource <code>path</code>."),
            ("enabled?", "bool", "Whether enabled."),
        ])
        use = function("resource.use", "resource.options")

        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(directory)
            output = Path(directory) / "output"
            lua_annotations.generate(
                [("resource.cpp", document("resource", [options, use]))],
                output,
                metadata)

            meta_lua = (output / "meta.lua").read_text(encoding="utf-8")
            self.assertIn("---@class resource.options", meta_lua)
            self.assertIn(
                "---@field path string|hash Resource `path`.",
                meta_lua)
            self.assertIn(
                "---@field enabled? boolean Whether enabled.",
                meta_lua)

    def test_class_fields_cannot_be_defined_in_source_and_metadata(self):
        vector3 = struct("vector3", [("x", "number", "X")])

        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(directory)
            with self.assertRaisesRegex(
                    ValueError,
                    "Class fields must be defined in source documentation or metadata"):
                lua_annotations.generate(
                    [("vmath.cpp", document("builtins", [vector3]))],
                    Path(directory) / "output",
                    metadata)

    def test_source_class_fields_can_use_metadata_operators(self):
        vector3 = struct("vector3", [("x", "number", "X")])

        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(directory)
            metadata_data = yaml.safe_load(metadata.read_text(encoding="utf-8"))
            metadata_data["classes"]["vector3"].pop("fields")
            metadata.write_text(
                yaml.safe_dump(metadata_data, sort_keys=False),
                encoding="utf-8")
            output = Path(directory) / "output"
            lua_annotations.generate(
                [("vmath.cpp", document("vmath", [vector3]))],
                output,
                metadata)

            meta_lua = (output / "meta.lua").read_text(encoding="utf-8")
            self.assertIn("---@class vector3", meta_lua)
            self.assertIn("---@field x number", meta_lua)
            self.assertIn("---@operator unm: vector3", meta_lua)

    def test_metadata_operator_overloads_are_rendered(self):
        matrix4 = struct("matrix4", [("m00", "number", "M00")])

        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(directory)
            metadata_data = yaml.safe_load(metadata.read_text(encoding="utf-8"))
            metadata_data["classes"]["matrix4"] = {
                "operators": {
                    "mul": [
                        ["matrix4", "matrix4"],
                        ["vector4", "vector4"],
                        ["number", "matrix4"],
                    ],
                },
            }
            metadata.write_text(
                yaml.safe_dump(metadata_data, sort_keys=False),
                encoding="utf-8")
            output = Path(directory) / "output"
            lua_annotations.generate(
                [("vmath.cpp", document("vmath", [matrix4]))],
                output,
                metadata)

            meta_lua = (output / "meta.lua").read_text(encoding="utf-8")
            self.assertIn("---@operator mul(matrix4): matrix4", meta_lua)
            self.assertIn("---@operator mul(vector4): vector4", meta_lua)
            self.assertIn("---@operator mul(number): matrix4", meta_lua)

    def test_source_typedef_is_rendered_as_alias(self):
        hash_type = typedef("hash", "userdata")

        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(directory)
            metadata_data = yaml.safe_load(metadata.read_text(encoding="utf-8"))
            metadata_data["aliases"].pop("hash")
            metadata.write_text(
                yaml.safe_dump(metadata_data, sort_keys=False),
                encoding="utf-8")
            output = Path(directory) / "output"
            lua_annotations.generate(
                [("hash.cpp", document("builtins", [hash_type]))],
                output,
                metadata)

            meta_lua = (output / "meta.lua").read_text(encoding="utf-8")
            self.assertIn("---hash type", meta_lua)
            self.assertIn("---@alias hash userdata", meta_lua)

    def test_source_typedef_can_be_rendered_as_nominal_class(self):
        hash_type = typedef("hash", "userdata")

        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(directory)
            metadata_data = yaml.safe_load(metadata.read_text(encoding="utf-8"))
            metadata_data["classes"]["hash"] = {}
            metadata.write_text(
                yaml.safe_dump(metadata_data, sort_keys=False),
                encoding="utf-8")
            output = Path(directory) / "output"
            lua_annotations.generate(
                [("hash.cpp", document("builtins", [hash_type]))],
                output,
                metadata)

            meta_lua = (output / "meta.lua").read_text(encoding="utf-8")
            self.assertIn(
                "---hash type\n---@class hash: userdata",
                meta_lua)
            self.assertNotIn("---@alias hash", meta_lua)

    def test_box2d_handle_types_remain_distinct_from_runtime_namespaces(self):
        handle_types = [
            typedef("b2World", "userdata"),
            typedef("b2Body", "userdata"),
            typedef("b2Joint", "userdata"),
            typedef("b2Shape", "userdata"),
            typedef("b2Chain", "userdata"),
        ]
        namespace_documents = [
            document("b2d", handle_types),
            document("b2d.world", []),
            document("b2d.body", []),
            document("b2d.joint", []),
            document("b2d.shape", []),
            document("b2d.chain", []),
        ]
        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(directory)
            metadata_data = yaml.safe_load(metadata.read_text(encoding="utf-8"))
            metadata_data["type_replacements"] = {}
            metadata.write_text(
                yaml.safe_dump(metadata_data, sort_keys=False),
                encoding="utf-8")
            output = Path(directory) / "output"
            lua_annotations.generate(
                [
                    ("box2d_%d.cpp" % index, box2d_document)
                    for index, box2d_document in enumerate(namespace_documents)
                ],
                output,
                metadata)

            meta_lua = (output / "meta.lua").read_text(encoding="utf-8")
            for type_name in ("b2World", "b2Body", "b2Joint", "b2Shape", "b2Chain"):
                self.assertIn("---@alias %s userdata" % type_name, meta_lua)
            for namespace in ("world", "body", "joint", "shape", "chain"):
                self.assertNotIn("---@alias b2d.%s userdata" % namespace, meta_lua)

    def test_source_enum_infers_members_from_constants(self):
        playback = enum("go.PLAYBACK")
        go_doc = document("go", [
            playback,
            constant("go.PLAYBACK_NONE"),
            constant("go.PLAYBACK_LOOP_FORWARD"),
        ])

        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(directory)
            output = Path(directory) / "output"
            lua_annotations.generate(
                [("go.cpp", go_doc)],
                output,
                metadata)

            go_lua = (output / "go.lua").read_text(encoding="utf-8")
            self.assertIn(
                "---@enum defold_enum.go.PLAYBACK: integer",
                go_lua)
            self.assertIn(
                "local __defold_enum_go_PLAYBACK = {",
                go_lua)
            self.assertIn("    PLAYBACK_NONE = nil,", go_lua)
            self.assertIn(
                "---@alias go.PLAYBACK defold_enum.go.PLAYBACK",
                go_lua)
            self.assertIn("---| `go.PLAYBACK_NONE`", go_lua)
            self.assertIn(
                "---@field PLAYBACK_NONE go.PLAYBACK",
                go_lua)

    def test_source_enum_infers_members_from_nested_constant_namespace(self):
        alignment = enum("editor.ui.ALIGNMENT", value_type="string")
        editor_doc = document("editor", [
            alignment,
            constant("editor.ui.ALIGNMENT.TOP"),
            constant("editor.ui.ALIGNMENT.BOTTOM"),
        ])

        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(directory)
            output = Path(directory) / "output"
            lua_annotations.generate(
                [("editor.apidoc", editor_doc)],
                output,
                metadata)

            editor_lua = (output / "editor.lua").read_text(encoding="utf-8")
            self.assertIn(
                "---@enum defold_enum.editor.ui.ALIGNMENT: string",
                editor_lua)
            self.assertIn(
                "---@alias editor.ui.ALIGNMENT "
                "defold_enum.editor.ui.ALIGNMENT",
                editor_lua)
            self.assertIn("---| `editor.ui.ALIGNMENT.TOP`", editor_lua)
            self.assertIn("---@field TOP editor.ui.ALIGNMENT", editor_lua)

    def test_constant_supports_explicit_value_type(self):
        socket_doc = document("socket", [
            constant("socket._VERSION", "string"),
        ])

        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(directory)
            output = Path(directory) / "output"
            lua_annotations.generate(
                [("socket.cpp", socket_doc)],
                output,
                metadata)

            socket_lua = (output / "socket.lua").read_text(encoding="utf-8")
            self.assertIn("---@field _VERSION string", socket_lua)

    def test_untyped_standalone_constant_is_rejected(self):
        socket_doc = document("socket", [constant("socket.VERSION")])

        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(directory)
            with self.assertRaisesRegex(
                    ValueError,
                    r"socket\.cpp: constant socket\.VERSION must belong to an "
                    r"enum or declare an explicit value type"):
                lua_annotations.generate(
                    [("socket.cpp", socket_doc)],
                    Path(directory) / "output",
                    metadata)

    def test_constant_enum_member_can_be_nil(self):
        format_enum = enum("graphics.TEXTURE_FORMAT")
        graphics_doc = document("graphics", [
            format_enum,
            constant(
                "graphics.TEXTURE_FORMAT_RGBA16F",
                "graphics.TEXTURE_FORMAT|nil"),
        ])

        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(directory)
            output = Path(directory) / "output"
            lua_annotations.generate(
                [("graphics.cpp", graphics_doc)],
                output,
                metadata)

            graphics_lua = (output / "graphics.lua").read_text(
                encoding="utf-8")
            self.assertIn(
                "---@field TEXTURE_FORMAT_RGBA16F graphics.TEXTURE_FORMAT|nil",
                graphics_lua)

    def test_source_enum_supports_explicit_members_and_value_type(self):
        properties = enum(
            "gui.PROP",
            ["gui.PROP_POSITION", "gui.PROP_ROTATION"],
            "string")
        gui_doc = document("gui", [
            properties,
            constant("gui.PROP_POSITION"),
            constant("gui.PROP_ROTATION"),
        ])

        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(directory)
            output = Path(directory) / "output"
            lua_annotations.generate(
                [("gui.cpp", gui_doc)],
                output,
                metadata)

            gui_lua = (output / "gui.lua").read_text(encoding="utf-8")
            self.assertIn(
                "---@enum defold_enum.gui.PROP: string",
                gui_lua)
            self.assertIn(
                "---@alias gui.PROP defold_enum.gui.PROP",
                gui_lua)
            self.assertIn("---| `gui.PROP_ROTATION`", gui_lua)

    def test_conflicting_source_typedefs_report_both_sources(self):
        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(directory)
            with self.assertRaisesRegex(
                    ValueError,
                    r"second\.cpp: conflicting duplicate typedef value "
                    r"\(previously defined in first\.cpp\)"):
                lua_annotations.generate(
                    [
                        ("first.cpp", document(
                            "builtins",
                            [typedef("value", "string")])),
                        ("second.cpp", document(
                            "builtins",
                            [typedef("value", "number")])),
                    ],
                    Path(directory) / "output",
                    metadata)

    def test_source_enum_rejects_missing_explicit_member(self):
        playback = enum(
            "go.PLAYBACK",
            ["go.PLAYBACK_NONE", "go.PLAYBACK_MISSING"])
        go_doc = document("go", [
            playback,
            constant("go.PLAYBACK_NONE"),
        ])

        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(directory)
            with self.assertRaisesRegex(
                    ValueError,
                    r"go\.cpp: enum go\.PLAYBACK references missing constant "
                    r"go\.PLAYBACK_MISSING"):
                lua_annotations.generate(
                    [("go.cpp", go_doc)],
                    Path(directory) / "output",
                    metadata)

    def test_conflicting_documented_classes_report_both_sources(self):
        first = struct("resource.options", [("value", "string", "Value")])
        second = struct("resource.options", [("value", "number", "Value")])

        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(directory)
            with self.assertRaisesRegex(
                    ValueError,
                    r"second\.cpp: conflicting duplicate class "
                    r"resource\.options \(previously defined in first\.cpp\)"):
                lua_annotations.generate(
                    [
                        ("first.cpp", document("resource", [first])),
                        ("second.cpp", document("resource", [second])),
                    ],
                    Path(directory) / "output",
                    metadata)

    def test_documented_class_unknown_type_reports_source_and_field(self):
        options = struct("resource.options", [
            ("value", "missing.type", "Value"),
        ])

        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(directory)
            with self.assertRaisesRegex(
                    ValueError,
                    r"resource\.cpp: resource\.options field value "
                    r"references unknown type 'missing\.type'"):
                lua_annotations.generate(
                    [("resource.cpp", document("resource", [options]))],
                    Path(directory) / "output",
                    metadata)

    def test_generic_metadata_correlates_parameter_and_return_types(self):
        normalize = function(
            "vmath.normalize",
            "vector3|vector4",
            "vector3|vector4")

        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(
                directory,
                generics={"vmath.normalize": "vector3|vector4"})
            output = Path(directory) / "output"
            lua_annotations.generate(
                [("vmath.cpp", document("vmath", [normalize]))],
                output,
                metadata)
            vmath_lua = (output / "vmath.lua").read_text(encoding="utf-8")
            self.assertIn("---@generic T: vector3|vector4", vmath_lua)
            self.assertIn("---@param value T", vmath_lua)
            self.assertIn("---@return T result", vmath_lua)

    def test_foreign_namespace_and_proto_messages(self):
        editor_doc = document("editor", [function("json.decode", "string")])
        model_doc = document("model", [message("model_animation_done", "hash")])

        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(directory)
            output = Path(directory) / "output"
            names = lua_annotations.generate(
                [("editor.apidoc", editor_doc), ("model.proto", model_doc)],
                output,
                metadata)

            self.assertEqual(["json.lua", "meta.lua"], names)
            self.assertIn(
                "function json.decode(value) end",
                (output / "json.lua").read_text(encoding="utf-8"))
            meta_lua = (output / "meta.lua").read_text(encoding="utf-8")
            self.assertIn("---@class message.model.model_animation_done", meta_lua)
            self.assertNotIn("message.model =", meta_lua)

    def test_foreign_namespace_does_not_override_owner_description(self):
        editor_doc = document("editor", [function("json.decode", "string")])
        json_doc = document("json", [function("json.encode", "string")])
        inputs = [
            ("editor.apidoc", editor_doc),
            ("json.cpp", json_doc),
        ]

        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(directory)
            contents = []
            for index, documents in enumerate((inputs, list(reversed(inputs)))):
                output = Path(directory) / ("output-%d" % index)
                lua_annotations.generate(documents, output, metadata)
                contents.append(
                    (output / "json.lua").read_text(encoding="utf-8"))

            self.assertEqual(contents[0], contents[1])
            self.assertIn(
                "---@class defold_api.json\n---json docs",
                contents[0])
            self.assertNotIn("---editor docs", contents[0])

    def test_conflicting_duplicate_message_fails_with_both_sources(self):
        first = document("model", [message("done", "string")])
        second = document("model", [message("done", "number")])

        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(directory)
            with self.assertRaisesRegex(
                    ValueError,
                    r"second\.proto: conflicting duplicate message message\.model\.done "
                    r"\(previously defined in first\.proto\)"):
                lua_annotations.generate(
                    [("first.proto", first), ("second.proto", second)],
                    Path(directory) / "output",
                    metadata)

    def test_output_is_deterministic_and_removes_stale_lua_files(self):
        go_doc = document("go", [
            function("go.play"),
            constant("go.PLAYBACK_ONCE_FORWARD", "integer"),
        ])
        editor_doc = document("editor", [function("editor.ui.show", "bool")])

        with tempfile.TemporaryDirectory() as directory:
            metadata = self.metadata(directory)
            output = Path(directory) / "output"
            output.mkdir()
            (output / "legacy_doc.lua").write_text("stale", encoding="utf-8")
            lua_annotations.generate(
                [("go.cpp", go_doc), ("editor.apidoc", editor_doc)],
                output,
                metadata)
            first = {
                path.name: path.read_bytes()
                for path in output.glob("*.lua")
            }
            lua_annotations.generate(
                [("editor.apidoc", editor_doc), ("go.cpp", go_doc)],
                output,
                metadata)
            second = {
                path.name: path.read_bytes()
                for path in output.glob("*.lua")
            }

            self.assertEqual(first, second)
            self.assertNotIn("legacy_doc.lua", second)
            self.assertTrue(all(b"\nreturn " not in content for content in second.values()))

    def test_installer_reads_pinned_version_and_extracts_platform_archive(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            project_clj = directory / "project.clj"
            project_clj.write_text(
                '{:packing {:lua-language-server-version "v1.7795"}}',
                encoding="utf-8")

            platform_zip = io.BytesIO()
            with zipfile.ZipFile(platform_zip, "w") as archive:
                archive.writestr(
                    "bin/x86_64-linux/bin/lua-language-server",
                    b"lua-language-server")
            release_zip = io.BytesIO()
            with zipfile.ZipFile(release_zip, "w") as archive:
                archive.writestr(
                    "lsp-lua-language-server/plugins/x86_64-linux.zip",
                    platform_zip.getvalue())

            def fake_download(url, destination):
                self.assertIn("/v1.7795/release.zip", url)
                Path(destination).write_bytes(release_zip.getvalue())

            env_file = directory / "github.env"
            with mock.patch.object(
                    build_docs.urllib.request,
                    "urlretrieve",
                    side_effect=fake_download):
                build_docs.install_lua_language_server(
                    project_clj,
                    "x86_64-linux",
                    directory / "lua-language-server",
                    env_file)

            executable = (
                directory / "lua-language-server" / "bin" /
                "lua-language-server").resolve()
            self.assertEqual(b"lua-language-server", executable.read_bytes())
            self.assertTrue(executable.stat().st_mode & 0o111)
            self.assertEqual(
                "DEFOLD_DOCS_LUALS_EXECUTABLE=%s\n" % executable,
                env_file.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
