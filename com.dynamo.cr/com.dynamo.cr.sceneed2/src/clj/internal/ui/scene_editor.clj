(ns internal.ui.scene-editor
  (:require [dynamo.editors :as e]
            [internal.ui.gl :refer :all])
  (:import [javax.media.opengl GL2]
           [java.awt Font]))

(deftype SceneEditor [state file]
  e/Editor
  (init [this site file])

  (create-controls [this parent]
    (let [canvas        (glcanvas parent)
          factory       (glfactory)
          _             (.setCurrent canvas)
          context       (.createExternalGLContext factory)
          _             (.makeCurrent context)
          gl            (.. context getGL getGL2)]
      (.glPolygonMode gl GL2/GL_FRONT GL2/GL_FILL)
      (.release context)
      (swap! state assoc
             :context context
             :canvas  canvas
             :small-text-renderer (text-renderer Font/SANS_SERIF Font/BOLD 12))))

  (save [this file monitor])
  (dirty? [this] false)
  (save-as-allowed? [this] false)
  (set-focus [this]))


;    private void render(GL2 gl, GLU glu) {
;        if (!isEnabled()) {
;            return;
;        }
;
;        List<Pass> passes = Arrays.asList(Pass.BACKGROUND, Pass.OUTLINE, Pass.ICON_OUTLINE, Pass.TRANSPARENT, Pass.ICON, Pass.MANIPULATOR, Pass.OVERLAY);
;        renderNodes(gl, glu, passes, null);
;    }

;        if (this.paintRequested || this.canvas == null)
;            return;
;
;        this.paintRequested = true;
;
;        Display.getCurrent().timerExec(15, new Runnable() {
;
;            @Override
;            public void run() {
;                paintRequested = false;
;                if (simulating || screenRecorder != null) {
;                    requestPaint();
;                }
;                paint();
;            }
;        });

;    private void paint() {
;        if (!this.canvas.isDisposed()) {
;            this.canvas.setCurrent();
;            this.context.makeCurrent();
;            GL2 gl = this.context.getGL().getGL2();
;            GLU glu = new GLU();
;            try {
;                gl.glDepthMask(true);
;                gl.glEnable(GL.GL_DEPTH_TEST);
;                gl.glClearColor(0.0f, 0.0f, 0.0f, 1);
;                gl.glClear(GL.GL_COLOR_BUFFER_BIT | GL.GL_DEPTH_BUFFER_BIT);
;                gl.glDisable(GL.GL_DEPTH_TEST);
;                gl.glDepthMask(false);
;
;                gl.glViewport(this.viewPort[0], this.viewPort[1], this.viewPort[2], this.viewPort[3]);
;
;                render(gl, glu);
;
;            } catch (Throwable e) {
;                logger.error("Error occurred while rendering", e);
;            } finally {
;                canvas.swapBuffers();
;                context.release();
;
;                if (screenRecorder != null) {
;                    screenRecorder.captureScreen();
;                }
;            }
;        }
;    }
