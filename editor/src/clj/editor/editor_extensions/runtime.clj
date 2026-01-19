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

(ns editor.editor-extensions.runtime
  "Editor Lua runtime

  This provides functionality for the editor scripts to access the editor, and
  for the editor to interact with editor scripts

  One important feature of the editor runtime is support for long-running
  computations without blocking the VM. It is achieved with coroutines: when
  the editor scripts attempt to perform a long-running computation, the
  Lua code execution suspends and returns control back to the editor. The editor
  then schedules the computation and, once it's done, resumes the execution.
  This runtime splits coroutine into 2 contexts: 'user' and 'system'. With this
  split, editor-related suspends are invisible to the editor script, so the
  editor script may still use coroutines internally without interfering with
  the editor.

  This namespace defines 2 ways to run Lua code:
    1. invoke-suspending - call the LuaFunction in a system coroutine, allowing
       it to use suspendable functions defined by the editor.
    2. invoke-immediate - call the LuaFunction with suspendable functions
       defined by the editor disabled. This way there is no coroutine overhead,
       and code executes significantly faster.

  Lua code evaluation is done using read / bind / invoke-{suspending,immediate}
  cycle:
    - read takes a string of Lua code and compiles it into a reusable prototype
    - bind takes a prototype and creates a 0-arg Lua function (a closure), that,
      when invoked, will evaluate the code in the runtime
    - invoke-{suspending,immediate} will actually evaluate the code.
  We split 'eval' into 'bind' and 'invoke-{suspending,immediate}' because there
  are different evaluation semantics for invoke-suspending and invoke-immediate
  that the users of the runtime need to choose."
  (:refer-clojure :exclude [read])
  (:require [cljfx.api :as fx]
            [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.editor-extensions.coerce :as coerce]
            [editor.editor-extensions.vm :as vm]
            [editor.future :as future]
            [editor.resource :as resource]
            editor.workspace)
  (:import [com.defold.editor.luart DefoldBaseLib DefoldCoroutine$Create DefoldCoroutine$Yield DefoldIoLib DefoldUserdata DefoldLuaFn DefoldVarargsLuaFn SearchPath]
           [editor.resource FileResource ZipResource MemoryResource]
           [editor.workspace BuildResource]
           [java.io File PrintStream Writer]
           [java.nio.charset StandardCharsets]
           [org.apache.commons.io.output WriterOutputStream]
           [org.luaj.vm2 LoadState LuaError LuaFunction LuaString LuaTable LuaValue OrphanedThread Varargs]
           [org.luaj.vm2.compiler LuaC]
           [org.luaj.vm2.lib Bit32Lib CoroutineLib PackageLib PackageLib$lua_searcher PackageLib$preload_searcher StringLib TableLib]
           [org.luaj.vm2.lib.jse JseMathLib JseOsLib]))

(set! *warn-on-reflection* true)

(deftype EditorExtensionsRuntime [lua-vm create resume status yield])

(deftype Suspend [f args])

(defprotocol SuspensionResult
  (^:private deliver-result [suspend-result])
  (^:private refresh-context? [suspend-result]))

(extend-protocol SuspensionResult
  nil
  (deliver-result [x] (vm/->varargs x))
  (refresh-context? [_] false)
  Object
  (deliver-result [x] (vm/->varargs x))
  (refresh-context? [_] false)
  Exception
  (deliver-result [ex] (throw ex))
  (refresh-context? [_] false))

(defn and-refresh-context
  "Given a value delivered from the suspendable function to the Lua runtime,
  instruct the runtime to refresh the execution context before delivering the
  result to the editor script"
  [result]
  (reify SuspensionResult
    (deliver-result [_] (deliver-result result))
    (refresh-context? [_] true)))

(defn ->lua
  "Convert Clojure data structure to Lua data structure"
  ^LuaValue [x]
  (vm/->lua x))

(extend-protocol vm/->Lua
  FileResource (->lua [x] (LuaString/valueOf (resource/proj-path x)))
  ZipResource (->lua [x] (LuaString/valueOf (resource/proj-path x)))
  BuildResource (->lua [x] (LuaString/valueOf (resource/proj-path x)))
  MemoryResource (->lua [_] (throw (LuaError. "Cannot represent memory resource in Lua"))))

(defn ->varargs
  "Convert 0 or more Clojure data structures into Lua varargs"
  ;; all lua values are singleton varargs
  ([] LuaValue/NONE)
  ([x] (->lua x))
  ([x y]
   (LuaValue/varargsOf (->lua x) (->lua y)))
  ([x y z & more]
   (LuaValue/varargsOf (into-array LuaValue (map vm/->lua (cons x (cons y (cons z more))))))))

