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

(ns integration.test-util-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.collection :as collection]
            [editor.defold-project :as project]
            [editor.game-object :as game-object]
            [editor.properties :as properties]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.sprite :as sprite]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :as test-support]))

(deftest number-type-preserving?-test
  (let [number-type-preserving? ; Silence inner assertions since we'll be triggering failures.
        (fn silenced-number-type-preserving? [a b]
          (with-redefs [do-report identity]
            (test-util/number-type-preserving? a b)))

        original-meta {:version "original"}
        altered-meta {:version "altered"}]

    (is (true? (number-type-preserving? (int 0) (int 0))))
    (is (true? (number-type-preserving? (long 0) (long 0))))
    (is (false? (number-type-preserving? (int 0) (long 0))))
    (is (false? (number-type-preserving? (long 0) (int 0))))

    (is (true? (number-type-preserving? (float 0.0) (float 0.0))))
    (is (true? (number-type-preserving? (double 0.0) (double 0.0))))
    (is (false? (number-type-preserving? (float 0.0) (double 0.0))))
    (is (false? (number-type-preserving? (double 0.0) (float 0.0))))

    (is (true? (number-type-preserving? [(float 0.0)] [(float 0.0)])))
    (is (true? (number-type-preserving? [(double 0.0)] [(double 0.0)])))
    (is (false? (number-type-preserving? [(float 0.0)] [(double 0.0)])))
    (is (false? (number-type-preserving? [(double 0.0)] [(float 0.0)])))

    (is (true? (number-type-preserving? [[(float 0.0)]] [[(float 0.0)]])))
    (is (true? (number-type-preserving? [[(double 0.0)]] [[(double 0.0)]])))
    (is (false? (number-type-preserving? [[(float 0.0)]] [[(double 0.0)]])))
    (is (false? (number-type-preserving? [[(double 0.0)]] [[(float 0.0)]])))

    (is (true? (number-type-preserving? (vector-of :float 0.0) (vector-of :float 0.0))))
    (is (true? (number-type-preserving? (vector-of :double 0.0) (vector-of :double 0.0))))
    (is (false? (number-type-preserving? (vector-of :float 0.0) (vector-of :double 0.0))))
    (is (false? (number-type-preserving? (vector-of :double 0.0) (vector-of :float 0.0))))

    (is (true? (number-type-preserving? [(float 0.0) (double 0.0)] [(float 0.0) (double 0.0)])))
    (is (true? (number-type-preserving? [(double 0.0) (float 0.0)] [(double 0.0) (float 0.0)])))
    (is (false? (number-type-preserving? [(float 0.0) (double 0.0)] [(double 0.0) (float 0.0)])))
    (is (false? (number-type-preserving? [(double 0.0) (float 0.0)] [(float 0.0) (double 0.0)])))

    (is (true? (number-type-preserving? [[(float 0.0) (double 0.0)]] [[(float 0.0) (double 0.0)] [(float 0.0) (double 0.0)]])))
    (is (true? (number-type-preserving? [[(float 0.0) (double 0.0)] [(float 0.0) (double 0.0)]] [[(float 0.0) (double 0.0)]])))
    (is (false? (number-type-preserving? [[(float 0.0) (double 0.0)]] [[(double 0.0) (float 0.0)] [(float 0.0) (double 0.0)]])))
    (is (false? (number-type-preserving? [[(float 0.0) (double 0.0)]] [[(float 0.0) (double 0.0)] [(double 0.0) (float 0.0)]])))
    (is (false? (number-type-preserving? [[(float 0.0) (double 0.0)] [(float 0.0) (double 0.0)]] [[(double 0.0) (float 0.0)]])))

    (is (true? (number-type-preserving? (with-meta [(float 0.0)] original-meta) (with-meta [(float 0.0)] original-meta))))
    (is (false? (number-type-preserving? (with-meta [(float 0.0)] original-meta) (with-meta [(float 0.0)] altered-meta))))
    (is (true? (number-type-preserving? (with-meta (vector-of :float 0.0) original-meta) (with-meta (vector-of :float 0.0) original-meta))))
    (is (false? (number-type-preserving? (with-meta (vector-of :float 0.0) original-meta) (with-meta (vector-of :float 0.0) altered-meta))))

    (letfn [(->control-points [empty-point num-fn]
              (mapv (fn [control-point]
                      (into empty-point
                            (map num-fn)
                            control-point))
                    [[0.0 0.0 1.0 0.0] [1.0 0.0 1.0 0.0]]))]
      (doseq [->curve [(fn make-curve [empty-point num-fn]
                         (properties/->curve (->control-points empty-point num-fn)))
                       (fn make-curve [empty-point num-fn]
                         (properties/->curve-spread (->control-points empty-point num-fn)
                                                    (num-fn 0.0)))]]

        (is (true? (number-type-preserving? (->curve [] float) (->curve [] float))))
        (is (true? (number-type-preserving? (->curve [] double) (->curve [] double))))
        (is (false? (number-type-preserving? (->curve [] float) (->curve [] double))))
        (is (false? (number-type-preserving? (->curve [] double) (->curve [] float))))

        (is (true? (number-type-preserving? (->curve (vector-of :float) float) (->curve (vector-of :float) float))))
        (is (true? (number-type-preserving? (->curve (vector-of :double) double) (->curve (vector-of :double) double))))
        (is (false? (number-type-preserving? (->curve (vector-of :float) float) (->curve (vector-of :double) double))))
        (is (false? (number-type-preserving? (->curve (vector-of :double) double) (->curve (vector-of :float) float))))

        (is (true? (number-type-preserving? (->curve (with-meta [] original-meta) float) (->curve (with-meta [] original-meta) float))))
        (is (false? (number-type-preserving? (->curve (with-meta [] original-meta) float) (->curve (with-meta [] altered-meta) float))))
        (is (true? (number-type-preserving? (->curve (with-meta (vector-of :float) original-meta) float) (->curve (with-meta (vector-of :float) original-meta) float))))
        (is (false? (number-type-preserving? (->curve (with-meta (vector-of :float) original-meta) float) (->curve (with-meta (vector-of :float) altered-meta) float))))

        (is (true? (number-type-preserving? (with-meta (->curve [] float) original-meta) (with-meta (->curve [] float) original-meta))))
        (is (false? (number-type-preserving? (with-meta (->curve [] float) original-meta) (with-meta (->curve [] float) altered-meta))))
        (is (true? (number-type-preserving? (with-meta (->curve (vector-of :float) float) original-meta) (with-meta (->curve (vector-of :float) float) original-meta))))
        (is (false? (number-type-preserving? (with-meta (->curve (vector-of :float) float) original-meta) (with-meta (->curve (vector-of :float) float) altered-meta))))))))

