;; Copyright 2020-2025 The Defold Foundation
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

(ns editor.notifications
  (:require [clojure.spec.alpha :as s]
            [dynamo.graph :as g]))

(s/def ::type #{:info :warning :error})
(s/def ::id any?)
(s/def ::text string?)
(s/def ::on-action ifn?)
(s/def ::action (s/keys :req-un [::text ::on-action]))
(s/def ::actions (s/coll-of ::action))
(s/def ::notification (s/keys :req-un [::type ::text]
                              :opt-un [::id ::actions]))

(g/defnode NotificationsNode
  (property notifications g/Any (default {:id->notification {}
                                          :ids []})))

(defn show!
  "Show a notification in the view

  Args:
    notifications-node    node id of NotificationsNode type
    notification          map with the following keys:
                            :type       required, :info, :warning or :error
                            :text       required, notification text string
                            :id         optional id, anything; submitting
                                        notification a second time with the same
                                        id will overwrite the first one
                            :actions    optional vector of maps with :text
                                        strings and :on-action 0-arg callbacks"
  [notifications-node notification]
  {:pre [(s/valid? ::notification notification)]}
  (g/update-property! notifications-node :notifications
                      (fn [{:keys [id->notification] :as notifications}]
                        (let [id (or (:id notification)
                                     (gensym (str (symbol ::id))))
                              exists (contains? id->notification id)]
                          (-> notifications
                              (update :id->notification assoc id notification)
                              (cond-> (not exists) (update :ids conj id))))))
  nil)

(defn close!
  "Close notification by id"
  [notifications-node id]
  (g/update-property! notifications-node :notifications
                      (fn [{:keys [id->notification] :as notifications}]
                        (if (contains? id->notification id)
                          (-> notifications
                              (update :id->notification dissoc id)
                              (update :ids (fn [ids]
                                             (filterv #(not= id %) ids))))
                          notifications)))
  nil)

(comment

  (editor.ui/run-now
    (show! (g/node-value 0 :notifications)
           {:type :info
            :id ::updatable
            :text (str "Updatable " (rand-int 10000))}))

  (editor.ui/run-now
    (show! (g/node-value 0 :notifications)
           {:type :error
            :text "An error occurred"}))

  (editor.ui/run-now
    (show! (g/node-value 0 :notifications)
           {:type :warning
            :text "!"}))

  (editor.ui/run-now
    (show!
      (g/node-value 0 :notifications)
      {:type :warning
       :text "Folder /defold-rive shadows a folder with the same name defined in a dependency: https://github.com/defold/extension-rive/archive/refs/tags/1.0.zip"
       :actions [{:text "Suppress warning for this folder"
                  :on-action #(tap> :suppress)}]}))

  ,)