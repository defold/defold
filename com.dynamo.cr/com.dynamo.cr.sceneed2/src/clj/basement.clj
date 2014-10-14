(ns basement)

(comment


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

  (require '[dynamo.ui :refer :all])
  (require 'internal.ui.editors)
  (require 'dynamo.project)
  (require '[dynamo.node :refer [defnode]])
  (import org.eclipse.swt.widgets.Text)

  (defnode Labeled
    (on :focus
        (println "FOCUS"))
    (on :destroy
        (println "DESTROY"))
    (on :create
        (let [l (doto (Text. (:parent event) 0) (.setText "Hello there"))]
          (set-property self :label l))))

  (defn add-labeled-part [prj]
    (let [tx-r (dynamo.project/transact prj (dynamo.project/new-node (make-labeled :_id -1)))
          labeled (dynamo.project/node-by-id prj (dynamo.project/resolve-tempid tx-r -1))]
      (swt-safe (internal.ui.editors/open-part labeled))))

  ;; before this works, you must open "dev/user.clj" and load it into a REPL

  (add-labeled-part (user/current-project))
  )
