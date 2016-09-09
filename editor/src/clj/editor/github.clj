(ns editor.github
  (:require
   [clojure.string :as string]
   [editor.system :as system])
  (:import
   (java.net URI URLEncoder)))

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
                (format "Version:" (or (system/defold-version) "dev"))
                (format "Sha:" (system/defold-sha1))
                (format "Platform: %s %s" (system/os-name) (system/os-version))]))

(defn new-issue-link
  []
  (URI. (str issue-repo "/issues/new?title=&body="
             (URLEncoder/encode (issue-body)))))
