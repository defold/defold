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

(ns editor.github
  (:require
   [clojure.string :as string]
   [editor.connection-properties :refer [connection-properties]]
   [editor.gl :as gl]
   [editor.system :as system])
  (:import
   (java.net URI URLEncoder)))

(set! *warn-on-reflection* true)

(def issue-repo (get-in connection-properties [:git-issues :url]))

(defn- default-fields
  []
  {"Defold version"    (or (system/defold-version) "dev")
   "Defold channel"    (or (system/defold-channel) "")
   "Defold editor sha" (or (system/defold-editor-sha1) "")
   "Defold engine sha" (or (system/defold-engine-sha1) "")
   "Build time"        (or (system/defold-build-time) "")
   "OS name"           (system/os-name)
   "OS version"        (system/os-version)
   "OS arch"           (system/os-arch)
   "Java version"      (system/java-runtime-version)})

(defn- issue-body
  ([fields]
   (->> ["<!-- NOTE! The information you specify will be publicly accessible. -->"
         "### Expected behaviour"
         ""
         "### Actual behaviour"
         ""
         "### Steps to reproduce"
         ""
         "<hr/>"
         ""
         (when (seq fields)
           ["<table>"
            (for [[name value] (sort-by first fields)]
              (format "<tr><td>%s</td><td>%s</td></tr>" name value))
            "<table>"])]
        flatten
        (string/join "\n"))))

(defn new-issue-link
  ([]
   (new-issue-link {}))
  ([fields]
      (let [gl-info (gl/info)
            fields (cond-> fields
                     gl-info (assoc "GPU" (:renderer gl-info)
                               "GPU Driver" (:version gl-info)))]
        (str issue-repo "/issues/new?title=&labels=new&body="
          (URLEncoder/encode (issue-body (merge (default-fields) fields)))))))

(defn new-suggestion-link
  []
  (format "%s/issues/new?title=%s&body=%s" issue-repo (URLEncoder/encode "[SUGGESTION] ") (URLEncoder/encode "<!-- NOTE! The information you specify will be publicly accessible. -->")))

(defn search-issues-link
  []
  (format "%s/issues" issue-repo))

(defn glgenbuffers-link
  []
  (format "%s/blob/master/editor/README_TROUBLESHOOTING_GLGENBUFFERS.md" issue-repo))
