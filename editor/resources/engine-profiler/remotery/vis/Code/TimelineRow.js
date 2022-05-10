

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


    var CANVAS_BORDER = 1;
    var SAMPLE_HEIGHT = 16;
    var SAMPLE_BORDER = 1;
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
        this.sampleBuffer = new glDynamicBuffer(gl, gl.FLOAT, 4, 8);
        this.colourBuffer = new glDynamicBuffer(gl, gl.FLOAT, 4, 8);
        
		// Create a vertex array for these buffers
		this.vertexArrayObject = gl.createVertexArray();
		gl.bindVertexArray(this.vertexArrayObject);
		this.sampleBuffer.BindAsInstanceAttribute(timeline.Program, "inSample_TextOffset");
		this.colourBuffer.BindAsInstanceAttribute(timeline.Program, "inColour_TextLength");

        // An initial SetSize call to restore containers to their original size after traces were loaded prior to this
        this.SetSize();
    }


    TimelineRow.prototype.SetSize = function()
    {
        this.LabelContainerNode.style.height = CANVAS_BORDER + SAMPLE_BORDER + SAMPLE_Y_SPACING * this.Depth;
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


    TimelineRow.prototype.DrawSampleHighlight = function(sample, depth, colour, y_scroll_offset)
    {
        if (depth <= this.Depth)
        {
            // Determine pixel range of the sample	
            var x0 = this.VisibleTimeRange.PixelOffset(sample.us_start);
            var x1 = x0 + this.VisibleTimeRange.PixelSize(sample.us_length);

            var offset_x = x0;
            var offset_y = this.LabelContainerNode.offsetTop + 2 + (depth - 1) * SAMPLE_Y_SPACING + y_scroll_offset;
            var size_x = x1 - x0;
            var size_y = SAMPLE_HEIGHT;

            // Normal rendering
            var ctx = this.timeline.drawContext;
            ctx.lineWidth = 2;
            ctx.strokeStyle = colour;
            ctx.strokeRect(offset_x + 2.5, offset_y - 0.5, size_x - 3, size_y + 1);
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


    TimelineRow.prototype.DrawBackground = function(hover_sample_info, y_scroll_offset)
    {
        // Fill box that shows the boundary between thread rows
        this.timeline.drawContext.fillStyle = "#444"
        var b = CANVAS_BORDER;
        this.timeline.drawContext.fillRect(b, this.YOffset() + y_scroll_offset + b, this.timeline.drawCanvas.width - b * 2, this.DisplayHeight() - b * 2);

        // Draw the selected sample for this row
        if (this.SelectSampleInfo != null)
        {
            const sample = this.SelectSampleInfo[1];
            const depth = this.SelectSampleInfo[2];
            this.DrawSampleHighlight(sample, depth, "#FF0000", y_scroll_offset);
        }

        // Draw the current hover sample if it's over this row
        if (hover_sample_info != null && hover_sample_info[3] == this)
        {
            const sample = hover_sample_info[1];
            const depth = hover_sample_info[2];
            const thread_row = hover_sample_info[3];
            this.DrawSampleHighlight(sample, depth, "#FFFFFF", y_scroll_offset);
        }
    }


    TimelineRow.prototype.Draw = function(gl, draw_text, y_scroll_offset)
    {
        let samples_per_depth = [];

        // Gather all root samples in the visible frame set
        for (var i in this.VisibleFrames)
        {
            var frame = this.VisibleFrames[i];
            GatherSamples(this, frame.Samples, 1, draw_text, samples_per_depth);
        }

        // Count number of samples required
        let nb_samples = 0;
        for (const samples_this_depth of samples_per_depth)
        {
            nb_samples += samples_this_depth.length;
        }

        // Resize buffers to match any new count of samples
        if (nb_samples > this.sampleBuffer.nbEntries)
        {
            this.sampleBuffer.ResizeToFitNextPow2(nb_samples);
            this.colourBuffer.ResizeToFitNextPow2(nb_samples);

            // Have to create a new VAO for these buffers
            this.vertexArrayObject = gl.createVertexArray();
            gl.bindVertexArray(this.vertexArrayObject);
            this.sampleBuffer.BindAsInstanceAttribute(this.timeline.Program, "inSample_TextOffset");
            this.colourBuffer.BindAsInstanceAttribute(this.timeline.Program, "inColour_TextLength");
        }

        // CPU write destination for samples
        let cpu_samples = this.sampleBuffer.cpuArray;
        let cpu_colours = this.colourBuffer.cpuArray;
        let sample_pos = 0;

        const empty_text_entry = {
            offset: 0,
            length: 1,
        };

        // Copy samples to the CPU buffer
        // TODO(don): Use a ring buffer instead and take advantage of timeline scrolling adding new samples at the beginning/end
        for (let depth = 0; depth < samples_per_depth.length; depth++)
        {
            let samples_this_depth = samples_per_depth[depth];
            for (const sample of samples_this_depth)
            {
                const text_entry = sample.name.textEntry != null ? sample.name.textEntry : empty_text_entry;

                cpu_samples[sample_pos + 0] = sample.us_start;
                cpu_samples[sample_pos + 1] = sample.us_length;
                cpu_samples[sample_pos + 2] = depth;
                cpu_samples[sample_pos + 3] = text_entry.offset;

                cpu_colours[sample_pos + 0] = sample.rgbColour[0];
                cpu_colours[sample_pos + 1] = sample.rgbColour[1];
                cpu_colours[sample_pos + 2] = sample.rgbColour[2];
                cpu_colours[sample_pos + 3] = text_entry.length;

                sample_pos += 4;
            }
        }

        // Upload to GPU
        this.sampleBuffer.UploadData();
        this.colourBuffer.UploadData();
        this.timeline.textBuffer.UploadData();

        // Set row parameters
        glSetUniform(gl, this.timeline.Program, "inRow.yOffset", this.YOffset() + y_scroll_offset);

        gl.bindVertexArray(this.vertexArrayObject);
        gl.drawArraysInstanced(gl.TRIANGLE_STRIP, 0, 4, nb_samples);
    }


    function GatherSamples(self, samples, depth, draw_text, samples_per_depth)
    {
        // Ensure there's enough entries for each depth
        while (depth >= samples_per_depth.length)
        {
            samples_per_depth.push([]);
        }
        let samples_this_depth = samples_per_depth[depth];

        for (var i in samples)
        {
            var sample = samples[i];
            samples_this_depth.push(sample);

            if (depth < self.Depth && sample.children != null)
                GatherSamples(self, sample.children, depth + 1, draw_text, samples_per_depth);
        }
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
        this.timeline.DrawAllRows();
    }


    TimelineRow.prototype.DecDepth = function()
    {
        if (this.Depth > 1)
        {
            this.Depth--;
            this.SetSize();

            // Trigger scroll handling to ensure reducing the depth reduces the display height
            this.timeline.ScrollVertically(0);

            this.timeline.DrawAllRows();
        }
    }


    TimelineRow.prototype.GetSampleAtPosition = function(time_us, mouse_y)
    {
        // Calculate depth of the mouse cursor
        var depth = Math.min(Math.floor(mouse_y / SAMPLE_Y_SPACING) + 1, this.Depth);

        // Search for the first frame to intersect this time
        for (var i in this.VisibleFrames)
        {
            // Use the sample's closed interval to detect hits.
            // Rendering of samples ensures a sample is never smaller than one pixel so that all samples always draw, irrespective
            // of zoom level. If a half-open interval is used then some visible samples will be unselectable due to them being
            // smaller than a pixel. This feels pretty odd and the closed interval fixes this feeling well.
            // TODO(don): There are still inconsistencies, need to shift to pixel range checking to match exactly.
            var frame = this.VisibleFrames[i];
            if (time_us >= frame.StartTime_us && time_us <= frame.EndTime_us)
            {
                var found_sample = FindSample(this, frame.Samples, time_us, depth, 1);
                if (found_sample != null)
                    return [ frame, found_sample[0], found_sample[1], this ];
            }
        }

        return null;
    }


    function FindSample(self, samples, time_us, target_depth, depth)
    {
        for (var i in samples)
        {
            var sample = samples[i];
            if (depth == target_depth)
            {
                if (time_us >= sample.us_start && time_us < sample.us_start + sample.us_length)
                    return [ sample, depth ];
            }

            else if (depth < target_depth && sample.children != null)
            {
                var found_sample = FindSample(self, sample.children, time_us, target_depth, depth + 1);
                if (found_sample != null)
                    return found_sample;
            }
        }

        return null;
    }


    return TimelineRow;
})();
