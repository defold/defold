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

(ns integration.scene-load-test
  (:require [clojure.edn :as edn]
            [clojure.java.io :as io]
            [clojure.test :refer [deftest is]]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.defold-project :as project]
            [editor.editor-tab :as editor-tab]
            [editor.fs :as fs]
            [editor.geom :as geom]
            [editor.localization :as localization]
            [editor.progress :as progress]
            [editor.resource :as resource]
            [editor.resource-loading :as resource-loading]
            [editor.resource-node :as resource-node]
            [editor.scene :as scene]
            [editor.scene-visibility :as scene-visibility]
            [editor.ui :as ui]
            [editor.view :as view]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [service.log :as log]
            [support.test-support :refer [with-clean-system]]
            [util.coll :as coll]
            [util.eduction :as e])
  (:import [javafx.event Event]
           [javafx.scene Scene]
           [javafx.scene.control MenuBar SplitPane Tab TabPane]
           [javafx.scene.layout GridPane VBox]
           [javafx.stage Stage]))

(set! *warn-on-reflection* true)

(defn- await-result [result]
  (let [value (deref result 30000 ::timeout)]
    (if (= ::timeout value)
      (throw (ex-info "Timed out waiting for resource preparation." {}))
      value)))

(g/defnode ControlledSceneResource
  (inherits resource-node/ResourceNode)
  (property signals g/Any)
  (property value g/Int (default 0))
  (input child-scenes g/Any :array)
  (output resource-info g/Any :cached (g/fnk [value] {:value value}))
  (output scene g/Any :cached
          (g/fnk [_node-id signals value child-scenes]
            (swap! (:evaluation-threads signals) conj (ui/on-ui-thread?))
            (deliver (:evaluation-started signals) true)
            (await-result (:evaluate signals))
            {:node-id _node-id
             :aabb geom/null-aabb
             :children child-scenes
             :value value})))

(defn- make-signals []
  {:load-started (promise)
   :load (promise)
   :evaluation-started (promise)
   :evaluate (promise)
   :evaluation-threads (atom [])})

