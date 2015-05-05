(ns editor.handler
  (:require [clojure.tools.macro :as ctm]
            [plumbing.core :refer [fnk]]))

(defonce ^:dynamic *handlers* (atom {}))

; TODO: Validate arguments for all functions and log appropriate message


(defmacro defhandler [command & body]
  (let [qname (keyword (str *ns*) (name command))
        fns (->> body
              (mapcat (fn [[fname fargs & fbody]]
                        [(keyword fname) `(fnk ~fargs ~@fbody)]))
              (apply hash-map))]
    `(swap! *handlers* assoc ~qname {:command ~command
                                     :fns ~fns})))

(defn- invoke [handler fn command-context]
  (when-let [f (get-in handler [:fns fn])]
    (f command-context)))

(defn- get-enabled [command command-context]
  (->> (vals @*handlers*)
    (filter #(when (= command (:command %)) %))
    (some (fn [h] (when (invoke h :enabled? command-context) h )))))

(defn run [command command-context]
  (when-let [handler (get-enabled command command-context)]
    (invoke handler :run command-context)))

(defn state [command command-context]
  (let [h (get-enabled command command-context)]
    (invoke h :state command-context)))

(defn enabled? [command command-context]
  (let [h (get-enabled command command-context)]
    (boolean (invoke h :enabled? command-context))))

(defn visible? [command command-context]
  (let [h (get-enabled command command-context)]
    (boolean (invoke h :visible? command-context))))
