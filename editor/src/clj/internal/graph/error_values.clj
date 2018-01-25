(ns internal.graph.error-values
  (:require [clojure.string :as string]
            [internal.graph.types :as gt]))

(set! *warn-on-reflection* true)

(def ^:private severity-levels {:info 0
                                :warning 10
                                :fatal 20})

(defprotocol ErrorSeverityProvider
  (error-severity [this]))

(defrecord ErrorValue [_node-id _label severity value message causes user-data]
  ErrorSeverityProvider
  (error-severity [_this] severity))

(defn error-value
  ([severity message]
    (error-value severity message nil))
  ([severity message user-data]
    (map->ErrorValue {:severity severity :message message :user-data user-data})))

(def error-info    (partial error-value :info))
(def error-warning (partial error-value :warning))
(def error-fatal   (partial error-value :fatal))

(defn ->error
  ([node-id label severity value message]
    (->error node-id label severity value message nil))
  ([node-id label severity value message user-data]
    (->ErrorValue node-id label severity value message nil user-data)))

(defn error?
  [x]
  (cond
    (sequential? x)          (some error? x)
    (instance? ErrorValue x) x
    :else                    nil))

(defn- sev? [level x] (< (or level 0) (or (severity-levels (error-severity x)) 0)))

(defn worse-than
  [severity x]
  (when (instance? ErrorValue x) (sev? (severity-levels severity) x)))

(defn- severity? [severity e]
  (and (satisfies? ErrorSeverityProvider e)
       (= (severity-levels severity)
          (severity-levels (error-severity e)))))

(def error-info?    (partial severity? :info))
(def error-warning? (partial severity? :warning))
(def error-fatal?   (partial severity? :fatal))

(defn- error-seq [e]
  (tree-seq :causes :causes e))

(defn- error-messages [e]
  (distinct (keep :message (error-seq e))))

(defn error-message [e]
  (string/join "\n" (error-messages e)))

(defn error-aggregate
  ([es]
   (let [max-severity (reduce (fn [result severity] (if (> (severity-levels result) (severity-levels severity))
                                                      result
                                                      severity))
                              :info (keep :severity es))]
     (map->ErrorValue {:severity max-severity :causes (vec es)})))
  ([es & kvs]
   (apply assoc (error-aggregate es) kvs)))

(defrecord ErrorPackage [packaged-errors]
  ErrorSeverityProvider
  (error-severity [_this]
    (if (some? packaged-errors)
      (error-severity packaged-errors)
      :info)))

(defn- unpack-if-package [error-or-package]
  (if (instance? ErrorPackage error-or-package)
    (:packaged-errors error-or-package)
    error-or-package))

(defn- flatten-packages [values node-id]
  (mapcat (fn [value]
            (cond
              (nil? value)
              nil

              (instance? ErrorValue value)
              [value]

              (instance? ErrorPackage value)
              (let [error-value (:packaged-errors value)]
                (if (= node-id (:_node-id error-value))
                  (:causes error-value)
                  [error-value]))

              (sequential? value)
              (flatten-packages value node-id)

              :else
              (throw (ex-info (str "Unsupported value of " (type value))
                              {:value value}))))
          values))

(defn flatten-errors [& errors]
  (some->> errors
           (map unpack-if-package)
           flatten
           (remove nil?)
           not-empty
           error-aggregate))

(defmacro precluding-errors [errors result]
  `(let [error-value# (flatten-errors ~errors)]
     (if (worse-than :info error-value#)
       error-value#
       ~result)))

(defn package-errors [node-id & errors]
  (assert (gt/node-id? node-id))
  (some-> errors (flatten-packages node-id) flatten-errors (assoc :_node-id node-id) ->ErrorPackage))

(defn unpack-errors [error-package]
  (assert (or (nil? error-package) (instance? ErrorPackage error-package)))
  (some-> error-package :packaged-errors))
