;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.scene-cache-test
  (:require [clojure.test :refer :all]
            [editor.scene-cache :as scene-cache]))

(defn- make-fn [context data]
  (let [key (gensym)]
    (swap! context assoc key data)
    key))

(defn- update-fn [context key data]
  (swap! context assoc key (inc data))
  key)

(defn- destroy-fn [context keys _]
  (doseq [key keys]
    (swap! context dissoc key)))

(scene-cache/register-object-cache! ::test make-fn update-fn destroy-fn)

(def context (atom {}))
(def request-id (gensym))

(defn- request-object! [data]
  (scene-cache/request-object! ::test request-id context data))

(defn- lookup-object []
  (scene-cache/lookup-object ::test request-id context))

(defn- prune! []
  (scene-cache/prune-context! context))

(defn- drop-context! []
  (scene-cache/drop-context! context))

(defn- retained? [key]
    (contains? @context key))

(defn- value [key]
  (get @context key))

(deftest life-cycle []
  (let [key (request-object! 1)]
    (is (retained? key))
    (is (= 1 (value key)))
    (prune!)
    (is (retained? key))
    (is (= 1 (value key)))
    (request-object! 2)
    (is (retained? key))
    ; update-fn should make it 3 via inc
    (is (= 3 (value key)))
    (prune!)
    (is (= key (lookup-object)))
    (is (retained? key))
    (prune!)
    (is (not (retained? key)))))

(deftest destroy-after-drop-context []
  (let [key (request-object! 1)]
    (is (retained? key))
    (drop-context!)
    (is (not (retained? key)))))

(deftest process-pending-deletions []
  (let [objects (atom {})
        destroy-calls (atom [])]
    (scene-cache/register-object-cache!
      ::process-pending-deletions
      (fn [context data]
        (swap! objects assoc [context ::object] data)
        ::object)
      (fn [_ _ _])
      (fn [context keys request-datas]
        (swap! destroy-calls conj [context keys request-datas])
        (doseq [key keys]
          (swap! objects dissoc [context key]))))

    (let [key (scene-cache/request-object! ::process-pending-deletions ::request ::context 1)]
      (is (contains? @objects [::context key]))

      ;; No deletion is pending before the cache is re-registered.
      (scene-cache/process-pending-deletions! ::context)
      (is (= [] @destroy-calls))
      (is (contains? @objects [::context key]))

      (scene-cache/register-object-cache! ::process-pending-deletions (fn [_ _]) (fn [_ _ _]) (fn [_ _ _]))

      ;; Pending deletions are scoped to their originating context.
      (scene-cache/process-pending-deletions! ::other-context)
      (is (= [] @destroy-calls))
      (is (contains? @objects [::context key]))

      ;; The matching context consumes the queued deletion exactly once.
      (scene-cache/process-pending-deletions! ::context)
      (is (= [[::context [key] [1]]] @destroy-calls))
      (is (not (contains? @objects [::context key])))

      (scene-cache/process-pending-deletions! ::context)
      (is (= [[::context [key] [1]]] @destroy-calls)))))

(deftest prune-context []
  (let [objects (atom {})
        destroy-calls (atom [])]
    (scene-cache/register-object-cache!
      ::prune-context
      (fn [context data]
        (swap! objects assoc [context ::object] data)
        ::object)
      (fn [_ _ _])
      (fn [context keys request-datas]
        (swap! destroy-calls conj [context keys request-datas])
        (doseq [key keys]
          (swap! objects dissoc [context key]))))

    (let [key (scene-cache/request-object! ::prune-context ::request ::context 1)]
      (is (contains? @objects [::context key]))

      ;; Other contexts do not prune this context's objects.
      (scene-cache/prune-context! ::other-context)
      (is (= [] @destroy-calls))
      (is (contains? @objects [::context key]))

      ;; The first matching prune clears usage tracking but retains the object.
      (scene-cache/prune-context! ::context)
      (is (= [] @destroy-calls))
      (is (contains? @objects [::context key]))

      ;; The next matching prune destroys the unused object exactly once.
      (scene-cache/prune-context! ::context)
      (is (= [[::context [key] [1]]] @destroy-calls))
      (is (not (contains? @objects [::context key])))

      (scene-cache/prune-context! ::context)
      (is (= [[::context [key] [1]]] @destroy-calls)))))
