(ns editor.ui.bindings
  (:refer-clojure :exclude [= and constantly empty? if map nil? not or some? when when-not])
  (:require [clojure.string :as string])
  (:import (javafx.beans InvalidationListener Observable)
           (javafx.beans.value ChangeListener ObservableBooleanValue ObservableObjectValue ObservableValue ObservableStringValue)
           (javafx.beans.binding Bindings BooleanBinding ObjectBinding)
           (javafx.beans.property Property)
           (javafx.collections ObservableList ObservableSet ObservableMap)))

(set! *warn-on-reflection* true)

(defn =
  ^BooleanBinding [^Object op1 ^ObservableObjectValue op2]
  (Bindings/equal op1 op2))

(defn and
  ^BooleanBinding [^ObservableBooleanValue first-op & rest-ops]
  (reduce #(Bindings/and %1 %2) first-op rest-ops))

(defn bind! [^Property property ^ObservableValue observable]
  (.bind property observable))

(defn blank?
  ^BooleanBinding [^ObservableValue observable]
  (assert (instance? ObservableValue observable))
  (Bindings/createBooleanBinding #(string/blank? (.getValue observable))
                                 (into-array Observable [observable])))

(defn constantly
  ^ObservableObjectValue [value]
  (reify ObservableObjectValue
    (get [_this] value)
    (getValue [_this] value)
    (^void addListener [_this ^ChangeListener _listener])
    (^void addListener [_this ^InvalidationListener _listener])
    (^void removeListener [_this ^ChangeListener _listener])
    (^void removeListener [_this ^InvalidationListener _listener])))

(defn empty?
  ^BooleanBinding [op]
  (cond
    (instance? ObservableList op) (Bindings/isEmpty ^ObservableList op)
    (instance? ObservableMap op) (Bindings/isEmpty ^ObservableMap op)
    (instance? ObservableSet op) (Bindings/isEmpty ^ObservableSet op)
    (instance? ObservableStringValue op) (Bindings/isEmpty ^ObservableStringValue op)
    :else (throw (ex-info (str "Cannot create isEmpty binding from " op)
                          {:op op}))))

(defn if
  ^ObjectBinding [^ObservableBooleanValue condition ^ObservableObjectValue then-value ^ObservableObjectValue otherwise-value]
  (-> (Bindings/when condition)
      (.then then-value)
      (.otherwise otherwise-value)))

(defn map
  ^ObjectBinding [^ObservableValue observable value-fn]
  (assert (instance? ObservableValue observable))
  (assert (ifn? value-fn))
  (Bindings/createObjectBinding #(value-fn (.getValue observable))
                                (into-array Observable [observable])))

(defn nil?
  ^BooleanBinding [^ObservableObjectValue op]
  (Bindings/isNull op))

(defn not
  ^BooleanBinding [^ObservableBooleanValue op]
  (Bindings/not op))

(defn or
  ^BooleanBinding [^ObservableBooleanValue first-op & rest-ops]
  (reduce #(Bindings/or %1 %2) first-op rest-ops))

(defn some?
  ^BooleanBinding [^ObservableObjectValue op]
  (Bindings/isNotNull op))

(defn when
  ^ObjectBinding [^ObservableBooleanValue condition ^ObservableObjectValue then-value]
  (let [^Object typed-nil nil]
    (-> (Bindings/when condition)
        (.then then-value)
        (.otherwise typed-nil))))

(defn when-not
  ^ObjectBinding [^ObservableBooleanValue condition ^ObservableObjectValue then-value]
  (when (not condition)
    then-value))
