(ns editor.handler
  (:require [clojure.tools.macro :as ctm]
            [plumbing.core :refer [fnk]]))

(def ^:dynamic *handlers* (atom {}))

; TODO: Validate arguments for all functions and log appropriate message

(defmacro defhandler [name command & body]
  (let [qname (keyword (str *ns*) (str name))
        fns (->> body
              (mapcat (fn [[fname fargs & fbody]]
                        [(keyword fname) `(fnk ~fargs ~@fbody)]))
              (apply hash-map))]
    `(swap! *handlers* assoc ~qname {:command ~command
                                     :fns ~fns})))

(defn- get-fn [command fn arg-map]
  (let [h (some #(when (= command (:command %)) %) (vals @*handlers*))]
    (when h
      (get-in h [:fns fn]))))

(defn run [command arg-map]
  (when-let [run (get-fn command :run arg-map)]
    (run arg-map)))

(defn state [command arg-map]
  (when-let [state (get-fn command :state arg-map)]
    (state arg-map)))

(defn enabled? [command arg-map]
  (if-let [enabled? (get-fn command :enabled? arg-map)]
    (enabled? arg-map)
    false))

(defn visible? [command arg-map]
  (if-let [visible? (get-fn command :visible? arg-map)]
    (visible? arg-map)
    false))