(defn ->clj
  "Convert Lua data structure to Clojure data structure

  Might lock the runtime Lua VM while deserializing tables
  By default, converts tables either to:
  - vectors, if not empty and all keys are positive ints
  - maps, otherwise. string keys are converted to keywords

  When coercer argument is provided, a more efficient conversion is used,
  converting to the exact specified data shape. See
  editor.editor-extensions.coerce for coercer options.

  Preserves LuaThread, File (from IoLib) and LuaFunction"
  ([^EditorExtensionsRuntime runtime lua-value]
   (vm/->clj lua-value (.-lua-vm runtime)))
  ([^EditorExtensionsRuntime runtime coercer lua-value]
   (coerce/coerce (.-lua-vm runtime) coercer lua-value)))

(defn coerces-to?
  "Check if a LuaValue satisfies the coercer"
  [^EditorExtensionsRuntime rt coercer lua-value]
  (not (coerce/failure? (coercer (.-lua-vm rt) lua-value))))

(def ^:private ^:dynamic *execution-context*
  "Map that holds editor-related data during editor script execution

  The map must have the following keys:
    :evaluation-context    the evaluation context for the current execution
    :rt                    the runtime used for execution
    :mode                  :suspendable or :immediate

  This dynamic var is private and should not be leaked to the users of this ns.
  It is bound when executing immediate and suspendable functions. For
  correctness, it should only be read using current-execution-context fn. It is
  necessary for executing the scripts in a context of a particular editor state
  (evaluation context) without passing the context explicitly to the scripts,
  since they should not store and reuse it. Any editor functions that need
  access to the execution context should receive it as an argument (see lua-fn
  and suspendable-lua-fn)"
  nil)

(defn- current-execution-context []
  {:pre [(some? *execution-context*)]}
  *execution-context*)

(defn- suspend! [f args]
  (let [ctx (current-execution-context)]
    (if (= :immediate (:mode ctx))
      (throw (LuaError. "Cannot use long-running editor function in immediate context")))
    (let [^EditorExtensionsRuntime runtime (:rt ctx)
          vm (.-lua-vm runtime)
          suspend (Suspend. f args)]
      (deliver-result (vm/unwrap-userdata (vm/invoke-1 vm (.-yield runtime) (->lua suspend)))))))

(defn wrap-suspendable-function
  ^LuaFunction [f]
  (DefoldLuaFn.
    (fn suspendable-function-wrapper [& args]
      (suspend! f args))))

(defmacro suspendable-lua-fn
  "Defines a suspendable Lua function

  The function will receive an execution context, as well as LuaValue args that
  were passed by the editor script.
  The function should either:
  - throw Exception to signal a script error
  - return any value - if Exception, it will be signaled as an error to the
    script, otherwise it will be coerced to LuaValue and delivered as a result
  - return a value as above, additionally wrapped using and-refresh-context to
    instruct the runtime to refresh the execution (evaluation) context of the
    running script
  - any of the above, but delivered asynchronously using a CompletableFuture

  Execution context is a map with the following keys:
    :evaluation-context    the evaluation context for the current execution
    :rt                    the runtime used for execution
    :mode                  :suspendable or :immediate"
  [& fn-tail]
  `(wrap-suspendable-function (fn ~@fn-tail)))

(defn wrap-suspendable-varargs-function
  ^LuaFunction [f]
  (DefoldVarargsLuaFn.
    (fn suspendable-varargs-function-wrapper [varargs]
      (suspend! f [varargs]))))

(defmacro suspendable-varargs-lua-fn
  "Defines a suspendable Lua function

  The function will receive 2 args:
    - an execution context
    - varargs arguments that were passed by the editor script.

  The function should either:
  - throw Exception to signal a script error
  - return any value - if Exception, it will be signaled as an error to the
    script, otherwise it will be coerced to LuaValue and delivered as a result
  - return a value as above, additionally wrapped using and-refresh-context to
    instruct the runtime to refresh the execution (evaluation) context of the
    running script
  - any of the above, but delivered asynchronously using a CompletableFuture

  Execution context is a map with the following keys:
    :evaluation-context    the evaluation context for the current execution
    :rt                    the runtime used for execution
    :mode                  :suspendable or :immediate"
  [& fn-tail]
  `(wrap-suspendable-varargs-function (fn ~@fn-tail)))

(defn wrap-immediate-function ^LuaFunction [f]
  (DefoldLuaFn.
    (fn immediate-function-wrapper [& args]
      (vm/->varargs (apply f (current-execution-context) args)))))

