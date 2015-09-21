(ns user)

;; These two lines initialize JavaFX and OpenGL when running from a pure REPL
(.toString (javax.media.opengl.GLProfile/getDefault))
(defonce force-toolkit-init (javafx.embed.swing.JFXPanel.))

(load-file "dev/tools.clj")
(require 'editor.boot)
