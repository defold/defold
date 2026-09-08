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

(ns editor.gltf-ui
  (:require [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.dialogs :as dialogs]
            [editor.gltf :as gltf]
            [editor.localization :as localization]
            [editor.notifications :as notifications]
            [editor.resource :as resource]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [util.coll :as coll]))

(def pbr-library-url "https://github.com/defold/asset-pbr/archive/refs/heads/master.zip")

(defn offer-pbr-library!
  "Offers the shader library once per batch of newly added project glTF files."
  [project localization-state added-resources]
  (when (and (coll/any? #(and (resource/file-resource? %)
                             (= :file (resource/source-type %))
                             (resource/loaded? %)
                             (resource/exists? %)
                             (#{"gltf" "glb"} (resource/type-ext %)))
                       added-resources)
             (not (coll/any? #(= pbr-library-url (str %))
                             (project/project-dependencies project)))
             (dialogs/make-confirmation-dialog
               localization-state
               {:title (localization/message "dialog.gltf-pbr-library.title")
                :icon :icon/circle-question
                :header (localization/message "dialog.gltf-pbr-library.header")
                :content (localization/message "dialog.gltf-pbr-library.content" {"url" pbr-library-url})
                :buttons [{:text (localization/message "dialog.button.cancel")
                           :cancel-button true
                           :result false}
                          {:text (localization/message "dialog.gltf-pbr-library.button.add")
                           :default-button true
                           :result true}]}))
    (ui/execute-command (ui/contexts (ui/main-scene) true)
                        :private/add-dependency {:dep-url pbr-library-url})))

(defn- update-diagnostics!
  "Shows changed extraction warnings and clears warnings for repaired or removed sources."
  [workspace old-diagnostics new-diagnostics]
  (let [notifications (workspace/notifications workspace)]
    (run! (fn [source-path]
            (when-not (contains? new-diagnostics source-path)
              (notifications/close! notifications [::diagnostics source-path])))
          (coll/keys old-diagnostics))
    (run! (fn [[source-path diagnostics]]
            (when (not= diagnostics (old-diagnostics source-path))
              (notifications/show!
                notifications
                {:id [::diagnostics source-path]
                 :type :warning
                 :message (localization/message "notification.gltf.unsupported-assets"
                                                {"file" source-path
                                                 "diagnostics" (coll/join-to-string "\n" diagnostics)})})))
          new-diagnostics)))

(defn register-resource-listener!
  "Connects glTF import prompts and extraction warnings to the editor resource lifecycle."
  [workspace project localization-state]
  (let [previous-diagnostics (atom {})
        handle-changes! (fn [changes]
                          ;; Show dialogs after resource-sync completes, outside graph transactions.
                          (ui/run-later
                            (when (g/node-exists? workspace)
                              (let [diagnostics (gltf/diagnostics (workspace/snapshot-cache workspace))]
                                (update-diagnostics! workspace @previous-diagnostics diagnostics)
                                (reset! previous-diagnostics diagnostics))
                              (offer-pbr-library! project localization-state (:added changes)))))]
    (workspace/add-resource-listener!
      workspace 0
      (reify resource/ResourceListener
        (handle-changes [_this changes _render-progress!]
          (handle-changes! changes))))
    (handle-changes! {})))
