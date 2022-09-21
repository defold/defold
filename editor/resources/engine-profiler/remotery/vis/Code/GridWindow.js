
class GridConfigSamples
{
    constructor()
    {
        this.nbFloatsPerSample = g_nbFloatsPerSample;

        this.columns = [];
        this.columns.push(new GridColumn("Sample Name", 196));
        this.columns.push(new GridColumn("Time (ms)", 56, g_sampleOffsetFloats_Length, 4));
        this.columns.push(new GridColumn("Self (ms)", 56, g_sampleOffsetFloats_Self, 4));
        this.columns.push(new GridColumn("Calls", 34, g_sampleOffsetFloats_Calls, 0));
        this.columns.push(new GridColumn("Depth", 34, g_sampleOffsetFloats_Recurse, 0));
    }
}

class GridConfigProperties
{
    constructor()
    {
        this.nbFloatsPerSample = 10;

        this.columns = [];
        this.columns.push(new GridColumn("Property Name", 196));
        this.columns.push(new GridColumn("Value", 90, 4, 4));
        this.columns.push(new GridColumn("Prev Value", 90, 6, 4));
    }
}

class GridColumn
{
    static ColumnTemplate = `<div class="GridNameHeader"></div>`;

    constructor(name, width, number_offset, nb_float_chars)
    {
        // Description
        this.name = name;
        this.width = width;
        this.numberOffset = number_offset;
        this.nbFloatChars = nb_float_chars;

        // Constants
        this.rowHeight = 15;
    }

    Attach(parent_node)
    {
        // Generate HTML for the header and parent it
        const column = DOM.Node.CreateHTML(GridColumn.ColumnTemplate);
        column.innerHTML = this.name;
        column.style.width = (this.width - 4) + "px";
        this.headerNode = parent_node.appendChild(column);
    }

    Draw(gl_canvas, x, buffer, scroll_pos, clip, nb_entries, nb_floats_per_sample)
    {
        // If a number offset in the data stream is provided, we're rendering numbers and not names
        if (this.numberOffset !== undefined)
        {
            this._DrawNumbers(gl_canvas, x, buffer, scroll_pos, clip, nb_entries, nb_floats_per_sample);
        }
        else
        {
            this._DrawNames(gl_canvas, x, buffer, scroll_pos, clip, nb_entries, nb_floats_per_sample);
        }
    }

    _DrawNames(gl_canvas, x, buffer, scroll_pos, clip, nb_entries, nb_floats_per_sample)
    {
        const gl = gl_canvas.gl;
        const program = gl_canvas.gridProgram;

        gl.useProgram(program);
        gl_canvas.SetTextUniforms(program);

        this._DrawAny(gl, program, x, buffer, scroll_pos, clip, nb_entries, nb_floats_per_sample);
    }

    _DrawNumbers(gl_canvas, x, buffer, scroll_pos, clip, nb_entries, nb_floats_per_sample)
    {
        const gl = gl_canvas.gl;
        const program = gl_canvas.gridNumberProgram;

        gl.useProgram(program);
        gl_canvas.SetFontUniforms(program);
        glSetUniform(gl, program, "inNumberOffset", this.numberOffset);
        glSetUniform(gl, program, "inNbFloatChars", this.nbFloatChars);

        this._DrawAny(gl, program, x, buffer, scroll_pos, clip, nb_entries, nb_floats_per_sample);
    }

    _DrawAny(gl, program, x, buffer, scroll_pos, clip, nb_entries, nb_floats_per_sample)
    {
        const clip_min_x = clip[0];
        const clip_min_y = clip[1];
        const clip_max_x = clip[2];
        const clip_max_y = clip[3];

        // Scrolled position of the grid
        const pos_x = clip_min_x + scroll_pos[0] + x;
        const pos_y = clip_min_y + scroll_pos[1];

        // Clip column to the window
        const min_x = Math.min(Math.max(clip_min_x, pos_x), clip_max_x);
        const min_y = Math.min(Math.max(clip_min_y, pos_y), clip_max_y);
        const max_x = Math.max(Math.min(clip_max_x, pos_x + this.width), clip_min_x);
        const max_y = Math.max(Math.min(clip_max_y, pos_y + nb_entries * this.rowHeight), clip_min_y);

        // Don't render if outside the bounds of the main window
        if (min_x > gl.canvas.width || max_x < 0 || min_y > gl.canvas.height || max_y < 0)
        {
            return;
        }
        
        const pixel_offset_x = Math.max(min_x - pos_x, 0);
        const pixel_offset_y = Math.max(min_y - pos_y, 0);

        // Viewport constants
        glSetUniform(gl, program, "inViewport.width", gl.canvas.width);
        glSetUniform(gl, program, "inViewport.height", gl.canvas.height);

        // Grid constants
        glSetUniform(gl, program, "inGrid.minX", min_x);
        glSetUniform(gl, program, "inGrid.minY", min_y);
        glSetUniform(gl, program, "inGrid.maxX", max_x);
        glSetUniform(gl, program, "inGrid.maxY", max_y);
        glSetUniform(gl, program, "inGrid.pixelOffsetX", pixel_offset_x);
        glSetUniform(gl, program, "inGrid.pixelOffsetY", pixel_offset_y);

        // Source data set buffers
        glSetUniform(gl, program, "inSamples", buffer.texture, 2);
        glSetUniform(gl, program, "inSamplesLength", buffer.nbEntries);
        glSetUniform(gl, program, "inFloatsPerSample", nb_floats_per_sample);
        glSetUniform(gl, program, "inNbSamples", buffer.nbEntries / nb_floats_per_sample);

        gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
    }
}

