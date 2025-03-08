
class GLCanvas
{
    constructor(width, height)
    {
        this.width = width;
        this.height = height;

        // Create a WebGL 2 canvas without premultiplied alpha
        this.glCanvas = document.createElement("canvas");
        this.glCanvas.width = width;
        this.glCanvas.height = height;
        this.gl = this.glCanvas.getContext("webgl2", { premultipliedAlpha: false, antialias: false });

        // Overlay the canvas on top of everything and make sure mouse events click-through
        this.glCanvas.style.position = "fixed";
        this.glCanvas.style.pointerEvents = "none";
        this.glCanvas.style.zIndex = 1000;
        document.body.appendChild(this.glCanvas);

        // Hook up resize event handler
        DOM.Event.AddHandler(window, "resize", () => this.OnResizeWindow());
        this.OnResizeWindow();

        // Compile needed shaders
        this.timelineProgram = glCreateProgramFromSource(this.gl, "TimelineVShader", TimelineVShader, "TimelineFShader", TimelineFShader);
        this.timelineHighlightProgram = glCreateProgramFromSource(this.gl, "TimelineHighlightVShader", TimelineHighlightVShader, "TimelineHighlightFShader", TimelineHighlightFShader);
        this.timelineGpuToCpuProgram = glCreateProgramFromSource(this.gl, "TimelineGpuToCpuVShader", TimelineGpuToCpuVShader, "TimelineGpuToCpuFShader", TimelineGpuToCpuFShader);
        this.timelineBackgroundProgram = glCreateProgramFromSource(this.gl, "TimelineBackgroundVShader", TimelineBackgroundVShader, "TimelineBackgroundFShader", TimelineBackgroundFShader);
        this.gridProgram = glCreateProgramFromSource(this.gl, "GridVShader", GridVShader, "GridFShader", GridFShader);
        this.gridNumberProgram = glCreateProgramFromSource(this.gl, "GridNumberVShader", GridNumberVShader, "GridNumberFShader", GridNumberFShader);
        this.windowProgram = glCreateProgramFromSource(this.gl, "WindowVShader", WindowVShader, "WindowFShader", WindowFShader);

		// Create the shader font resources
        this.font = new glFont(this.gl);
		this.textBuffer = new glTextBuffer(this.gl, this.font);
        this.nameMap = new NameMap(this.textBuffer);

        // Kick off the rendering refresh
        this.OnDrawHandler = null;
        this.Draw(performance.now());
    }

    SetOnDraw(handler)
    {
        this.OnDrawHandler = handler;
    }

    ClearTextResources()
    {
        this.nameMap = new NameMap(this.textBuffer);
    }

    OnResizeWindow()
    {
        // Resize to match the window
        this.width = window.innerWidth;
        this.height = window.innerHeight;
        this.glCanvas.width = window.innerWidth;
        this.glCanvas.height = window.innerHeight;
    }

    SetFontUniforms(program)
    {
		// Font texture may not be loaded yet
		if (this.font.atlasTexture != null)
		{
            const gl = this.gl;
			glSetUniform(gl, program, "inFontAtlasTexture", this.font.atlasTexture, 0);
            glSetUniform(gl, program, "inTextBufferDesc.fontWidth", this.font.fontWidth);
            glSetUniform(gl, program, "inTextBufferDesc.fontHeight", this.font.fontHeight);
        }
    }

    SetTextUniforms(program)
    {
        const gl = this.gl;
        this.SetFontUniforms(program);
        this.textBuffer.SetAsUniform(gl, program, "inTextBuffer", 1);
    }

    SetContainerUniforms(program, container)
    {
        const gl = this.gl;
        const container_rect = container.getBoundingClientRect();
        glSetUniform(gl, program, "inContainer.x0", container_rect.left);
        glSetUniform(gl, program, "inContainer.y0", container_rect.top);
        glSetUniform(gl, program, "inContainer.x1", container_rect.left + container_rect.width);
        glSetUniform(gl, program, "inContainer.y1", container_rect.top + container_rect.height);
    }

    EnableBlendPremulAlpha()
    {
        const gl = this.gl;
        gl.enable(gl.BLEND);
        gl.blendFunc(gl.ONE, gl.ONE_MINUS_SRC_ALPHA);
    }

    DisableBlend()
    {
        const gl = this.gl;
        gl.disable(gl.BLEND);
    }

    Draw(timestamp)
    {
        // Setup the viewport and clear the screen
        const gl = this.gl;
        gl.viewport(0, 0, gl.canvas.width, gl.canvas.height);
        gl.clearColor(0, 0, 0, 0);
        gl.clear(gl.COLOR_BUFFER_BIT);

        // Chain to the Draw handler
        const seconds = timestamp / 1000.0;
        if (this.OnDrawHandler != null)
        {
            this.OnDrawHandler(gl, seconds);
        }

        // Reschedule
        window.requestAnimationFrame((timestamp) => this.Draw(timestamp));
    }
};
