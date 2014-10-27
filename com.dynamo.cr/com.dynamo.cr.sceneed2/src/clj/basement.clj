(ns basement)

(comment

  ;; review all instances of @world-ref (e.g., new-transaction-context)
  ;; should deref once to ensure atomicity

  ;; node events
  ;; selection
  :select
  :deselect

  ;; lifecycle
  :create
  :init
  :post-create
  :pre-destroy
  :destroy

  ;; tree manipulation
  :attach-to-parent
  :post-attach-to-parent
  :attach-child
  :post-attach-child
  :detach-child
  :post-detach-child
  :detach-from-parent
  :post-detach-from-parent

  ;; value changes
  :property-change
  :cache-invalidate

  ;; file interaction
  :load-source
  :save-source

  ;; clipboard (no need for specific copy or begin-drag ops, because everything is a value.)
  :can-paste?
  :accept-paste
  :can-drop?
  :accept-drop

  (defn make-project
    [eclipse-project branch tx-report-chan]
    (let [msgbus (a/chan 100)
          pubch  (a/pub msgbus :node-id (fn [_] (a/dropping-buffer 100)))]
      {:handlers        {:loaders {"clj" on-load-code}}
       :graph           (dg/empty-graph)
       :cache-keys      {}
       :cache           (make-cache)
       :tx-report-chan  tx-report-chan
       :eclipse-project eclipse-project
       :branch          branch
       :publish-to      msgbus
       :subscribe-to    pubch}))


  (require '[dynamo.ui :refer :all])
  (require 'internal.ui.editors)
  (require '[internal.query :as iq])
  (require '[dynamo.project :as p])
  (require '[dynamo.system :as ds])
  (require '[dynamo.node :as n])
  (import org.eclipse.swt.widgets.Text)

  (n/defnode Labeled
    (on :focus
        (println "FOCUS"))
    (on :destroy
        (println "DESTROY"))
    (on :create
        (let [l (doto (Text. (:parent event) 0) (.setText "Hello there"))]
          (set-property self :label l))))

  (defn add-labeled-part [world-ref]
    (let [tx-r (ds/transact world-ref (ds/new-node (make-labeled :_id -1)))
          labeled (iq/node-by-id world-ref (ds/resolve-tempid tx-r -1))]
      (swt-safe (internal.ui.editors/open-part labeled))))

  ;; before this works, you must open "dev/user.clj" and load it into a REPL

  (add-labeled-part (user/the-world))

  (defrecord ProjectSubsystem [project-state tx-report-queue]
    component/Lifecycle
    (start [this] this)

    (stop [this]
      (if project-state
        (close-project this)
        this))

    ProjectLifecycle
    (open-project [this eclipse-project branch]
      (when project-state
        (close-project this))
      (let [project-state (ref (p/make-project eclipse-project branch tx-report-queue))]
        (e/with-project project-state
          (doseq [source (filter clojure-source? (resource-seq eclipse-project))]
            (p/load-resource project-state (file/project-path project-state source)))
          (assoc this :project-state project-state))))

    (close-project [this]
      (when project-state
        (e/with-project project-state
          (p/dispose-project project-state))
        (dissoc this :project-state))))

  )
