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

(ns editor.defold-project-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.localization :as localization]
            [editor.placeholder-resource :as placeholder-resource]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [internal.graph.types :as gt]
            [service.log :as log]
            [support.test-support :refer [with-clean-system]]
            [util.coll :as coll]
            [util.eduction :as e])
  (:import [clojure.lang ArityException]))

(g/defnode ANode
  (inherits resource-node/ResourceNode)
  (property value-piece g/Str)
  (property value g/Str
            (set (fn [evaluation-context self _old-value _new-value]
                   (let [input (g/node-value self :value-input evaluation-context)]
                     (g/set-property self :value-piece (str (first input)))))))
  (property source-resource resource/Resource
            (set (fn [evaluation-context self _old-value new-value]
                   (let [project (project/get-project (:basis evaluation-context))]
                     (:tx-data (project/connect-resource-node evaluation-context project new-value self [[:value :value-input]]))))))
  (input project-node-id g/NodeID)
  (input value-input g/Str))

(g/defnode BNode
  (inherits resource-node/ResourceNode)
  (property value g/Str))

(g/defnode CachedSaveValueNode
  (inherits resource-node/ResourceNode)
  (output save-value g/Any :cached (g/constantly nil)))

(defn- register-resource-types [workspace types]
  (for [type types]
    (apply workspace/register-resource-type workspace (flatten (vec type)))))

