(ns editor.github
  (:require
   [clojure.string :as string]
   [editor.system :as system])
  (:import
   (java.net URI URLEncoder)))

(set! *warn-on-reflection* true)

(def issue-repo "https://github.com/defold/editor2-issues")

(defn- default-fields
  []
  {"Defold version" (or (system/defold-version) "dev")
   "Defold sha"     (or (system/defold-sha1) "")
   "OS name"        (system/os-name)
   "OS version"     (system/os-version)
   "OS arch"        (system/os-arch)
   "Java version"   (system/java-runtime-version)})

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
            (for [[name value] fields]
              (format "<tr><td>%s</td><td>%s</td></tr>" name value))
            "<table>"])]
        flatten
        (string/join "\n"))))

(defn new-issue-link
  ([]
   (new-issue-link {}))
  ([fields]
      (str issue-repo "/issues/new?title=&labels=new&body="
           (URLEncoder/encode (issue-body (merge (default-fields) fields))))))

(defn new-praise-link
  []
  (format "%s/issues/new?title=%s&body=%s" issue-repo (URLEncoder/encode "[PRAISE] ") (URLEncoder/encode "<!-- NOTE! The information you specify will be publicly accessible. -->")))