(defmacro lua-fn
  "Defines a regular Lua function

  The function will receive an execution context, as well LuaValue args that
  were passed by the editor script. Returned value will be coerced to LuaValue.
  If the returned value is already a LuaValue, it will be left as is. Execution
  context is a map with the following keys:
    :evaluation-context    the evaluation context for the current execution
    :rt                    the runtime used for execution
    :mode                  :suspendable or :immediate"
  [& fn-tail]
  `(wrap-immediate-function (fn ~@fn-tail)))

(defn wrap-immediate-varargs-function
  ^LuaFunction [f]
  (DefoldVarargsLuaFn.
    (fn immediate-varargs-function-wrapper [varargs]
      (vm/->varargs (f (current-execution-context) varargs)))))

(defmacro varargs-lua-fn
  "Defines a regular Lua function

  The function will receive 2 arguments:
    - an execution context
    - varargs arguments that were passed by the editor script.

  Returned value will be coerced to Varargs. If the returned value is already a
  LuaValue or Varargs, it will be left as is.

  Execution context is a map with the following keys:
    :evaluation-context    the evaluation context for the current execution
    :rt                    the runtime used for execution
    :mode                  :suspendable or :immediate"
  [& fn-tail]
  `(wrap-immediate-varargs-function (fn ~@fn-tail)))

(defn read
  "Read a chunk of lua code and return a Prototype for bind

  Chunk can be a string of Lua code, or anything convertible to an InputStream,
  e.g. File, URI, URL, Socket or byte array"
  ([chunk]
   (vm/read chunk "REPL"))
  ([chunk chunk-name]
   (vm/read chunk chunk-name)))

(def ^:private coronest-prototype
  (read (slurp (io/resource "coronest.lua")) "coronest.lua"))

(defn bind
  "Bind a prototype to the runtime

  Returns 0-arg LuaFunction (closure) that evaluates the prototype Lua code in
  this runtime when invoked. Does not execute the code by itself"
  ^LuaFunction [^EditorExtensionsRuntime runtime prototype]
  (vm/bind prototype (.-lua-vm runtime)))

(defn wrap-userdata
  "Wraps the value into a LuaUserdata"
  ([x]
   (vm/wrap-userdata x))
  ([x string-representation]
   (DefoldUserdata. x string-representation)))

(defn- writer->print-stream [^Writer writer]
  (-> writer
      (WriterOutputStream. StandardCharsets/UTF_8)
      (PrintStream. true StandardCharsets/UTF_8)))

(defn- merge-env-impl! [globals m]
  (reduce-kv
    (fn [^LuaValue acc k v]
      (let [lua-key (->lua k)
            old-lua-val (.get acc lua-key)]
        (if (and (map? v) (.istable old-lua-val))
          (merge-env-impl! old-lua-val v)
          (.set acc lua-key (->lua v)))
        acc))
    globals
    m))

