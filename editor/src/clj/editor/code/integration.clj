(ns editor.code.integration
  (:require [clojure.java.io :as io]))

(defonce use-new-code-editor?
  (.exists (io/file (System/getProperty "user.home") ".defold" ".newcode")))
