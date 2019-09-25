(ns editor.luart
  (:refer-clojure :exclude [read eval])
  (:require [clojure.string :as string])
  (:import [org.luaj.vm2 Globals LoadState LuaBoolean LuaClosure LuaDouble LuaFunction LuaInteger LuaNil LuaValue LuaValue$None LuaString LuaTable LuaUserdata Varargs Prototype]
           [clojure.lang Fn IPersistentMap IPersistentVector Keyword]
           [org.luaj.vm2.lib Bit32Lib CoroutineLib PackageLib StringLib TableLib VarArgFunction]
           [org.luaj.vm2.lib.jse JseBaseLib JseMathLib JseOsLib]
           [java.io ByteArrayInputStream File PrintStream]
           [org.apache.commons.io.output WriterOutputStream]
           [org.luaj.vm2.compiler LuaC]
           [java.nio.charset Charset]
           [com.defold.editor.luart IoLib]))

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
    (let [kvs (persistent!
                (loop [k LuaValue/NIL
                       acc (transient [])]
                  (let [pair (.next t k)]
                    (if (.isnil (.arg pair 1))
                      acc
                      (recur (.arg pair 1) (conj! acc (lua->clj pair)))))))]
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
  (clj->lua [x] (LuaValue/valueOf (pr-str x)))

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
    (.set globals (clj->lua k) (clj->lua v))))

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
  ^Globals [resource-path->input-stream validate-opened-filename extra-globals display-output!]
  (doto (Globals.)
    (.load (proxy [JseBaseLib] []
             (findResource [filename]
               (resource-path->input-stream (str "/" filename)))))
    (.load (PackageLib.))
    (-> (.get "package") (.set "config" (string/join "\n" [File/pathSeparatorChar \; \? \! \-])))
    (.load (Bit32Lib.))
    (.load (TableLib.))
    (.load (StringLib.))
    (.load (CoroutineLib.))
    (.load (JseMathLib.))
    (.load (proxy [IoLib] []
             (openFile [filename readMode appendMode updateMode binaryMode]
               (let [^IoLib this this]
                 (proxy-super openFile
                              (validate-opened-filename filename readMode)
                              readMode
                              appendMode
                              updateMode
                              binaryMode)))))
    (-> (.get "io") (.set "tmpfile" LuaValue/NIL))
    (.load (JseOsLib.))
    (-> (.get "os") (doto
                      (.set "execute" LuaValue/NIL)
                      (.set "exit" LuaValue/NIL)
                      (.set "remove" LuaValue/NIL)
                      (.set "rename" LuaValue/NIL)
                      (.set "setlocale" LuaValue/NIL)
                      (.set "tmpname" LuaValue/NIL)))
    (LoadState/install)
    (LuaC/install)
    (-> (.-STDOUT) (set! (line-print-stream #(display-output! :out %))))
    (-> (.-STDERR) (set! (line-print-stream #(display-output! :err %))))
    (set-globals! extra-globals)))

(defn read
  ^Prototype [^String chunk chunk-name]
  (.compile LuaC/instance
            (ByteArrayInputStream. (.getBytes chunk (Charset/forName "UTF-8")))
            chunk-name))

(defn eval [prototype env]
  (.call (LuaClosure. prototype env)))