(deftest loading
  (with-clean-system
    (test-util/with-ui-run-later-rebound
      (let [load-counter (atom 0)
            seen-read-opts (atom [])
            seen-load-opts (atom [])
            sanitized-owner-resources (atom [])

            read-a
            (fn read-a [read-opts _owner-resource readable]
              (swap! seen-read-opts conj read-opts)
              (read-string (slurp readable)))

            sanitize-a
            (fn sanitize-a [read-opts owner-resource source-value]
              (is (resource/resource? owner-resource))
              (is (coll/any? #(identical? read-opts %) @seen-read-opts))
              (swap! sanitized-owner-resources conj owner-resource)
              (update source-value :b #((:resolve-proj-path-fn read-opts) owner-resource %)))

            dependencies-a
            (fn dependencies-a [_read-opts _owner-resource source-value]
              (keep source-value [:b]))

            connect-a
            (fn connect-a [project self _resource]
              (g/connect project :_node-id self :project-node-id))

            load-a
            (fn load-a [{:keys [project] :as load-opts} {:keys [owner-resource resource source-value] self :node-id :as node-load-info}]
              (is (identical? owner-resource resource))
              (swap! load-counter inc)
              (swap! seen-load-opts conj load-opts)
              (is (string? (:disk-sha256 node-load-info)))
              (is (= [(:b source-value)] (:dependency-proj-paths node-load-info)))
              (let [source-resource (workspace/resolve-resource owner-resource (:b source-value))]
                (e/concat
                  (g/callback-ec
                    (fn check-connect-fn-happened [evaluation-context]
                      (is (identical? load-opts (:load-opts @(:tx-data-context evaluation-context))))
                      (is (= project (g/node-value self :project-node-id evaluation-context)))))
                  (g/set-property self :value-piece "set incorrectly")
                  (g/set-property self :source-resource source-resource)
                  (g/set-property self :value "bogus value"))))

            load-b
            (fn load-b [load-opts {:keys [owner-resource resource] self :node-id :as node-load-info}]
              (is (identical? owner-resource resource))
              (swap! load-counter inc)
              (swap! seen-load-opts conj load-opts)
              (is (not (contains? node-load-info :source-value)))
              (let [data (read-string (slurp resource))]
                (g/set-property self :value (:value data))))

            workspace (workspace/make-workspace (.getAbsolutePath (io/file "test/resources/load_project"))
                                                {}
                                                {}
                                                test-util/localization)]
        (g/transact
          (concat
            (placeholder-resource/register-resource-types workspace)
            (register-resource-types workspace [{:ext "type_a"
                                                 :node-type ANode
                                                 :read-fn (fn [read-opts owner-resource readable]
                                                            (sanitize-a read-opts owner-resource (read-a read-opts owner-resource readable)))
                                                 :dependencies-fn dependencies-a
                                                 :connect-fn connect-a
                                                 :load-fn load-a
                                                 :label "Type A"}
                                                {:ext "type_b"
                                                 :node-type BNode
                                                 :load-fn load-b
                                                 :label "Type B"}])))
        (workspace/resource-sync! workspace)
        (let [project (test-util/setup-project! workspace)
              a1 (project/get-resource-node project "/a1.type_a")]
          (is (= 3 @load-counter))
          (is (= 2 (count @seen-read-opts)))
          (is (= #{"/a1.type_a" "/a2.type_a"}
                 (into #{} (map resource/proj-path) @sanitized-owner-resources)))
          (is (coll/every? #(identical? (first @seen-read-opts) %) @seen-read-opts))
          (is (coll/every? #(ifn? (:resolve-proj-path-fn %)) @seen-read-opts))
          (is (coll/every? #(= project (:project %)) @seen-load-opts))
          (is (coll/every? #(identical? (first @seen-load-opts) %) @seen-load-opts))
          (let [expected-workspace workspace

                {:keys [code-preprocessor
                        editable->type-ext->resource-type
                        resolve-resource-fn
                        script-intelligence
                        workspace]}
                (first @seen-load-opts)

                owner-resource (workspace/find-resource workspace "/a1.type_a")
                target-resource (workspace/find-resource workspace "/a2.type_a")]

            (is (= expected-workspace workspace))
            (is (= (project/code-preprocessors project) code-preprocessor))
            (is (= (project/script-intelligence project) script-intelligence))
            (doseq [editable [true false]]
              (is (= (workspace/get-resource-type-map workspace editable)
                     (editable->type-ext->resource-type editable))))
            (with-redefs [g/now (fn [] (throw (AssertionError. "Unexpected graph query")))
                          g/unsafe-basis (fn [] (throw (AssertionError. "Unexpected graph query")))
                          g/node-value (fn [& _] (throw (AssertionError. "Unexpected graph evaluation")))
                          g/raw-property-value (fn [& _] (throw (AssertionError. "Unexpected graph property read")))]
              (is (identical? target-resource (resolve-resource-fn owner-resource "a2.type_a")))
              (is (identical? target-resource (resolve-resource-fn owner-resource "/a2.type_a")))
              (is (identical? target-resource (resolve-resource-fn nil "/a2.type_a")))
              (is (thrown-with-msg?
                    Exception
                    #"Unable to resolve relative path \"a2.type_a\" without a base-proj-path."
                    (resolve-resource-fn nil "a2.type_a")))
              (is (nil? (resolve-resource-fn owner-resource nil)))
              (is (nil? (resolve-resource-fn owner-resource "")))
              (let [missing-resource (resolve-resource-fn owner-resource "missing.type_a")]
                (is (resource/file-resource? missing-resource))
                (is (= "/missing.type_a" (resource/proj-path missing-resource)))
                (is (identical? missing-resource (resolve-resource-fn owner-resource "missing.type_a"))))))
          (is (= "t" (g/node-value a1 :value-piece))))))))

;; Verifies only callback arity errors use plugin-incompatibility handling.
;; Other illegal arguments retain their existing handling or propagation.
(deftest resource-type-function-invalid-arguments
  (doseq [function-key
          [:connect-fn
           :dependencies-fn
           :load-fn
           :read-fn
           :source-value-fn]

          [failure exception-type invalid-fn]
          [[:too-few
            ArityException
            (fn too-few-arguments [] nil)]
           [:too-many
            ArityException
            (fn too-many-arguments [_a _b _c _d _e _f _g _h] nil)]
           [:illegal-argument
            IllegalArgumentException
            (fn illegal-argument [& _args]
              (throw (IllegalArgumentException. "Intentional illegal argument")))]]]

    (testing (str function-key " " failure)
      (let [load-project
            (fn load-project []
              (with-clean-system
                (test-util/with-ui-run-later-rebound
                  (let [applied-load-tx-counter (atom 0)
                        project-path (.getAbsolutePath (io/file "test/resources/load_project"))
                        workspace (workspace/make-workspace project-path {} {} test-util/localization)]
                    (g/transact
                      (e/concat
                        (placeholder-resource/register-resource-types workspace)
                        (workspace/register-resource-type workspace
                          (assoc {:ext "type_b"
                                  :node-type BNode
                                  :read-fn (fn read-resource [_read-opts _owner-resource readable]
                                             (read-string (slurp readable)))
                                  :load-fn (fn load-resource [_load-opts {:keys [node-id source-value]}]
                                             (e/concat
                                               (g/callback swap! applied-load-tx-counter inc)
                                               (g/set-property node-id :value (:value source-value))))
                                  :write-fn pr-str}
                            function-key invalid-fn))))
                    (workspace/resource-sync! workspace)
                    (let [project (log/without-logging (test-util/setup-project! workspace))
                          node-id (project/get-resource-node project "/b.type_b")
                          error-value (g/node-value node-id :build-targets)]
                      {:defective (g/defective? node-id)
                       :error-value (g/error-value? error-value)
                       :exception-type (type (-> error-value :user-data :exception))
                       :applied-load-tx-count @applied-load-tx-counter
                       :save-data-connected (coll/any? #(= node-id (gt/source-id %))
                                                       (g/inputs (g/now) project :save-data))
                       :save-data-error (g/error-value? (g/node-value node-id :save-data))
                       :severity (:severity error-value)
                       :message (:message error-value)})))))]

        (cond
          (and (= :illegal-argument failure)
               (or (= :connect-fn function-key)
                   (= :source-value-fn function-key)))
          ;; Not caught.
          (is (thrown-with-msg?
                IllegalArgumentException
                #"Intentional illegal argument"
                (load-project)))

          (and (= :illegal-argument failure)
               (= :dependencies-fn function-key))
          ;; Caught and ignored. Reports no dependencies.
          (is (= {:defective false
                  :error-value false
                  :exception-type nil
                  :applied-load-tx-count 1
                  :save-data-connected true
                  :save-data-error false
                  :severity nil
                  :message nil}
                 (load-project)))

          :else
          ;; Caught and marks the node defective.
          (is (= {:defective true
                  :error-value true
                  :exception-type exception-type
                  :applied-load-tx-count 0
                  :save-data-connected false
                  :save-data-error true
                  :severity :fatal
                  :message (if (= ArityException exception-type)
                             (localization/message
                               "error.resource-not-loaded-update-extension"
                               {"resource" "/b.type_b"})
                             (localization/message
                               "error.resource-not-loaded-with-error"
                               {"resource" "/b.type_b"
                                "error" "Intentional illegal argument"}))}
                 (load-project))))))))

;; Verifies defective nodes retain their read error in both save output caches.
;; Evaluating jammed outputs alone would not detect missing cache entries.
(deftest read-errors-are-cached-for-save-outputs
  (with-clean-system
    (test-util/with-ui-run-later-rebound
      (let [project-path (.getAbsolutePath (io/file "test/resources/load_project"))
            workspace (workspace/make-workspace project-path {} {} test-util/localization)]
        (g/transact
          (e/concat
            (placeholder-resource/register-resource-types workspace)
            (workspace/register-resource-type workspace
              :ext "type_b"
              :node-type CachedSaveValueNode
              :read-fn (fn read-resource [_read-opts _owner-resource _readable]
                         (throw (ex-info "Intentional read failure" {})))
              :connect-fn (fn connect-resource [project node-id _resource]
                            (g/connect node-id :save-data project :save-data))
              :load-fn (fn load-resource [_load-opts _node-load-info] nil)
              :write-fn pr-str)))
        (workspace/resource-sync! workspace)
        (let [project (log/without-logging (test-util/setup-project! workspace))
              node-id (project/get-resource-node project "/b.type_b")
              read-error (:save-data (g/node-value node-id :_output-jammers))]
          (is (g/defective? node-id))
          (is (g/error-value? read-error))
          (doseq [label [:save-data :save-value]]
            (testing (name label)
              (is (identical? read-error
                              (get (g/cache) (g/endpoint node-id label)))))))))))

(deftest embedded-load-owner-resource
  (doseq [editability [:editable :non-editable]]
    (testing (name editability)
      (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")]
        (with-open [_ (test-util/make-directory-deleter project-path)]
          (when (= :non-editable editability)
            (test-util/set-non-editable-directories! project-path ["/nested"]))
          (with-clean-system
            (test-util/with-ui-run-later-rebound
              (let [workspace (test-util/setup-workspace! project-path)
                    loaded (atom [])
                    prototype-desc {:embedded-components
                                    [{:id "probe"
                                      :type "ownerprobe"
                                      :data {:path "target.type_b"}}]}]
                (g/transact
                  (workspace/register-resource-type workspace
                    :ext "ownerprobe"
                    :editable (= :editable editability)
                    :node-type BNode
                    :read-fn (fn [_read-opts _owner-resource readable]
                               (read-string (slurp readable)))
                    :write-fn pr-str
                    :load-fn (fn [load-opts {:keys [owner-resource resource source-value] :as node-load-info}]
                               (swap! loaded conj [load-opts node-load-info])
                               (is (resource/memory-resource? resource))
                               (is (= source-value (:data resource)))
                               (is (= "/nested/target.type_b"
                                      (resource/proj-path (workspace/resolve-resource owner-resource (:path source-value)))))
                               nil)))
                (test-util/write-file-resource! workspace "/nested/standalone.go" prototype-desc)
                (test-util/write-file-resource! workspace "/nested/container.collection"
                  {:name "container"
                   :embedded-instances [{:id "embedded" :data prototype-desc}]})
                (workspace/resource-sync! workspace)
                (let [project (test-util/setup-project! workspace)]
                  (is (= 2 (count @loaded)))
                  (is (= #{"/nested/standalone.go" "/nested/container.collection"}
                         (into #{} (map (comp resource/proj-path :owner-resource second)) @loaded)))
                  (is (coll/every? #(= project (:project (first %))) @loaded))
                  (is (coll/every? #(identical? (ffirst @loaded) (first %)) @loaded)))))))))))
