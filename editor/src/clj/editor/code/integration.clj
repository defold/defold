(ns editor.code.integration
  (:require [clojure.java.io :as io]))

(defonce use-new-code-editor?
  (not (.exists (io/file (System/getProperty "user.home") ".defold" ".oldcode"))))
