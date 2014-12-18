(ns dynamo.property
  (:require [schema.core :as s]
            [dynamo.types :as t]
            [dynamo.ui :as ui]
            [dynamo.util :refer :all]
            [internal.property :as ip])
  (:import [org.eclipse.core.resources IResource]))

(set! *warn-on-reflection* true)

(defmacro defproperty [name value-type & body-forms]
  (apply ip/def-property-type-descriptor name value-type body-forms))

(defproperty NonNegativeInt s/Int
  (validation (comp not neg?)))

(defproperty Resource IResource)

(defproperty Vec3 t/Vec3
  (default [0.0 0.0 0.0]))

; TODO: this should be a tagged Vec3
(defproperty Color [(s/one s/Int "r")
                    (s/one s/Int "g")
                    (s/one s/Int "b")])

(defprotocol Presenter
  (control-for-property [this])
  (settings-for-control [this value])
  (on-event [this path event value]))

(defn no-change [] nil)
(defn intermediate-value [v] {:update-type :intermediate :value v})
(defn final-value [v]        {:update-type :final :value v})
(defn reset-default []       {:update-type :reset})

(defn is-enter-key? [event]
  (#{\return \newline} (:character event)))

(defrecord DefaultPresenter []
  Presenter
  (control-for-property [_]
    {:type :label})
  (settings-for-control [_ value]
    {:text (str value)}))

(def default-presenter
  (->DefaultPresenter))

(defn register-presenter
  [registry value-type presenter]
  (assoc registry value-type presenter))

(defn lookup-presenter
  [registry property]
  (get registry (:value-type property) default-presenter))
