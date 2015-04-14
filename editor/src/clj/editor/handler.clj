(ns editor.handler
  (:require [clojure.tools.macro :as ctm]
            [plumbing.core :refer [fnk]]))

(def ^:dynamic *handlers* (atom {}))

(defmacro defhandler [name command context & body]
  (let [qname (keyword (str *ns*) (str name))
        fns (->> body
              (mapcat (fn [[fname fargs fbody]]
                        [(keyword fname) `(fnk ~fargs ~fbody)]))
              (apply hash-map))]
    `(swap! *handlers* assoc ~qname {:context ~context
                                     :command ~command
                                     :fns ~fns})))

(defn- get-fn [command context fn arg-map]
  (let [h (some #(when (and (= command (:command %))
                            (= context (:context %))) %) (vals @*handlers*))]
    (when h
      (get-in h [:fns fn]))))

(defn run [command context arg-map]
  (when-let [run (get-fn command context :run arg-map)]
    (run arg-map)))

(defn enabled? [command context arg-map]
  (if-let [enabled? (get-fn command context :enabled? arg-map)]
    (enabled? arg-map)
    false))

(defn visible? [command context arg-map]
  (if-let [visible? (get-fn command context :visible? arg-map)]
    (visible? arg-map)
    false))
