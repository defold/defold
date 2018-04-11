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
  (scene-cache/prune-object-caches! context))

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
