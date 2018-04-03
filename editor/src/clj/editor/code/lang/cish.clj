(ns editor.code.lang.cish)

;; Pattern definitions translated from vscode:
;;
;;   https://github.com/Microsoft/vscode/blob/master/extensions/cpp/syntaxes/c.json
;;   https://github.com/Microsoft/vscode/blob/master/extensions/cpp/syntaxes/c%2B%2B.json
;;
;; Problems:
;;  - No support for repositories. Can partly be replaced by def's and concat'ing patterns. But circular definitions are a hassle.
;;  - Regexp grammar slightly different
;;  - \n often used to match end of line. Our lines don't have an ending \n, so replace with $.
;;
;; We should really expand our grammar support and auto translate the grammar definitions.
;;

(def ^:private c-line-continuation-character-patterns
  [{:match #"(\\)$"
    :captures {1 {:name "constant.character.escape.line-continuation.c"}}}])

(def ^:private c-comments-patterns
  [{
    :captures {1 {:name "meta.toc-list.banner.block.c"}}
    :match #"^/\* =(\s*.*?)\s*= \*/$$?"
    :name "comment.block.c"
    }
   {
    :begin #"/\*"
    :begin-captures {0 {:name "punctuation.definition.comment.begin.c"}}
    :end #"\*/"
    :end-captures {0 {:name "punctuation.definition.comment.end.c"}}
    :name "comment.block.c"
    }
   {
    :match #"\*/.*$"
    :name "invalid.illegal.stray-comment-end.c"
    }
   {
    :captures {1 {:name "meta.toc-list.banner.line.c"}}
    :match #"^// =(\s*.*?)\s*=\s*$$?"
    :name "comment.line.banner.cpp"
    }
   {
    :begin #"(^[ \t]+)?(?=//)"
    :begin-captures {1 {:name "punctuation.whitespace.comment.leading.cpp"}}
    :end #"(?!\G)"
    :patterns [{:begin #"//"
                :begin-captures {0 {:name "punctuation.definition.comment.cpp"}}
                :end #"(?=$)"
                :name "comment.line.double-slash.cpp"
                :patterns c-line-continuation-character-patterns
                }
               ]
    }])

(def ^:private c-numbers-patterns
  [{:match #"\b((0(x|X)[0-9a-fA-F]([0-9a-fA-F']*[0-9a-fA-F])?)|(0(b|B)[01]([01']*[01])?)|(([0-9]([0-9']*[0-9])?\.?[0-9]*([0-9']*[0-9])?)|(\.[0-9]([0-9']*[0-9])?))((e|E)(\+|-)?[0-9]([0-9']*[0-9])?)?)(L|l|UL|ul|u|U|F|f|ll|LL|ull|ULL)?\b"
    :name "constant.numeric.c"
    }
   ])

(def ^:private c-string-escaped-char-patterns
  [{:match #"(?x)\\ (
\\                |
[abefnprtv'\"?]   |
[0-3]\d{0,2}      |
[4-7]\d?          |
x[a-fA-F0-9]{0,2} |
u[a-fA-F0-9]{0,4} |
U[a-fA-F0-9]{0,8} )"
    :name "constant.character.escape.c"
    }
   {:match #"\\."
    :name "invalid.illegal.unknown-escape.c"}
   ])