(g/defnode CachedSaveValueOutputNode
  (output save-value g/Keyword :cached (g/constantly :save-value))
  (output save-data g/Keyword :cached (g/fnk [save-value] save-value)))

(g/defnode UncachedSaveValueOutputNode
  (output save-value g/Keyword (g/constantly :save-value))
  (output save-data g/Keyword :cached (g/fnk [save-value] save-value)))

(deftest cached-save-data-outputs-test
  (test-support/with-clean-system

    (testing "Node that caches the save-value output."
      (let [node-id (g/make-node! world CachedSaveValueOutputNode)]
        (is (= #{} (test-util/cached-save-data-outputs node-id)))
        (g/node-value node-id :save-data)
        (is (= #{:save-data :save-value} (test-util/cached-save-data-outputs node-id)))))

    (testing "Node does not cache the save-value output."
      (let [node-id (g/make-node! world UncachedSaveValueOutputNode)]
        (is (= #{} (test-util/cached-save-data-outputs node-id)))
        (g/node-value node-id :save-data)
        (is (= #{:save-data} (test-util/cached-save-data-outputs node-id)))))))

(deftest cacheable-save-data-endpoints-test
  (test-util/with-loaded-project

    (testing "Resource that caches the save-value output."
      (let [resource (workspace/find-resource workspace "/game_object/test.go")
            node-id (project/get-resource-node project resource)]
        (assert (contains? (g/cached-outputs (g/node-type* node-id)) :save-value))
        (is (= #{(g/endpoint node-id :save-data)
                 (g/endpoint node-id :save-value)}
               (test-util/cacheable-save-data-endpoints node-id)))))

    (testing "Resource that does not cache the save-value output."
      (let [resource (workspace/find-resource workspace "/script/props.script")
            node-id (project/get-resource-node project resource)]
        (assert (not (contains? (g/cached-outputs (g/node-type* node-id)) :save-value)))
        (is (= #{(g/endpoint node-id :save-data)}
               (test-util/cacheable-save-data-endpoints node-id)))))))

(deftest cacheable-save-data-outputs-test
  (test-util/with-loaded-project

    (testing "Resource that caches the save-value output."
      (let [resource (workspace/find-resource workspace "/game_object/test.go")
            node-id (project/get-resource-node project resource)]
        (assert (contains? (g/cached-outputs (g/node-type* node-id)) :save-value))
        (is (= #{:save-data :save-value} (test-util/cacheable-save-data-outputs node-id)))))

    (testing "Resource that does not cache the save-value output."
      (let [resource (workspace/find-resource workspace "/script/props.script")
            node-id (project/get-resource-node project resource)]
        (assert (not (contains? (g/cached-outputs (g/node-type* node-id)) :save-value)))
        (is (= #{:save-data} (test-util/cacheable-save-data-outputs node-id)))))))

(deftest uncached-save-data-outputs-test
  (test-util/with-loaded-project

    (testing "Resource that caches the save-value output."
      (let [node-id (project/get-resource-node project "/game_object/test.go")]
        (g/clear-system-cache!)
        (is (= #{:save-data :save-value} (test-util/uncached-save-data-outputs node-id)))
        (g/node-value node-id :save-data)
        (is (= #{} (test-util/uncached-save-data-outputs node-id)))))

    (testing "Resource that does not cache the save-value output."
      (let [node-id (project/get-resource-node project "/script/props.script")]
        (g/clear-system-cache!)
        (is (= #{:save-data} (test-util/uncached-save-data-outputs node-id)))
        (g/node-value node-id :save-data)
        (is (= #{} (test-util/uncached-save-data-outputs node-id)))))))

(deftest run-event-loop-test

  (testing "Exit function terminates event loop."
    (is (nil? (test-util/run-event-loop! (fn [exit-event-loop!]
                                           (exit-event-loop!)))))
    (is (nil? (test-util/run-event-loop! (fn [exit-event-loop!]
                                           (ui/run-now
                                             (exit-event-loop!))))))
    (is (nil? (test-util/run-event-loop! (fn [exit-event-loop!]
                                           (ui/run-later
                                             (exit-event-loop!))))))
    (is (nil? (test-util/run-event-loop! (fn [exit-event-loop!]
                                           (ui/run-later
                                             (ui/run-later
                                               (exit-event-loop!)))))))
    (is (nil? (test-util/run-event-loop! (fn [exit-event-loop!]
                                           (ui/run-later
                                             (ui/run-now
                                               (exit-event-loop!)))))))
    (is (nil? (test-util/run-event-loop! (fn [exit-event-loop!]
                                           (ui/run-now
                                             (ui/run-later
                                               (exit-event-loop!))))))))

  (testing "Throwing terminates event loop."
    (is (thrown? AssertionError (test-util/run-event-loop!
                                  (fn [_exit-event-loop!]
                                    (assert false)))))
    (is (thrown? AssertionError (test-util/run-event-loop!
                                  (fn [_exit-event-loop!]
                                    (ui/run-now
                                      (assert false))))))
    (is (thrown? AssertionError (test-util/run-event-loop!
                                  (fn [_exit-event-loop!]
                                    (ui/run-later
                                      (assert false))))))
    (is (thrown? AssertionError (test-util/run-event-loop!
                                  (fn [_exit-event-loop!]
                                    (ui/run-later
                                      (ui/run-later
                                        (assert false)))))))
    (is (thrown? AssertionError (test-util/run-event-loop!
                                  (fn [_exit-event-loop!]
                                    (ui/run-later
                                      (ui/run-now
                                        (assert false)))))))
    (is (thrown? AssertionError (test-util/run-event-loop!
                                  (fn [_exit-event-loop!]
                                    (ui/run-now
                                      (ui/run-later
                                        (assert false))))))))

  (testing "Blocks until terminated."
    (let [begin (System/currentTimeMillis)]
      (test-util/run-event-loop! (fn [exit-event-loop!]
                                   (Thread/sleep 300)
                                   (exit-event-loop!)))
      (is (<= 230 (- (System/currentTimeMillis) begin)))))

  (testing "Performs side-effects in order."
    (let [side-effects (atom [])]
      (test-util/run-event-loop! (fn [exit-event-loop!]
                                   (ui/run-later
                                     (swap! side-effects conj :b)
                                     (exit-event-loop!))
                                   (swap! side-effects conj :a)))
      (is (= [:a :b] @side-effects))))

  (testing "Executes actions on the calling thread."
    (let [calling-thread (Thread/currentThread)
          action-threads (atom [])]
      (test-util/run-event-loop! (fn [exit-event-loop!]
                                   (swap! action-threads conj (Thread/currentThread))
                                   (ui/run-now
                                     (swap! action-threads conj (Thread/currentThread)))
                                   (ui/run-later
                                     (swap! action-threads conj (Thread/currentThread))
                                     (exit-event-loop!))))
      (is (= [calling-thread calling-thread calling-thread] @action-threads))))

  (testing "UI thread checks."
    (let [errors (atom [])
          results (atom [])]
      (test-util/run-event-loop! (fn [exit-event-loop!]
                                   (swap! results conj (ui/on-ui-thread?))
                                   (ui/run-now
                                     (swap! results conj (ui/on-ui-thread?)))
                                   (ui/run-later
                                     (swap! results conj (ui/on-ui-thread?))
                                     (future
                                       (try
                                         (swap! results conj (ui/on-ui-thread?))
                                         (ui/run-now
                                           (swap! results conj (ui/on-ui-thread?))
                                           (future
                                             (try
                                               (swap! results conj (ui/on-ui-thread?))
                                               (catch Throwable error
                                                 (swap! errors conj error))
                                               (finally
                                                 (exit-event-loop!)))))
                                         (catch Throwable error
                                           (swap! errors conj error)
                                           (exit-event-loop!)))))))
      (is (empty? @errors))
      (is (= [true true true false true false] @results)))))

(defn- embedded?
  ([expected-node-type node-id]
   (embedded? (g/now) expected-node-type node-id))
  ([basis expected-node-type node-id]
   (and (= expected-node-type (g/node-type* basis node-id))
        (resource/memory-resource? (resource-node/resource basis node-id)))))

(deftest collection-creation-test
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")]
    (with-open [_ (test-util/make-directory-deleter project-path)]
      (test-support/with-clean-system
        (let [workspace (test-util/setup-workspace! world project-path)
              project (test-util/setup-project! workspace)
              sprite-resource-type (workspace/get-resource-type workspace "sprite")
              sprite (test-util/make-resource-node! project "/sprite.sprite")
              chair (test-util/make-resource-node! project "/chair.go")
              chair-referenced-sprite (test-util/add-referenced-component! chair (resource-node/resource sprite))
              chair-embedded-sprite (test-util/add-embedded-component! chair sprite-resource-type)
              room (test-util/make-resource-node! project "/room.collection")
              room-referenced-chair (test-util/add-referenced-game-object! room (resource-node/resource chair))
              room-embedded-chair (test-util/add-embedded-game-object! room)
              room-embedded-chair-referenced-sprite (test-util/add-referenced-component! room-embedded-chair (resource-node/resource sprite))
              room-embedded-chair-embedded-sprite (test-util/add-embedded-component! room-embedded-chair sprite-resource-type)
              house (test-util/make-resource-node! project "/house.collection")
              house-referenced-room (test-util/add-referenced-collection! house (resource-node/resource room))]

          (testing "Created node types."
            (is (= sprite/SpriteNode (g/node-type* sprite)))
            (is (= game-object/GameObjectNode (g/node-type* chair)))
            (is (= game-object/ReferencedComponent (g/node-type* chair-referenced-sprite)))
            (is (= game-object/EmbeddedComponent (g/node-type* chair-embedded-sprite)))
            (is (= collection/CollectionNode (g/node-type* room)))
            (is (= collection/ReferencedGOInstanceNode (g/node-type* room-referenced-chair)))
            (is (= collection/EmbeddedGOInstanceNode (g/node-type* room-embedded-chair)))
            (is (= game-object/ReferencedComponent (g/node-type* room-embedded-chair-referenced-sprite)))
            (is (= game-object/EmbeddedComponent (g/node-type* room-embedded-chair-embedded-sprite)))
            (is (= collection/CollectionInstanceNode (g/node-type* house-referenced-room))))

          (testing "Query functions."

            (testing "On chair game object."
              (is (= [chair-referenced-sprite] (test-util/referenced-components chair)))
              (is (= sprite (test-util/to-component-resource-node-id chair-referenced-sprite)))

              (is (= [chair-embedded-sprite] (test-util/embedded-components chair)))
              (is (embedded? sprite/SpriteNode (test-util/to-component-resource-node-id chair-embedded-sprite))))

            (testing "On room collection."
              (is (= [room-referenced-chair] (test-util/referenced-game-objects room)))
              (is (= chair (g/override-original (test-util/to-game-object-node-id room-referenced-chair))))

              (is (= [room-embedded-chair] (test-util/embedded-game-objects room)))
              (is (embedded? game-object/GameObjectNode (test-util/to-game-object-node-id room-embedded-chair)))

              (is (= [room-embedded-chair-referenced-sprite] (test-util/referenced-components room-embedded-chair)))
              (is (= sprite (test-util/to-component-resource-node-id room-embedded-chair-referenced-sprite)))

              (is (= [room-embedded-chair-embedded-sprite] (test-util/embedded-components room-embedded-chair)))
              (is (embedded? sprite/SpriteNode (test-util/to-component-resource-node-id room-embedded-chair-embedded-sprite))))

            (testing "On house collection."
              (is (= [house-referenced-room] (test-util/referenced-collections house)))
              (is (= room (g/override-original (test-util/to-collection-node-id house-referenced-room)))))))))))