(defn make
  "Construct fresh Lua runtime for editor scripts

  Returned runtime is thread safe and does not block the LuaVM when it executes
  long-running computations started with suspendable functions.

  Optional kv-args:
    :find-resource    2-arg function from an execution context and a proj-path
                      that returns either nil if the resource is not found or
                      InputStream to read the contents of found resource,
                      defaults to (constantly nil); execution context is a map
                      with the following keys:
                        :evaluation-context    the evaluation context for the
                                               current execution
                        :rt                    the runtime used for execution
                        :mode                  :suspendable or :immediate
    :resolve-file     2-arg function from an execution context and file path
                      string to a resolved file string; execution context is a
                      map with the following keys:
                        :evaluation-context    the evaluation context for the
                                               current execution
                        :rt                    the runtime used for execution
                        :mode                  :suspendable or :immediate
    :close-written    LuaFunction that will be invoked after closing a File that
                      was written to
    :out              Writer for the runtime standard output
    :err              Writer for the runtime error output
    :env              possibly nested map of values to merge with the initial
                      environment"
  ^EditorExtensionsRuntime [& {:keys [out err env find-resource resolve-file close-written]
                               :or {out *out* err *err*}}]
  (let [vm (vm/make)
        package-lib (PackageLib.)
        ;; We don't need to hold the lock on a LuaVM here because we are
        ;; creating it and no other thread can access it yet
        globals (doto (vm/env vm)
                  (.load (DefoldBaseLib.
                           (if find-resource
                             #(find-resource (current-execution-context) (str "/" %))
                             (constantly nil))))
                  (.load package-lib)
                  (.load (Bit32Lib.))
                  (.load (TableLib.))
                  (.load (StringLib.))
                  (.load (CoroutineLib.))
                  (.load (JseMathLib.))
                  (.load (DefoldIoLib.
                           (if resolve-file
                             #(resolve-file (current-execution-context) %)
                             identity)
                           close-written))
                  (.load (JseOsLib.))
                  (LoadState/install)
                  (LuaC/install)
                  (-> .-STDOUT (set! (writer->print-stream out)))
                  (-> .-STDERR (set! (writer->print-stream err))))
        package (.get globals "package")
        ;; Before splitting the coroutine module into 2 contexts (user and
        ;; system), we override the coroutine.create() function with a one that
        ;; conveys the thread bindings to the coroutine thread. This way, both
        ;; in system and user coroutines, the *execution-context* var will be
        ;; properly set.
        _ (doto (.get globals "coroutine")
            (.set "create" (DefoldCoroutine$Create. globals))
            (.set "yield" (DefoldCoroutine$Yield. globals)))

        ;; Now we split the coroutine into 2 contexts
        coronest (vm/invoke-1 vm (vm/bind coronest-prototype vm))
        user-coroutine (vm/invoke-1 vm coronest (->lua "user"))
        system-coroutine (vm/invoke-1 vm coronest (->lua "system"))]

    ;; Don't allow requiring java classes
    (.set package "searchers" (->lua [(PackageLib$preload_searcher. package-lib)
                                      (PackageLib$lua_searcher. package-lib)]))

    ;; Override coroutine as configured by CoroutineLib with user coroutine context
    (.set globals "coroutine" user-coroutine)
    (-> package (.get "loaded") (.set "coroutine" user-coroutine))

    ;; Always use forward slashes for resource path separators
    (.set package "searchpath" (SearchPath. globals))

    ;; Missing in luaj 3.0.1
    (.set package "config" (str File/separatorChar "\n;\n?\n!\n-"))

    (merge-env-impl! globals env)

    (EditorExtensionsRuntime.
      vm
      (.get system-coroutine "create")
      (.get system-coroutine "resume")
      (.get system-coroutine "status")
      (.get system-coroutine "yield"))))

(def ^:private lua-str-dead (->lua "dead"))

(defn- invoke-suspending-impl [execution-context ^EditorExtensionsRuntime runtime co & lua-args]
  (binding [*execution-context* execution-context]
    (let [vm (.-lua-vm runtime)
          ^Varargs lua-success+rest-varargs (apply vm/invoke vm (.-resume runtime) co lua-args)]
      (if (->clj runtime coerce/boolean (.arg1 lua-success+rest-varargs))
        (if (= lua-str-dead (vm/invoke-1 vm (.-status runtime) co))
          (future/completed (.subargs lua-success+rest-varargs 2))
          (let [^Suspend suspend (->clj runtime coerce/userdata (.arg lua-success+rest-varargs 2))]
            (-> (try
                  (future/wrap (apply (.-f suspend) execution-context (.-args suspend)))
                  (catch Throwable e (future/failed e)))
                ;; treat thrown Exceptions as error signals to the scripts
                (future/catch (fn [e]
                                (if (instance? OrphanedThread e)
                                  (throw e)
                                  e)))
                (future/then-async
                  (fn [result]
                    (if (refresh-context? result)
                      (let [update-cache! (bound-fn* g/update-cache-from-evaluation-context!)
                            new-context {:evaluation-context (g/make-evaluation-context)
                                         :rt runtime
                                         :mode :suspendable}]
                        (fx/on-fx-thread (update-cache! (:evaluation-context execution-context)))
                        (invoke-suspending-impl new-context runtime co (vm/wrap-userdata result)))
                      (invoke-suspending-impl execution-context runtime co (vm/wrap-userdata result))))))))
        (future/failed (LuaError. ^String (->clj runtime coerce/to-string (.arg lua-success+rest-varargs 2))))))))

(defn stdout
  "Get runtime output stream"
  ^PrintStream [^EditorExtensionsRuntime rt]
  (.-STDOUT (vm/env (.-lua-vm rt))))

(defn stderr
  "Get runtime error stream"
  ^PrintStream [^EditorExtensionsRuntime rt]
  (.-STDERR (vm/env (.-lua-vm rt))))

(defn invoke-suspending
  "Invoke a potentially long-running LuaFunction

  Returns a CompletableFuture that will be either completed normally with
  the returned LuaValues as a Varargs object or exceptionally.

  Runtime will start invoking the LuaFunction on the calling thread, then will
  move the execution to background threads if necessary. This means that
  invoke-suspending might return a completed CompletableFuture"
  [^EditorExtensionsRuntime runtime lua-fn & lua-args]
  (let [co (vm/invoke-1 (.-lua-vm runtime) (.-create runtime) lua-fn)
        execution-context {:evaluation-context (g/make-evaluation-context)
                           :rt runtime
                           :mode :suspendable}]
    (apply invoke-suspending-impl execution-context runtime co lua-args)))

