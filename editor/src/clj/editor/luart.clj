;; Copyright 2020 The Defold Foundation
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

(ns editor.luart
  (:refer-clojure :exclude [read eval])
  (:require [clojure.string :as string])
  (:import [clojure.lang Fn IPersistentMap IPersistentVector Keyword]
           [com.defold.editor.luart IoLib]
           [org.apache.commons.io.output WriterOutputStream]
           [org.luaj.vm2 Globals LoadState LuaBoolean LuaClosure LuaDouble LuaFunction LuaInteger LuaNil LuaValue LuaValue$None LuaString LuaTable LuaUserdata Varargs Prototype]
           [org.luaj.vm2.compiler LuaC]
           [org.luaj.vm2.lib Bit32Lib CoroutineLib PackageLib StringLib TableLib VarArgFunction]
           [org.luaj.vm2.lib.jse JseBaseLib JseMathLib JseOsLib]
           [java.io ByteArrayInputStream PrintStream]
           [java.nio.charset Charset]))

(set! *warn-on-reflection* true)

(defprotocol LuaConversions
  (clj->lua ^LuaValue [this])
  (lua->clj [this]))

(extend-protocol LuaConversions
  nil
  (clj->lua [_] LuaValue/NIL)
  LuaNil
  (lua->clj [_] nil)
  LuaValue$None
  (lua->clj [_] nil)

  Number
  (clj->lua [n] (LuaValue/valueOf (double n)))
  LuaInteger
  (lua->clj [i] (.tolong i))
  LuaDouble
  (lua->clj [d] (.todouble d))

  Boolean
  (clj->lua [b] (LuaValue/valueOf b))
  LuaBoolean
  (lua->clj [b] (.toboolean b))

  String
  (clj->lua [s] (LuaValue/valueOf s))
  LuaString
  (lua->clj [s] (.tojstring s))

  LuaUserdata
  (lua->clj [userdata]
    (.userdata userdata))
  (clj->lua [userdata]
    userdata)

  IPersistentVector
  (clj->lua [v]
    (LuaValue/listOf (into-array LuaValue (mapv clj->lua v))))
  IPersistentMap
  (clj->lua [m]
    (LuaValue/tableOf
      (into-array LuaValue
                  (into []
                        (comp
                          cat
                          (map clj->lua))
                        m))))
  Varargs
  (lua->clj [varargs]
    (mapv #(lua->clj (.arg varargs (inc %))) (range (.narg varargs))))
  LuaTable
  (lua->clj [t]
    (let [kvs (loop [k LuaValue/NIL
                     acc (transient [])]
                (let [pair (.next t k)]
                  (if (.isnil (.arg pair 1))
                    (persistent! acc)
                    (recur (.arg pair 1) (conj! acc (lua->clj pair))))))]
      (if (and (seq kvs)
               (= (mapv first kvs)
                  (range 1 (inc (count kvs)))))
        (mapv second kvs)
        (into (array-map)
              (map (fn [[k v]]
                     [(if (string? k) (keyword (string/replace k "_" "-")) k) v]))
              kvs))))

  Keyword
  (clj->lua [k] (LuaValue/valueOf (string/replace (name k) "-" "_")))

  Object
  (clj->lua [x] (LuaValue/userdataOf x))

  Fn
  (clj->lua [f]
    f
    (proxy [VarArgFunction] []
      (invoke [varargs]
        (let [args (if (instance? LuaValue varargs)
                     [(lua->clj varargs)]
                     (lua->clj varargs))]
          (LuaValue/varargsOf (into-array LuaValue [(clj->lua (apply f args))]))))))
  LuaFunction
  (lua->clj [f]
    f))

(defn wrap-user-data [x]
  (LuaValue/userdataOf x))

(defn invoke
  ([^LuaFunction f]
   (.call f))
  ([^LuaFunction f ^LuaValue arg]
   (.call f arg)))

(defn- set-globals! [^LuaValue globals m]
  (doseq [[k v] m]
    (if (map? v)
      (let [lua-key (clj->lua k)
            x (.get globals lua-key)]
        (if (.istable x)
          (set-globals! x v)
          (.set globals lua-key (clj->lua v))))
      (.set globals (clj->lua k) (clj->lua v)))))

(defn- line-print-stream [f]
  (let [sb (StringBuilder.)]
    (PrintStream.
      (WriterOutputStream.
        (PrintWriter-on #(doseq [^char ch %]
                           (if (= \newline ch)
                             (let [str (.toString sb)]
                               (.delete sb 0 (.length sb))
                               (f str))
                             (.append sb ch)))
                        nil)
        "UTF-8")
      true
      "UTF-8")))

(defn make-env
  "Create lua environment

  Expected keys:
  - `:find-resource` (required) - function from resource path to optional input
    stream, gets called when lua requires other lua modules
  - `:open-file` (required) - function called with file name and a boolean
    indicating whether it's open for read (true) or write (false), should return
    resolved file name (string)
  - `:out` (required) - function called when lua prints a line of text, called
    with 2 args: type of output (`:err` or `:out`) and single-line string
  - `:globals` (optional) - additional globals that will be merged into existing
    ones, you can set values to `nil` to remove parts of globals"
  ^Globals [& {:keys [find-resource
                      open-file
                      globals
                      out]}]
  (doto (Globals.)
    (.load (proxy [JseBaseLib] []
             (findResource [filename]
               (find-resource (str "/" filename)))))
    (.load (PackageLib.))
    (.load (Bit32Lib.))
    (.load (TableLib.))
    (.load (StringLib.))
    (.load (CoroutineLib.))
    (.load (JseMathLib.))
    (.load (proxy [IoLib] []
             (openFile [filename read-mode append-mode update-mode binary-mode]
               (let [^IoLib this this]
                 (proxy-super openFile
                              (open-file filename read-mode)
                              read-mode
                              append-mode
                              update-mode
                              binary-mode)))))
    (.load (JseOsLib.))
    (LoadState/install)
    (LuaC/install)
    (-> (.-STDOUT) (set! (line-print-stream #(out :out %))))
    (-> (.-STDERR) (set! (line-print-stream #(out :err %))))
    (set-globals! globals)))

(defn read
  ^Prototype [^String chunk chunk-name]
  (.compile LuaC/instance
            (ByteArrayInputStream. (.getBytes chunk (Charset/forName "UTF-8")))
            chunk-name))

(defn eval [prototype env]
  (.call (LuaClosure. prototype env)))