class GridWindow
{
    static GridTemplate = `
        <div style="height:100%; overflow: hidden; position: relative; display:flex; flex-flow: column;">
            <div style="height: 16px; flex: 0 1 auto;"></div>
            <div style="flex: 1 1 auto;"></div>
        </div>
    `;

    constructor(wm, name, offset, gl_canvas, config)
    {
        this.nbEntries = 0;
        this.scrollPos = [ 0, 0 ];

        // Window setup
        this.xPos = 10 + offset * 410;
        this.window = wm.AddWindow(name, 100, 100, 100, 100, null, this);
        this.window.ShowNoAnim();
        this.visible = true;

        // Cache how much internal padding the window has, for clipping
        const style = getComputedStyle(this.window.BodyNode);
        this.bodyPadding = parseFloat(style.padding);

        // Create the Grid host HTML
        const grid_node = DOM.Node.CreateHTML(GridWindow.GridTemplate);
        this.gridNode = this.window.BodyNode.appendChild(grid_node);
        this.headerNode = this.gridNode.children[0];
        this.contentNode = this.gridNode.children[1];

        // Build column data
        this.nbFloatsPerSample = config.nbFloatsPerSample;
        this.columns = config.columns;
        for (let column of this.columns)
        {
            column.Attach(this.headerNode);
        }
        this._PositionHeaders();

        // Header nodes have 1 pixel borders so the first column is required to have a width 1 less than everything else
        // To counter that, shift the first header node one to the left (it will clip) so that it can have its full width
        this.columns[0].headerNode.style.marginLeft = "-1px";

        // Setup for pan/wheel scrolling
        this.mouseInteraction = new MouseInteraction(this.window.BodyNode);
        this.mouseInteraction.onMoveHandler = (mouse_state, mx, my) => this._OnMouseMove(mouse_state, mx, my);
        this.mouseInteraction.onScrollHandler = (mouse_state) => this._OnMouseScroll(mouse_state);

        const gl = gl_canvas.gl;
        this.glCanvas = gl_canvas;
        this.sampleBuffer = new glDynamicBuffer(gl, glDynamicBufferType.Texture, gl.FLOAT, 1);
    }

    Close()
    {
        this.window.Close();
    }

    static AnimatedMove(self, top_window, bottom_window, val)
    {
        self.xPos = val;
        self.WindowResized(top_window, bottom_window);
    }

    SetXPos(xpos, top_window, bottom_window)
    {
        Anim.Animate(
            Bind(GridWindow.AnimatedMove, this, top_window, bottom_window),
            this.xPos, 10 + xpos * 410, 0.25);
    }

    SetVisible(visible)
    {
        if (visible != this.visible)
        {
            if (visible == true)
                this.window.ShowNoAnim();
            else
                this.window.HideNoAnim();

            this.visible = visible;
        }
    }

    WindowResized(top_window, bottom_window)
    {
        const top = top_window.Position[1] + top_window.Size[1] + 10;
        this.window.SetPosition(this.xPos, top_window.Position[1] + top_window.Size[1] + 10);
        this.window.SetSize(400, bottom_window.Position[1] - 10 - top);
    }

    UpdateEntries(nb_entries, samples)
    {
        // This tracks the latest actual entry count
        this.nbEntries = nb_entries;

        // Resize buffers to match any new entry count
        if (nb_entries * this.nbFloatsPerSample > this.sampleBuffer.nbEntries)
        {
            this.sampleBuffer.ResizeToFitNextPow2(nb_entries * this.nbFloatsPerSample);
        }

        // Copy and upload the entry data
        this.sampleBuffer.cpuArray.set(samples);
        this.sampleBuffer.UploadData();
    }

    Draw()
    {
        // Establish content node clipping rectangle
        const rect = this.contentNode.getBoundingClientRect();
        const clip = [
            rect.left,
            rect.top,
            rect.left + rect.width,
            rect.top + rect.height,
        ];

        // Draw columns, left-to-right
        let x = 0;
        for (let column of this.columns)
        {
            column.Draw(this.glCanvas, x, this.sampleBuffer, this.scrollPos, clip, this.nbEntries, this.nbFloatsPerSample);
            x += column.width + 1;
        }
    }

    _PositionHeaders()
    {
        let x = this.scrollPos[0];
        for (let i in this.columns)
        {
            const column = this.columns[i];
            column.headerNode.style.left = x + "px";
            x += column.width;
            x += (i >= 1) ? 1 : 0;
        }
   }

    _OnMouseMove(mouse_state, mouse_offset_x, mouse_offset_y)
    {
        this.scrollPos[0] = Math.min(0, this.scrollPos[0] + mouse_offset_x);
        this.scrollPos[1] = Math.min(0, this.scrollPos[1] + mouse_offset_y);

        this._PositionHeaders();
    }

    _OnMouseScroll(mouse_state)
    {
        this.scrollPos[1] = Math.min(0, this.scrollPos[1] + mouse_state.WheelDelta * 15);
    }
}