(ns editor.script-api-test
  (:require [clojure.string :as string]
            [clojure.test :refer :all]
            [editor.script-api :as sapi]))

(defn std
  ([name type]
   (std name type nil))
  ([name type doc]
   {:type type
    :name name
    :doc doc
    :display-string name
    :insert-string name}))

(def just-a-variable
  "
- name: other
  type: number")

(def just-a-variable-expected-result
  {"" [(std "other" :variable)]})

(def empty-table
  "
- name: table
  type: table")

(def empty-table-expected-result
  {"" [(std "table" :namespace)]
   "table" []})

(def table-with-members
  "
- name: other
  type: table
  desc: 'Another table'
  members:
    - name: Hello
      type: number")

(def table-with-members-expected-result
  {"" [(std "other" :namespace "Another table")]
   "other" [(std "other.Hello" :variable)]})

(def function-with-one-parameter
  "
- name: fun
  type: function
  desc: This is super great function!
  parameters:
    - name: plopp
      type: plupp")

(def function-with-one-parameter-expected-result
  {""
   [{:type :function
     :name "fun"
     :doc "This is super great function!"
     :display-string "fun(plopp)"
     :insert-string "fun(plopp)"
     :tab-triggers {:select ["plopp"]
                    :exit ")"}}]})

(def empty-top-level-definition
  "- ")

(def empty-top-level-definition-expected-result
  {"" []})

(def broken-table-member-list
  "
- name: other
  type: table
  desc: 'Another table'
  members:
    - nam")

(def broken-table-member-list-expected-result
  {"" [(std "other" :namespace "Another table")]
   "other" []})

(defn do-convert
  [source]
  (sapi/combine-conversions (sapi/convert-lines (string/split-lines source))))

(deftest conversions
  (are [x y] (= x (do-convert y))
    just-a-variable-expected-result just-a-variable
    empty-table-expected-result empty-table
    table-with-members-expected-result table-with-members
    function-with-one-parameter-expected-result function-with-one-parameter
    empty-top-level-definition-expected-result empty-top-level-definition
    broken-table-member-list-expected-result broken-table-member-list))

(def base_doc
  "
- desc: 'Documentation for the Lua base standard library.


    From [Lua 5.1 Reference Manual](https://www.lua.org/manual/5.1/)

    by Roberto Ierusalimschy, Luiz Henrique de Figueiredo, Waldemar Celes.


    Copyright &copy; 2006-2012 Lua.org, PUC-Rio.


    Freely available under the terms of the [Lua license](https://www.lua.org/license.html).'
  members:
  - desc: 'Issues an  error when

      the value of its argument v is false (i.e., [type: nil] or [type: false]);

      otherwise, returns all its arguments.

      message is an error message;

      when absent, it defaults to \"assertion failed!\"'
    name: assert
    parameters:
    - desc: ''
      name: v
      type: ''
    - desc: ''
      name: message
      optional: true
      type: ''
    type: function
  - desc: 'This function is a generic interface to the garbage collector.

      It performs different functions according to its first argument, opt:




      \"collect\"

      : performs a full garbage-collection cycle.

      This is the default option.




      \"stop\"

      : stops the garbage collector.




      \"restart\"

      : restarts the garbage collector.




      \"count\"

      : returns the total memory in use by Lua (in Kbytes).




      \"step\"

      : performs a garbage-collection step.

      The step \"size\" is controlled by arg

      (larger values mean more steps) in a non-specified way.

      If you want to control the step size

      you must experimentally tune the value of arg.

      Returns true if the step finished a collection cycle.




      \"setpause\"

      : sets arg as the new value for the <em>pause</em> of

      the collector .

      Returns the previous value for <em>pause</em>.




      \"setstepmul\"

      : sets arg as the new value for the <em>step multiplier</em> of

      the collector .

      Returns the previous value for <em>step</em>.'
    name: collectgarbage
    parameters:
    - desc: ''
      name: opt
      optional: true
      type: ''
    - desc: ''
      name: arg
      optional: true
      type: ''
    type: function
  - desc: 'Opens the named file and executes its contents as a Lua chunk.

      When called without arguments,

      dofile executes the contents of the standard input (stdin).

      Returns all values returned by the chunk.

      In case of errors, dofile propagates the error

      to its caller (that is, dofile does not run in protected mode).'
    name: dofile
    parameters:
    - desc: ''
      name: filename
      optional: true
      type: ''
    type: function
  - desc: 'Terminates the last protected function called

      and returns message as the error message.

      Function error never returns.


      Usually, error adds some information about the error position

      at the beginning of the message.

      The level argument specifies how to get the error position.

      With level 1 (the default), the error position is where the

      error function was called.

      Level 2 points the error to where the function

      that called error was called; and so on.

      Passing a level 0 avoids the addition of error position information

      to the message.'
    name: error
    parameters:
    - desc: ''
      name: message
      type: ''
    - desc: ''
      name: level
      optional: true
      type: ''
    type: function
  - desc: 'A global variable (not a function) that

      holds the global environment (that is, `_G._G = _G`).

      Lua itself does not use this variable;

      changing its value does not affect any environment,

      nor vice-versa.

      (Use setfenv to change environments.)'
    name: _G
    type: number
  - desc: 'Returns the current environment in use by the function.

      f can be a Lua function or a number

      that specifies the function at that stack level:

      Level 1 is the function calling getfenv.

      If the given function is not a Lua function,

      or if f is 0,

      getfenv returns the global environment.

      The default for f is 1.'
    name: getfenv
    parameters:
    - desc: ''
      name: f
      optional: true
      type: ''
    type: function
  - desc: 'If object does not have a metatable, returns [type: nil].

      Otherwise,

      if the object''s metatable has a `\"__metatable\"` field,

      returns the associated value.

      Otherwise, returns the metatable of the given object.'
    name: getmetatable
    parameters:
    - desc: ''
      name: object
      type: ''
    type: function
  - desc: 'Returns three values: an iterator function, the table t, and 0,

      so that the construction



      ```lua

      for i,v in ipairs(t) do body end

      ```


      will iterate over the pairs (`1,t[1]`), (`2,t[2]`), ...,

      up to the first integer key absent from the table.'
    name: ipairs
    parameters:
    - desc: ''
      name: t
      type: ''
    type: function
  - desc: 'Loads a chunk using function func to get its pieces.

      Each call to func must return a string that concatenates

      with previous results.

      A return of an empty string, [type: nil], or no value signals the end of the
      chunk.


      If there are no errors,

      returns the compiled chunk as a function;

      otherwise, returns [type: nil] plus the error message.

      The environment of the returned function is the global environment.


      chunkname is used as the chunk name for error messages

      and debug information.

      When absent,

      it defaults to \"=(load)\".'
    name: load
    parameters:
    - desc: ''
      name: func
      type: ''
    - desc: ''
      name: chunkname
      optional: true
      type: ''
    type: function
  - desc: 'Similar to load,

      but gets the chunk from file filename

      or from the standard input,

      if no file name is given.'
    name: loadfile
    parameters:
    - desc: ''
      name: filename
      optional: true
      type: ''
    type: function
  - desc: 'Similar to load,

      but gets the chunk from the given string.


      To load and run a given string, use the idiom



      ```lua

      assert(loadstring(s))()

      ```



      When absent,

      chunkname defaults to the given string.'
    name: loadstring
    parameters:
    - desc: ''
      name: string
      type: ''
    - desc: ''
      name: chunkname
      optional: true
      type: ''
    type: function
  - desc: 'Allows a program to traverse all fields of a table.

      Its first argument is a table and its second argument

      is an index in this table.

      next returns the next index of the table

      and its associated value.

      When called with [type: nil] as its second argument,

      next returns an initial index

      and its associated value.

      When called with the last index,

      or with [type: nil] in an empty table,

      next returns [type: nil].

      If the second argument is absent, then it is interpreted as [type: nil].

      In particular,

      you can use `next(t)` to check whether a table is empty.


      The order in which the indices are enumerated is not specified,

      <em>even for numeric indices</em>.

      (To traverse a table in numeric order,

      use a numerical for or the ipairs function.)


      The behavior of next is <em>undefined</em> if,

      during the traversal,

      you assign any value to a non-existent field in the table.

      You may however modify existing fields.

      In particular, you may clear existing fields.'
    name: next
    parameters:
    - desc: ''
      name: table
      type: ''
    - desc: ''
      name: index
      optional: true
      type: ''
    type: function
  - desc: 'Returns three values: the next function, the table t, and [type: nil],

      so that the construction



      ```lua

      for k,v in pairs(t) do body end

      ```


      will iterate over all key-{}value pairs of table t.


      See function next for the caveats of modifying

      the table during its traversal.'
    name: pairs
    parameters:
    - desc: ''
      name: t
      type: ''
    type: function
  - desc: 'Calls function f with

      the given arguments in <em>protected mode</em>.

      This means that any error inside `f` is not propagated;

      instead, pcall catches the error

      and returns a status code.

      Its first result is the status code (a boolean),

      which is true if the call succeeds without errors.

      In such case, pcall also returns all results from the call,

      after this first result.

      In case of any error, pcall returns [type: false] plus the error message.'
    name: pcall
    parameters:
    - desc: ''
      name: f
      type: ''
    - desc: ''
      name: arg1
      type: ''
    - desc: ''
      name: '...'
      type: ''
    type: function
  - desc: 'Receives any number of arguments,

      and prints their values to stdout,

      using the tostring function to convert them to strings.

      print is not intended for formatted output,

      but only as a quick way to show a value,

      typically for debugging.

      For formatted output, use string.format.'
    name: print
    parameters:
    - desc: ''
      name: '...'
      type: ''
    type: function
  - desc: 'Checks whether v1 is equal to v2,

      without invoking any metamethod.

      Returns a boolean.'
    name: rawequal
    parameters:
    - desc: ''
      name: v1
      type: ''
    - desc: ''
      name: v2
      type: ''
    type: function
  - desc: 'Gets the real value of `table[index]`,

      without invoking any metamethod.

      table must be a table;

      index may be any value.'
    name: rawget
    parameters:
    - desc: ''
      name: table
      type: ''
    - desc: ''
      name: index
      type: ''
    type: function
  - desc: 'Sets the real value of `table[index]` to value,

      without invoking any metamethod.

      table must be a table,

      index any value different from [type: nil],

      and value any Lua value.


      This function returns table.'
    name: rawset
    parameters:
    - desc: ''
      name: table
      type: ''
    - desc: ''
      name: index
      type: ''
    - desc: ''
      name: value
      type: ''
    type: function
  - desc: 'If index is a number,

      returns all arguments after argument number index.

      Otherwise, index must be the string `\"#\"`,

      and select returns the total number of extra arguments it received.'
    name: select
    parameters:
    - desc: ''
      name: index
      type: ''
    - desc: ''
      name: '...'
      type: ''
    type: function
  - desc: 'Sets the environment to be used by the given function.

      f can be a Lua function or a number

      that specifies the function at that stack level:

      Level 1 is the function calling setfenv.

      setfenv returns the given function.


      As a special case, when f is 0 setfenv changes

      the environment of the running thread.

      In this case, setfenv returns no values.'
    name: setfenv
    parameters:
    - desc: ''
      name: f
      type: ''
    - desc: ''
      name: table
      type: ''
    type: function
  - desc: 'Sets the metatable for the given table.

      (You cannot change the metatable of other types from Lua, only from C.)

      If metatable is [type: nil],

      removes the metatable of the given table.

      If the original metatable has a `\"__metatable\"` field,

      raises an error.


      This function returns table.'
    name: setmetatable
    parameters:
    - desc: ''
      name: table
      type: ''
    - desc: ''
      name: metatable
      type: ''
    type: function
  - desc: 'Tries to convert its argument to a number.

      If the argument is already a number or a string convertible

      to a number, then tonumber returns this number;

      otherwise, it returns [type: nil].


      An optional argument specifies the base to interpret the numeral.

      The base may be any integer between 2 and 36, inclusive.

      In bases above 10, the letter ''A'' (in either upper or lower case)

      represents 10, ''B'' represents 11, and so forth,

      with ''Z'' representing 35.

      In base 10 (the default), the number can have a decimal part,

      as well as an optional exponent part .

      In other bases, only unsigned integers are accepted.'
    name: tonumber
    parameters:
    - desc: ''
      name: e
      type: ''
    - desc: ''
      name: base
      optional: true
      type: ''
    type: function
  - desc: 'Receives an argument of any type and

      converts it to a string in a reasonable format.

      For complete control of how numbers are converted,

      use string.format.


      If the metatable of e has a `\"__tostring\"` field,

      then tostring calls the corresponding value

      with e as argument,

      and uses the result of the call as its result.'
    name: tostring
    parameters:
    - desc: ''
      name: e
      type: ''
    type: function
  - desc: 'Returns the type of its only argument, coded as a string.

      The possible results of this function are

      \"nil\" (a string, not the value [type: nil]),

      \"number\",

      \"string\",

      \"boolean\",

      \"table\",

      \"function\",

      \"thread\",

      and \"userdata\".'
    name: type
    parameters:
    - desc: ''
      name: v
      type: ''
    type: function
  - desc: 'Returns the elements from the given table.

      This function is equivalent to



      ```lua

      return list[i], list[i+1], ..., list[j]

      ```


      except that the above code can be written only for a fixed number

      of elements.

      By default, i is 1 and j is the length of the list,

      as defined by the length operator .'
    name: unpack
    parameters:
    - desc: ''
      name: list
      type: ''
    - desc: ''
      name: i
      optional: true
      type: ''
    - desc: ''
      name: j
      optional: true
      type: ''
    type: function
  - desc: 'A global variable (not a function) that

      holds a string containing the current interpreter version.

      The current contents of this variable is \"Lua 5.1\".'
    name: _VERSION
    type: number
  - desc: 'This function is similar to pcall,

      except that you can set a new error handler.


      xpcall calls function f in protected mode,

      using err as the error handler.

      Any error inside f is not propagated;

      instead, xpcall catches the error,

      calls the err function with the original error object,

      and returns a status code.

      Its first result is the status code (a boolean),

      which is true if the call succeeds without errors.

      In this case, xpcall also returns all results from the call,

      after this first result.

      In case of any error,

      xpcall returns [type: false] plus the result from err.'
    name: xpcall
    parameters:
    - desc: ''
      name: f
      type: ''
    - desc: ''
      name: err
      type: ''
    type: function
  - desc: 'Creates a module.

      If there is a table in `package.loaded[name]`,

      this table is the module.

      Otherwise, if there is a global table t with the given name,

      this table is the module.

      Otherwise creates a new table t and

      sets it as the value of the global name and

      the value of `package.loaded[name]`.

      This function also initializes t._NAME with the given name,

      t._M with the module (t itself),

      and t._PACKAGE with the package name

      (the full module name minus last component; see below).

      Finally, module sets t as the new environment

      of the current function and the new value of `package.loaded[name]`,

      so that require returns t.


      If name is a compound name

      (that is, one with components separated by dots),

      module creates (or reuses, if they already exist)

      tables for each component.

      For instance, if name is a.b.c,

      then module stores the module table in field c of

      field b of global a.


      This function can receive optional <em>options</em> after

      the module name,

      where each option is a function to be applied over the module.'
    name: module
    parameters:
    - desc: ''
      name: name
      type: ''
    - desc: ''
      name: '...'
      optional: true
      type: ''
    type: function
  - desc: 'Loads the given module.

      The function starts by looking into the package.loaded table

      to determine whether modname is already loaded.

      If it is, then require returns the value stored

      at `package.loaded[modname]`.

      Otherwise, it tries to find a <em>loader</em> for the module.


      To find a loader,

      require is guided by the package.loaders array.

      By changing this array,

      we can change how require looks for a module.

      The following explanation is based on the default configuration

      for package.loaders.


      First require queries `package.preload[modname]`.

      If it has a value,

      this value (which should be a function) is the loader.

      Otherwise require searches for a Lua loader using the

      path stored in package.path.

      If that also fails, it searches for a C loader using the

      path stored in package.cpath.

      If that also fails,

      it tries an <em>all-in-one</em> loader .


      Once a loader is found,

      require calls the loader with a single argument, modname.

      If the loader returns any value,

      require assigns the returned value to `package.loaded[modname]`.

      If the loader returns no value and

      has not assigned any value to `package.loaded[modname]`,

      then require assigns true to this entry.

      In any case, require returns the

      final value of `package.loaded[modname]`.


      If there is any error loading or running the module,

      or if it cannot find any loader for the module,

      then require signals an error.'
    name: require
    parameters:
    - desc: ''
      name: modname
      type: ''
    type: function
  name: base
  type: table
")

(require '[editor.code-completion :as cc])

(do-convert base_doc)

(do-convert just-a-variable)
{"" [ { :type :variable, :name "other", :doc nil, :display-string "other", :insert-string "other" } ]}

(some #(= "other" (:name %))
      (get
        (cc/combine-completions {}
                                (do-convert just-a-variable)
                                {"" [ { :type :variable, :name "something", :doc nil, :display-string "something", :insert-string "something" } ]}
                                )
        ""))

(count (get
         (cc/combine-completions {}
                                 (do-convert just-a-variable)
                                 {"" [ { :type :variable, :name "something", :doc nil, :display-string "something", :insert-string "something" } ]}
                                 )
         "")
       )

;; DONE(mags): What is going on?
;; Looks like combine-completions is not doing the RIGHT THING with the maps?
;; Copy, gut the builtins and repro!

(require '[internal.util :as util])

(defn combine-completions
  [one two three]
  (merge-with (fn [dest new]
                (let [taken-display-strings (into #{} (map :display-string) dest)]
                  (into dest
                        (comp (remove #(contains? taken-display-strings (:display-string %)))
                              (util/distinct-by :display-string))
                        new)))
              one two three))

(combine-completions (do-convert base_doc)
                     (do-convert just-a-variable)
                     {"" [ { :type :variable, :name "something", :doc nil, :display-string "something", :insert-string "something" } ]})

(cc/combine-completions (do-convert base_doc)
                     (do-convert just-a-variable)
                     {"" [ { :type :variable, :name "something", :doc nil, :display-string "something", :insert-string "something" } ]})

;; TODO(mags): Looks like make-local-completions and combine-completions in
;; code_completion.clj mess things up. What does the input look like to those
;; functions? Need to find out so that I can test this properly instead of
;; assuming they take map input of the same structure as the script-intelligence
;; output.

