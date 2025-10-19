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
            [dynamo.graph :as g]
            [editor.localization :as localization]))

(s/def ::type #{:info :warning :error})
(s/def ::id any?)
(s/def ::message localization/message-pattern?)
(s/def ::on-action ifn?)
(s/def ::action (s/keys :req-un [::message ::on-action]))
(s/def ::actions (s/coll-of ::action))
(s/def ::notification (s/keys :req-un [::type ::message]
                              :opt-un [::id ::actions]))

(g/defnode NotificationsNode
  (property notifications g/Any (default {:id->notification {}
                                          :ids []})))

(defn show
  "Create transaction steps for showing a notification in the view

  Args:
    notifications-node    node id of NotificationsNode type
    notification          map with the following keys:
                            :type       required, :info, :warning or :error
                            :message    required, notification MessagePattern
                            :id         optional id, anything; submitting
                                        notification a second time with the same
                                        id will overwrite the first one
                            :actions    optional vector of maps with :message
                                        MessagePatterns and :on-action
                                        0-arg callbacks"
  [notifications-node notification]
  {:pre [(s/valid? ::notification notification)]}
  (g/update-property
    notifications-node :notifications
    (fn [{:keys [id->notification] :as notifications}]
      (let [id (or (:id notification)
                   (gensym (str (symbol ::id))))
            exists (contains? id->notification id)]
        (-> notifications
            (update :id->notification assoc id notification)
            (cond-> (not exists) (update :ids conj id)))))))

(defn show!
  "Show a notification in the view

  Args:
    notifications-node    node id of NotificationsNode type
    notification          map with the following keys:
                            :type       required, :info, :warning or :error
                            :message    required, notification MessagePattern
                            :id         optional id, anything; submitting
                                        notification a second time with the same
                                        id will overwrite the first one
                            :actions    optional vector of maps with :message
                                        MessagePatterns and :on-action
                                        0-arg callbacks"
  [notifications-node notification]
  (g/transact (show notifications-node notification))
  nil)

(defn close
  "Create transaction steps for closing a notification by id"
  [notifications-node id]
  (g/update-property
    notifications-node :notifications
    (fn [{:keys [id->notification] :as notifications}]
      (if (contains? id->notification id)
        (-> notifications
            (update :id->notification dissoc id)
            (update :ids (fn [ids]
                           (filterv #(not= id %) ids))))
        notifications))))

(defn close!
  "Close notification by id"
  [notifications-node id]
  (g/transact (close notifications-node id))
  nil)

(comment

  (editor.ui/run-now
    (show! (g/node-value 0 :notifications)
           {:type :info
            :id ::updatable
            :message (localization/message "dialog.desktop-entry.creation-failed.content" {"error" (rand-int 10000)})}))

  (editor.ui/run-now
    (show! (g/node-value 0 :notifications)
           {:type :error
            :message (localization/message "dialog.button.close")}))

  (editor.ui/run-now
    (show! (g/node-value 0 :notifications)
           {:type :warning
            :message (localization/message "dialog.button.close")}))

  (editor.ui/run-now
    (show!
      (g/node-value 0 :notifications)
      {:type :warning
       :message (localization/message "dialog.button.close")
       :actions [{:message (localization/message "dialog.button.close")
                  :on-action #(tap> :suppress)}]}))

  ,)