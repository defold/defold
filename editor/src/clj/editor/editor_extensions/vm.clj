;; Copyright 2020-2024 The Defold Foundation
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

(ns editor.editor-extensions.vm
  "Lua VM construction and interaction

  Lua VMs are not thread-safe. This namespace defines a set of APIs to
  interact with a Lua VM safely and concurrently using reentrant locks
  for synchronizing access

  An important caveat about locking: coroutines run in their own threads. To
  prevent deadlocking, we must not lock the VM in the coroutine thread, because
  at the same time we might be holding a lock in another thread that invoked a
  Lua script that uses coroutines. The with-lock macro is aware of this and will
  not actually lock in coroutine threads. We trust luaj coroutines to
  multi-thread safely in coroutines."
  (:refer-clojure :exclude [read])
  (:import [clojure.lang Named]
           [java.io ByteArrayInputStream]
           [java.nio.charset StandardCharsets]
           [java.util List Map]
           [java.util.concurrent.locks ReentrantLock]
           [org.luaj.vm2 Globals LuaBoolean LuaClosure LuaDouble LuaFunction LuaInteger LuaNil LuaString LuaTable LuaUserdata LuaValue Prototype Varargs]
           [org.luaj.vm2.compiler LuaC]))

(set! *warn-on-reflection* true)

(defn read
  "Read a string with a chunk of lua code and return a Prototype for bind"
  ^Prototype [chunk chunk-name]
  (.compile LuaC/instance
            (ByteArrayInputStream. (.getBytes ^String chunk StandardCharsets/UTF_8))
            chunk-name))

(deftype LuaVM [env lock])

(defn ^Globals env
  "Return VM environment (Globals)

  Returned value is not thread safe"
  [^LuaVM vm]
  (.-env vm))

(defn ^ReentrantLock lock
  "Return lock used for synchronising access to this VM"
  [^LuaVM vm]
  (.-lock vm))

(defn bind
  "Bind a prototype into the VM, returns a 0-arg LuaFunction that evaluates the
  prototype code when invoked"
  [prototype vm]
  (LuaClosure. prototype (env vm)))

(defn must-lock?
  "Checks if the current thread must lock the VM when interacting with it"
  []
  (not (.startsWith (.getName (Thread/currentThread)) "Coroutine-")))

(defmacro with-lock
  "Convenience macro for interacting with a Lua VM under a lock"
  [vm & body]
  `(if (must-lock?)
     (let [lock# (lock ~vm)]
       (try
         (.lock lock#)
         (do ~@body)
         (finally (.unlock lock#))))
     (do ~@body)))

(defn make
  "Create a fresh Lua VM

  See org.luaj.vm2.lib.jse.JsePlatform/standardGlobals for typical setup"
  []
  (LuaVM. (Globals.) (ReentrantLock.)))

(defn- invoke ^Varargs [vm ^LuaFunction lua-fn args]
  (with-lock vm (.invoke lua-fn (LuaValue/varargsOf (into-array LuaValue args)))))

(defn invoke-1
  "Call a lua function while holding a lock on the VM

  Return a LuaValue, the first value returned by the function"
  ^LuaValue [vm lua-fn & lua-args]
  (.arg1 (invoke vm lua-fn lua-args)))

(defn invoke-all
  "Call a lua function while holding a lock on the VM
  Return a vector of all returned LuaValues"
  [vm ^LuaFunction lua-fn & lua-args]
  (let [varargs (invoke vm lua-fn lua-args)
        n (.narg varargs)]
    (loop [acc (transient [])
           i 0]
      (if (= n i)
        (persistent! acc)
        (let [next-i (inc i)]
          (recur (conj! acc (.arg varargs next-i)) next-i))))))

(defn wrap-userdata
  "Wraps the value into a LuaUserdata"
  [x]
  (LuaValue/userdataOf x))

(defn unwrap-userdata
  "Returns the wrapped value"
  [^LuaUserdata userdata]
  (.userdata userdata))

(defprotocol ->Lua
  (->lua [x]))

(defprotocol ->Clj
  (->clj [lua-value vm]))

(extend-protocol ->Lua
  nil (->lua [_] LuaValue/NIL)
  Object (->lua [x] (LuaValue/userdataOf x))
  LuaValue (->lua [x] x)
  Number (->lua [x] (LuaValue/valueOf (double x)))
  Boolean (->lua [x] (LuaValue/valueOf x))
  String (->lua [x] (LuaValue/valueOf x))
  Named (->lua [x] (LuaValue/valueOf (.getName x)))
  List (->lua [x] (LuaValue/listOf (into-array LuaValue (mapv ->lua x))))
  Map (->lua [x] (LuaValue/tableOf (into-array LuaValue (into [] (comp cat (map ->lua)) x)))))

(defn- lua-table->clj-map-or-vector [^LuaTable table vm]
  (loop [prev-k LuaValue/NIL
         acc-v nil
         acc-m nil]
    (let [varargs (.next table prev-k)
          lua-k (.arg1 varargs)]
      (if (.isnil lua-k)
        (or (some-> acc-v persistent!)
            (some-> acc-m persistent!)
            {})
        (let [lua-v (.arg varargs 2)
              clj-k (->clj lua-k vm)
              clj-v (->clj lua-v vm)]
          (cond
            acc-m
            (recur lua-k nil (assoc! acc-m (cond-> clj-k (string? clj-k) keyword) clj-v))

            (pos-int? clj-k)                   ;; grow acc-v
            (let [acc-v (or acc-v (transient []))
                  i (dec clj-k)                ;; 1-indexed to 0-indexed
                  acc-v (loop [acc-v acc-v]
                          (if (< (count acc-v) i)
                            (recur (conj! acc-v nil))
                            acc-v))]
              (recur lua-k (assoc! acc-v i clj-v) nil))

            :else                              ;; convert to map
            (let [acc-m (transient {(cond-> clj-k (string? clj-k) keyword) clj-v})
                  acc-m (if acc-v
                          (let [len (count acc-v)]
                            (loop [i 0
                                   acc-m acc-m]
                              (if (= i len)
                                acc-m
                                (let [v (acc-v i)]
                                  (if (nil? v)
                                    (recur (inc i) acc-m)
                                    (recur (inc i) (assoc! acc-m (inc i) v)))))))
                          acc-m)]
              (recur lua-k nil acc-m))))))))

(extend-protocol ->Clj
  ;; Class hierarchy:
  ;; Varargs
  ;;   LuaValue
  ;;     LuaTable
  ;;     LuaThread
  ;;     LuaBoolean
  ;;     LuaUserdata
  ;;     LuaString
  ;;     LuaNumber
  ;;       LuaDouble
  ;;       LuaInteger
  ;;     LuaNil
  ;;     File
  ;;     LuaFunction
  LuaValue (->clj [x _] x)
  LuaNil (->clj [_ _] nil)
  LuaBoolean (->clj [x _] (.toboolean x))
  LuaUserdata (->clj [x _] (.userdata x))
  LuaString (->clj [x _] (.tojstring x))
  LuaDouble (->clj [x _] (.todouble x))
  LuaInteger (->clj [x _] (.tolong x))
  LuaTable (->clj [table vm]
             ;; only hold the lock for the mutable lua table access
             (with-lock vm (lua-table->clj-map-or-vector table vm))))
