(ns editor.handler
  (:require [plumbing.core :refer [fnk]]))

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

(defn- invoke [handler fn command-vars]
  (when-let [f (get-in handler [:fns fn])]
    (f command-vars)))

(defn- do-get-enabled [command command-context]
  (->> (vals @*handlers*)
    (filter #(when (and (= command (:command %)) (= (:name command-context) (:context %))) %))
    (some (fn [h] (when (invoke h :enabled? (:env command-context)) [h command-context])))))

(defn- get-enabled [command command-contexts]
  (some identity (map (fn [ctx] (do-get-enabled command ctx)) command-contexts)))

(defn run [command command-contexts]
  (when-let [[h c] (get-enabled command command-contexts)]
    (invoke h :run (:env c))))

(defn state [command command-contexts]
  (let [[h c] (get-enabled command command-contexts)]
    (invoke h :state (:env c))))

(defn enabled? [command command-contexts]
  (let [[h c] (get-enabled command command-contexts)]
    (boolean (invoke h :enabled? (:env c)))))
