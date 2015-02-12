(ns jogl-samples.rawgl2es2
  (:import [javax.media.opengl GL GL2ES2 GLAutoDrawable GLEventListener GLProfile GLCapabilities]
           [com.jogamp.newt.opengl GLWindow]
           [com.jogamp.common.nio Buffers]
           [com.jogamp.opengl.util Animator]
           [java.nio FloatBuffer]))

;; This is not a good example of state handling in a Clojure program
;; it is purely intended to mimic the original Java code for this example.
(def vertex-shader       (atom 0))
(def fragment-shader     (atom 0))
(def shader-program      (atom 0))
(def mvp-matrix-location (atom 0))
(def vbo-handles         (atom nil))
(def t0                  (System/currentTimeMillis))

(defn ^int color-buffer-handle [] (aget ^ints @vbo-handles 0))
(defn ^int vertex-buffer-handle [] (aget ^ints @vbo-handles 1))

(def vertex-shader-string
  ; For GLSL 1 and 1.1 code i highly recomend to not include a
  ; GLSL ES language #version line, GLSL ES section 3.4
  ; Many GPU drivers refuse to compile the shader if #version is different from
  ; the drivers internal GLSL version.
  ;
  ; This demo use GLSL version 1.1 (the implicit version)
	"#version 150

   #if __VERSION__ >= 130
	  #define attribute in
	  #define varying out
	 #endif

	 #ifdef GL_ES
	 precision mediump float;
	 precision mediump int;
	 #endif

	 uniform mat4    uniform_Projection;
	 attribute vec4  attribute_Position;
	 attribute vec4  attribute_Color;

	 varying vec4    varying_Color;

	 void main(void)
	 {
	   varying_Color = attribute_Color;
	   gl_Position = uniform_Projection * attribute_Position;
   }")

(def fragment-shader-string
  "#version 150

   #if __VERSION__ >= 130
     #define varying in
     out vec4 mgl_FragColor;
     #define texture2D texture
     #define gl_FragColor mgl_FragColor
   #endif

   #ifdef GL_ES
     precision mediump float;
     precision mediump int;
   #endif

   varying   vec4    varying_Color;  // incomming varying data to the
                                     // frament shader
                                     // sent from the vertex shader
   void main (void)
   {
     gl_FragColor = varying_Color;
   }")

(defmacro idx [pos u v]
  `(int (+ ~pos ~u (* ~v 4))))

(defn gl-mult-matrix-f
  [^FloatBuffer a ^FloatBuffer b ^FloatBuffer d]
  (let [ap (.position a)
        bp (.position b)
        dp (.position d)]
    (doseq [i (range 4)]
      (let [ai0 (.get a (idx ap i 0))
            ai1 (.get a (idx ap i 1))
            ai2 (.get a (idx ap i 2))
            ai3 (.get a (idx ap i 3))]
        (.put d (idx 0 i 0) (+ (* ai0 (.get b (idx bp 0 0))) (* ai1 (.get b (idx bp 1 0))) (* ai2 (.get b (idx bp 2 0))) (* ai3 (.get b (idx bp 3 0)))))
        (.put d (idx 0 i 1) (+ (* ai0 (.get b (idx bp 0 1))) (* ai1 (.get b (idx bp 1 1))) (* ai2 (.get b (idx bp 2 1))) (* ai3 (.get b (idx bp 3 1)))))
        (.put d (idx 0 i 2) (+ (* ai0 (.get b (idx bp 0 2))) (* ai1 (.get b (idx bp 1 2))) (* ai2 (.get b (idx bp 2 2))) (* ai3 (.get b (idx bp 3 2)))))
        (.put d (idx 0 i 3) (+ (* ai0 (.get b (idx bp 0 3))) (* ai1 (.get b (idx bp 1 3))) (* ai2 (.get b (idx bp 2 3))) (* ai3 (.get b (idx bp 3 3)))))))))

(defn multiply [a b]
  (let [tmp (float-array 16)]
    (gl-mult-matrix-f (FloatBuffer/wrap a) (FloatBuffer/wrap b) (FloatBuffer/wrap tmp))
    tmp))

(defn translate [m x y z]
  (let [tmp (float-array [1.0 0.0 0.0 0.0
                          0.0 1.0 0.0 0.0
                          0.0 0.0 1.0 0.0
                            x   y   z 1.0])]
    (multiply m tmp)))

(defn rotate [m a x y z]
  (let [s     (Math/sin (Math/toRadians a))
        c     (Math/cos (Math/toRadians a))
        one-c (- 1.0 c)
        rot   (float-array [(+ (* x x one-c) c)        (+ (* y x one-c) (* z s))   (- (* x z one-c) (* y s))   0.0
                            (- (* x y one-c) (* z s))  (+ (* y y one-c) c)         (+ (* y z one-c) (* x s))   0.0
                            (+ (* x z one-c) (* y s))  (- (* y z one-c) (* x s))   (+ (* z z one-c) c)         0.0
                            0.0                        0.0                         0.0                         1.0])]
    (multiply m rot)))

(defn identity-matrix []
  (float-array [1 0 0 0
                0 1 0 0
                0 0 1 0
                0 0 0 1]))

(defn make-window []
  (let [profile (GLProfile/get GLProfile/GL2ES2)
        caps    (GLCapabilities. profile)]
    (.setBackgroundOpaque caps false)
    (doto (GLWindow/create caps)
      (.setTitle "Raw GL2ES2 Demo")
      (.setSize 1920 1080)
      (.setUndecorated false)
      (.setPointerVisible true)
      (.setVisible true))))

(defn single-string-array [s] (into-array String [s]))

(defn do-init [^GLAutoDrawable drawable]
  (let [gl (.. drawable getGL getGL2ES2)]
    (println "Chosen GLCapabilities: " (.getChosenGLCapabilities drawable))
    (println "Init GL is: " (.getName (class gl)))
    (println "GL_VENDOR: " (.glGetString gl GL/GL_VENDOR))
    (println "GL_RENDERER: " (.glGetString gl GL/GL_RENDERER))
    (println "GL_VERSION: " (.glGetString gl GL/GL_VERSION))

    (let [vert-shader (.glCreateShader gl GL2ES2/GL_VERTEX_SHADER)]
      (.glShaderSource gl vert-shader 1 (single-string-array vertex-shader-string) (int-array [(count vertex-shader-string)]) 0)
      (.glCompileShader gl vert-shader)
      (let [compiled (int-array 1)]
        (.glGetShaderiv gl vert-shader GL2ES2/GL_COMPILE_STATUS compiled 0)
        (if (not= 0 (aget compiled 0))
          (do
            (println "Hooray! Vertex shader compiled.")
            (reset! vertex-shader vert-shader))
          (let [log-length (int-array 1)]
            (.glGetShaderiv gl vert-shader GL2ES2/GL_INFO_LOG_LENGTH log-length 0)
            (let [log (byte-array (aget log-length 0))]
              (.glGetShaderInfoLog gl vert-shader (aget log-length 0) nil 0 log 0)
              (throw (Exception. (str "Error compiling the vertex shader: " (String. log)))))))))

    (let [frag-shader (.glCreateShader gl GL2ES2/GL_FRAGMENT_SHADER)]
      (.glShaderSource gl frag-shader 1 (single-string-array fragment-shader-string) (int-array [(count fragment-shader-string)]) 0)
      (.glCompileShader gl frag-shader)
      (let [compiled (int-array 1)]
        (.glGetShaderiv gl frag-shader GL2ES2/GL_COMPILE_STATUS compiled 0)
        (if (not= 0 (aget compiled 0))
          (do
            (println "Hooray! Fragment shader compiled.")
            (reset! fragment-shader frag-shader))
          (let [log-length (int-array 1)]
            (.glGetShaderiv gl frag-shader GL2ES2/GL_INFO_LOG_LENGTH log-length 0)
            (let [log (byte-array (aget log-length 0))]
              (.glGetShaderInfoLog gl frag-shader (aget log-length 0) nil 0 log 0)
              (throw (Exception. (str "Error compiling the fragment shader: " (String. log)))))))))

    (let [progn (.glCreateProgram gl)]
      (.glAttachShader gl progn @vertex-shader)
      (.glAttachShader gl progn @fragment-shader)

      (.glBindAttribLocation gl progn 0 "attribute_Position")
      (.glBindAttribLocation gl progn 1 "attribute_Color")

      (.glLinkProgram gl progn)

      (reset! mvp-matrix-location (.glGetUniformLocation gl progn "uniform_Projection"))
      (reset! shader-program progn))

    (let [vbo (int-array 2)]
      (.glGenBuffers gl 2 vbo 0)
      (reset! vbo-handles vbo)))

  (println :fragment-shader @fragment-shader)
  (println :vertex-shader @vertex-shader)
  (println :shader-program @shader-program)
  (println :vbo-handles (seq @vbo-handles)))


(defn do-reshape [^GLAutoDrawable drawable x y w h]
  (let [gl (.. drawable getGL getGL2ES2)]
    (.glViewport gl (/ (- w h) 2) 0 h h)))

(defn do-dispose [^GLAutoDrawable drawable]
  (println "Cleanup, remember to release shaders")
  (let [gl (.. drawable getGL getGL2ES2)]
    (.glUseProgram gl 0)
    (.glDeleteBuffers gl 2 @vbo-handles 0)
    (reset! vbo-handles 0)
    (.glDetachShader gl @shader-program @vertex-shader)
    (.glDeleteShader gl @vertex-shader)
    (.glDetachShader gl @shader-program @fragment-shader)
    (.glDeleteShader gl @fragment-shader)
    (.glDeleteProgram gl @shader-program)
    (reset! shader-program 0)
    (reset! vertex-shader 0)
    (reset! fragment-shader 0)))

(defn do-display [^GLAutoDrawable drawable]
  (let [gl    (.. drawable getGL getGL2ES2)
        t1    (System/currentTimeMillis)
        theta (* 0.005 (- t1 t0))
        s     (Math/sin theta)]

    (.glClearColor gl 1 0 1 0.5)
    (.glClear gl (bit-or GL/GL_STENCIL_BUFFER_BIT GL/GL_COLOR_BUFFER_BIT GL/GL_DEPTH_BUFFER_BIT))

    (.glUseProgram gl @shader-program)

    (let [i   (identity-matrix)
          mvp (translate i 0 0 -0.1)
          mvp (rotate    mvp   (* 30 s) 1 0 1)]
      (.glUniformMatrix4fv gl @mvp-matrix-location 1 false mvp 0))

    (let [vertices (float-array [ 0  1 0
                                 -1 -1 0
                                  1 -1 0])
          fb-vertices (Buffers/newDirectFloatBuffer vertices)]
      (.glBindBuffer gl GL/GL_ARRAY_BUFFER (vertex-buffer-handle))
      (.glBufferData gl GL/GL_ARRAY_BUFFER (* (count vertices) 4) fb-vertices GL/GL_STATIC_DRAW)
      (.glVertexAttribPointer gl 0 3 GL/GL_FLOAT false 0 0)
      (.glEnableVertexAttribArray gl 0))

    (let [colors (float-array [ 1 0 0 1
                                0 0 0 1
                                1 1 0 0.9])
          fb-colors (Buffers/newDirectFloatBuffer colors)]
      (.glBindBuffer gl GL/GL_ARRAY_BUFFER (color-buffer-handle))
      (.glBufferData gl GL/GL_ARRAY_BUFFER (* (count colors) 4) fb-colors GL/GL_STATIC_DRAW)
      (.glVertexAttribPointer gl 1 4 GL/GL_FLOAT false 0 0)
      (.glEnableVertexAttribArray gl 1))

    (.glDrawArrays gl GL/GL_TRIANGLES 0 3)
    (.glDisableVertexAttribArray gl 0)
    (.glDisableVertexAttribArray gl 1)))

(defn attach-listener! [^GLWindow window]
  (.addGLEventListener window
    (reify GLEventListener
      (^void init [this ^GLAutoDrawable drawable]
        (do-init drawable))

      (^void reshape [this ^GLAutoDrawable drawable ^int x ^int y ^int w ^int h]
        (do-reshape drawable x y w h))

      (^void display [this ^GLAutoDrawable drawable]
        (do-display drawable))

      (^void dispose [this ^GLAutoDrawable drawable]
        (do-dispose drawable)))))

(defn main []
  (let [window (make-window)]
    (attach-listener! window)
    (doto (Animator.)
      (.add window)
      (.start))))
