(ns editor.github
  (:require
   [clojure.string :as string]
   [editor.gl :as gl]
   [editor.system :as system])
  (:import
   (java.net URI URLEncoder)))

(set! *warn-on-reflection* true)

(def issue-repo "https://github.com/defold/editor2-issues")

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
  (format "%s/blob/master/faq/glgenbuffers.md" issue-repo))
