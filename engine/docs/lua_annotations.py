#!/usr/bin/env python
# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0

"""Aggregate Lua language-server annotations for the Defold API."""

import fnmatch
import html
import os
import re
import tempfile
from collections import defaultdict
from html.parser import HTMLParser
from pathlib import Path

import yaml

import script_doc_ddf_pb2


GENERATED_NOTICE = """--[[
Generated using the Defold build pipeline

./scripts/build.py build_docs
]]

---@meta
---@diagnostic disable: lowercase-global
---@diagnostic disable: missing-return
---@diagnostic disable: args-after-dots
"""


def load_metadata(path):
    with open(path, encoding="utf-8") as metadata_file:
        metadata = yaml.safe_load(metadata_file)
    if not isinstance(metadata, dict):
        raise ValueError("Lua annotation metadata must be a mapping")
    return metadata


def write_if_changed(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and path.read_text(encoding="utf-8") == data:
        return False
    fd, temporary_name = tempfile.mkstemp(prefix=".%s." % path.name, dir=str(path.parent))
    temporary_path = Path(temporary_name)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as output:
            output.write(data)
        os.replace(str(temporary_path), str(path))
    finally:
        if temporary_path.exists():
            temporary_path.unlink()
    return True


class _LuaDocHtmlConverter(HTMLParser):
    BLOCK_TAGS = {"br", "div", "li", "p", "tr", "ul", "ol", "dl", "dt", "dd", "pre"}

    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.parts = []
        self.link_targets = []

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag in self.BLOCK_TAGS:
            self.parts.append("\n")
        if tag == "li":
            self.parts.append("- ")
        elif tag in ("code", "tt"):
            self.parts.append("`")
        elif tag in ("strong", "b"):
            self.parts.append("**")
        elif tag in ("em", "i"):
            self.parts.append("*")
        elif tag == "a":
            self.parts.append("[")
            self.link_targets.append(attrs.get("href", ""))

    def handle_endtag(self, tag):
        if tag in ("code", "tt"):
            self.parts.append("`")
        elif tag in ("strong", "b"):
            self.parts.append("**")
        elif tag in ("em", "i"):
            self.parts.append("*")
        elif tag == "a":
            target = self.link_targets.pop() if self.link_targets else ""
            self.parts.append("](%s)" % target if target else "]")
        if tag in self.BLOCK_TAGS:
            self.parts.append("\n")

    def handle_data(self, data):
        self.parts.append(data)

    def text(self):
        value = "".join(self.parts)
        value = re.sub(r"[ \t]+\n", "\n", value)
        value = re.sub(r"\n[ \t]+", "\n", value)
        value = re.sub(r"\n{3,}", "\n\n", value)
        return value.strip()


def lua_doc_text(value):
    if not value:
        return ""
    value = re.sub(r"\[icon:.*?\]", "", value)
    value = re.sub(r"\[type:(.*?)\]", r"`\1`", value)
    value = re.sub(r"\[ref:(.*?)\]", r"`\1`", value)
    converter = _LuaDocHtmlConverter()
    converter.feed(html.unescape(value))
    value = converter.text()
    value = re.sub(r"^\s*:\s*", "", value, flags=re.MULTILINE)
    return value


def lua_doc_lines(value):
    value = lua_doc_text(value)
    if not value:
        return []
    return ["---%s" % line if line else "---" for line in value.splitlines()]


def _replace_type_identifier(expression, source, target):
    pattern = r"(?<![\w.])%s(?![\w.]|\s*:)" % re.escape(source)
    return re.sub(pattern, target, expression)


def _normalize_legacy_function_type(expression):
    match = re.match(r"^function(?:\((.*)\))?(.*)$", expression)
    if not match:
        return expression
    arguments = (match.group(1) or "").strip()
    suffix = match.group(2) or ""
    if not arguments:
        return "fun()" + suffix

    normalized = []
    for index, argument in enumerate(arguments.split(","), 1):
        argument = argument.strip()
        if not argument:
            continue
        if ":" in argument:
            normalized.append(argument)
        elif argument == "...":
            normalized.append("...:any")
        elif re.match(r"^[A-Za-z_]\w*$", argument) and argument not in {
                "nil", "any", "boolean", "number", "integer", "string",
                "userdata", "function", "thread", "table"}:
            normalized.append("%s:any" % argument)
        else:
            normalized.append("arg%d:any" % index)
    return "fun(%s)%s" % (", ".join(normalized), suffix)


def normalize_type(expression, metadata):
    expression = html.unescape(expression or "any").strip()
    if expression.startswith("function"):
        expression = _normalize_legacy_function_type(expression)
    if re.match(r"^\[[^\[\]]+\]$", expression):
        expression = expression[1:-1].strip() + "[]"
    for source, target in metadata.get("type_replacements", {}).items():
        expression = _replace_type_identifier(expression, source, target)
    expression = re.sub(r"\s*\|\s*", "|", expression)
    expression = re.sub(r"\s*,\s*", ", ", expression)
    expression = re.sub(r"\b([A-Za-z_]\w*)\.\.\.", "...", expression)
    return expression


def join_types(types, metadata):
    values = [normalize_type(value, metadata) for value in types if value]
    return "|".join(values) if values else "any"


class _TypeExpressionParser:
    TOKEN = re.compile(r"\s*(?:(\.\.\.)|(\[\])|([A-Za-z_][A-Za-z0-9_.]*)|([{}()<>,:|?]))")

    def __init__(self, expression):
        self.expression = expression
        self.tokens = []
        self.references = []
        position = 0
        while position < len(expression):
            match = self.TOKEN.match(expression, position)
            if not match:
                raise ValueError("unexpected token at column %d" % (position + 1))
            self.tokens.append(next(group for group in match.groups() if group is not None))
            position = match.end()
        self.index = 0

    def peek(self):
        return self.tokens[self.index] if self.index < len(self.tokens) else None

    def take(self, expected=None):
        token = self.peek()
        if token is None:
            raise ValueError("unexpected end of expression")
        if expected is not None and token != expected:
            raise ValueError("expected '%s', got '%s'" % (expected, token))
        self.index += 1
        return token

    @staticmethod
    def is_identifier(token):
        return token is not None and re.match(r"^[A-Za-z_][A-Za-z0-9_.]*$", token)

    def parse(self):
        if not self.tokens:
            raise ValueError("empty expression")
        self.parse_union()
        if self.peek() is not None:
            raise ValueError("unexpected token '%s'" % self.peek())
        return self.references

    def parse_union(self):
        self.parse_postfix()
        while self.peek() == "|":
            self.take("|")
            self.parse_postfix()

    def parse_postfix(self):
        self.parse_primary()
        while self.peek() in ("[]", "?"):
            self.take()

    def parse_primary(self):
        token = self.peek()
        if token == "{":
            self.parse_structural_table()
        elif token == "(":
            self.parse_tuple_or_group()
        elif token == "fun":
            self.parse_function()
        elif self.is_identifier(token):
            identifier = self.take()
            self.references.append(identifier)
            if self.peek() == "<":
                self.parse_generic_arguments(expected_count=2 if identifier == "table" else None)
        else:
            raise ValueError("expected a type, got '%s'" % token)

    def parse_generic_arguments(self, expected_count=None):
        self.take("<")
        count = 1
        self.parse_union()
        while self.peek() == ",":
            self.take(",")
            self.parse_union()
            count += 1
        self.take(">")
        if expected_count is not None and count != expected_count:
            raise ValueError("expected %d generic arguments, got %d" % (expected_count, count))

    def parse_structural_table(self):
        self.take("{")
        if self.peek() != "}":
            while True:
                field_name = self.take()
                if not self.is_identifier(field_name):
                    raise ValueError("invalid table field '%s'" % field_name)
                if self.peek() == "?":
                    self.take("?")
                self.take(":")
                self.parse_union()
                if self.peek() != ",":
                    break
                self.take(",")
                if self.peek() == "}":
                    break
        self.take("}")

    def parse_tuple_or_group(self):
        self.take("(")
        self.parse_union()
        while self.peek() == ",":
            self.take(",")
            self.parse_union()
        self.take(")")

    def parse_function(self):
        self.take("fun")
        self.take("(")
        if self.peek() != ")":
            while True:
                name = self.take()
                if name != "..." and not self.is_identifier(name):
                    raise ValueError("invalid function parameter '%s'" % name)
                if self.peek() == "?":
                    self.take("?")
                self.take(":")
                self.parse_union()
                if self.peek() != ",":
                    break
                self.take(",")
        self.take(")")
        if self.peek() == ":":
            self.take(":")
            self.parse_union()


def _parse_type_expression(expression):
    return _TypeExpressionParser(expression).parse()


def _type_references(expression):
    return _parse_type_expression(expression)


def _is_valid_type_expression(expression):
    try:
        _parse_type_expression(expression)
        return True
    except ValueError:
        return False


def _canonical_name(document, element):
    name = element.name.strip()
    namespace = document.info.namespace.strip()
    if namespace == "builtins" or "." in name or ":" in name:
        return name
    return "%s.%s" % (namespace, name) if namespace else name


def _namespace_of(name):
    return name.rsplit(".", 1)[0] if "." in name else ""


def _field_name(name):
    return name.rsplit(".", 1)[-1]


def _root_namespace(name):
    return name.split(".", 1)[0] if "." in name else "builtins"


def _normalize_parameter_name(name):
    if "..." in name:
        return "..."
    return re.sub(r"\W", "_", name)


def _parameter_type(parameter, metadata):
    return join_types(parameter.types, metadata)


def _return_type(return_value, metadata):
    return join_types(return_value.types, metadata)


def _correlated_type(function_name, expression, metadata):
    constraint = metadata.get("generics", {}).get(function_name)
    return "T" if constraint and expression == constraint else expression


def _function_signature(element, metadata, include_names=True):
    parameters = []
    for parameter in element.parameters:
        name = _normalize_parameter_name(parameter.name)
        param_type = _parameter_type(parameter, metadata)
        if include_names:
            if parameter.is_optional and name != "...":
                name += "?"
            parameters.append("%s:%s" % (name, param_type))
        else:
            parameters.append(param_type)
    returns = [_return_type(value, metadata) for value in element.returnvalues]
    signature = "fun(%s)" % ", ".join(parameters)
    if returns:
        signature += ":%s" % (returns[0] if len(returns) == 1 else "(%s)" % ", ".join(returns))
    return signature


def _function_identity(element, metadata):
    return _function_signature(element, metadata, include_names=False)


def _function_stub_parameter_names(element):
    names = [_normalize_parameter_name(parameter.name) for parameter in element.parameters]
    if any(not re.match(r"^[A-Za-z_]\w*$", name) and name != "..." for name in names):
        return ["..."]
    if "..." in names:
        return names[:names.index("...") + 1]
    return names


def _function_stub_parameters(element):
    return ", ".join(_function_stub_parameter_names(element))


def _excluded_function(name, metadata):
    field = _field_name(name)
    if name in metadata.get("excluded_functions", []) or field in metadata.get("excluded_functions", []):
        return True
    for pattern in metadata.get("excluded_function_patterns", []):
        if fnmatch.fnmatch(name, pattern) or fnmatch.fnmatch(field, pattern):
            return True
    return False


def _method_target(name, metadata):
    field = _field_name(name)
    if ":" not in field:
        return None
    receiver, method_name = field.split(":", 1)
    class_name = metadata.get("method_classes", {}).get(receiver)
    return (class_name, method_name) if class_name else None


def _constant_value_type(name, enum_members):
    for enum_name, enum in enum_members.items():
        if name in enum["members"]:
            return enum["value_type"]
    return "integer"


def _collect_enum_members(constants, metadata):
    result = {}
    for enum_name, options in metadata.get("enums", {}).items():
        options = options or {}
        members = list(options.get("members", []))
        if not members:
            prefix = enum_name + "_"
            members = sorted(name for name in constants if name.startswith(prefix))
        result[enum_name] = {
            "members": members,
            "value_type": options.get("value_type", "integer"),
        }
    return result


def _collect(documents, metadata):
    standard_namespaces = set(metadata.get("standard_namespaces", []))
    functions = defaultdict(lambda: defaultdict(list))
    values = defaultdict(dict)
    messages = defaultdict(dict)
    documented_classes = {}
    class_methods = defaultdict(lambda: defaultdict(list))
    descriptions = {}
    constants = set()

    for source_path, document in documents:
        if not document.HasField("info") or document.info.language != "Lua":
            continue
        if document.info.namespace in standard_namespaces:
            continue

        for element in document.elements:
            name = _canonical_name(document, element)
            root = _root_namespace(name)
            descriptions.setdefault(root, document.info.description)
            if element.type == script_doc_ddf_pb2.FUNCTION:
                method_target = _method_target(name, metadata)
                if method_target:
                    class_name, method_name = method_target
                    class_methods[class_name][method_name].append((source_path, element))
                elif not _excluded_function(name, metadata):
                    functions[root][name].append((source_path, element))
            elif element.type in (script_doc_ddf_pb2.CONSTANT, script_doc_ddf_pb2.VARIABLE):
                values[root][name] = (source_path, element)
                if element.type == script_doc_ddf_pb2.CONSTANT:
                    constants.add(name)
            elif element.type == script_doc_ddf_pb2.MESSAGE:
                class_name = "message.%s.%s" % (document.info.namespace, element.name.rsplit(".", 1)[-1])
                previous = messages[root].get(class_name)
                if previous:
                    previous_path, previous_element = previous
                    previous_fields = [
                        (field.name, tuple(field.types), field.is_optional)
                        for field in previous_element.parameters
                    ]
                    fields = [
                        (field.name, tuple(field.types), field.is_optional)
                        for field in element.parameters
                    ]
                    if fields != previous_fields:
                        raise ValueError(
                            "%s: conflicting duplicate message %s (previously defined in %s)"
                            % (source_path, class_name, previous_path))
                else:
                    messages[root][class_name] = (source_path, element)
            elif element.type in (script_doc_ddf_pb2.STRUCT, script_doc_ddf_pb2.CLASS):
                previous = documented_classes.get(name)
                if previous:
                    previous_path, previous_element = previous
                    previous_fields = [
                        (member.name, member.type)
                        for member in previous_element.members
                    ]
                    fields = [
                        (member.name, member.type)
                        for member in element.members
                    ]
                    if fields != previous_fields:
                        raise ValueError(
                            "%s: conflicting duplicate class %s (previously defined in %s)"
                            % (source_path, name, previous_path))
                else:
                    documented_classes[name] = (source_path, element)

    return (
        functions,
        values,
        messages,
        documented_classes,
        class_methods,
        descriptions,
        constants)


def _namespace_sets(functions, values):
    by_root = defaultdict(set)
    for root, root_functions in functions.items():
        for name in root_functions:
            namespace = _namespace_of(name)
            while namespace:
                by_root[root].add(namespace)
                namespace = _namespace_of(namespace)
    for root, root_values in values.items():
        for name in root_values:
            namespace = _namespace_of(name)
            while namespace:
                by_root[root].add(namespace)
                namespace = _namespace_of(namespace)
    return by_root


def _render_namespace(namespace, child_namespaces, fields, description):
    lines = ["---@class defold_api.%s" % namespace]
    if description:
        lines.extend(lua_doc_lines(description))
    for child in sorted(child_namespaces):
        lines.append("---@field %s defold_api.%s" % (_field_name(child), child))
    for field_name, field_type, field_doc in sorted(fields):
        lines.extend(lua_doc_lines(field_doc))
        lines.append("---@field %s %s" % (field_name, field_type))
    lines.append("%s = {}" % namespace)
    return lines


def _render_function(name, variants, metadata):
    unique = []
    identities = set()
    for source_path, element in variants:
        identity = _function_identity(element, metadata)
        if identity not in identities:
            identities.add(identity)
            unique.append((source_path, element))

    source_path, primary = unique[0]
    lines = lua_doc_lines(primary.description or primary.brief)
    stub_parameter_names = _function_stub_parameter_names(primary)
    if len(stub_parameter_names) != len(primary.parameters):
        lines.append("---@overload %s" % _function_signature(primary, metadata))
    for _, overload in unique[1:]:
        lines.append("---@overload %s" % _function_signature(overload, metadata))
    generic = metadata.get("generics", {}).get(name)
    if generic:
        lines.append("---@generic T: %s" % generic)
    for parameter in primary.parameters[:len(stub_parameter_names)]:
        param_name = _normalize_parameter_name(parameter.name)
        if parameter.is_optional and param_name != "...":
            param_name += "?"
        doc = lua_doc_text(parameter.doc).replace("\n", " ")
        lines.append("---@param %s %s%s" % (
            param_name,
            _correlated_type(name, _parameter_type(parameter, metadata), metadata),
            " " + doc if doc else ""))
    for return_value in primary.returnvalues:
        doc = lua_doc_text(return_value.doc).replace("\n", " ")
        name_part = " %s" % return_value.name if return_value.name else ""
        lines.append("---@return %s%s%s" % (
            _correlated_type(name, _return_type(return_value, metadata), metadata),
            name_part,
            " " + doc if doc else ""))
    lines.append("function %s(%s) end" % (name, _function_stub_parameters(primary)))
    return lines


def _render_message(class_name, element, metadata):
    lines = ["---@class %s" % class_name]
    lines.extend(lua_doc_lines(element.description or element.brief))
    for parameter in element.parameters:
        name = _normalize_parameter_name(parameter.name)
        if parameter.is_optional:
            name += "?"
        doc = lua_doc_text(parameter.doc).replace("\n", " ")
        lines.append("---@field %s %s%s" % (
            name,
            _parameter_type(parameter, metadata),
            " " + doc if doc else ""))
    return lines


def _render_documented_class(class_name, element, metadata):
    lines = ["---@class %s" % class_name]
    lines.extend(lua_doc_lines(element.description or element.brief))
    for member in element.members:
        if not re.match(r"^[A-Za-z_]\w*\??$", member.name):
            raise ValueError(
                "Class %s has invalid member name '%s'"
                % (class_name, member.name))
        description = lua_doc_text(member.doc).replace("\n", " ")
        lines.append("---@field %s %s%s" % (
            member.name,
            normalize_type(member.type, metadata),
            " " + description if description else ""))
    return lines


def _method_signature(class_name, element, metadata):
    parameters = ["self:%s" % class_name]
    for parameter in element.parameters:
        name = _normalize_parameter_name(parameter.name)
        if parameter.is_optional and name != "...":
            name += "?"
        parameters.append("%s:%s" % (name, _parameter_type(parameter, metadata)))
    returns = [_return_type(value, metadata) for value in element.returnvalues]
    signature = "fun(%s)" % ", ".join(parameters)
    if returns:
        signature += ":%s" % (
            returns[0] if len(returns) == 1 else "(%s)" % ", ".join(returns))
    return signature


def _render_class_method(class_name, method_name, variants, metadata):
    signatures = []
    for _, element in variants:
        signature = _method_signature(class_name, element, metadata)
        if signature not in signatures:
            signatures.append(signature)
    primary = variants[0][1]
    description = lua_doc_text(primary.description or primary.brief).replace("\n", " ")
    return "---@field %s %s%s" % (
        method_name,
        "|".join(signatures),
        " " + description if description else "")


def _render_enum(enum_name, enum):
    lines = ["---@alias %s %s" % (enum_name, enum["value_type"])]
    lines.extend("---| `%s`" % member for member in enum["members"])
    return lines


def _render_module(root, functions, values, namespaces, descriptions, enum_members, metadata):
    lines = [GENERATED_NOTICE.rstrip(), ""]
    namespace_fields = defaultdict(list)
    child_namespaces = defaultdict(set)

    for namespace in namespaces:
        parent = _namespace_of(namespace)
        if parent:
            child_namespaces[parent].add(namespace)

    for name, (_, element) in values.items():
        namespace = _namespace_of(name)
        if not namespace:
            continue
        value_type = "any"
        if element.type == script_doc_ddf_pb2.CONSTANT:
            value_type = _constant_value_type(name, enum_members)
        namespace_fields[namespace].append((_field_name(name), value_type, element.description or element.brief))

    for namespace in sorted(namespaces, key=lambda value: (value.count("."), value)):
        description = descriptions.get(root, "") if namespace == root else ""
        lines.extend(_render_namespace(
            namespace,
            child_namespaces.get(namespace, set()),
            namespace_fields.get(namespace, []),
            description))
        lines.append("")

    for enum_name, enum in sorted(enum_members.items()):
        if _root_namespace(enum_name) == root and enum["members"]:
            lines.extend(_render_enum(enum_name, enum))
            lines.append("")

    for name, variants in sorted(functions.items()):
        lines.extend(_render_function(name, variants, metadata))
        lines.append("")

    for name, (_, element) in sorted(values.items()):
        if "." not in name:
            lines.extend(lua_doc_lines(element.description or element.brief))
            lines.append("---@type %s" % (
                _constant_value_type(name, enum_members)
                if element.type == script_doc_ddf_pb2.CONSTANT
                else "any"))
            lines.append("%s = nil" % name)
            lines.append("")

    return "\n".join(lines).rstrip() + "\n"


def _render_meta(metadata, messages, documented_classes, class_methods):
    lines = [GENERATED_NOTICE.rstrip(), ""]
    for name, target in sorted(metadata.get("aliases", {}).items()):
        lines.append("---@alias %s %s" % (name, target))
    if metadata.get("aliases"):
        lines.append("")

    duplicate_class_names = set(metadata.get("classes", {})) & set(documented_classes)
    if duplicate_class_names:
        raise ValueError(
            "Classes must be defined in source documentation or metadata, not both: %s"
            % ", ".join(sorted(duplicate_class_names)))

    class_names = (
        set(metadata.get("classes", {}))
        | set(documented_classes)
        | set(class_methods))
    for class_name in sorted(class_names):
        documented_class = documented_classes.get(class_name)
        if documented_class:
            _, element = documented_class
            lines.extend(_render_documented_class(class_name, element, metadata))
        class_data = metadata.get("classes", {}).get(class_name) or {}
        if not documented_class:
            lines.append("---@class %s" % class_name)
        for field_name, field_type in sorted((class_data.get("fields") or {}).items()):
            if not isinstance(field_type, str):
                raise ValueError(
                    "Metadata class field '%s.%s' must be a type string"
                    % (class_name, field_name))
            lines.append("---@field %s %s" % (field_name, field_type))
        for method_name, variants in sorted(class_methods.get(class_name, {}).items()):
            lines.append(_render_class_method(
                class_name,
                method_name,
                variants,
                metadata))
        for operator, operator_types in sorted((class_data.get("operators") or {}).items()):
            parameter_type, result_type = operator_types
            if parameter_type is None:
                lines.append("---@operator %s: %s" % (operator, result_type))
            else:
                lines.append("---@operator %s(%s): %s" % (operator, parameter_type, result_type))
        lines.append("")

    for root in sorted(messages):
        for class_name, (_, element) in sorted(messages[root].items()):
            lines.extend(_render_message(class_name, element, metadata))
            lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def _known_annotation_types(
        metadata,
        enum_members,
        messages,
        documented_classes,
        class_methods):
    known = set(metadata.get("known_types", []))
    known.update(metadata.get("aliases", {}).keys())
    known.update(metadata.get("classes", {}).keys())
    known.update(documented_classes.keys())
    known.update(class_methods.keys())
    known.update(enum_members.keys())
    for enum in enum_members.values():
        known.update(enum["members"])
    for root_messages in messages.values():
        known.update(root_messages.keys())
    known.add("T")
    return known


def _validate_types(
        functions,
        messages,
        documented_classes,
        class_methods,
        metadata,
        enum_members,
        metadata_path):
    known = _known_annotation_types(
        metadata,
        enum_members,
        messages,
        documented_classes,
        class_methods)
    errors = []

    def validate(expression, source_path, symbol):
        if not _is_valid_type_expression(expression):
            errors.append("%s: %s has invalid type expression '%s'" % (source_path, symbol, expression))
            return
        for reference in _type_references(expression):
            if reference not in known:
                errors.append("%s: %s references unknown type '%s' in '%s'" % (
                    source_path, symbol, reference, expression))

    for root_functions in functions.values():
        for name, variants in root_functions.items():
            for source_path, element in variants:
                for parameter in element.parameters:
                    validate(_parameter_type(parameter, metadata), source_path, "%s parameter %s" % (name, parameter.name))
                for return_value in element.returnvalues:
                    validate(_return_type(return_value, metadata), source_path, "%s return %s" % (name, return_value.name))
    for root_messages in messages.values():
        for name, (source_path, element) in root_messages.items():
            for parameter in element.parameters:
                validate(_parameter_type(parameter, metadata), source_path, "%s field %s" % (name, parameter.name))
    for class_name, (source_path, element) in documented_classes.items():
        for member in element.members:
            validate(
                normalize_type(member.type, metadata),
                source_path,
                "%s field %s" % (class_name, member.name))
    for class_name, methods in class_methods.items():
        for method_name, variants in methods.items():
            for source_path, element in variants:
                for parameter in element.parameters:
                    validate(
                        _parameter_type(parameter, metadata),
                        source_path,
                        "%s:%s parameter %s" % (
                            class_name,
                            method_name,
                            parameter.name))
                for return_value in element.returnvalues:
                    validate(
                        _return_type(return_value, metadata),
                        source_path,
                        "%s:%s return %s" % (
                            class_name,
                            method_name,
                            return_value.name))

    for enum_name, enum in enum_members.items():
        if not enum["members"]:
            errors.append("Enum alias '%s' has no matching constant members" % enum_name)
        validate(enum["value_type"], metadata_path, "enum %s" % enum_name)

    for alias_name, alias_type in metadata.get("aliases", {}).items():
        validate(alias_type, metadata_path, "alias %s" % alias_name)
    for class_name, class_data in metadata.get("classes", {}).items():
        for field_name, field_type in (class_data.get("fields") or {}).items():
            if not isinstance(field_type, str):
                errors.append(
                    "%s: %s field %s must be a type string"
                    % (metadata_path, class_name, field_name))
                continue
            validate(field_type, metadata_path, "%s field %s" % (class_name, field_name))
        for operator, operator_types in (class_data.get("operators") or {}).items():
            parameter_type, result_type = operator_types
            if parameter_type is not None:
                validate(parameter_type, metadata_path, "%s operator %s parameter" % (class_name, operator))
            validate(result_type, metadata_path, "%s operator %s result" % (class_name, operator))
    for function_name, constraint in metadata.get("generics", {}).items():
        validate(constraint, metadata_path, "%s generic constraint" % function_name)

    if errors:
        raise ValueError("Lua annotation validation failed:\n" + "\n".join("  " + error for error in errors))


def generate(documents, output_dir, metadata_path, strict=True):
    metadata = load_metadata(metadata_path)
    (
        functions,
        values,
        messages,
        documented_classes,
        class_methods,
        descriptions,
        constants,
    ) = _collect(documents, metadata)
    enum_members = _collect_enum_members(constants, metadata)
    namespaces = _namespace_sets(functions, values)

    if strict:
        _validate_types(
            functions,
            messages,
            documented_classes,
            class_methods,
            metadata,
            enum_members,
            str(metadata_path))

    outputs = {
        "meta.lua": _render_meta(
            metadata,
            messages,
            documented_classes,
            class_methods)}
    roots = sorted(set(functions) | set(values))
    for root in roots:
        root_namespaces = namespaces.get(root, set())
        if root != "builtins" and root not in root_namespaces:
            root_namespaces = set(root_namespaces)
            root_namespaces.add(root)
        content = _render_module(
            root,
            functions.get(root, {}),
            values.get(root, {}),
            root_namespaces,
            descriptions,
            enum_members,
            metadata)
        if content.strip():
            outputs["%s.lua" % root] = content

    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    for name, content in outputs.items():
        write_if_changed(output_dir / name, content)
    for existing in output_dir.glob("*.lua"):
        if existing.name not in outputs:
            existing.unlink()
    return sorted(outputs)
