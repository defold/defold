(ns editor.editor-extensions
  (:require [clojure.java.shell :as shell]
            [clojure.spec.alpha :as s]
            [clojure.stacktrace :as stacktrace]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.data :as data]
            [editor.code.util :as code.util]
            [editor.console :as console]
            [editor.defold-project :as project]
            [editor.error-reporting :as error-reporting]
            [editor.graph-util :as gu]
            [editor.handler :as handler]
            [editor.luart :as luart]
            [editor.resource :as resource]
            [editor.util :as util]
            [editor.workspace :as workspace])
  (:import [org.luaj.vm2 LuaError LuaValue LuaFunction Prototype]
           [clojure.lang MultiFn]
           [com.defold.editor Platform]
           [java.io File]
           [java.nio.file Path]))

(set! *warn-on-reflection* true)

(defn- unwrap-error-values [arr]
  (mapv #(cond-> % (g/error? %) :value) arr))

(g/defnode EditorExtensions
  (input project-prototypes g/Any :array :substitute unwrap-error-values)
  (input library-prototypes g/Any :array :substitute unwrap-error-values)
  (output project-prototypes g/Any (gu/passthrough project-prototypes))
  (output library-prototypes g/Any (gu/passthrough library-prototypes)))

(defn make [graph]
  (first (g/tx-nodes-added (g/transact (g/make-node graph EditorExtensions)))))

(defprotocol UI
  (reload-resources! [this]
    "Synchronously reload resources, throw exception if failed")
  (can-execute? [this command]
    "Ask user to execute `command` (a vector of command name string and argument
    strings)")
  (display-output! [this type string]
    "Display extension output, `type` is either `:out` or `:err`, string may be
    multiline"))

(defn- lua-fn? [x]
  (instance? LuaFunction x))

(defn- resource-path? [x]
  (and (string? x) (string/starts-with? x "/")))

(def ^:private node-id? int?)

(s/def ::node-id
  (s/or :internal-id node-id?
        :resource-path resource-path?))

(s/def :ext-module/get-commands lua-fn?)
(s/def :ext-module/on-build-started lua-fn?)
(s/def :ext-module/on-build-successful lua-fn?)
(s/def :ext-module/on-build-failed lua-fn?)
(s/def :ext-module/on-bundle-started lua-fn?)
(s/def :ext-module/on-bundle-successful lua-fn?)
(s/def :ext-module/on-bundle-failed lua-fn?)
(s/def ::module
  (s/keys :opt-un [:ext-module/get-commands
                   :ext-module/on-build-started
                   :ext-module/on-build-successful
                   :ext-module/on-build-failed
                   :ext-module/on-bundle-started
                   :ext-module/on-bundle-successful
                   :ext-module/on-bundle-failed]))

(s/def :ext-action/action #{"set" "shell"})
(s/def :ext-action/node-id ::node-id)
(s/def :ext-action/property #{"text"})
(s/def :ext-action/value any?)
(s/def :ext-action/command (s/coll-of string?))
(defmulti action-spec :action)
(defmethod action-spec "set" [_]
  (s/keys :req-un [:ext-action/action
                   :ext-action/node-id
                   :ext-action/property
                   :ext-action/value]))
(defmethod action-spec "shell" [_]
  (s/keys :req-un [:ext-action/action
                   :ext-action/command]))
(s/def ::action (s/multi-spec action-spec :action))
(s/def ::actions (s/coll-of ::action))

(s/def :ext-command/label string?)
(s/def :ext-command/locations (s/coll-of #{"Edit" "View" "Assets" "Outline"}
                                         :distinct true :min-count 1))
(s/def :ext-command/type #{"resource"})
(s/def :ext-command/cardinality #{"one" "many"})
(s/def :ext-command/selection (s/keys :req-un [:ext-command/type :ext-command/cardinality]))
(s/def :ext-command/query (s/keys :opt-un [:ext-command/selection]))
(s/def :ext-command/active lua-fn?)
(s/def :ext-command/run lua-fn?)
(s/def ::command (s/keys :req-un [:ext-command/label
                                  :ext-command/locations]
                         :opt-un [:ext-command/query
                                  :ext-command/active
                                  :ext-command/run]))
(s/def ::commands (s/coll-of ::command))

(defn- ext-state [project]
  (-> project
      (g/node-value :editor-extensions)
      (g/user-data :state)))

(def ^:private ^:dynamic *execution-context*
  "A map with following keys:
   - `:project-id`
   - `:evaluation-context`
   - `:ui` - instance of UI protocol
   - `:request-sync` - volatile with boolean indicating whether extension
     implicitly asked to perform resource sync, for example by writing to file"
  nil)

(defn- execute-with!
  "Executes function passing it an ext-map on an agent thread, returns a future
  with result of that function execution

  Possible options:
  - `:state` (optional) - extensions state
  - `:evaluation-context` (optional, defaults to a fresh one) - evaluation
    context used by extension
  - `:report-exceptions` (optional, default true) - whether uncaught exceptions
    should be reported to sentry"
  ([project f]
   (execute-with! project {} f))
  ([project options f]
   (let [result-promise (promise)
         state (or (:state options) (ext-state project))]
     (send (:ext-agent state)
           (fn [ext-map]
             (let [evaluation-context (or (:evaluation-context options) (g/make-evaluation-context))]
               (binding [*execution-context* {:project project
                                              :evaluation-context evaluation-context
                                              :ui (:ui state)
                                              :request-sync (volatile! false)}]
                 (let [ret (try
                             [nil (f ext-map)]
                             (catch Throwable e
                               (when (:report-exceptions options)
                                 (error-reporting/report-exception! e))
                               [e nil]))]
                   (when-not (contains? options :evaluation-context)
                     (g/update-cache-from-evaluation-context! evaluation-context))
                   (when @(:request-sync *execution-context*)
                     (reload-resources! (:ui state)))
                   (result-promise ret))))
             ext-map))
     (future (let [[err ret] @result-promise]
               (if err
                 (throw err)
                 ret))))))

(defn- ->lua-string [x]
  (cond
    (keyword? x) (pr-str (string/replace (name x) "-" "_"))
    (map? x) (str "{"
                  (string/join ", " (map #(str "[" (->lua-string (key %)) "] = " (->lua-string (val %)))
                                         x))
                  "}")
    (coll? x) (str "{" (string/join ", " (map ->lua-string x)) "}")
    (lua-fn? x) (str "<function>")
    :else (pr-str x)))

(defn- spec-pred->reason [pred]
  (or (cond
        (symbol? pred)
        (let [unqualified (symbol (name pred))]
          (case unqualified
            (coll? vector?) "is not an array"
            int? "is not an integer"
            string? "is not a string"
            lua-fn? "is not a function"
            action-spec (str "needs \"action\" key to be "
                             (->> (.getMethodTable ^MultiFn action-spec)
                                  keys
                                  sort
                                  (map #(str \" % \"))
                                  (util/join-words ", " " or ")))
            distinct? "should not have repeated elements"
            node-id? "is not a node id"
            resource-path? "is not a resource path"
            nil))

        (set? pred)
        (str "is not " (->> pred
                            sort
                            (map #(str \" % \"))
                            (util/join-words ", " " or ")))

        (coll? pred)
        (cond
          (= '(contains? %) (take 2 pred))
          (str "needs " (->lua-string (last pred)) " key")

          (and (= '<= (first pred)) (number? (second pred)))
          (str "needs at least " (second pred) " element" (when (< 1 (second pred)) "s"))))
      (pr-str pred)))

(defn- error-message [label path ^Throwable ex]
  (str label
       (when path (str " in " path))
       " failed:\n"
       (if-let [problems (::s/problems (ex-data ex))]
         (->> problems
              (map #(str (->lua-string (:val %)) " " (spec-pred->reason (s/abbrev (:pred %)))))
              (string/join "\n"))
         (.getMessage ex))))

(defn- handle-extension-error [options ^Exception ex]
  (let [message (error-message (:label options "Extension") (:path options) ex)
        default (:default options ::re-throw)]
    (display-output! (:ui options) :err message)
    (if (= ::re-throw default)
      (throw ex)
      default)))

(defmacro ^:private try-with-extension-exceptions
  "Execute body with thrown exceptions being reported to user

  Available options:
  - `:ui` (required) - ui used to report exceptions
  - `:path` (optional) - file path to extension related to `body` execution
  - `:label` (optional, default \"Extension\") - description of a thing that may
    throw exception
  - `:default` (optional) - default value that is returned
    from this macro if `body` throws exception, will re-throw exception if this
    option is not provided"
  [options-expr & body]
  `(try
     ~@body
     (catch Throwable e#
       (handle-extension-error ~options-expr e#))))

(defn- ensure-spec [spec x]
  (if (s/valid? spec x)
    x
    (throw (ex-info "Spec assertion failed" (s/explain-data spec x)))))

(defn- execute-all-top-level-functions!
  "Executes all top-level editor script function defined by `fn-keyword`

  Returns a vector of [`path` `ret`] tuples, removing all results that threw
  exception, where:
  - `path` is a file path of editor script
  - `ret` is a clojure data structure returned from function in that file"
  [project state fn-keyword opts]
  @(execute-with! project {:state state}
     (fn [ext-map]
       (let [lua-opts (luart/clj->lua opts)
             label (->lua-string fn-keyword)]
         (into []
               (keep
                 (fn [[file-path ^LuaValue f]]
                   (when-let [ret (try-with-extension-exceptions
                                    {:ui (:ui state)
                                     :path file-path
                                     :label label
                                     :default nil}
                                    (luart/lua->clj (luart/invoke f lua-opts)))]
                     [file-path ret])))
               (get-in ext-map [:all fn-keyword]))))))

(defn- add-all-entry [m path module]
  (reduce-kv
    (fn [acc k v]
      (update acc k (fnil conj []) [path v]))
    m
    module))

(def hooks-file-path "/hooks.editor_script")

(defn- re-create-ext-agent [state env]
  (assoc state
    :ext-agent
    (agent (transduce
             (mapcat state)
             (completing
               (fn [acc x]
                 (cond
                   (instance? LuaError x)
                   (handle-extension-error {:label "Compilation"
                                            :ui (:ui state)
                                            :default acc}
                                           x)

                   (instance? Prototype x)
                   (let [^Prototype proto x
                         proto-path (.tojstring (.-source proto))]
                     (if-let [module (try-with-extension-exceptions
                                       {:label "Loading"
                                        :path proto-path
                                        :default nil
                                        :ui (:ui state)}
                                       (ensure-spec ::module (luart/lua->clj (luart/eval proto env))))]
                       (-> acc
                           (update :all add-all-entry proto-path module)
                           (cond-> (= hooks-file-path proto-path)
                                   (assoc :hooks module)))
                       acc))

                   :else
                   (throw (ex-info (str "Unexpected prototype value: " x) {:prototype x})))))
             {}
             [:library-prototypes :project-prototypes])
           :error-handler (fn [_ ex]
                            (error-reporting/report-exception! ex)))))

(defn- node-id->type-keyword [node-id ec]
  (let [{:keys [basis]} ec]
    (:k (g/node-type basis (g/node-by-id basis node-id)))))

(defmulti ext-get (fn [node-id property ec]
                    [(node-id->type-keyword node-id ec) property]))

(defmethod ext-get :default [node-id property ec]
  (throw (LuaError. (str (name (node-id->type-keyword node-id ec))
                         " has no \""
                         property
                         "\" property"))))

(defmethod ext-get [:editor.code.resource/CodeEditorResourceNode "text"] [node-id _ ec]
  (clojure.string/join \newline (g/node-value node-id :lines ec)))

(defmethod ext-get [:editor.resource/ResourceNode "path"] [node-id _ ec]
  (resource/resource->proj-path (g/node-value node-id :resource ec)))

(defn- node-id-or-path->node-id [node-id-or-path project evaluation-context]
  (if (string? node-id-or-path)
    (project/get-resource-node project node-id-or-path evaluation-context)
    node-id-or-path))

(defn- do-ext-get [node-id-or-path property]
  (ensure-spec ::node-id node-id-or-path)
  (let [{:keys [evaluation-context project]} *execution-context*]
    (ext-get (node-id-or-path->node-id node-id-or-path project evaluation-context)
             property
             evaluation-context)))

(defn- transact! [txs _execution-context]
  (g/transact txs))

(defn- shell! [commands execution-context]
  (let [{:keys [evaluation-context project ui]} execution-context
        root (-> project
                 (g/node-value :workspace evaluation-context)
                 (g/node-value :root evaluation-context))]
    (doseq [cmd+args commands]
      (if (can-execute? ui cmd+args)
        (apply shell/sh (concat cmd+args [:dir root]))
        (throw (ex-info (str "Command \"" (string/join " " cmd+args) "\" aborted") {:cmd cmd+args}))))
    (reload-resources! ui)))

(defmulti transaction-action->txs (fn [action evaluation-context]
                                    [(:action action)
                                     (node-id->type-keyword (:node-id action) evaluation-context)
                                     (:property action)]))

(defmethod transaction-action->txs :default [action evaluation-context]
  (throw (LuaError.
           (format "Can't %s \"%s\" property of %s"
                   (:action action)
                   (:property action)
                   (name (node-id->type-keyword (:node-id action) evaluation-context))))))

(defmethod transaction-action->txs ["set" :editor.code.resource/CodeEditorResourceNode "text"]
  [action _]
  (let [node-id (:node-id action)
        value (:value action)]
    [(g/set-property node-id :modified-lines (code.util/split-lines value))
     (g/update-property node-id :invalidated-rows conj 0)
     (g/set-property node-id :cursor-ranges [#code/range[[0 0] [0 0]]])
     (g/set-property node-id :regions [])]))

(defmulti action->batched-executor+input (fn [action _execution-context]
                                           (:action action)))

(defmethod action->batched-executor+input "set" [action execution-context]
  (let [{:keys [project evaluation-context]} execution-context]
    [transact! (transaction-action->txs
                 (update action :node-id node-id-or-path->node-id project evaluation-context)
                 evaluation-context)]))

(defmethod action->batched-executor+input "shell" [action _]
  [shell! (:command action)])

(defn- perform-actions! [actions execution-context]
  (ensure-spec ::actions actions)
  (doseq [[executor inputs] (eduction (map #(action->batched-executor+input % execution-context))
                                      (partition-by first)
                                      (map (juxt ffirst #(mapv second %)))
                                      actions)]
    (executor inputs execution-context)))

(defn- hook-exception->error [^Throwable ex project label]
  (let [^Throwable root (stacktrace/root-cause ex)
        message (ex-message root)
        [_ file line :as match] (re-find console/line-sub-regions-pattern message)]
    (g/map->error
      (cond-> {:_node-id (or (when match (project/get-resource-node project file))
                             (project/get-resource-node project hooks-file-path))
               :message (error-message label hooks-file-path root)
               :severity :fatal}

              line
              (assoc-in [:user-data :cursor-range]
                        (data/line-number->CursorRange (Integer/parseInt line)))))))

(defn execute-hook!
  "Execute hook defined in this `project`, may throw, returns nil by default

  Available options:
  - `:opts` (optional) - map that will be serialized to lua table and passed to
    lua function hook. **WARNING** all node ids should be wrapped with `reduced`
    so they are passed as userdata to lua, since Lua's doubles and integers lack
    necessary precision
  - `:exception-policy` (optional) - keyword indicating how this function should
    behave when exception is thrown because of an extension code. Can be:
    - `:as-error` - transform exception to error value suitable for graph
    - `:ignore` - return nil
    when no `exception-policy` is provided, will re-throw exception"
  [project hook-keyword options]
  (when-let [state (ext-state project)]
    (let [opts (:opts options ::no-opts)
          ex-label (str "hook " (->lua-string hook-keyword))]
      (try
        @(execute-with! project {:state state :report-exceptions false}
           (fn [ext-map]
             (try-with-extension-exceptions {:label ex-label
                                             :path hooks-file-path
                                             :ui (:ui state)}
               (some-> (get-in ext-map [:hooks hook-keyword])
                       (as-> lua-fn
                             (if (= opts ::no-opts)
                               (luart/lua->clj (luart/invoke lua-fn))
                               (luart/lua->clj (luart/invoke lua-fn (luart/clj->lua opts)))))
                       (perform-actions! *execution-context*)))))
        nil
        (catch Throwable e
          (case (:exception-policy options)
            :as-error (hook-exception->error e project ex-label)
            :ignore nil
            (throw e)))))))

(defn- continue [acc env lua-fn f & args]
  (let [new-lua-fn (fn [env m]
                     (lua-fn env (apply f m args)))]
    ((acc new-lua-fn) env)))

(defmacro gen-query [acc-sym [env-sym cont-sym] & body-expr]
  `(fn [lua-fn#]
     (fn [~env-sym]
       (let [~cont-sym (partial continue ~acc-sym ~env-sym lua-fn#)]
         ~@body-expr))))

(defmulti gen-selection-query (fn [q _acc _project]
                                (:type q)))

(defn- ensure-selection-cardinality [selection q]
  (if (= "one" (:cardinality q))
    (when (= 1 (count selection))
      (first selection))
    selection))

(defn- node-ids->lua-selection [selection q]
  (ensure-selection-cardinality (mapv luart/wrap-user-data selection) q))

(defmethod gen-selection-query "resource" [q acc project]
  (gen-query acc [env cont]
    (let [evaluation-context (or (:evaluation-context env)
                                 (g/make-evaluation-context))
          selection (:selection env)]
      (when-let [res (or (some-> selection
                                 (handler/adapt-every resource/ResourceNode)
                                 (node-ids->lua-selection q))
                         (some-> selection
                                 (handler/adapt-every resource/Resource)
                                 (->> (keep #(project/get-resource-node project % evaluation-context)))
                                 (node-ids->lua-selection q)))]
        (cont assoc :selection res)))))

(defn- compile-query [q project]
  (reduce-kv
    (fn [acc k v]
      (case k
        :selection (gen-selection-query v acc project)
        acc))
    (fn [lua-fn]
      (fn [env]
        (lua-fn env {})))
    q))

(defn- command->dynamic-handler [{:keys [label query active run locations]} path project state]
  (let [lua-fn->env-fn (compile-query query project)
        contexts (into #{}
                       (map {"Assets" :asset-browser
                             "Outline" :outline
                             "Edit" :global
                             "View" :global})
                       locations)
        locations (into #{}
                        (map {"Assets" :editor.asset-browser/context-menu-end
                              "Outline" :editor.outline-view/context-menu-end
                              "Edit" :editor.app-view/edit-end
                              "View" :editor.app-view/view-end})
                        locations)]
    {:context-definition contexts
     :menu-item {:label label}
     :locations locations
     :fns (cond-> {}
                  active
                  (assoc :active?
                         (lua-fn->env-fn
                           (fn [env opts]
                             (-> project
                                 (execute-with!
                                   {:state state
                                    :evaluation-context (:evaluation-context env)}
                                   (fn [_]
                                     (try-with-extension-exceptions {:label (str label "'s \"active\"")
                                                                     :default false
                                                                     :path path
                                                                     :ui (:ui state)}
                                       (luart/lua->clj (luart/invoke active (luart/clj->lua opts))))))
                                 (deref 100 false)))))
                  run
                  (assoc :run
                         (lua-fn->env-fn
                           (fn [_ opts]
                             (execute-with! project {:state state}
                               (fn [_]
                                 (try-with-extension-exceptions {:label (str label "'s \"run\"")
                                                                 :default nil
                                                                 :path path
                                                                 :ui (:ui state)}
                                   (when-let [actions (luart/lua->clj (luart/invoke run (luart/clj->lua opts)))]
                                     (perform-actions! actions *execution-context*)))))))))}))

(defn- reload-commands! [project]
  (let [state (ext-state project)]
    (handler/register-dynamic! ::commands
      (for [[path ret] (execute-all-top-level-functions! project state :get-commands {})
            :let [commands (try-with-extension-exceptions
                             {:label "Reloading commands"
                              :default nil
                              :path path
                              :ui (:ui state)}
                             (ensure-spec ::commands ret))]
            command commands
            :let [dynamic-handler (try-with-extension-exceptions
                                    {:label (:label command "Reloaded command")
                                     :path path
                                     :default nil
                                     :ui (:ui state)}
                                    (command->dynamic-handler command path project state))]
            :when dynamic-handler]
        dynamic-handler))))

(defn- ensure-file-path-in-project-directory
  ^String [^Path project-path ^String file-name]
  (let [normalized-path (-> project-path
                            (.resolve file-name)
                            .normalize)]
    (if (.startsWith normalized-path project-path)
      (.toString normalized-path)
      (throw (LuaError. (str "Can't open "
                             file-name
                             ": outside of project directory"))))))

(defn- open-file [project-path file-name read-mode]
  (let [file-path (ensure-file-path-in-project-directory project-path file-name)]
    (when-not read-mode
      (vreset! (:request-sync *execution-context*) true))
    file-path))

(defn- find-resource [project evaluation-context path]
  (some-> (project/get-resource-node project path evaluation-context)
          (g/node-value :lines evaluation-context)
          (data/lines-input-stream)))

(defn- remove-file [^Path project-path ^String file-name]
  (let [file-path (ensure-file-path-in-project-directory project-path file-name)
        file (File. file-path)]
    (when-not (.exists file)
      (throw (LuaError. (str "No such file or directory: " file-name))))
    (when-not (.delete file)
      (throw (LuaError. (str "Failed to delete " file-name))))
    (vreset! (:request-sync *execution-context*) true)))

(defn reload! [project kind ui]
  (g/with-auto-evaluation-context ec
    (g/user-data-swap!
      (g/node-value project :editor-extensions ec)
      :state
      (fn [state]
        (let [extensions (g/node-value project :editor-extensions ec)
              project-path (-> project
                               (project/workspace ec)
                               (workspace/project-path ec)
                               .toPath
                               .normalize)
              env (luart/make-env
                    :find-resource #(find-resource project ec %)
                    :open-file #(open-file project-path %1 %2)
                    :out #(display-output! ui %1 %2)
                    :globals {"editor" {"get" do-ext-get
                                        "platform" (.getPair (Platform/getHostPlatform))}
                              "package" {"config" (string/join "\n" [File/pathSeparatorChar \; \? \! \-])}
                              "io" {"tmpfile" nil}
                              "os" {"execute" nil
                                    "exit" nil
                                    "remove" #(remove-file project-path %)
                                    "rename" nil
                                    "setlocale" nil
                                    "tmpname" nil}})]
          (-> state
              (assoc :ui ui)
              (cond-> (#{:library :all} kind)
                      (assoc :library-prototypes
                             (g/node-value extensions :library-prototypes ec))

                      (#{:project :all} kind)
                      (assoc :project-prototypes
                             (g/node-value extensions :project-prototypes ec)))
              (re-create-ext-agent env))))))
  (reload-commands! project))
