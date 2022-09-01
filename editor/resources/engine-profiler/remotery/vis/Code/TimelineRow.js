

TimelineRow = (function()
{
    const RowLabelTemplate = `
        <div class='TimelineRow'>
            <div class='TimelineRowCheck TimelineBox'>
                <input class='TimelineRowCheckbox' type='checkbox' />
            </div>
            <div class='TimelineRowExpand TimelineBox NoSelect'>
                <div class='TimelineRowExpandButton'>+</div>
            </div>
            <div class='TimelineRowExpand TimelineBox NoSelect'>
                <div class='TimelineRowExpandButton'>-</div>
            </div>
            <div class='TimelineRowLabel TimelineBox'></div>
            <div style="clear:left"></div>
        </div>`


    var SAMPLE_HEIGHT = 16;
    var SAMPLE_BORDER = 2;
    var SAMPLE_Y_SPACING = SAMPLE_HEIGHT + SAMPLE_BORDER * 2;


    function TimelineRow(gl, name, timeline, frame_history, check_handler)
    {
        this.Name = name;
        this.timeline = timeline;

        // Create the row HTML and add to the parent
        this.LabelContainerNode = DOM.Node.CreateHTML(RowLabelTemplate);
        const label_node = DOM.Node.FindWithClass(this.LabelContainerNode, "TimelineRowLabel");
        label_node.innerHTML = name;
        timeline.TimelineLabels.Node.appendChild(this.LabelContainerNode);

        // All sample view windows visible by default
        const checkbox_node = DOM.Node.FindWithClass(this.LabelContainerNode, "TimelineRowCheckbox");
        checkbox_node.checked = true;
        checkbox_node.addEventListener("change", (e) => check_handler(name, e));

        // Manually hook-up events to simulate div:active
        // I can't get the equivalent CSS to work in Firefox, so...
        const expand_node_0 = DOM.Node.FindWithClass(this.LabelContainerNode, "TimelineRowExpand", 0);
        const expand_node_1 = DOM.Node.FindWithClass(this.LabelContainerNode, "TimelineRowExpand", 1);
        const inc_node = DOM.Node.FindWithClass(expand_node_0, "TimelineRowExpandButton");
        const dec_node = DOM.Node.FindWithClass(expand_node_1, "TimelineRowExpandButton");
        inc_node.addEventListener("mousedown", ExpandButtonDown);
        inc_node.addEventListener("mouseup", ExpandButtonUp);
        inc_node.addEventListener("mouseleave", ExpandButtonUp);
        dec_node.addEventListener("mousedown", ExpandButtonDown);
        dec_node.addEventListener("mouseup", ExpandButtonUp);
        dec_node.addEventListener("mouseleave", ExpandButtonUp);

        // Pressing +/i increases/decreases depth
        inc_node.addEventListener("click", () => this.IncDepth());
        dec_node.addEventListener("click", () => this.DecDepth());

        // Frame index to start at when looking for first visible sample
        this.StartFrameIndex = 0;

        this.FrameHistory = frame_history;
        this.VisibleFrames = [ ];
        this.VisibleTimeRange = null;
        this.Depth = 1;

        // Currently selected sample
        this.SelectSampleInfo = null;

        // Create WebGL sample buffers
        this.sampleBuffer = new glDynamicBuffer(gl, glDynamicBufferType.Buffer, gl.FLOAT, 4);
        this.colourBuffer = new glDynamicBuffer(gl, glDynamicBufferType.Buffer, gl.UNSIGNED_BYTE, 4);

        // An initial SetSize call to restore containers to their original size after traces were loaded prior to this
        this.SetSize();
    }


    TimelineRow.prototype.SetSize = function()
    {
        this.LabelContainerNode.style.height = SAMPLE_Y_SPACING * this.Depth;
    }


    TimelineRow.prototype.SetVisibleFrames = function(time_range)
    {
        // Clear previous visible list
        this.VisibleFrames = [ ];
        if (this.FrameHistory.length == 0)
            return;

        // Store a copy of the visible time range rather than referencing it
        // This prevents external modifications to the time range from affecting rendering/selection
        time_range = time_range.Clone();
        this.VisibleTimeRange = time_range;

        // The frame history can be reset outside this class
        // This also catches the overflow to the end of the frame list below when a thread stops sending samples
        var max_frame = Math.max(this.FrameHistory.length - 1, 0);
        var start_frame_index = Math.min(this.StartFrameIndex, max_frame);

        // First do a back-track in case the time range moves negatively
        while (start_frame_index > 0)
        {
            var frame = this.FrameHistory[start_frame_index];
            if (time_range.Start_us > frame.StartTime_us)
                break;
            start_frame_index--;
        }

        // Then search from this point for the first visible frame
        while (start_frame_index < this.FrameHistory.length)
        {
            var frame = this.FrameHistory[start_frame_index];
            if (frame.EndTime_us > time_range.Start_us)
                break;
            start_frame_index++;
        }

        // Gather all frames up to the end point
        this.StartFrameIndex = start_frame_index;
        for (var i = start_frame_index; i < this.FrameHistory.length; i++)
        {
            var frame = this.FrameHistory[i];
            if (frame.StartTime_us > time_range.End_us)
                break;
            this.VisibleFrames.push(frame);
        }
    }


    TimelineRow.prototype.DrawSampleHighlight = function(gl_canvas, container, frame, offset, depth, selected)
    {
        if (depth <= this.Depth)
        {
            const gl = gl_canvas.gl;
            const program = gl_canvas.timelineHighlightProgram;

            gl_canvas.SetContainerUniforms(program, container);
    
            // Set row parameters
            const row_rect = this.LabelContainerNode.getBoundingClientRect();
            glSetUniform(gl, program, "inRow.yOffset", row_rect.top);
    
            // Set sample parameters
            const float_offset = offset / 4;
            glSetUniform(gl, program, "inStartMs", frame.sampleFloats[float_offset + g_sampleOffsetFloats_Start]);
            glSetUniform(gl, program, "inLengthMs", frame.sampleFloats[float_offset + g_sampleOffsetFloats_Length]);
            glSetUniform(gl, program, "inDepth", depth);

            // Set colour
            glSetUniform(gl, program, "inColourR", 1.0);
            glSetUniform(gl, program, "inColourG", selected ? 0.0 : 1.0);
            glSetUniform(gl, program, "inColourB", selected ? 0.0 : 1.0);

            gl_canvas.EnableBlendPremulAlpha();
            gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
            gl_canvas.DisableBlend();
        }
    }


    TimelineRow.prototype.DrawSampleGpuToCpu = function(gl_canvas, container, frame, offset, depth)
    {
        // Is this a GPU sample?
        const float_offset = offset / 4;
        const start_ms = frame.sampleFloats[float_offset + g_sampleOffsetFloats_GpuToCpu];
        if (start_ms > 0)
        {
            const gl = gl_canvas.gl;
            const program = gl_canvas.timelineGpuToCpuProgram;

            gl_canvas.SetContainerUniforms(program, container);

            // Set row parameters
            const row_rect = this.LabelContainerNode.getBoundingClientRect();
            glSetUniform(gl, program, "inRow.yOffset", row_rect.top);

            // Set sample parameters
            const length_ms = frame.sampleFloats[float_offset + g_sampleOffsetFloats_Start] - start_ms;
            glSetUniform(gl, program, "inStartMs", start_ms);
            glSetUniform(gl, program, "inLengthMs", length_ms);
            glSetUniform(gl, program, "inDepth", depth);

            gl_canvas.EnableBlendPremulAlpha();
            gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
            gl_canvas.DisableBlend();
        }
    }


    TimelineRow.prototype.DisplayHeight = function()
    {
        return this.LabelContainerNode.clientHeight;
    }


    TimelineRow.prototype.YOffset = function()
    {
        return this.LabelContainerNode.offsetTop;
    }


    function GatherSamples(self, frame, samples_per_depth)
    {
        const sample_data_view = frame.sampleDataView;

        for (let offset = 0; offset < sample_data_view.byteLength; offset += g_nbBytesPerSample)
        {
            depth = sample_data_view.getUint8(offset + g_sampleOffsetBytes_Depth) + 1;
            if (depth > self.Depth)
            {
                continue;
            }

            // Ensure there's enough entries for each depth
            while (depth >= samples_per_depth.length)
            {
                samples_per_depth.push([]);
            }

            let samples_this_depth = samples_per_depth[depth];
            samples_this_depth.push([frame, offset]);
        }
    }


    TimelineRow.prototype.Draw = function(gl_canvas, container)
    {
        let samples_per_depth = [];

        // Gather all sample data in the visible frame set
        for (var i in this.VisibleFrames)
        {
            var frame = this.VisibleFrames[i];
            GatherSamples(this, frame, samples_per_depth);
        }

        // Count number of samples required
        let nb_samples = 0;
        for (const samples_this_depth of samples_per_depth)
        {
            nb_samples += samples_this_depth.length;
        }

        // Resize buffers to match any new count of samples
        const gl = gl_canvas.gl;
        const program = gl_canvas.timelineProgram;
        if (nb_samples > this.sampleBuffer.nbEntries)
        {
            this.sampleBuffer.ResizeToFitNextPow2(nb_samples);
            this.colourBuffer.ResizeToFitNextPow2(nb_samples);

            // Have to create a new VAO for these buffers
            this.vertexArrayObject = gl.createVertexArray();
            gl.bindVertexArray(this.vertexArrayObject);
            this.sampleBuffer.BindAsInstanceAttribute(program, "inSample_TextOffset");
            this.colourBuffer.BindAsInstanceAttribute(program, "inColour_TextLength");
        }

        // CPU write destination for samples
        let cpu_samples = this.sampleBuffer.cpuArray;
        let cpu_colours = this.colourBuffer.cpuArray;
        let sample_pos = 0;

        // TODO(don): Pack offsets into the sample buffer, instead?
        // Puts all samples together into one growing buffer (will need ring buffer management).
        // Offset points into that.
        // Remains to be seen how much of this can be done given the limitations of WebGL2...

        // Copy samples to the CPU buffer
        // TODO(don): Use a ring buffer instead and take advantage of timeline scrolling adding new samples at the beginning/end
        for (let depth = 0; depth < samples_per_depth.length; depth++)
        {
            let samples_this_depth = samples_per_depth[depth];
            for (const [frame, offset] of samples_this_depth)
            {
                const float_offset = offset / 4;

                cpu_samples[sample_pos + 0] = frame.sampleFloats[float_offset + g_sampleOffsetFloats_Start];
                cpu_samples[sample_pos + 1] = frame.sampleFloats[float_offset + g_sampleOffsetFloats_Length];
                cpu_samples[sample_pos + 2] = depth;
                cpu_samples[sample_pos + 3] = frame.sampleFloats[float_offset + g_sampleOffsetFloats_NameOffset];

                cpu_colours[sample_pos + 0] = frame.sampleDataView.getUint8(offset + g_sampleOffsetBytes_Colour + 0);
                cpu_colours[sample_pos + 1] = frame.sampleDataView.getUint8(offset + g_sampleOffsetBytes_Colour + 1);
                cpu_colours[sample_pos + 2] = frame.sampleDataView.getUint8(offset + g_sampleOffsetBytes_Colour + 2);
                cpu_colours[sample_pos + 3] = frame.sampleFloats[float_offset + g_sampleOffsetFloats_NameLength];

                sample_pos += 4;
            }
        }

        // Upload to GPU
        this.sampleBuffer.UploadData();
        this.colourBuffer.UploadData();

        gl_canvas.SetContainerUniforms(program, container);

        // Set row parameters
        const row_rect = this.LabelContainerNode.getBoundingClientRect();
        glSetUniform(gl, program, "inRow.yOffset", row_rect.top);

        gl.bindVertexArray(this.vertexArrayObject);
        gl.drawArraysInstanced(gl.TRIANGLE_STRIP, 0, 4, nb_samples);
    }


    TimelineRow.prototype.SetSelectSample = function(sample_info)
    {
        this.SelectSampleInfo = sample_info;
    }


    function ExpandButtonDown(evt)
    {
        var node = DOM.Event.GetNode(evt);
        DOM.Node.AddClass(node, "TimelineRowExpandButtonActive");
    }


    function ExpandButtonUp(evt)
    {
        var node = DOM.Event.GetNode(evt);
        DOM.Node.RemoveClass(node, "TimelineRowExpandButtonActive");
    }


    TimelineRow.prototype.IncDepth = function()
    {
        this.Depth++;
        this.SetSize();
    }


    TimelineRow.prototype.DecDepth = function()
    {
        if (this.Depth > 1)
        {
            this.Depth--;
            this.SetSize();

            // Trigger scroll handling to ensure reducing the depth reduces the display height
            this.timeline.MoveVertically(0);
        }
    }


    TimelineRow.prototype.GetSampleAtPosition = function(time_us, mouse_y)
    {
        // Calculate depth of the mouse cursor
        const depth = Math.min(Math.floor(mouse_y / SAMPLE_Y_SPACING) + 1, this.Depth);

        // Search for the first frame to intersect this time
        for (let i in this.VisibleFrames)
        {
            // Use the sample's closed interval to detect hits.
            // Rendering of samples ensures a sample is never smaller than one pixel so that all samples always draw, irrespective
            // of zoom level. If a half-open interval is used then some visible samples will be unselectable due to them being
            // smaller than a pixel. This feels pretty odd and the closed interval fixes this feeling well.
            // TODO(don): There are still inconsistencies, need to shift to pixel range checking to match exactly.
            const frame = this.VisibleFrames[i];
            if (time_us >= frame.StartTime_us && time_us <= frame.EndTime_us)
            {
                const found_sample = FindSample(this, frame, time_us, depth, 1);
                if (found_sample != null)
                {
                    return [ frame, found_sample[0], found_sample[1], this ];
                }
            }
        }

        return null;
    }


    function FindSample(self, frame, time_us, target_depth, depth)
    {
        // Search entire frame of samples looking for a depth and time range that contains the input time
        const sample_data_view = frame.sampleDataView;
        for (let offset = 0; offset < sample_data_view.byteLength; offset += g_nbBytesPerSample)
        {
            depth = sample_data_view.getUint8(offset + g_sampleOffsetBytes_Depth) + 1;
            if (depth == target_depth)
            {
                const us_start = sample_data_view.getFloat32(offset + g_sampleOffsetBytes_Start, true) * 1000.0;
                const us_length = sample_data_view.getFloat32(offset + g_sampleOffsetBytes_Length, true) * 1000.0;
                if (time_us >= us_start && time_us < us_start + us_length)
                {
                    return [ offset, depth ];
                }
            }
        }

        return null;
    }


    return TimelineRow;
})();
