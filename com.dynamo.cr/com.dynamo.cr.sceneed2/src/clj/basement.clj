(ns basement)

(comment
  (defn lookup [event]
    (println "default variable" (.getDefaultVariable (.getApplicationContext event)))
    (println (active-editor (.getApplicationContext event)))
    println)

  (defn say-hello [^ExecutionEvent event] (println "Output here."))

  (defcommand a-command "com.dynamo.cr.clojure-eclipse" "com.dynamo.cr.clojure-eclipse.commands.hello" "Speak!")
  (defhandler hello a-command #'say-hello)


  ;; This file explores alternate syntax for the "above the waterline" code.

  (on :resize
        (let [editor      (:editor self)
              canvas      (:canvas @(:state editor))
              camera-node (p/resource-feeding-into project-state self :camera)
              client      (.getClientArea canvas)
              viewport    (t/->Region 0 (.width client) 0 (.height client))
              aspect      (/ (double (.width client)) (.height client))
              camera      (:camera camera-node)
              new-camera  (-> camera
                            (c/set-orthographic (:fov camera)
                                              aspect
                                              -100000
                                              100000)
                            (assoc :viewport viewport))]
          (set-property camera-node :camera new-camera)
          (ui/request-repaint editor)))

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


  ;; another approach to weaving together some simples into compounds
  ;; this would go into an API file
  (defn project-path [pathname] (f/project-path (e/current-project) pathname))
  (defn load-shader  [path]     (shader/make-shader *gl* path))

  ;; this would go into a node file (or other visible file)
  (defnk produce-shader :- s/Int
    [this gl]
    (load-shader (project-file "/builtins/tools/atlas/pos_uv")))


  )