(defn invoke-suspending-1
  "Invoke a potentially long-running LuaFunction

  Returns a CompletableFuture that will be either completed normally with
  the first returned LuaValue or exceptionally.

  Runtime will start invoking the LuaFunction on the calling thread, then will
  move the execution to background threads if necessary. This means that
  invoke-suspending might return a completed CompletableFuture"
  [^EditorExtensionsRuntime runtime lua-fn & lua-args]
  (future/then
    (apply invoke-suspending runtime lua-fn lua-args)
    #(.arg1 ^Varargs %)))

(defn- invoke-immediate-impl [^EditorExtensionsRuntime runtime vm-invoke-fn lua-fn lua-args evaluation-context]
  (let [context-provided (some? evaluation-context)
        evaluation-context (or evaluation-context (g/make-evaluation-context))
        result (binding [*execution-context* {:evaluation-context evaluation-context
                                              :rt runtime
                                              :mode :immediate}]
                 (apply vm-invoke-fn (.-lua-vm runtime) lua-fn lua-args))]
    (when-not context-provided
      (g/update-cache-from-evaluation-context! evaluation-context))
    result))

(defn invoke-immediate-1
  "Invoke a short-running LuaFunction

  Returns the first returned LuaValue. No calls to suspending functions are
  allowed during this invocation.

  Args:
    runtime                the editor Lua runtime
    lua-fn                 LuaFunction to invoke
    lua-args*              0 or more LuaValue arguments
    evaluation-context?    optional evaluation context for the execution"
  {:arglists '([runtime lua-fn lua-args* evaluation-context?])}
  [runtime lua-fn & rest-args]
  (let [last-arg (last rest-args)
        context-provided (g/evaluation-context? last-arg)
        evaluation-context (when context-provided last-arg)
        lua-args (if context-provided (butlast rest-args) rest-args)]
    (invoke-immediate-impl runtime vm/invoke-1 lua-fn lua-args evaluation-context)))

(defn invoke-immediate
  "Invoke a short-running LuaFunction

  Returns all returned LuaValues as a Varargs object. No calls to suspending
  functions are allowed during this invocation.

  Args:
    runtime                the editor Lua runtime
    lua-fn                 LuaFunction to invoke
    lua-args*              0 or more LuaValue arguments
    evaluation-context?    optional evaluation context for the execution"
  {:arglists '([runtime lua-fn lua-args* evaluation-context?])}
  [^EditorExtensionsRuntime runtime lua-fn & rest-args]
  (let [last-arg (last rest-args)
        context-provided (g/evaluation-context? last-arg)
        evaluation-context (when context-provided last-arg)
        lua-args (if context-provided (butlast rest-args) rest-args)]
    (invoke-immediate-impl runtime vm/invoke lua-fn lua-args evaluation-context)))

(defn eq?
  "Checks 2 lua values for equality, with metadata processing"
  [^EditorExtensionsRuntime rt ^LuaValue a ^LuaValue b]
  {:pre [(instance? LuaValue a) (instance? LuaValue b)]}
  (vm/with-lock (.-lua-vm rt)
    (.eq_b a b)))

(defn tables-shallow-eq?
  "Checks if 2 lua tables are shallow equal (i.e. have the same keys and values)"
  [^EditorExtensionsRuntime rt ^LuaTable a ^LuaTable b]
  {:pre [(instance? LuaTable a) (instance? LuaTable b)]}
  (if (identical? a b)
    true
    (vm/with-lock (.-lua-vm rt)
      (let [a-entry-count
            (loop [entry-count 0
                   prev-key LuaValue/NIL]
              (let [varargs (.next a prev-key)
                    lua-k (.arg1 varargs)]
                (if (.isnil lua-k)
                  entry-count
                  (let [lua-v (.arg varargs 2)]
                    (if (.eq_b (.get b lua-k) lua-v)
                      (recur (inc entry-count) lua-k)
                      -1)))))]
        (if (neg? a-entry-count)
          false
          (zero?
            (loop [count-diff a-entry-count
                   prev-key LuaValue/NIL]
              (let [lua-k (.arg1 (.next b prev-key))]
                (if (.isnil lua-k)
                  count-diff
                  (let [next-count-diff (dec count-diff)]
                    (if (neg? next-count-diff)
                      next-count-diff
                      (recur next-count-diff lua-k))))))))))))