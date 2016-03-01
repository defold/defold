(ns jfx)

;; These two lines initialize JavaFX and OpenGL when running from a pure REPL
;; It also messes up the menus

(.toString (javax.media.opengl.GLProfile/getDefault))
(javafx.embed.swing.JFXPanel.)