(defn- call-with-scene-project [signals-by-proj-path test-fn]
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")]
    (with-open [_ (test-util/make-directory-deleter project-path)]
      (with-clean-system {:cache-size 1000 :on-system-thread? ui/on-ui-thread?}
        (let [{:keys [^Stage stage ^TabPane tab-pane] :as environment}
              (ui/run-now
                (let [workspace (test-util/setup-workspace! project-path)]
                  (g/transact
                    (workspace/register-resource-type workspace
                      :ext "pendingscene"
                      :node-type ControlledSceneResource
                      :view-types [:scene]
                      :textual? true
                      :auto-connect-save-data? false

                      :read-fn
                      (fn read-scene [_read-opts _resource input]
                        (let [content (slurp input)]
                          (edn/read-string content)))

                      :write-fn pr-str

                      :dependencies-fn
                      (fn scene-dependencies [_read-opts _resource source-value]
                        (:dependencies source-value []))

                      :load-fn
                      (fn load-scene [{:keys [proj-path->resource-node-id]} {:keys [node-id resource source-value]}]
                        (let [proj-path (resource/proj-path resource)
                              signals (signals-by-proj-path proj-path)]
                          (deliver (:load-started signals) (ui/on-ui-thread?))
                          (await-result (:load signals))
                          (coll/into-> (:dependencies source-value)
                            [(g/set-property node-id :signals signals)
                             (g/set-property node-id :value (:value source-value 0))]
                            (map (fn [proj-path]
                                   (g/connect (proj-path->resource-node-id proj-path) :scene node-id :child-scenes))))))))
                  (run! (fn [[proj-path signals]]
                          (test-util/write-file-resource! workspace proj-path (:content signals {})))
                        signals-by-proj-path)
                  (workspace/resource-sync! workspace)
                  (let [project (test-util/setup-project! workspace)
                        prefs (test-util/make-test-prefs)
                        menu-bar (MenuBar.)
                        right-split (SplitPane.)
                        tool-tab-pane (TabPane.)

                        editor-tabs-split
                        (doto (SplitPane.)
                          (.setId "editor-tabs-split"))

                        quick-help-box
                        (doto (VBox.)
                          (.setId "quick-help-box"))

                        quick-help-items
                        (doto (GridPane.)
                          (.setId "quick-help-items"))

                        root
                        (doto (VBox.)
                          (ui/children! [menu-bar editor-tabs-split right-split tool-tab-pane quick-help-box quick-help-items]))

                        scene (Scene. root)

                        stage
                        (doto (Stage.)
                          (.setScene scene))

                        _ (.applyCss root)

                        [app-view _timer]
                        (app-view/make-app-view project stage menu-bar editor-tabs-split right-split tool-tab-pane prefs test-util/localization)

                        scene-visibility (scene-visibility/make-scene-visibility-node! prefs app-view)]

                    (g/transact
                      {:undoable false}
                      (e/concat
                        (g/connect project :_node-id app-view :project-id)
                        (g/connect scene-visibility :_node-id app-view :scene-visibility)
                        (g/connect app-view :active-resource-node+type scene-visibility :active-resource-node+type)
                        (g/connect app-view :active-scene scene-visibility :active-scene)
                        (coll/into-> [:outline-selection-by-resource-node :selected-node-ids-by-resource-node :selected-node-properties-by-resource-node :sub-selections-by-resource-node]
                          []
                          (mapcat #(g/connect project % app-view %)))))
                    {:workspace workspace
                     :project project
                     :prefs prefs
                     :app-view app-view
                     :stage stage
                     :tab-pane (first (.getItems editor-tabs-split))})))

              load-states (atom [])]

          (try
            (test-fn
              (assoc environment
                :open!
                (fn open-scene! [proj-path]
                  (ui/run-now
                    (let [{:keys [app-view prefs project workspace]} environment
                          resource (workspace/find-resource workspace proj-path)]
                      (app-view/open-resource! app-view prefs test-util/localization project resource)
                      (let [tab (ui/selected-tab tab-pane)
                            view (editor-tab/view-node-id tab)]
                        (when (g/node-instance? view/PendingWorkbenchView view)
                          (swap! load-states conj (g/node-value view :load-state)))
                        tab))))))
            (finally
              (ui/run-now
                (doseq [tab (vec (.getTabs tab-pane))]
                  (Event/fireEvent tab (Event. Tab/CLOSED_EVENT)))
                (.clear (.getTabs tab-pane))
                (.close stage))
              (run! (fn [signals]
                      (deliver (:load signals) true)
                      (deliver (:evaluate signals) true))
                    (coll/vals signals-by-proj-path))
              (doseq [load-state (coll/into-> @load-states [] (distinct))]
                (await-result (:completion @load-state)))
              ;; Tab deletion is deferred until close dispatch finishes.
              (ui/run-now nil))))))))

(defn- tab-load-state [tab]
  (ui/run-now
    (g/node-value (editor-tab/view-node-id tab) :load-state)))

;; A scene tab appears immediately and stays inert through evaluation.
;; Completion installs the real view in the same tab and preserves its identity.
(deftest pending-scene-tab-completes-test
  (let [signals (make-signals)]
    (call-with-scene-project
      {"/first.pendingscene" signals}
      (fn test-scene-tab-completion [{:keys [app-view project open!]}]
        (let [tab (open! "/first.pendingscene")
              load-state (tab-load-state tab)
              reports (atom [(:progress @load-state)])]
          (add-watch load-state ::progress
                     (fn record-progress [_ _ previous current]
                       (when (not= (:progress previous) (:progress current))
                         (swap! reports conj (:progress current)))))
          (is (false? (await-result (:load-started signals))))
          (ui/run-now
            (is (nil? (g/node-value app-view :active-resource)))
            (is (nil? (g/node-value app-view :active-resource-node+type)))
            (is (= [] (g/node-value app-view :selected-node-ids)))
            (is (nil? (scene/active-scene-view app-view)))
            (let [resource-node (project/get-resource-node project "/first.pendingscene")]
              (is (not (resource-node/loaded? resource-node))))
            (g/with-auto-evaluation-context evaluation-context
              (is (= ["/first.pendingscene" :scene] (editor-tab/prefs-data tab evaluation-context)))))
          (is (identical? tab (open! "/first.pendingscene")))
          (deliver (:load signals) true)
          (await-result (:evaluation-started signals))
          (is (nil? (ui/run-now (scene/active-scene-view app-view))))
          (deliver (:evaluate signals) true)
          (is (= :ready (await-result (:completion @load-state))))
          (ui/run-now
            (is (= tab (g/node-value app-view :active-tab)))
            (is (= (editor-tab/view-node-id tab) (scene/active-scene-view app-view)))
            (let [active-resource (g/node-value app-view :active-resource)]
              (is (= "/first.pendingscene" (resource/proj-path active-resource)))))
          (is (= [false] @(:evaluation-threads signals)))
          (let [loading-resource-message
                (localization/message "progress.loading-resource" {"resource" "/first.pendingscene"})

                working-message (localization/message "progress.working")]

            (is (coll/any? #(= loading-resource-message (progress/message %)) @reports))
            (is (coll/any? #(and (= working-message (progress/message %))
                                 (nil? (progress/fraction %)))
                           @reports))
            (is (coll/any? #(and (= working-message (progress/message %))
                                 (some? (progress/fraction %)))
                           @reports))))))))

;; Closing during materialization cancels that tab without installing a view.
;; Another pending tab can finish independently and retain its active selection.
(deftest closing-pending-scene-tab-cancels-loading-test
  (let [first-signals (make-signals)
        second-signals (make-signals)]
    (call-with-scene-project
      {"/first.pendingscene" first-signals
       "/second.pendingscene" second-signals}
      (fn test-closing-during-materialization [{:keys [app-view tab-pane open!]}]
        (let [first-tab (open! "/first.pendingscene")
              first-load-state (tab-load-state first-tab)
              second-tab (open! "/second.pendingscene")
              second-load-state (tab-load-state second-tab)]
          (await-result (:load-started first-signals))
          (await-result (:load-started second-signals))
          (ui/run-now
            (Event/fireEvent first-tab (Event. Tab/CLOSED_EVENT))
            (.remove (.getTabs ^TabPane tab-pane) first-tab))
          (deliver (:load first-signals) true)
          (is (= :cancelled (await-result (:completion @first-load-state))))
          (is (not (realized? (:evaluation-started first-signals))))
          (deliver (:load second-signals) true)
          (deliver (:evaluate second-signals) true)
          (is (= :ready (await-result (:completion @second-load-state))))
          (ui/run-now
            (is (= [second-tab] (vec (.getTabs ^TabPane tab-pane))))
            (is (= second-tab (g/node-value app-view :active-tab)))
            (is (nil? (editor-tab/view-node-id first-tab)))))))))

;; A closed tab discards an in-flight scene evaluation and cannot be resurrected
;; when its production function returns after cancellation.
(deftest closing-pending-scene-tab-cancels-evaluation-test
  (let [signals (make-signals)]
    (deliver (:load signals) true)
    (call-with-scene-project
      {"/first.pendingscene" signals}
      (fn test-closing-during-evaluation [{:keys [app-view tab-pane open!]}]
        (let [tab (open! "/first.pendingscene")
              load-state (tab-load-state tab)]
          (await-result (:evaluation-started signals))
          (ui/run-now
            (Event/fireEvent tab (Event. Tab/CLOSED_EVENT))
            (.remove (.getTabs ^TabPane tab-pane) tab))
          (deliver (:evaluate signals) true)
          (is (= :cancelled (await-result (:completion @load-state))))
          (ui/run-now
            (is (= [] (vec (.getTabs ^TabPane tab-pane))))
            (is (nil? (g/node-value app-view :active-resource)))
            (is (= [] (g/node-value app-view :scene-view-ids)))))))))

;; Editing during evaluation invalidates the prepared scene. The background
;; task retries before activating the tab, avoiding stale cached data.
(deftest pending-scene-retries-invalidated-evaluation-test
  (let [signals (make-signals)]
    (deliver (:load signals) true)
    (call-with-scene-project
      {"/first.pendingscene" signals}
      (fn test-retrying-invalidated-evaluation [{:keys [project open!]}]
        (let [tab (open! "/first.pendingscene")
              load-state (tab-load-state tab)]
          (await-result (:evaluation-started signals))
          (ui/run-now
            (let [resource-node (project/get-resource-node project "/first.pendingscene")]
              (g/set-property! resource-node :value 1)))
          (deliver (:evaluate signals) true)
          (is (= :ready (await-result (:completion @load-state))))
          (is (= [false false] @(:evaluation-threads signals)))
          (ui/run-now
            (let [resource-node (project/get-resource-node project "/first.pendingscene")
                  scene (g/node-value resource-node :scene)]
              (is (= 1 (:value scene))))))))))

;; Resource synchronization can replace a resource while its scene is being
;; prepared. The pending tab follows the replacement and evaluates it afresh.
(deftest pending-scene-follows-replaced-resource-test
  (let [signals (make-signals)]
    (deliver (:load signals) true)
    (call-with-scene-project
      {"/first.pendingscene" signals}
      (fn test-following-replaced-resource [{:keys [app-view project workspace open!]}]
        (let [tab (open! "/first.pendingscene")
              load-state (tab-load-state tab)

              original-resource-node
              (ui/run-now
                (project/get-resource-node project "/first.pendingscene"))]

          (await-result (:evaluation-started signals))
          (ui/run-now
            (test-util/write-file-resource! workspace "/first.pendingscene" {:value 2})
            (workspace/resource-sync! workspace)
            (is (not= original-resource-node (project/get-resource-node project "/first.pendingscene"))))
          (deliver (:evaluate signals) true)
          (is (= :ready (await-result (:completion @load-state))))
          (ui/run-now
            (let [resource-node (project/get-resource-node project "/first.pendingscene")
                  scene (g/node-value resource-node :scene)]
              (is (= resource-node (g/node-value app-view :active-resource-node)))
              (is (= 2 (:value scene)))))
          (is (= [false false] @(:evaluation-threads signals))))))))

;; Deleting a resource during evaluation closes its pending tab and prevents
;; the completed background evaluation from reopening the deleted resource.
(deftest pending-scene-resource-deleted-test
  (let [signals (make-signals)]
    (deliver (:load signals) true)
    (call-with-scene-project
      {"/first.pendingscene" signals}
      (fn test-deleting-pending-resource [{:keys [app-view tab-pane workspace open!]}]
        (let [tab (open! "/first.pendingscene")
              load-state (tab-load-state tab)]
          (await-result (:evaluation-started signals))
          (ui/run-now
            (let [resource (workspace/find-resource workspace "/first.pendingscene")]
              (fs/delete-file! (io/as-file resource)))
            (workspace/resource-sync! workspace))
          (deliver (:evaluate signals) true)
          (is (= :cancelled (await-result (:completion @load-state))))
          (ui/run-now
            (is (= [] (vec (.getTabs ^TabPane tab-pane))))
            (is (nil? (g/node-value app-view :active-resource)))))))))

;; Finishing an inactive tab preserves the current pending tab and its editing
;; context, guarding against background completion switching active resources.
(deftest pending-scene-completion-preserves-active-tab-test
  (let [first-signals (make-signals)
        second-signals (make-signals)]
    (call-with-scene-project
      {"/first.pendingscene" first-signals
       "/second.pendingscene" second-signals}
      (fn test-preserving-active-tab [{:keys [app-view open!]}]
        (let [first-tab (open! "/first.pendingscene")
              first-load-state (tab-load-state first-tab)
              second-tab (open! "/second.pendingscene")
              second-load-state (tab-load-state second-tab)]
          (deliver (:load first-signals) true)
          (deliver (:evaluate first-signals) true)
          (is (= :ready (await-result (:completion @first-load-state))))
          (ui/run-now
            (is (= second-tab (g/node-value app-view :active-tab)))
            (is (nil? (g/node-value app-view :active-resource)))
            (is (nil? (scene/active-scene-view app-view))))
          (deliver (:load second-signals) true)
          (deliver (:evaluate second-signals) true)
          (is (= :ready (await-result (:completion @second-load-state))))
          (ui/run-now
            (is (= second-tab (g/node-value app-view :active-tab)))
            (let [active-resource (g/node-value app-view :active-resource)]
              (is (= "/second.pendingscene" (resource/proj-path active-resource))))))))))

;; Invalid resource data shows a readable failure in the pending tab, guarding
;; against installing a defective Scene View or leaving progress running.
(deftest pending-scene-invalid-resource-test
  (log/without-logging
    (call-with-scene-project
      {"/broken.pendingscene" (assoc (make-signals) :content "}")}
      (fn test-invalid-resource [{:keys [app-view open!]}]
        (let [tab (open! "/broken.pendingscene")
              load-state (tab-load-state tab)]
          (is (= :failed (await-result (:completion @load-state))))
          (let [error-message (:error-message @load-state)]
            (is (string? (test-util/localization error-message))))
          (ui/run-now
            (is (nil? (scene/active-scene-view app-view)))
            (is (nil? (g/node-value app-view :active-resource)))))))))

;; Reopening a failed pending tab after fixing and synchronizing the resource
;; starts a new load process, clears the failure, and preserves the same Tab.
(deftest reopening-failed-pending-scene-tab-test
  (log/without-logging
    (let [signals (assoc (make-signals) :content "}")]
      (call-with-scene-project
        {"/broken.pendingscene" signals}
        (fn test-reopening-failed-pending-tab [{:keys [app-view project tab-pane workspace open!]}]
          (let [tab (open! "/broken.pendingscene")
                load-state (tab-load-state tab)
                failed-completion (:completion @load-state)]
            (is (= :failed (await-result failed-completion)))
            (deliver (:load signals) true)
            (ui/run-now
              (test-util/write-file-resource! workspace "/broken.pendingscene" {:value 42})
              (workspace/resource-sync! workspace))
            (is (identical? tab (open! "/broken.pendingscene")))
            (let [retry-completion (:completion @load-state)]
              (is (not (identical? failed-completion retry-completion)))
              (is (nil? (:error-message @load-state)))
              (is (identical? tab (open! "/broken.pendingscene")))
              (is (identical? retry-completion (:completion @load-state)))
              (ui/run-now
                (is (nil? (scene/active-scene-view app-view))))
              (deliver (:evaluate signals) true)
              (is (= :ready (await-result retry-completion)))
              (ui/run-now
                (let [resource-node (project/get-resource-node project "/broken.pendingscene")
                      view (editor-tab/view-node-id tab)]
                  (is (= [tab] (vec (.getTabs ^TabPane tab-pane))))
                  (is (= resource-node (g/node-value app-view :active-resource-node)))
                  (is (= view (scene/active-scene-view app-view)))))
              (is (= :failed (await-result failed-completion)))
              (is (= [false] @(:evaluation-threads signals))))))))))

;; Resource preparation evaluates and reports progress for the requested output
;; label, guarding against retaining scene-specific evaluation after reuse.
(deftest load-and-evaluate-resource-output-test
  (let [signals (assoc (make-signals) :content {:value 42})]
    (deliver (:load signals) true)
    (call-with-scene-project
      {"/first.pendingscene" signals}
      (fn test-evaluating-requested-output [{:keys [project]}]
        (let [resource-node
              (ui/run-now
                (project/get-resource-node project "/first.pendingscene"))

              reports (atom [])
              loading-message (localization/message "progress.loading")
              working-message (localization/message "progress.working")]

          (resource-loading/load-and-evaluate-resource!
            project resource-node "/first.pendingscene" :resource-info
            (fn render-progress! [progress]
              (swap! reports conj progress))
            (constantly false))
          (is (not (realized? (:evaluation-started signals))))
          (let [first-progress (first @reports)
                final-progress (peek @reports)]
            (is (= loading-message (progress/message first-progress)))
            (is (coll/any? #(and (= working-message (progress/message %))
                                 (nil? (progress/fraction %)))
                           @reports))
            (is (== 1 (progress/fraction final-progress))))
          (ui/run-now
            (is (resource-node/loaded? resource-node))
            (is (= {:value 42} (g/node-value resource-node :resource-info)))))))))

;; Dependencies are materialized before scene evaluation begins, and selection
;; requested by a later open is deferred until the pending scene is ready.
(deftest pending-scene-loads-dependencies-first-test
  (let [dependency-signals (make-signals)
        root-signals (assoc (make-signals) :content {:dependencies ["/dependency.pendingscene"]})]
    (deliver (:load dependency-signals) true)
    (deliver (:evaluate dependency-signals) true)
    (call-with-scene-project
      {"/root.pendingscene" root-signals
       "/dependency.pendingscene" dependency-signals}
      (fn test-loading-dependencies-first [{:keys [app-view prefs project workspace open!]}]
        (let [tab (open! "/root.pendingscene")
              load-state (tab-load-state tab)]
          (is (false? (await-result (:load-started root-signals))))
          (is (false? (await-result (:load-started dependency-signals))))
          (ui/run-now
            (let [dependency-resource-node (project/get-resource-node project "/dependency.pendingscene")
                  root-resource (workspace/find-resource workspace "/root.pendingscene")]
              (is (resource-node/loaded? dependency-resource-node))
              (app-view/open-resource! app-view prefs test-util/localization project
                                       root-resource {:select-node dependency-resource-node})
              (is (= [] (g/node-value app-view :selected-node-ids)))))
          (is (not (realized? (:evaluation-started dependency-signals))))
          (deliver (:load root-signals) true)
          (deliver (:evaluate root-signals) true)
          (is (= :ready (await-result (:completion @load-state))))
          (ui/run-now
            (is (= [(project/get-resource-node project "/dependency.pendingscene")]
                   (g/node-value app-view :selected-node-ids))))
          (is (= [false] @(:evaluation-threads dependency-signals)))
          (is (= [false] @(:evaluation-threads root-signals))))))))
