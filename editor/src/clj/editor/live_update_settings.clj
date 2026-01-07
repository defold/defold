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

(ns editor.live-update-settings
  (:require [clojure.java.io :as io]
            [cognitect.aws.client.api :as aws]
            [cognitect.aws.config :as aws.config]
            [cognitect.aws.credentials :as aws.credentials]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.form :as form]
            [editor.fs :as fs]
            [editor.graph-util :as gu]
            [editor.localization :as localization]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.settings :as settings]
            [editor.settings-core :as settings-core]
            [service.log :as log]))

(def live-update-icon "icons/32/Icons_04-Project-file.png")

(def get-bucket-names
  (memoize
    (fn [{:keys [profile]}]
      (try
        (let [credentials (aws.credentials/profile-credentials-provider profile)
              client (aws/client {:api :s3 :credentials-provider credentials})]
          (->> (aws/invoke client {:op :ListBuckets})
               :Buckets
               (mapv :Name)))
        (catch Exception e
          (log/warn :exception e)
          [])))))

(defn- get-config-file-profiles []
  ;; Lookup sequence taken from [[aws.credentials/profile-credentials-provider]]
  (if-let [file-path (or
                       (some-> (fs/evaluate-path "$AWS_SHARED_CREDENTIALS_FILE") fs/existing-path)
                       (some-> (fs/evaluate-path "$AWS_CREDENTIAL_PROFILES_FILE") fs/existing-path)
                       (fs/existing-path (fs/evaluate-path "~/.aws/credentials")))]
    (try
      (vec (sort (keys (aws.config/parse (io/file file-path)))))
      (catch Exception e
        (log/warn :exception e)
        []))
    []))

(g/defnode LiveUpdateSettingsNode
  (inherits resource-node/ResourceNode)

  (input settings-map g/Any)

  (output amazon-creds g/Any :cached
          (g/fnk [settings-map]
                 (let [profile (get settings-map ["liveupdate" "amazon-credential-profile"])]
                   (when (and profile (pos? (count profile)))
                     {:profile profile}))))

  (output amazon-buckets g/Any :cached (g/fnk [amazon-creds] (if amazon-creds (get-bucket-names amazon-creds) [])))

  (input form-data g/Any)
  (output form-data g/Any :cached
          (g/fnk [form-data amazon-buckets]
                 (-> form-data
                     (assoc :navigation false)
                     (form/update-form-setting ["liveupdate" "amazon-credential-profile"]
                                               #(assoc %
                                                       :type :choicebox
                                                       :from-string str :to-string str
                                                       :options (mapv (fn [profile]
                                                                        [profile profile])
                                                                      (sort (get-config-file-profiles)))))
                     (form/update-form-setting ["liveupdate" "amazon-bucket"]
                                               #(assoc %
                                                       :type :choicebox
                                                       :from-string str
                                                       :to-string str
                                                       :options (mapv (fn [bucket]
                                                                        [bucket bucket])
                                                                      (sort amazon-buckets)))))))

  (input save-value g/Any)
  (output save-value g/Any (gu/passthrough save-value))
  (output build-targets g/Any :cached (g/constantly [])))

(def ^:private basic-meta-info
  (with-open [rdr (io/reader (io/resource "liveupdate-meta.properties"))]
    (settings-core/load-meta-properties rdr)))

(defn- load-live-update-settings [project self resource source-value]
  (let [graph-id (g/node-id->graph-id self)]
    (concat
      (g/make-nodes graph-id [settings-node settings/SettingsNode]
                    (g/connect settings-node :_node-id self :nodes)
                    (g/connect settings-node :settings-map self :settings-map)
                    (g/connect settings-node :save-value self :save-value)
                    (g/connect settings-node :form-data self :form-data)
                    (settings/load-settings-node project self settings-node resource source-value basic-meta-info nil)))))

(defn register-resource-types [workspace]
  (resource-node/register-settings-resource-type workspace
    :ext "settings"
    :label (localization/message "resource.type.settings")
    :node-type LiveUpdateSettingsNode
    :load-fn load-live-update-settings
    :meta-settings (:settings basic-meta-info)
    :icon live-update-icon
    :view-types [:cljfx-form-view :text]))

(defn get-live-update-settings-path [project]
  (resource/resource->proj-path (get (project/settings project) ["liveupdate" "settings"])))
