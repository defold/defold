(ns editor.luacheck
  (:require [clojure.java.io :as io]
            [clojure.java.shell :as shell]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.lua :as lua]
            [editor.resource :as resource])
  (:import (java.io File)))

(set! *warn-on-reflection* true)

(defn- join-file
  ^File [^File file & tokens]
  (apply io/file file (map (fn [^String token]
                             (if (.startsWith token "/")
                               (subs token 1)
                               token))
                           tokens)))

(def ^:private luacheck-output-line-pattern #"(.*?):(\d+):(\d+): \((.*?)\) (.*)")

(defn- parse-issue [^File project-root luacheck-output-line]
  (when-some [[_ abs-path row-digits _col-digits code description] (re-matches luacheck-output-line-pattern luacheck-output-line)]
    (let [proj-path (resource/file->proj-path project-root (io/as-file abs-path))
          line (Long/parseUnsignedLong row-digits)
          message (str description " [LUACHECK-" (subs code 1) "]")
          user-data {:filename proj-path
                     :line line
                     :code code}]
      (case (.charAt ^String code 0)
        \E (g/error-fatal message user-data)
        \W (g/error-warning message user-data)))))

(def ^:private luacheck-static-args
  (into ["luacheck"
         "--codes"
         "--formatter" "plain"]
        (concat (mapcat (partial vector "--globals")
                        (sort lua/defold-callbacks))
                (mapcat (partial vector "--read-globals")
                        (sort lua/defold-globals)))))

(defn check [^File project-root proj-path source-code-reader]
  (let [source-file (join-file project-root proj-path)
        source-file-dir (.getParentFile source-file)
        cache-file (join-file project-root ".internal/.luacheckcache")
        {:keys [exit out]} (apply shell/sh
                                  (into luacheck-static-args
                                        ["--cache" (.getAbsolutePath cache-file)
                                         "--filename" (.getAbsolutePath source-file)
                                         "-"
                                         :dir source-file-dir
                                         :in source-code-reader]))]
    (when (not (zero? exit))
      (into []
            (keep (partial parse-issue project-root))
            (string/split-lines out)))))
