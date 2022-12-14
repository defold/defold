;; Copyright 2020-2022 The Defold Foundation
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

(ns editor.engine.build-errors
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.data :refer [line-number->CursorRange]]
            [editor.defold-project :as project]
            [editor.field-expression :as field-expression]
            [editor.resource :as resource])
  (:import [com.dynamo.bob CompileExceptionError LibraryException MultipleCompileException MultipleCompileException$Info Task TaskResult]
           [com.dynamo.bob.bundle BundleHelper$ResourceInfo]
           [com.dynamo.bob.fs IResource]
           [java.net UnknownHostException]
           [org.apache.commons.io FilenameUtils]))

(set! *warn-on-reflection* true)

(def ^:private no-information-message "An error occurred while building Native Extensions, but the server provided no information.")

(defn- root-task ^Task [^Task task]
  (if-let [parent (.getProductOf task)]
    (recur parent)
    task))

(defprotocol ErrorInfoProvider
  (error-message [this] "Returns a non-empty string describing the issue.")
  (error-path [this] "Returns a slash-prefixed project-relative path to the offending resource, or nil if the issue does not relate to a particular resource.")
  (error-range [this] "Returns a CursorRange denoting the offending region in the file. Can be nil if the issue does not relate to a particular region.")
  (error-severity [this] "Returns either :info, :warning, or :fatal."))

(extend-type CompileExceptionError
  ErrorInfoProvider
  (error-message [this] (.getMessage this))
  (error-path [this] (some->> this .getResource .getPath (str "/")))
  (error-range [this] (line-number->CursorRange (.getLineNumber this)))
  (error-severity [_this] :fatal))

(extend-type MultipleCompileException$Info
  ErrorInfoProvider
  (error-message [this] (.getMessage this))
  (error-path [this] (some->> this .getResource .getPath (str "/")))
  (error-range [this] (line-number->CursorRange (.getLineNumber this)))
  (error-severity [this] (condp = (.getSeverity this)
                           MultipleCompileException$Info/SEVERITY_ERROR :fatal
                           MultipleCompileException$Info/SEVERITY_WARNING :warning
                           MultipleCompileException$Info/SEVERITY_INFO :info)))

(extend-type BundleHelper$ResourceInfo
  ErrorInfoProvider
  (error-message [this] (.message this))
  (error-path [this] (some->> this .resource (str "/")))
  (error-range [this] (line-number->CursorRange (.lineNumber this)))
  (error-severity [this] (condp = (.severity this)
                           "error" :fatal
                           "warning" :warning
                           :info)))

(extend-type TaskResult
  ErrorInfoProvider
  (error-message [this] (if-let [message (.getMessage this)]
                          message
                          (throw (or (.getException this)
                                     (ex-info "TaskResult failed without message or exception." {})))))
  (error-path [this]
    (when-some [^IResource resource (some->> this .getTask root-task .getInputs first)]
      (str "/" (.getPath resource))))
  (error-range [this] (line-number->CursorRange (.getLineNumber this)))
  (error-severity [_this] :fatal))

(defn- error-info-provider->cause [project evaluation-context e]
  (let [node-id (when-let [path (error-path e)]
                  (project/get-resource-node project path evaluation-context))
        cursor-range (error-range e)
        message (error-message e)
        adjusted-message (if (or (string/blank? message)
                                 (string/ends-with? message "internal server error"))
                           no-information-message
                           (string/trim message))
        severity (error-severity e)]
    (g/map->error
      {:_node-id node-id
       :message adjusted-message
       :severity severity
       :user-data (when cursor-range
                    {:cursor-range cursor-range})})))

