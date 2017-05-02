(ns util.luaj
  (:require [clojure.java.io :as io]
            [clojure.string :as string])
  (:import [java.io FileNotFoundException]
           [org.luaj.vm2 LuaBoolean LuaDouble LuaError LuaFunction LuaInteger LuaNil LuaTable LuaValue LuaString Varargs]
           [org.luaj.vm2.lib BaseLib ResourceFinder]
           [org.luaj.vm2.lib.jse JsePlatform]))

(set! *warn-on-reflection* true)

(defprotocol InputStreamFactory
  (->input-stream [this path]))

(defn ->state [is-factory]
  (let [state (JsePlatform/standardGlobals)
        new-finder (reify ResourceFinder
                     (findResource [this filename]
                       (->input-stream is-factory filename)))]
    (set! (. BaseLib FINDER) new-finder)
    state))

(defn- throw-load-error [msg filename]
  (if (string/starts-with? msg "cannot open")
    (throw (FileNotFoundException. msg))
    (if (string/starts-with? msg filename)
      (let [msg (subs msg (count filename))
            ms (re-matches #":(\d+):(.*)" msg)]
        (if ms
          (let [[_ line msg] ms]
            (throw (ex-info (string/trim msg) {:line (Integer/parseInt line)})))
          (throw (ex-info msg {})))))))

(defn load-script [^LuaValue state ^String filename]
  (let [result (.invoke (.get state "loadfile") (LuaValue/valueOf filename))]
    (if (instance? LuaFunction result)
      result
      (when (instance? Varargs result)
        (throw-load-error (.tojstring (.arg result 2)) filename)))))

(defn run-script [^LuaValue script]
  (when script
    (let [^Varargs res (.invoke script)]
      (when (> (.narg res) 0)
        (let [res (.arg res 1)]
          (if (instance? LuaError res)
            (throw res)
            res))))))

(defmulti lua->clj type)

(defmethod lua->clj LuaBoolean
  [^LuaBoolean v]
  (.toboolean v))

(defmethod lua->clj LuaInteger
  [^LuaInteger v]
  (.toint v))

(defmethod lua->clj LuaDouble
  [^LuaDouble v]
  (.todouble v))

(defmethod lua->clj LuaNil
  [^LuaNil v]
  nil)

(defmethod lua->clj LuaTable
  [^LuaTable v]
  (loop [k (LuaValue/NIL)
         i (Integer. 1)
         res []]
    (let [n (.next v k)
          k (.arg1 n)]
      (if (.isnil k)
        res
        (let [v (lua->clj (.arg n 2))
              kv (lua->clj k)]
          (if (= kv i)
            (recur k (inc i) (conj res v))
            (let [res (if (map? res)
                        res
                        (into {} (map-indexed (fn [i v] [(inc i) v]) res)))]
              (recur k nil (assoc res (if (string? kv) (keyword kv) kv) v)))))))))

(defn- clj->lua [v]
  (cond
    (string? v) (LuaValue/valueOf ^String v)
    (map? v) (LuaValue/tableOf (into-array LuaValue (interleave (map (fn [k] (clj->lua (if (keyword? k) (name k) k))) (keys v)) (map clj->lua (vals v)))))
    (vector? v) (LuaValue/listOf (into-array LuaValue (map clj->lua v)))
    (seq? v) (LuaValue/listOf (into-array LuaValue (map clj->lua v)))
    (integer? v) (LuaValue/valueOf ^int v)
    (number? v) (LuaValue/valueOf ^double v)
    true (LuaValue/valueOf ^boolean (boolean v))))

(defmethod lua->clj LuaFunction
  [^LuaFunction v]
  (fn [args]
    (let [res (.invoke v (LuaValue/varargsOf (into-array LuaValue (map clj->lua args))))]
      (case (.narg res)
        0 nil
        1 (lua->clj (.arg res 1))))))

(defmethod lua->clj LuaString
  [^LuaString v]
  (.tojstring v))

(defn get-value [^LuaValue script ^String name]
  (lua->clj (.get script name)))
