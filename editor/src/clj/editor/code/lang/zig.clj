;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.code.lang.zig)

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

;; Pattern definitions adopted from vscode:
;;   https://github.com/ziglang/vscode-zig/blob/master/syntaxes/zig.tmLanguage.json
;;
;; Adoptions made:
;; * Broke out :repository entries into defines, since we don't support
;;   repository includes.
;; * Lifted children of parent rules without a :match rule, since we assume
;;   every rule has a :match or :begin regex pattern. The parent scope :name was
;;   moved to any child rules that did not have a :name.
;; * Replaced {,2} syntax in regex pattern with the Java equivalent {0,2}.
;; * Added :indent and :line-comment.

(def ^:private string-content-patterns
  [{:name "constant.character.escape.zig"
    :match #"\\([nrt'\"\\]|(x[0-9a-fA-F]{2})|(u\{[0-9a-fA-F]+\}))"}
   {:name "invalid.illegal.unrecognized-string-escape.zig"
    :match #"\\."}])

(def ^:private number-patterns
  [{:name "constant.numeric.hexfloat.zig"
    :match #"\b0x[0-9a-fA-F][0-9a-fA-F_]*(\.[0-9a-fA-F][0-9a-fA-F_]*)?([pP][+-]?[0-9a-fA-F_]+)?\b"}
   {:name "constant.numeric.float.zig"
    :match #"\b[0-9][0-9_]*(\.[0-9][0-9_]*)?([eE][+-]?[0-9_]+)?\b"}
   {:name "constant.numeric.decimal.zig"
    :match #"\b[0-9][0-9_]*\b"}
   {:name "constant.numeric.hexadecimal.zig"
    :match #"\b0x[a-fA-F0-9_]+\b"}
   {:name "constant.numeric.octal.zig"
    :match #"\b0o[0-7_]+\b"}
   {:name "constant.numeric.binary.zig"
    :match #"\b0b[01_]+\b"}
   {:name "constant.numeric.invalid.zig"
    :match #"\b[0-9](([eEpP][+-])|[0-9a-zA-Z_])*(\.(([eEpP][+-])|[0-9a-zA-Z_])*)?([eEpP][+-])?[0-9a-zA-Z_]*\b"}])

(def ^:private support-patterns
  [{:name "support.function.builtin.zig"
    :match #"@[_a-zA-Z][_a-zA-Z0-9]*"}])

(def ^:private string-patterns
  [{:name "string.quoted.double.zig"
    :begin #"\""
    :begin-captures {0 {:name "punctuation.definition.string.quoted.begin.zig"}}
    :end #"\""
    :end-captures {0 {:name "punctuation.definition.string.quoted.end.zig"}}
    :patterns string-content-patterns}
   {:name "string.multiline.zig"
    :begin #"\\\\"
    :end #"$"}
   {:name "string.quoted.single.zig"
    :match #"(')([^'\\]|\\(x\h{2}|[0-2][0-7]{0,2}|3[0-6][0-7]?|37[0-7]?|[4-7][0-7]?|.))(')"
    :captures {1 {:name "punctuation.definition.string.quoted.begin.zig"}
               4 {:name "punctuation.definition.string.quoted.end.zig"}}}])

(def ^:private comment-contents-patterns
  [{:name "keyword.todo.zig"
    :match #"\b(TODO|FIXME|XXX|NOTE)\b:?"}])

(def ^:private comment-patterns
  [{:name "comment.line.documentation.zig"
    :begin #"//[!/](?=[^/])"
    :end #"$"
    :patterns comment-contents-patterns}
   {:name "comment.line.double-slash.zig"
    :begin #"//"
    :end #"$"
    :patterns comment-contents-patterns}])

(def ^:private variable-patterns
  [{:match #"\b(fn)\s+([A-Z][a-zA-Z0-9]*)\b"
    :captures {1 {:name "storage.type.function.zig"}
               2 {:name "entity.name.type.zig"}}}
   {:match #"\b(fn)\s+([_a-zA-Z][_a-zA-Z0-9]*)\b"
    :captures {1 {:name "storage.type.function.zig"}
               2 {:name "entity.name.function.zig"}}}
   {:name "entity.name.function.string.zig"
    :begin #"\b(fn)\s+@\""
    :end #"\""
    :begin-captures {1 {:name "storage.type.function.zig"}}
    :patterns string-content-patterns}
   {:name "keyword.default.zig"
    :match #"\b(const|var|fn)\b"}
   {:name "entity.name.type.zig"
    :match #"([A-Z][a-zA-Z0-9]*)(?=\s*\()"}
   {:name "entity.name.function.zig"
    :match #"([_a-zA-Z][_a-zA-Z0-9]*)(?=\s*\()"}
   {:name "variable.zig"
    :match #"\b[_a-zA-Z][_a-zA-Z0-9]*\b"}
   {:name "variable.string.zig"
    :begin #"@\""
    :end #"\""
    :patterns string-content-patterns}])

(def ^:private keyword-patterns
  [{:name "keyword.control.repeat.zig"
    :match #"\binline\b(?!\s*\bfn\b)"}
   {:name "keyword.control.repeat.zig"
    :match #"\b(while|for)\b"}
   {:name "keyword.storage.zig"
    :match #"\b(extern|packed|export|pub|noalias|inline|comptime|volatile|align|linksection|threadlocal|allowzero|noinline|callconv)\b"}
   {:name "keyword.structure.zig"
    :match #"\b(struct|enum|union|opaque)\b"}
   {:name "keyword.statement.zig"
    :match #"\b(asm|unreachable)\b"}
   {:name "keyword.control.flow.zig"
    :match #"\b(break|return|continue|defer|errdefer)\b"}
   {:name "keyword.control.async.zig"
    :match #"\b(await|resume|suspend|async|nosuspend)\b"}
   {:name "keyword.control.trycatch.zig"
    :match #"\b(try|catch)\b"}
   {:name "keyword.control.conditional.zig"
    :match #"\b(if|else|switch|orelse)\b"}
   {:name "keyword.constant.default.zig"
    :match #"\b(null|undefined)\b"}
   {:name "keyword.constant.bool.zig"
    :match #"\b(true|false)\b"}
   {:name "keyword.default.zig"
    :match #"\b(usingnamespace|test|and|or)\b"}
   {:name "keyword.type.zig"
    :match #"\b(bool|void|noreturn|type|error|anyerror|anyframe|anytype|anyopaque)\b"}
   {:name "keyword.type.integer.zig"
    :match #"\b(f16|f32|f64|f80|f128|u\d+|i\d+|isize|usize|comptime_int|comptime_float)\b"}
   {:name "keyword.type.c.zig"
    :match #"\b(c_char|c_short|c_ushort|c_int|c_uint|c_long|c_ulong|c_longlong|c_ulonglong|c_longdouble)\b"}])

(def ^:private operator-patterns
  [{:name "keyword.operator.c-pointer.zig"
    :match #"\[*c\]"}
   {:name "keyword.operator.comparison.zig"
    :match #"(\b(and|or)\b)|(==|!=)"}
   {:name "keyword.operator.arithmetic.zig"
    :match #"(-%?|\+%?|\*%?|/|%)=?"}
   {:name "keyword.operator.bitwise.zig"
    :match #"(<<%?|>>|!|&|\^|\|)=?"}
   {:name "keyword.operator.special.zig"
    :match #"(==|\+\+|\*\*|->)"}])

(def grammar
  {:name "Zig"
   :scope-name "source.zig"
   :line-comment "//"
   :indent {:begin #"^.*\{[^}\"\']*$|^.*\([^\)\"\']*$|^\s*\{\}$"
            :end #"^\s*(\s*/[*].*[*]/\s*)*\}|^\s*(\s*/[*].*[*]/\s*)*\)"}
   :auto-insert {:characters {\[ \]
                              \( \)
                              \{ \}
                              \" \"
                              \' \'}
                 :close-characters #{\] \) \} \" \'}
                 :exclude-scopes #{"punctuation.definition.string.quoted.begin.zig"
                                   "string.quoted.double.zig"
                                   "string.quoted.single.zig"
                                   "string.multiline.zig"
                                   "constant.character.escape.zig"
                                   "invalid.illegal.unrecognized-string-escape.zig"}
                 :open-scopes {\" "punctuation.definition.string.quoted.begin.zig"
                               \' "punctuation.definition.string.quoted.begin.zig"}
                 :close-scopes {\" "punctuation.definition.string.quoted.end.zig"
                                \' "punctuation.definition.string.quoted.end.zig"}}
   :patterns (into []
                   cat
                   [comment-patterns
                    string-patterns
                    keyword-patterns
                    operator-patterns
                    number-patterns
                    support-patterns
                    variable-patterns])})
