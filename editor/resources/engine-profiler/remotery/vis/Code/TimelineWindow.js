
// TODO(don): Separate all knowledge of threads from this timeline

TimelineWindow = (function()
{
	var BORDER = 10;

	function TimelineWindow(wm, name, settings, check_handler, gl_canvas)
	{
		this.Settings = settings;
		this.glCanvas = gl_canvas;

		// Create timeline window
		this.Window = wm.AddWindow("Timeline", 10, 20, 100, 100, null, this);
		this.Window.SetTitle(name);
		this.Window.ShowNoAnim();

		this.timelineMarkers = new TimelineMarkers(this);

		// DO THESE need to be containers... can they just be divs?
		// divs need a retrieval function
		this.TimelineLabelScrollClipper = this.Window.AddControlNew(new WM.Container(10, 10, 10, 10));
		DOM.Node.AddClass(this.TimelineLabelScrollClipper.Node, "TimelineLabelScrollClipper");
		this.TimelineLabels = this.TimelineLabelScrollClipper.AddControlNew(new WM.Container(0, 0, 10, 10));
		DOM.Node.AddClass(this.TimelineLabels.Node, "TimelineLabels");

		// Ordered list of thread rows on the timeline
		this.ThreadRows = [ ];

		// Create timeline container
		this.TimelineContainer = this.Window.AddControlNew(new WM.Container(10, 10, 800, 160));
		DOM.Node.AddClass(this.TimelineContainer.Node, "TimelineContainer");

		// Setup mouse interaction
		this.mouseInteraction = new MouseInteraction(this.TimelineContainer.Node);
		this.mouseInteraction.onClickHandler = (mouse_state) => OnMouseClick(this, mouse_state);
		this.mouseInteraction.onMoveHandler = (mouse_state, mx, my) => OnMouseMove(this, mouse_state, mx, my);
		this.mouseInteraction.onHoverHandler = (mouse_state) => OnMouseHover(this, mouse_state);
		this.mouseInteraction.onScrollHandler = (mouse_state) => OnMouseScroll(this, mouse_state);

		// Allow user to click on the thread name to deselect any threads as finding empty space may be difficult
		DOM.Event.AddHandler(this.TimelineLabels.Node, "mousedown", (evt) => OnLabelMouseDown(this, evt));

		this.Window.SetOnResize(Bind(OnUserResize, this));

		this.Clear();

		this.OnHoverHandler = null;
		this.OnSelectedHandler = null;
		this.OnMovedHandler = null;
		this.CheckHandler = check_handler;

		this.yScrollOffset = 0;

		this.HoverSampleInfo = null;
		this.lastHoverThreadName = null;
	}


	TimelineWindow.prototype.Clear = function()
	{
		// Clear out labels
		this.TimelineLabels.ClearControls();

		this.ThreadRows = [ ];
		this.TimeRange = new PixelTimeRange(0, 200 * 1000, this.TimelineContainer.Node.clientWidth);
	}


	TimelineWindow.prototype.SetOnHover = function(handler)
	{
		this.OnHoverHandler = handler;
	}


	TimelineWindow.prototype.SetOnSelected = function(handler)
	{
		this.OnSelectedHandler = handler;
	}


	TimelineWindow.prototype.SetOnMoved = function(handler)
	{
		this.OnMovedHandler = handler;
	}


	TimelineWindow.prototype.WindowResized = function(x, width, top_window)
	{
		// Resize window
		var top = top_window.Position[1] + top_window.Size[1] + 10;
		this.Window.SetPosition(x, top);
		this.Window.SetSize(width - 2 * 10, 260);

		ResizeInternals(this);
	}


	TimelineWindow.prototype.OnSamples = function(thread_name, frame_history)
	{
		// Shift the timeline to the last entry on this thread
		var last_frame = frame_history[frame_history.length - 1];
		this.TimeRange.SetEnd(last_frame.EndTime_us);

		// Search for the index of this thread
		var thread_index = -1;
		for (var i in this.ThreadRows)
		{
			if (this.ThreadRows[i].Name == thread_name)
			{
				thread_index = i;
				break;
			}
		}

		// If this thread has not been seen before, add a new row to the list
		if (thread_index == -1)
		{
			var row = new TimelineRow(this.glCanvas.gl, thread_name, this, frame_history, this.CheckHandler);
			this.ThreadRows.push(row);

			// Sort thread rows in the collection by name
			this.ThreadRows.sort((a, b) => a.Name.localeCompare(b.Name));

			// Resort the view by removing timeline row nodes from their DOM parents and re-adding
			const thread_rows = new Array();
			for (let thread_row of this.ThreadRows)
			{
				this.TimelineLabels.Node.removeChild(thread_row.LabelContainerNode);
				thread_rows.push(thread_row);
			}
			for (let thread_row of thread_rows)
			{
				this.TimelineLabels.Node.appendChild(thread_row.LabelContainerNode);
			}
		}
	}


	TimelineWindow.prototype.DrawBackground = function()
	{
		const gl = this.glCanvas.gl;
		const program = this.glCanvas.timelineBackgroundProgram;
		gl.useProgram(program);

        // Set viewport parameters
        glSetUniform(gl, program, "inViewport.width", gl.canvas.width);
        glSetUniform(gl, program, "inViewport.height", gl.canvas.height);
		
		this.glCanvas.SetContainerUniforms(program, this.TimelineContainer.Node);

		// Set row parameters
		const row_rect = this.TimelineLabels.Node.getBoundingClientRect();
		glSetUniform(gl, program, "inYOffset", row_rect.top);

		gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);

		this.timelineMarkers.Draw(this.TimeRange);
	}


	TimelineWindow.prototype.Deselect = function(thread_name)
	{
		for (let thread_row of this.ThreadRows)
		{
			if (thread_name == thread_row.Name)
			{
				thread_row.SelectSampleInfo = null;
			}
		}
	}


	TimelineWindow.prototype.DrawSampleHighlights = function()
	{
		const gl = this.glCanvas.gl;
        const program = this.glCanvas.timelineHighlightProgram;
		gl.useProgram(program);

        // Set viewport parameters
        glSetUniform(gl, program, "inViewport.width", gl.canvas.width);
        glSetUniform(gl, program, "inViewport.height", gl.canvas.height);

		// Set time range parameters
		const time_range = this.TimeRange;
        time_range.SetAsUniform(gl, program);

		for (let thread_row of this.ThreadRows)
		{
			// Draw highlight for hover row
			if (this.HoverSampleInfo != null && this.HoverSampleInfo[3] == thread_row)
			{
				const frame = this.HoverSampleInfo[0];
				const offset = this.HoverSampleInfo[1];
				const depth = this.HoverSampleInfo[2];
				thread_row.DrawSampleHighlight(this.glCanvas, this.TimelineContainer.Node, frame, offset, depth, false);
			}

			// Draw highlight for any samples selected on this row
			if (thread_row.SelectSampleInfo != null)
			{
				const frame = thread_row.SelectSampleInfo[0];
				const offset = thread_row.SelectSampleInfo[1];
				const depth = thread_row.SelectSampleInfo[2];
				thread_row.DrawSampleHighlight(this.glCanvas, this.TimelineContainer.Node, frame, offset, depth, true);
			}
		}
	}


	TimelineWindow.prototype.DrawSampleGpuToCpu = function()
	{
		const gl = this.glCanvas.gl;
		const program = this.glCanvas.timelineGpuToCpuProgram;
		gl.useProgram(program);

        // Set viewport parameters
        glSetUniform(gl, program, "inViewport.width", gl.canvas.width);
        glSetUniform(gl, program, "inViewport.height", gl.canvas.height);

		// Set time range parameters
		const time_range = this.TimeRange;
        time_range.SetAsUniform(gl, program);

		// Draw pointer for hover rows
		for (let thread_row of this.ThreadRows)
		{
			if (this.HoverSampleInfo != null && this.HoverSampleInfo[3] == thread_row)
			{
				const frame = this.HoverSampleInfo[0];
				const offset = this.HoverSampleInfo[1];
				const depth = this.HoverSampleInfo[2];
				thread_row.DrawSampleGpuToCpu(this.glCanvas, this.TimelineContainer.Node, frame, offset, depth);
			}
		}
	}


	TimelineWindow.prototype.Draw = function()
	{
		this.DrawBackground();

		const gl = this.glCanvas.gl;
        const program = this.glCanvas.timelineProgram;
		gl.useProgram(program);

        // Set viewport parameters
        glSetUniform(gl, program, "inViewport.width", gl.canvas.width);
        glSetUniform(gl, program, "inViewport.height", gl.canvas.height);

		// Set time range parameters
		const time_range = this.TimeRange;
        time_range.SetAsUniform(gl, program);

        this.glCanvas.SetTextUniforms(program);

		for (let i in this.ThreadRows)
		{
			var thread_row = this.ThreadRows[i];
			thread_row.SetVisibleFrames(time_range);
			thread_row.Draw(this.glCanvas, this.TimelineContainer.Node);
		}

		this.DrawSampleHighlights();
		this.DrawSampleGpuToCpu();
	}


	function OnUserResize(self, evt)
	{
		ResizeInternals(self);
	}

	function ResizeInternals(self)
	{
		// .TimelineRowLabel
		// .TimelineRowExpand
		// .TimelineRowExpand
		// .TimelineRowCheck
		// Window padding
		let offset_x = 145+19+19+19+10;

		let MarkersHeight = 18;

		var parent_size = self.Window.Size;

		self.timelineMarkers.Resize(BORDER + offset_x, 10, parent_size[0] - 2* BORDER - offset_x, MarkersHeight);

		// Resize controls
		self.TimelineContainer.SetPosition(BORDER + offset_x, 10 + MarkersHeight);
		self.TimelineContainer.SetSize(parent_size[0] - 2 * BORDER - offset_x, parent_size[1] - MarkersHeight - 40);

		self.TimelineLabelScrollClipper.SetPosition(10, 10 + MarkersHeight);
		self.TimelineLabelScrollClipper.SetSize(offset_x, parent_size[1] - MarkersHeight - 40);
		self.TimelineLabels.SetSize(offset_x, parent_size[1] - MarkersHeight - 40);

		// Adjust time range to new width
		const width = self.TimelineContainer.Node.clientWidth;
		self.TimeRange.SetPixelSpan(width);
	}


	function OnMouseScroll(self, mouse_state)
	{
		let scale = 1.11;
		if (mouse_state.WheelDelta > 0)
			scale = 1 / scale;

		// What time is the mouse hovering over?
		let mouse_pos = self.TimelineMousePosition(mouse_state);
		let time_us = self.TimeRange.TimeAtPosition(mouse_pos[0]);

		// Calculate start time relative to the mouse hover position
		var time_start_us = self.TimeRange.Start_us - time_us;

		// Scale and offset back to the hover time
		self.TimeRange.Set(time_start_us * scale + time_us, self.TimeRange.Span_us * scale);

		if (self.OnMovedHandler)
		{
			self.OnMovedHandler(self);
		}
	}


	TimelineWindow.prototype.SetTimeRange = function(start_us, span_us)
	{
		this.TimeRange.Set(start_us, span_us);
	}


	TimelineWindow.prototype.DisplayHeight = function()
	{
		// Sum height of each thread row
		let height = 0;
		for (thread_row of this.ThreadRows)
		{
			height += thread_row.DisplayHeight();
		}

		return height;
	}


	TimelineWindow.prototype.MoveVertically = function(y_scroll)
	{
		// Calculate the minimum negative value the position of the labels can be to account for scrolling to the bottom
		// of the label/depth list
		let display_height = this.DisplayHeight();
		let container_height = this.TimelineLabelScrollClipper.Node.clientHeight;
		let minimum_y = Math.min(container_height - display_height, 0.0);

		// Resize the label container to match the display height
		this.TimelineLabels.Node.style.height = Math.max(display_height, container_height);

		// Increment the y-scroll using just-calculated limits
		let old_y_scroll_offset = this.yScrollOffset;
		this.yScrollOffset = Math.min(Math.max(this.yScrollOffset + y_scroll, minimum_y), 0);

		// Calculate how much the labels should actually scroll after limiting and apply
		let y_scroll_px = this.yScrollOffset - old_y_scroll_offset;
		this.TimelineLabels.Node.style.top = this.TimelineLabels.Node.offsetTop + y_scroll_px;
	}


	TimelineWindow.prototype.TimelineMousePosition = function(mouse_state)
	{
		// Position of the mouse relative to the timeline container
		let node_offset = DOM.Node.GetPosition(this.TimelineContainer.Node);
		let mouse_x = mouse_state.Position[0] - node_offset[0];
		let mouse_y = mouse_state.Position[1] - node_offset[1];

		// Offset by the amount of scroll
		mouse_y -= this.yScrollOffset;

		return [ mouse_x, mouse_y ];
	}


	TimelineWindow.prototype.GetHoverThreadRow = function(mouse_pos)
	{
		// Search for the thread row the mouse intersects
		let height = 0;
		for (let thread_row of this.ThreadRows)
		{
			let row_height = thread_row.DisplayHeight();
			if (mouse_pos[1] >= height && mouse_pos[1] < height + row_height)
			{
				// Mouse y relative to row start
				mouse_pos[1] -= height;
				return thread_row;
			}
			height += row_height;
		}

		return null;
	}


	function OnMouseClick(self, mouse_state)
	{
		// Are we hovering over a thread row?
		const mouse_pos = self.TimelineMousePosition(mouse_state);
		const hover_thread_row = self.GetHoverThreadRow(mouse_pos);
		if (hover_thread_row != null)
		{
			// Are we hovering over a sample?
			const time_us = self.TimeRange.TimeAtPosition(mouse_pos[0]);
			const sample_info = hover_thread_row.GetSampleAtPosition(time_us, mouse_pos[1]);
			if (sample_info != null)
			{
				// Toggle deselect if this sample is already selected
				if (hover_thread_row.SelectSampleInfo != null &&
					sample_info[0] == hover_thread_row.SelectSampleInfo[0] && sample_info[1] == hover_thread_row.SelectSampleInfo[1] &&
					sample_info[2] == hover_thread_row.SelectSampleInfo[2] && sample_info[3] == hover_thread_row.SelectSampleInfo[3])
				{
					hover_thread_row.SetSelectSample(null);
					self.OnSelectedHandler?.(hover_thread_row.Name, null);
				}

				// Otherwise select
				else
				{
					hover_thread_row.SetSelectSample(sample_info);
					self.OnSelectedHandler?.(hover_thread_row.Name, sample_info);
				}
			}

			// Deselect if not hovering over a sample
			else
			{
				self.OnSelectedHandler?.(hover_thread_row.Name, null);
			}
		}
	}


	function OnLabelMouseDown(self, evt)
	{
		// Deselect sample on this thread
		const mouse_state = new Mouse.State(evt);
		let mouse_pos = self.TimelineMousePosition(mouse_state);
		const thread_row = self.GetHoverThreadRow(mouse_pos);
		self.OnSelectedHandler?.(thread_row.Name, null);
	}


	function OnMouseMove(self, mouse_state, move_offset_x, move_offset_y)
	{
		// Shift the visible time range with mouse movement
		const time_offset_us = move_offset_x / self.TimeRange.usPerPixel;
		self.TimeRange.SetStart(self.TimeRange.Start_us - time_offset_us);

		// Control vertical movement
		self.MoveVertically(move_offset_y);

		// Notify
		self.OnMovedHandler?.(self);
	}


	function OnMouseHover(self, mouse_state)
	{
		// Check for hover ending
		if (mouse_state == null)
		{
			self.OnHoverHandler?.(self.lastHoverThreadName, null);
			return;
		}

		// Are we hovering over a thread row?
		const mouse_pos = self.TimelineMousePosition(mouse_state);
		const hover_thread_row = self.GetHoverThreadRow(mouse_pos);
		if (hover_thread_row != null)
		{
			// Are we hovering over a sample?
			const time_us = self.TimeRange.TimeAtPosition(mouse_pos[0]);
			self.HoverSampleInfo = hover_thread_row.GetSampleAtPosition(time_us, mouse_pos[1]);

			// Exit hover for the last hover row
			self.OnHoverHandler?.(self.lastHoverThreadName, null);
			self.lastHoverThreadName = hover_thread_row.Name;

			// Tell listeners which sample we're hovering over
			self.OnHoverHandler?.(hover_thread_row.Name, self.HoverSampleInfo);
		}
		else
		{
			self.HoverSampleInfo = null;
		}
	}


	return TimelineWindow;
})();

