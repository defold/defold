(ns editor.handler
  (:require [plumbing.core :refer [fnk]]
            [dynamo.graph :as g]))

(defonce ^:dynamic *handlers* (atom {}))

; TODO: Validate arguments for all functions and log appropriate message

(defmacro defhandler [command context & body]
  (let [qname (keyword (str *ns*) (name command))
        fns (->> body
              (mapcat (fn [[fname fargs & fbody]]
                        [(keyword fname) `(fnk ~fargs ~@fbody)]))
              (apply hash-map))]
    `(swap! *handlers* assoc ~qname {:command ~command
                                     :context ~context
                                     :fns ~fns})))

(defn- get-fnk [handler fsym]
  (get-in handler [:fns fsym]))

(defn- invoke-fnk [f command-context]
  (f (:env command-context)))

(defn- do-get-active [command command-context]
  (->> (vals @*handlers*)
    (filter #(when (and (= command (:command %)) (= (:name command-context) (:context %))) %))
    (some (fn [handler]
            (let [f (get-fnk handler :active?)]
              (when (or (nil? f) (invoke-fnk f command-context))
                [handler command-context]))))))

(defn- get-active [command command-contexts user-data]
  (some (fn [command-context] (do-get-active command (assoc-in command-context [:env :user-data] user-data))) command-contexts))

(defn run [command command-contexts user-data]
  (let [[handler command-context] (get-active command command-contexts user-data)]
    (invoke-fnk (get-fnk handler :run) command-context)))

(defn state [command command-contexts user-data]
  (when-let [[handler command-context] (get-active command command-contexts user-data)]
    (invoke-fnk (get-fnk handler :state) command-context)))

(defn enabled? [command command-contexts user-data]
  (when-let [[handler command-context] (get-active command command-contexts user-data)]
    (let [f (get-fnk handler :enabled?)]
      (boolean (or (nil? f) (invoke-fnk f command-context))))))

(defn label [command command-contexts user-data]
  (when-let [[handler command-context] (get-active command command-contexts user-data)]
    (when-let [f (get-fnk handler :label)]
      (invoke-fnk f command-context))))

(defn active? [command command-contexts user-data]
  (boolean (get-active command command-contexts user-data)))

(defn options [command command-contexts user-data]
  (when-let [[handler command-context] (get-active command command-contexts user-data)]
    (when-let [f (get-fnk handler :options)]
      (invoke-fnk f command-context))))

(defn single-selection?
  ([selection] (= 1 (count selection)))
  ([selection type]
   (and (single-selection? selection)
        (g/node-instance? type (first selection)))))

(defn get-single-selection [selection type]
  (and (single-selection? selection type)
       (first selection)))
