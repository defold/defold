(ns editor.github
  (:require
   [clojure.string :as string]
   [editor.system :as system])
  (:import
   (java.net URI URLEncoder)))

(set! *warn-on-reflection* true)

(def issue-repo "https://github.com/defold/editor2-issues")

(defn issue-body
  []
  (string/join "\n"
               ["### Expected behaviour"
                ""
                "### Actual behaviour"
                ""
                "### Steps to reproduce"
                ""
                "<hr/>"
                (format "Defold version: %s" (or (system/defold-version) "dev"))
                (format "Defold sha: %s" (or (system/defold-sha1) ""))
                (format "Platform: %s %s (%s)" (system/os-name) (system/os-version) (system/os-arch))
                (format "Java version: %s" (system/java-runtime-version))]))

(defn new-issue-link
  []
  (URI. (str issue-repo "/issues/new?title=&body="
             (URLEncoder/encode (issue-body)))))

(defn new-praise-link
  []
  (URI. (str issue-repo "/issues/new?title=" (URLEncoder/encode "[PRAISE] "))))
