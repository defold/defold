(ns editor.handler
  (:require [clojure.tools.macro :as ctm]
            [plumbing.core :refer [fnk]]))

(defonce ^:dynamic *handlers* (atom {}))

; TODO: Validate arguments for all functions and log appropriate message

(defmacro defhandler [name command & body]
  (let [qname (keyword (str *ns*) (str name))
        fns (->> body
              (mapcat (fn [[fname fargs & fbody]]
                        [(keyword fname) `(fnk ~fargs ~@fbody)]))
              (apply hash-map))]
    `(swap! *handlers* assoc ~qname {:command ~command
                                     :fns ~fns})))

(defn- get-fn [command fn command-context]
  (let [h (some #(when (= command (:command %)) %) (vals @*handlers*))]
    (when h
      (get-in h [:fns fn]))))

(defn run [command command-context]
  (when-let [run (get-fn command :run command-context)]
    (run command-context)))

(defn state [command command-context]
  (when-let [state (get-fn command :state command-context)]
    (state command-context)))

(defn enabled? [command command-context]
  (if-let [enabled? (get-fn command :enabled? command-context)]
    (enabled? command-context)
    false))

(defn visible? [command command-context]
  (if-let [visible? (get-fn command :visible? command-context)]
    (visible? command-context)
    false))
