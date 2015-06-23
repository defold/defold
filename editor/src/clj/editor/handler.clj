(ns editor.handler
  (:require [plumbing.core :refer [fnk]]))

(defonce ^:dynamic *handlers* (atom {}))

; TODO: Validate arguments for all functions and log appropriate message

(defmacro defhandler [command context & body]
  (let [qname (keyword (str *ns*) (str (name command) (gensym)))
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

(defn- get-active [command command-contexts]
  (some (fn [command-context] (do-get-active command command-context)) command-contexts))

(defn run [command command-contexts]
  (let [[handler command-context] (get-active command command-contexts)]
    (invoke-fnk (get-fnk handler :run) command-context)))

(defn state [command command-contexts]
  (when-let [[handler command-context] (get-active command command-contexts)]
    (invoke-fnk (get-fnk handler :state) command-context)))

(defn enabled? [command command-contexts]
  (when-let [[handler command-context] (get-active command command-contexts)]
    (let [f (get-fnk handler :enabled?)]
      (boolean (or (nil? f) (invoke-fnk f command-context))))))

(defn label [command command-contexts]
  (when-let [[handler command-context] (get-active command command-contexts)]
    (when-let [f (get-fnk handler :label)]
      (invoke-fnk f command-context))))

(defn active? [command command-contexts]
  (boolean (get-active command command-contexts)))
