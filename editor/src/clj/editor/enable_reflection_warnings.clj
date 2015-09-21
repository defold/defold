(ns editor.enable-reflection-warnings)

;;; uncomment this when you want to profile for reflection warnings.
#_(alter-var-root #'clojure.core/*warn-on-reflection* (constantly true))