(def ^:private c-string-placeholder-patterns
  [{:match #"(?x) %
(\d+\$)?                             # field (argument #)
[\#0\- +']*                          # flags
[,;:_]?                              # separator character (AltiVec)
((-?\d+)|\*(-?\d+\$)?)?              # minimum field width
(\.((-?\d+)|\*(-?\d+\$)?)?)?         # precision
(hh|h|ll|l|j|t|z|q|L|vh|vl|v|hv|hl)? # length modifier
[diouxXDOUeEfFgGaACcSspn%]           # conversion type"
    :name "constant.other.placeholder.c"
    }
   {:match #"(%)(?!\"\s*(PRI|SCN))"
    :captures {1 {:name "invalid.illegal.placeholder.c"}}
    }])

(def ^:private c-strings-patterns
  [{:begin #"\""
    :begin-captures {0 {:name "punctuation.definition.string.begin.c"}}
    :end #"\""
    :end-captures {0 {:name "punctuation.definition.string.end.c"}}
    :name "string.quoted.double.c"
    :patterns (concat c-string-escaped-char-patterns
                      c-string-placeholder-patterns
                      c-line-continuation-character-patterns)
    }
   {:begin #"'"
    :begin-captures {0 {:name "punctuation.definition.string.begin.c"}}
    :end #"'"
    :end-captures {0 {:name "punctuation.definition.string.end.c"}}
    :name "string.quoted.single.c"
    :patterns (concat c-string-escaped-char-patterns
                      c-line-continuation-character-patterns)}])

(def ^:private c-operators-patterns
  [{
    :match #"(?<![\w$])(sizeof)(?![\w$])"
    :name "keyword.operator.sizeof.c"
    }
   {
    :match #"--"
    :name "keyword.operator.decrement.c"
    }
   {
    :match #"\+\+"
    :name "keyword.operator.increment.c"
    }
   {
    :match #"%=|\+=|-=|\*=|(?<!\()/="
    :name "keyword.operator.assignment.compound.c"
    }
   {
    :match #"&=|\^=|<<=|>>=|\|="
    :name "keyword.operator.assignment.compound.bitwise.c"
    }
   {
    :match #"<<|>>"
    :name "keyword.operator.bitwise.shift.c"
    }
   {
    :match #"!=|<=|>=|==|<|>"
    :name "keyword.operator.comparison.c"
    }
   {
    :match #"&&|!|\|\|"
    :name "keyword.operator.logical.c"
    }
   {
    :match #"&|\||\^|~"
    :name "keyword.operator.c"
    }
   {
    :match #"="
    :name "keyword.operator.assignment.c"
    }
   {
    :match #"%|\*|/|-|\+"
    :name "keyword.operator.c"
    }
   ;; skipped ternary operator bogus
   ])

(def ^:private c-libc-patterns
  [{
    :captures {1 {:name "punctuation.whitespace.support.function.leading.c"}
               2 {:name "support.function.C99.c"}}
    :match #"(?x) (\s*) \b
(_Exit|(?:nearbyint|nextafter|nexttoward|netoward|nan)[fl]?|a(?:cos|sin)h?[fl]?|abort|abs|asctime|assert
|atan(?:[h2]?[fl]?)?|atexit|ato[ifl]|atoll|bsearch|btowc|cabs[fl]?|cacos|cacos[fl]|cacosh[fl]?
|calloc|carg[fl]?|casinh?[fl]?|catanh?[fl]?|cbrt[fl]?|ccosh?[fl]?|ceil[fl]?|cexp[fl]?|cimag[fl]?
|clearerr|clock|clog[fl]?|conj[fl]?|copysign[fl]?|cosh?[fl]?|cpow[fl]?|cproj[fl]?|creal[fl]?
|csinh?[fl]?|csqrt[fl]?|ctanh?[fl]?|ctime|difftime|div|erfc?[fl]?|exit|fabs[fl]?
|exp(?:2[fl]?|[fl]|m1[fl]?)?|fclose|fdim[fl]?|fe[gs]et(?:env|exceptflag|round)|feclearexcept
|feholdexcept|feof|feraiseexcept|ferror|fetestexcept|feupdateenv|fflush|fgetpos|fgetw?[sc]
|floor[fl]?|fmax?[fl]?|fmin[fl]?|fmod[fl]?|fopen|fpclassify|fprintf|fputw?[sc]|fread|free|freopen
|frexp[fl]?|fscanf|fseek|fsetpos|ftell|fwide|fwprintf|fwrite|fwscanf|genv|get[sc]|getchar|gmtime
|gwc|gwchar|hypot[fl]?|ilogb[fl]?|imaxabs|imaxdiv|isalnum|isalpha|isblank|iscntrl|isdigit|isfinite
|isgraph|isgreater|isgreaterequal|isinf|isless(?:equal|greater)?|isw?lower|isnan|isnormal|isw?print
|isw?punct|isw?space|isunordered|isw?upper|iswalnum|iswalpha|iswblank|iswcntrl|iswctype|iswdigit|iswgraph
|isw?xdigit|labs|ldexp[fl]?|ldiv|lgamma[fl]?|llabs|lldiv|llrint[fl]?|llround[fl]?|localeconv|localtime
|log[2b]?[fl]?|log1[p0][fl]?|longjmp|lrint[fl]?|lround[fl]?|malloc|mbr?len|mbr?towc|mbsinit|mbsrtowcs
|mbstowcs|memchr|memcmp|memcpy|memmove|memset|mktime|modf[fl]?|perror|pow[fl]?|printf|puts|putw?c(?:har)?
|qsort|raise|rand|remainder[fl]?|realloc|remove|remquo[fl]?|rename|rewind|rint[fl]?|round[fl]?|scalbl?n[fl]?
|scanf|setbuf|setjmp|setlocale|setvbuf|signal|signbit|sinh?[fl]?|snprintf|sprintf|sqrt[fl]?|srand|sscanf
|strcat|strchr|strcmp|strcoll|strcpy|strcspn|strerror|strftime|strlen|strncat|strncmp|strncpy|strpbrk
|strrchr|strspn|strstr|strto[kdf]|strtoimax|strtol[dl]?|strtoull?|strtoumax|strxfrm|swprintf|swscanf
|system|tan|tan[fl]|tanh[fl]?|tgamma[fl]?|time|tmpfile|tmpnam|tolower|toupper|trunc[fl]?|ungetw?c|va_arg
|va_copy|va_end|va_start|vfw?printf|vfw?scanf|vprintf|vscanf|vsnprintf|vsprintf|vsscanf|vswprintf|vswscanf
|vwprintf|vwscanf|wcrtomb|wcscat|wcschr|wcscmp|wcscoll|wcscpy|wcscspn|wcsftime|wcslen|wcsncat|wcsncmp|wcsncpy
|wcspbrk|wcsrchr|wcsrtombs|wcsspn|wcsstr|wcsto[dkf]|wcstoimax|wcstol[dl]?|wcstombs|wcstoull?|wcstoumax|wcsxfrm
|wctom?b|wmem(?:set|chr|cpy|cmp|move)|wprintf|wscanf)\b"}])

(def ^:private c-storage-types-patterns
  [{:match #"\b(asm|__asm__|auto|bool|_Bool|char|_Complex|double|enum|float|_Imaginary|int|long|short|signed|struct|typedef|union|unsigned|void)\b"
    :name "storage.type.c"}])

(def ^:private c-error-warning-directive-patterns
  [{:begin #"^\s*(#)\s*(error|warning)\b"
    :captures {1 {:name "punctuation.definition.directive.c"}
               2 {:name "keyword.control.directive.diagnostic.c"}}
    :end #"$"
    :name "meta.preprocessor.diagnostic.c"
    :patterns (concat c-comments-patterns
                      c-strings-patterns
                      c-line-continuation-character-patterns)}])

(def ^:private c-include-patterns
  [{
    :begin #"^\s*(#)\s*(include(?:_next)?|import)\b\s*"
    :begin-captures {1 {:name "punctuation.definition.directive.c"}
                     2 {:name "keyword.control.directive.c"}}
    :end #"(?=(?://|/\*))|(?<!\\)(?=$)"
    :name "meta.preprocessor.include.c"
    :patterns (concat c-line-continuation-character-patterns
                      [{
                        :begin #"\""
                        :begin-captures {0 {:name "punctuation.definition.string.begin.c"}}
                        :end #"\""
                        :end-captures {0 {:name "punctuation.definition.string.end.c"}}
                        :name "string.quoted.double.include.c"
                        }
                       {
                        :begin #"<"
                        :begin-captures {0 {:name "punctuation.definition.string.begin.c"}}
                        :end #">"
                        :end-captures {0 {:name "punctuation.definition.string.end.c"}}
                        :name "string.quoted.other.lt-gt.include.c"
                        }
                       ])}])

(def ^:private c-pragma-patterns
  [{
    :begin #"^\s*(#)\s*(pragma)\b"
    :begin-captures {1 {:name "punctuation.definition.directive.c"}
                     2 {:name "keyword.control.directive.pragma.c"}}
    :end #"(?=(?://|/\*))|(?<!\\)(?=$)"
    :name "meta.preprocessor.pragma.c"
    :patterns (concat c-strings-patterns
                      [{
                        :match #"[a-zA-Z_$][\w\-$]*"
                        :name "entity.other.attribute-name.pragma.preprocessor.c"
                        }
                       ]
                      c-numbers-patterns
                      c-line-continuation-character-patterns)}])

(def ^:private c-line-patterns
  [{:begin #"^\s*(#)\s*(line)\b"
    :begin-captures {1 {:name "punctuation.definition.directive.c"}
                     2 {:name "keyword.control.directive.line.c"}}
    :end #"(?=(?://|/\*))|(?<!\\)(?=$)"
    :name "meta.preprocessor.c"
    :patterns (concat c-strings-patterns
                      c-numbers-patterns
                      c-line-continuation-character-patterns)}])

(def ^:private c-naive-define-patterns
  [{:begin #"(?x)
^\s* (\#\s*define) \s+                     # define
([a-zA-Z_$][\w$]*)                         # macro name
(?:
  (\()
    (
      \s* [a-zA-Z_$][\w$]* \s*             # first argument
      (?:(?:,) \s* [a-zA-Z_$][\w$]* \s*)*  # additional arguments
      (?:\.\.\.)?                          # varargs ellipsis?
    )
  (\))
)?"
    :begin-captures {1 {:name "keyword.control.directive.define.c"}
                     2 {:name "entity.name.function.preprocessor.c"}
                     3 {:name "punctuation.definition.parameters.begin.c"}
                     4 {:name "variable.parameters.preprocessor.c"}
                     5 {:name "punctuation.definition.parameters.end.c"}}
    :end #"(?=(?://|/\*))|(?<!\\)(?=$)"
    :name "meta.preprocessor.macro.c"
    :patterns []}]) ;; here we should work on the content also...

(def ^:private c-undef-patterns
  [{
    :begin #"^\s*(#)\s*(undef)\b"
    :begin-captures {1 {:name "punctuation.definition.directive.c"}
                     2 {:name "keyword.control.directive.undef.c"}}
    :end #"(?=(?://|/\*))|(?<!\\)(?=$)"
    :name "meta.preprocessor.c"
    :patterns (concat [{:match #"[a-zA-Z_$][\w$]*"
                        :name "entity.name.function.preprocessor.c"
                        }]
                      c-line-continuation-character-patterns)}])

(def ^:private c-sys-types-patterns
  [{:match #"\b(u_char|u_short|u_int|u_long|ushort|uint|u_quad_t|quad_t|qaddr_t|caddr_t|daddr_t|div_t|dev_t|fixpt_t|blkcnt_t|blksize_t|gid_t|in_addr_t|in_port_t|ino_t|key_t|mode_t|nlink_t|id_t|pid_t|off_t|segsz_t|swblk_t|uid_t|id_t|clock_t|size_t|ssize_t|time_t|useconds_t|suseconds_t)\b"
    :name "support.type.sys-types.c"}])

(def ^:private c-pthread-types-patterns
  [{:match #"\b(pthread_attr_t|pthread_cond_t|pthread_condattr_t|pthread_mutex_t|pthread_mutexattr_t|pthread_once_t|pthread_rwlock_t|pthread_rwlockattr_t|pthread_t|pthread_key_t)\b"
    :name "support.type.pthread.c"}])

(def ^:private c-stdint-types-patterns
  [{:match #"(?x) \b
(int8_t|int16_t|int32_t|int64_t|uint8_t|uint16_t|uint32_t|uint64_t|int_least8_t
|int_least16_t|int_least32_t|int_least64_t|uint_least8_t|uint_least16_t|uint_least32_t
|uint_least64_t|int_fast8_t|int_fast16_t|int_fast32_t|int_fast64_t|uint_fast8_t
|uint_fast16_t|uint_fast32_t|uint_fast64_t|intptr_t|uintptr_t|intmax_t|intmax_t
|uintmax_t|uintmax_t)
\b"
    :name "support.type.stdint.c"}])

(def ^:private c-square-bracket-patterns
  [{
    :match #"(\[)|(\])"
    :captures {1 {:name "punctuation.definition.begin.bracket.square.c"}
               2 {:name "punctuation.definition.end.bracket.square.c"}}}])

(def ^:private c-control-keywords-patterns
  [{:match #"\b(break|case|continue|default|do|else|for|goto|if|_Pragma|return|switch|while)\b"
    :name "keyword.control.c"}])

(def ^:private c-storage-modifiers-patterns
  [{
    :match #"\b(const|extern|register|restrict|static|volatile|inline)\b"
    :name "storage.modifier.c"}])

(def ^:private c-language-constants-patterns
  [{
    :match #"\b(NULL|true|false|TRUE|FALSE)\b"
    :name "constant.language.c"}])

(def ^:private c-terminator-patterns
  [{:match #";"
    :name "punctuation.terminator.statement.c"}])

(def ^:private c-separator-patterns
  [{:match #","
    :name "punctuation.separator.delimiter.c"}])

(def grammar
  {:name "CISH"
   :scope-name "source.cish"
   :indent {:begin #"^.*\{[^}\"\']*$|^.*\([^\)\"\']*$|^\s*\{\}$"
            :end #"^\s*(\s*/[*].*[*]/\s*)*\}|^\s*(\s*/[*].*[*]/\s*)*\)"}
   :patterns (concat c-comments-patterns
                     c-storage-types-patterns
                     c-control-keywords-patterns
                     c-storage-modifiers-patterns
                     c-language-constants-patterns
                     c-operators-patterns
                     c-numbers-patterns
                     c-strings-patterns

                     c-error-warning-directive-patterns
                     c-include-patterns
                     c-pragma-patterns
                     c-line-patterns

                     c-naive-define-patterns
                     c-undef-patterns

                     c-sys-types-patterns
                     c-pthread-types-patterns
                     c-stdint-types-patterns
                     c-libc-patterns

                     c-square-bracket-patterns
                     c-terminator-patterns
                     c-separator-patterns

                     [{:match #"\b(friend|explicit|virtual|override|final|noexcept)\b"
                       :name "storage.modifier.cish"
                       }
                      {:match #"\b(private:|protected:|public:)"
                       :name "storage.modifier.cish"
                       }
                      {:match #"\b(catch|operator|try|throw|using)\b"
                       :name "keyword.control.cish"
                       }
                      {:match #"\bdelete\b(\s*\[\])?|\bnew\b(?!])"
                       :name "keyword.control.cish"
                       }
                      {:match #"<="
                       :name "keyword.operator.cish"
                       }
                      {:match #"\bthis\b"
                       :name "variable.language.this.cish"
                       }
                      {:match #"\bnulptr\b"
                       :name "constant.language.cish"
                       }
                      {:match #"\btemplate\b\s*"
                       :name "storage.type.template.cish"
                       }
                      {:match #"\b(const_cast|dynamic_cast|reinterpret_cast|static_cast)\b\s*"
                       :name "keyword.operator.cast.cish"
                       }
                      {:match #"::"
                       :name "punctuation.separator.namespace.access.cpp"
                       }
                      {:match #"\b(and|and_eq|bitand|bitor|compl|not|not_eq|or|or_eq|typeid|xor|xor_eq|alignof|alignas)\b"
                       :name "keyword.operator.cpp"
                       }
                      {
                       :match #"\b(class|decltype|wchar_t|char16_t|char32_t)\b"
                       :name "storage.type.cpp"
                       }
                      {
                       :match #"\b(constexpr|export|mutable|typename|thread_local)\b"
                       :name "storage.modifier.cpp"
                       }
                      ;; strings
                      {
                       :begin #"(u|u8|U|L)?R?\""
                       :begin-captures {0 {:name "punctuation.definition.string.begin.cpp"}}
                       :end #"\""
                       :end-captures {0 {:name "punctuation.definition.string.end.cpp"}}
                       :name "string.quoted.double.cpp"
                       :patterns (concat [{:match #"\\u\h{4}|\\U\h{8}"
                                           :name "constant.character.escape.cpp"
                                           }
                                          {
                                           :match #"\\['\"?\\abfnrtv]"
                                           :name "constant.character.escape.cpp"
                                           }
                                          {
                                           :match #"\\[0-7]{13}"
                                           :name "constant.character.escape.cpp"
                                           }
                                          {
                                           :match #"\\x\h+"
                                           :name "constant.character.escape.cpp"
                                           }
                                          ]
                                         c-string-placeholder-patterns)
                       }])})