(def ^:private invalid-lib-re #"(?m)^Invalid lib in extension '(.+?)'.*$")
(def ^:private manifest-ext-name-re1 #"(?m)^name:\s*\"(.+)\"\s*$") ; Double-quoted name
(def ^:private manifest-ext-name-re2 #"(?m)^name:\s*'(.+)'\s*$") ; Single-quoted name

(defn- invalid-lib-match->cause [project evaluation-context [all manifest-ext-name] manifest-ext-resources-by-name]
  (assert (string? (not-empty all)))
  (assert (string? (not-empty manifest-ext-name)))
  (let [manifest-ext-resource (manifest-ext-resources-by-name manifest-ext-name)
        node-id (project/get-resource-node project manifest-ext-resource evaluation-context)]
    (g/map->error
      {:_node-id node-id
       :message all
       :severity :fatal})))

(defn- try-parse-manifest-ext-name [resource]
  (when (satisfies? io/IOFactory resource)
    (let [file-contents (slurp resource)]
      (second (or (re-find manifest-ext-name-re1 file-contents)
                  (re-find manifest-ext-name-re2 file-contents))))))

(defn- extension-manifest? [resource]
  (= "ext.manifest" (resource/resource-name resource)))

(defn- extension-manifests [project evaluation-context]
  (filterv extension-manifest? (g/node-value project :resources evaluation-context)))

(defn- extension-manifest-node-ids [project evaluation-context]
  (keep #(project/get-resource-node project % evaluation-context) (extension-manifests project evaluation-context)))

(defn- get-manifest-ext-resources-by-name [project evaluation-context]
  (into {}
        (keep (fn [resource]
                (when-let [manifest-ext-name (try-parse-manifest-ext-name resource)]
                  [manifest-ext-name resource])))
        (extension-manifests project evaluation-context)))

(defn- try-parse-invalid-lib-error-causes [project evaluation-context log]
  (when-let [matches (not-empty (re-seq invalid-lib-re log))]
    (let [manifest-ext-resources-by-name (get-manifest-ext-resources-by-name project evaluation-context)]
      (mapv #(invalid-lib-match->cause project evaluation-context % manifest-ext-resources-by-name) matches))))

(def ^:private compilation-line-patterns
  "Regular expressions for matching different types of output from native code
  compilation."
  [[:note-warning-error
    ;; Group 1: File name
    ;; Group 2: Line number
    ;; Group 3: Column number
    ;; Group 4: warning/error
    ;; Group 5: Message
    #"^([^:]+)[:\(](\d+)[\):](\d+)?:?\s*(?:fatal)?\s*(warning|error|note):?\s*(.*)$"]
   [:undefined-reference
    ;; Group 1: File name
    ;; Group 2: Line number
    ;; Group 3: Message
    #"^([^:]+):(\d+):\s*(undefined reference.*)$"]
   [:undefined-symbols
    #"^Undefined symbols.*:$"]
   [:included-from
    ;; Group 1: In file included from/from
    ;; Group 2: File name
    ;; Group 3: Line number
    #"^(In file included from|\s+from)\s+(.*):(\d+).*$"]
   [:in-function
    #"^.*:\sIn [function|constructor|destructor|member].*:$"]
   [:marker
    #"^\s*~*\s*\^\s*~*\s*$"]
   [:summary
    ;; Group 1: The word warning/warnings
    ;; Group 2: The word error/errors
    #"^\d+\s(warnings?)?\s?(?:and)?(?:\s\d+\s)?(errors?)?.*$"]
   [:tool-output
    ;; Group 1: error:/warning:
    ;; Group 2: Rest of line after program name or error:/warning:.
    #"^(?:(?!:\/| |\\).)*: (error|warning)?:?\s*(.*)$"]
   [:javac
    #"^javac\s.*$"]
   [:jar-conflict
    #"Uncaught translation error:*.+"]
   [:empty
    #"^$"]])

(defn- buildpath->projpath
  "First path will be converted to use Unix separators, then this function
  looks for upload/ or build/ in path and removes everything up to and including
  the first occurrence, leaving the slash.
  Eg. upload/myproj/myfile.file would become /myproj/myfile.file
  If upload/ or build/ are not found the path will be returned as is if it
  starts with a slash, otherwise it will be returned with a slash prepended."
  [^String path]
  (let [unix-path (FilenameUtils/separatorsToUnix path)
        idx (.indexOf unix-path "upload/")
        [idx word-len] (if (= -1 idx)
                         [(.indexOf unix-path "build/") (count "build")]
                         [idx (count "upload")])]
    (if-not (= -1 idx)
      (subs path (+ idx word-len))
      (if (string/starts-with? unix-path "/")
        unix-path
        (str "/" unix-path)))))

(defn- try-match-compilation-regexes
  "Returns a pair [name match] for the first regex match in
  `compilation-line-patterns` where name is the keyword naming the pattern and
  match is the output from re-matches. Returns nil if no pattern matches."
  [line]
  (some (fn [[name pattern]]
          (when-let [match (re-matches pattern line)]
            [name match]))
        compilation-line-patterns))

(defn- parse-compilation-line
  "Parse a single line of gcc/clang compiler output. Produces a map containing at
  least the keys :type and :message. Other possible keys are :file, :line
  and :column.
  Full example:
  {:type :warning
   :file \"/hello/world.cpp\"
   :line 10
   :column 2
   :message \"Something error happens!\"}
  Possible values for :type are :error, :warning, :note, :included-from, :marker,
  :summary, :in-function, :javac, :empty and :unknown.
  The :file value, if it represents a project resource, will be a project root
  relative Unix style path starting with a forward slash."
  [line ext-manifest-file]
  (let [[name match] (or (try-match-compilation-regexes line)
                         [:unknown line])]
    (case name
      :note-warning-error {:type (keyword (match 4))
                           :file (buildpath->projpath (match 1))
                           :line (field-expression/to-int (match 2))
                           :column (field-expression/to-int (match 3))
                           :message (match 5)}
      :included-from {:type :included-from
                      :file (buildpath->projpath (match 2))
                      :line (field-expression/to-int (match 3))
                      :message line}
      :tool-output {:type (if (match 1)
                            (keyword (match 1))
                            :note)
                    :file ext-manifest-file
                    :message line}
      :undefined-reference {:type :error ;; Undefined references break the build.
                            :file (buildpath->projpath (match 1))
                            :line (field-expression/to-int (match 2))
                            :message (match 3)}
      :undefined-symbols {:type :error
                          :message line}
      :jar-conflict {:type :error
                     :file ext-manifest-file
                     :message line}
      {:type name :message line})))

(defn- merge-compilation-messages
  "Add the message from line to the end of the message of current."
  [current line]
  (if (empty? current)
    line
    (update current :message str \newline (:message line))))

(defn- conj-compilation-entry
  "Conjoin current to acc (accumulator) if current is not empty and is not of
  type :unknown."
  [acc current]
  (if (and (not-empty current)
           (not= :unknown (:type current)))
    (conj acc current)
    acc))

(defn- next-compilation-line
  "Helper function for applying actions in the loop of `parse-compilation-log`."
  [{:keys [lines current acc] :as state} line & actions]
  (let [next-state (merge state {:current line
                                 :lines (next lines)
                                 :included-from? false})]
    (reduce
      (fn [state action]
        (case action
          :conj-message (assoc state :current (merge-compilation-messages current line))
          :conj-message-to-previous (merge state {:current (merge-compilation-messages (last acc) (merge-compilation-messages current line))
                                                  :acc (pop acc)})
          :included-from (assoc state :included-from? true)
          :replace-file (assoc state :current (merge (:current state) (select-keys line [:file :line])))
          :replace-type (assoc state :current (assoc (:current state) :type (:type line)))
          :conj-entry (assoc state :acc (conj-compilation-entry acc current))))
      next-state
      actions)))

(defn- ignore-line-and-start-next-entry
  [state original-ext-manifest-file]
  (-> (next-compilation-line state {} :conj-entry)
      (assoc :ext-manifest-file original-ext-manifest-file)))

(defn find-ext-manifest-relative-to-resource
  "Find the ext.manifest file for the native extension that proj-path belongs to."
  [project ^String proj-path evaluation-context]
  (let [nodes-by-resource-path (g/node-value project :nodes-by-resource-path evaluation-context)
        find-resource (fn [path] (g/node-value (nodes-by-resource-path path) :resource evaluation-context))]
    (->> proj-path
         (iterate resource/parent-proj-path)
         (take-while not-empty)
         (map find-resource)
         (some (fn [resource]
                 (when resource
                   (->> (resource/children resource)
                        (some (fn [child]
                                (when (= (resource/resource-name child) "ext.manifest")
                                  (resource/proj-path child)))))))))))

(defn parse-compilation-log
  "Parse the output from native extension compilation and produce a vector of maps
  describing each entry. See the documentation of `parse-compilation-line` for
  an explanation of the keys that can be present in the maps. Any message in the
  log that cannot be attributed to a line in a specific file will either be
  attributed to ext.manifest or to no resource. The ext.manifest file used for
  this attribution is the one supplied, or the one nearest the current resource
  being parsed if none was supplied. If no ext.manifest file is supplied and
  none can be found then such entries will not receive a resource attribution."
  [text project ext-manifest-file]
  (g/with-auto-evaluation-context evaluation-context
    (let [original-ext-manifest-file ext-manifest-file
          nodes-by-resource-path (g/node-value project :nodes-by-resource-path evaluation-context)]
      (loop [{:keys [lines current acc included-from? ext-manifest-file] :as state}
             {:lines (string/split-lines text)
              :current {}
              :acc []
              :included-from? false
              :ext-manifest-file original-ext-manifest-file}]
        (if-not lines
          (if (= :included-from (:type current))
            (conj (pop acc) (merge-compilation-messages (last acc) current))
            (conj-compilation-entry acc current))
          (let [line (parse-compilation-line (first lines) ext-manifest-file)
                project-resource? (contains? nodes-by-resource-path (:file line))
                ;; Make sure we point to a project file always. For errors that
                ;; manifest in included files that live on the build server, this
                ;; means that we will point at the include statement. Not strictly
                ;; correct but it is the best we can do in the confines of the
                ;; project. Also find the most relevant ext.manifest file for the
                ;; current resource so we have something to associate for example
                ;; link errors with.
                [line ext-manifest-file] (if-not project-resource?
                                           [(merge line (select-keys current [:file :line :column]))
                                            (or ext-manifest-file (find-ext-manifest-relative-to-resource project (:file current) evaluation-context))]
                                           [line
                                            (or ext-manifest-file (find-ext-manifest-relative-to-resource project (:file line) evaluation-context))])
                state (assoc state :ext-manifest-file ext-manifest-file)
                next-state (case (:type line)
                             :included-from (if included-from?
                                              (if project-resource?
                                                ;; Each successive included-from gets more
                                                ;; specific so if it refers to a project file,
                                                ;; use that.
                                                (next-compilation-line state line :conj-message :replace-file :included-from)
                                                (next-compilation-line state line :conj-message :included-from))
                                              (if (= :included-from (:type current))
                                                ;; If we have not found a warning, error or note yet we merge this entry into previous.
                                                (next-compilation-line state line :conj-message-to-previous :included-from)
                                                ;; New entry if "included from" does not follow another "included from".
                                                (next-compilation-line state line :conj-entry :included-from)))
                             ;; Unknown are a catch-all for informational messages without a category. Eg. source code lines.
                             :unknown (next-compilation-line state line :conj-message)
                             ;; Marker is the ^ pointing at the column where the error manifests.
                             :marker (next-compilation-line state line :conj-message)
                             ;; Summary line marks the end of current entry.
                             ;; Next batch of errors could be from another native extension so we reset the ext.manifest reference.
                             :summary (ignore-line-and-start-next-entry state original-ext-manifest-file)
                             ;; Start of new entry with some extra information from gcc that we don't need. Do same as for summary line.
                             :in-function (ignore-line-and-start-next-entry state original-ext-manifest-file)
                             ;; Empty lines separate entries for some compilers/tools
                             :empty (ignore-line-and-start-next-entry state original-ext-manifest-file)
                             ;; javac invocation line, ignore.
                             :javac (ignore-line-and-start-next-entry state original-ext-manifest-file)
                             ;; Error/warning/note
                             (if included-from?
                               ;; If the last line was an "included from" line and we encounter a
                               ;; note, warning or error that means the previous lines were
                               ;; information belonging to the note/warning/error message. So we
                               ;; attach the message from this line and change the type of the
                               ;; previous to the type of this one.
                               (if project-resource?
                                 ;; If the note/warning/error refers to a project file, let that take precedence.
                                 (next-compilation-line state line :conj-message :replace-type :replace-file)
                                 (next-compilation-line state line :conj-message :replace-type))
                               ;; New entry.
                               (next-compilation-line state line :conj-entry)))]
            (recur next-state)))))))

(defn- parsed-compilation-entry->ErrorInfoProvider
  "Convert a log entry produced by `parse-compilation-log` into to an
  ErrorInfoProvider."
  [error]
  (reify ErrorInfoProvider
    (error-message [_] (:message error))
    (error-path [_] (:file error))
    (error-range [_] (line-number->CursorRange (:line error) (:column error)))
    (error-severity [_]
      (case (:type error)
        :warning :warning
        :error :fatal
        :note :info
        :info))))

(defn- try-parse-compiler-error-causes
  [project evaluation-context log ext-manifest-file]
  (->> (parse-compilation-log log project ext-manifest-file)
       (distinct)
       (map parsed-compilation-entry->ErrorInfoProvider)
       (map (partial error-info-provider->cause project evaluation-context))
       (not-empty)))

(defn- generic-extension-error-causes [project evaluation-context log]
  ;; This is a catch-all that simply dumps the entire log output into the Build
  ;; Errors view in case we've failed to parse more meaningful errors from it.
  ;; It will split log lines across multiple build error entries, since the
  ;; control we use currently does not cope well with multi-line strings.
  (let [node-id (first (extension-manifest-node-ids project evaluation-context))]
    (or (when-let [log-lines (seq (drop-while string/blank? (string/split-lines (string/trim-newline log))))]
          (into []
                (map-indexed (fn [index log-line]
                               (g/map->error
                                 {:_node-id node-id
                                  :message log-line
                                  :severity (if (zero? index)
                                              :fatal
                                              :info)})))
                log-lines))
        [(g/map->error
           {:_node-id node-id
            :message no-information-message
            :severity :fatal})])))

(defn failed-tasks-error-causes [project evaluation-context failed-tasks]
  (mapv (partial error-info-provider->cause project evaluation-context)
        failed-tasks))

(defn unsupported-platform-error [platform]
  (ex-info (str "Unsupported platform " platform)
           {:type ::unsupported-platform-error
            :platform platform}))

(defn unsupported-platform-error? [exception]
  (= ::unsupported-platform-error (:type (ex-data exception))))

(defn unsupported-platform-error-causes [project evaluation-context]
  [(g/map->error
     {:_node-id (first (extension-manifest-node-ids project evaluation-context))
      :message "Native Extensions are not yet supported for the target platform"
      :severity :fatal})])

(defn missing-resource-error [prop-name referenced-proj-path referencing-node-id]
  (ex-info (format "%s '%s' could not be found" prop-name referenced-proj-path)
           {:type ::missing-resource-error
            :node-id referencing-node-id
            :proj-path referenced-proj-path}))

(defn- missing-resource-error? [exception]
  (= ::missing-resource-error (:type (ex-data exception))))

(defn- missing-resource-error-causes [^Throwable exception]
  [(g/map->error
     {:_node-id (:node-id (ex-data exception))
      :message (.getMessage exception)
      :severity :fatal})])

(defn build-error [platform status log]
  (ex-info (format "Failed to build engine, status %d: %s" status log)
           {:type ::build-error
            :platform platform
            :status status
            :log log}))

(defn build-error? [exception]
  (= ::build-error (:type (ex-data exception))))

(defn- build-error-causes [project evaluation-context exception]
  (let [log (:log (ex-data exception))
        ext-manifest-seq (get-manifest-ext-resources-by-name project evaluation-context)
        ext-manifest-file (when (= 1 (count ext-manifest-seq)) ;; If we have more than one, better not to pick one at random to avoid misinformation.
                            (resource/proj-path (second (first ext-manifest-seq))))]
    (or (try-parse-invalid-lib-error-causes project evaluation-context log)
        (try-parse-compiler-error-causes project evaluation-context log ext-manifest-file)
        (generic-extension-error-causes project evaluation-context log))))

(defn- compile-exception-error-causes [project evaluation-context exception]
  [(error-info-provider->cause project evaluation-context exception)])

(defn- output-log-path [project evaluation-context ^MultipleCompileException exception]
  (let [log-path (.getLogPath exception)]
    [(g/map->error
      {:_node-id nil ;; The editor cannot currently reference files in the /build folder
       :message (str "For the full log, see " log-path)
       :severity :warning})]))

(defn- multiple-compile-exception-error-causes [project evaluation-context ^MultipleCompileException exception]
  (let [log (.getRawLog exception)
        ext-manifest-file (find-ext-manifest-relative-to-resource
                            project
                            (buildpath->projpath (.getPath (.getContextResource exception)))
                            evaluation-context)]
    (into [] (concat
              (or (try-parse-invalid-lib-error-causes project evaluation-context log)
                  (try-parse-compiler-error-causes project evaluation-context log ext-manifest-file)
                  (generic-extension-error-causes project evaluation-context log))
              
              (output-log-path project evaluation-context exception)))))

(defn- library-exception-error-causes [project evaluation-context ^Throwable exception]
  [(g/map->error
     {:_node-id (project/get-resource-node project "/game.project" evaluation-context)
      :message (.getMessage exception)
      :severity :fatal})])

(defn- invalid-build-server-url-causes [ex]
  [(g/map->error {:message (str "Invalid build server URL: " (ex-message ex))
                  :severity :fatal})])

(defn- generic-error-causes [ex]
  [(g/map->error {:message (str "Failed: " (ex-message ex))
                  :severity :fatal})])

(defn handle-build-error! [render-error! project evaluation-context exception]
  (render-error!
    {:causes (cond
               (unsupported-platform-error? exception)
               (unsupported-platform-error-causes project evaluation-context)

               (missing-resource-error? exception)
               (missing-resource-error-causes exception)

               (build-error? exception)
               (build-error-causes project evaluation-context exception)

               (instance? CompileExceptionError exception)
               (compile-exception-error-causes project evaluation-context exception)

               (instance? MultipleCompileException exception)
               (multiple-compile-exception-error-causes project evaluation-context exception)

               (instance? LibraryException exception)
               (library-exception-error-causes project evaluation-context exception)

               (instance? UnknownHostException exception)
               (invalid-build-server-url-causes exception)

               :else
               (generic-error-causes exception))}))
