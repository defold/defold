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

(ns editor.library
  (:require [editor.localization :as localization]
            [editor.progress :as progress]
            [editor.settings-core :as settings-core]
            [util.path :as path])
  (:import [com.dynamo.bob Progress Progress$Reporter]
           [com.dynamo.bob Project]
           [com.dynamo.bob.archive EngineVersion]
           [com.dynamo.bob.util Library Library$Problem$DefoldMinVersion Library$Problem$FetchFailed Library$Problem$InstallFailed Library$Problem$InvalidArchive Library$Problem$Missing Library$Result]))

(set! *warn-on-reflection* true)

(defn parse-uris [uri-string]
  (settings-core/parse-setting-value {:type :list :element {:type :url}} uri-string))

(defn result-message [^Library$Result result]
  {:pre [(.problem result)]}
  (let [problem (.problem result)]
    (localization/message
      "notification.fetch-libraries.dependency-problem"
      {"uri" (.uri result)
       "problem" (condp instance? problem
                   Library$Problem$Missing
                   (localization/message "notification.fetch-libraries.problem.missing")

                   Library$Problem$FetchFailed
                   (localization/message "notification.fetch-libraries.problem.fetch-failed")

                   Library$Problem$InvalidArchive
                   (localization/message "notification.fetch-libraries.problem.invalid-archive")

                   Library$Problem$DefoldMinVersion
                   (localization/message
                     "notification.fetch-libraries.problem.defold-min-version"
                     {"required" (.required ^Library$Problem$DefoldMinVersion problem)
                      "current" EngineVersion/version})

                   Library$Problem$InstallFailed
                   (localization/message "notification.fetch-libraries.problem.install-failed")

                   (localization/message "notification.fetch-libraries.problem.unknown" {"problem" problem}))})))

(defn cached [project-directory library-uris]
  (Library/cached (or library-uris []) (path/of project-directory Project/LIB_DIR)))

(defn fetch! [project-directory library-uris render-progress!]
  (Library/fetch
    (or library-uris [])
    (path/of project-directory Project/LIB_DIR)
    nil
    nil
    (Progress.
      (reify Progress$Reporter
        (report [_ message fraction] (render-progress! (progress/bob message fraction)))
        (close [_] (render-progress! progress/done))
        (isCanceled [_] false)))))
