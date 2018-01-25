(ns editor.code.integration
  (:require [clojure.java.io :as io]))

(defonce use-new-code-editor?
  true ; TODO: Too much work to port old script node to work with resource properties. Old code editor will be completely removed soon.
  #_(not (.exists (io/file (System/getProperty "user.home") ".defold" ".oldcode"))))
