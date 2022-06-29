class MouseInteraction
{
	constructor(node)
	{
		this.node = node;

		// Current interaction state
		this.mouseDown = false;
		this.lastMouseState = null;
		this.mouseMoved = false;

		// Empty user handlers
		this.onClickHandler = null;
		this.onMoveHandler = null;
		this.onHoverHandler = null;
        this.onScrollHandler = null;

		// Setup DOM handlers
		DOM.Event.AddHandler(this.node, "mousedown", (evt) => this._OnMouseDown(evt));
		DOM.Event.AddHandler(this.node, "mouseup", (evt) => this._OnMouseUp(evt));
		DOM.Event.AddHandler(this.node, "mousemove", (evt) => this._OnMouseMove(evt));
		DOM.Event.AddHandler(this.node, "mouseleave", (evt) => this._OnMouseLeave(evt));

        // Mouse wheel is a little trickier
		const mouse_wheel_event = (/Firefox/i.test(navigator.userAgent)) ? "DOMMouseScroll" : "mousewheel";
		DOM.Event.AddHandler(this.node, mouse_wheel_event, (evt) => this._OnMouseScroll(evt));
	}

	_OnMouseDown(evt)
	{
		this.mouseDown = true;
		this.lastMouseState = new Mouse.State(evt);
		this.mouseMoved = false;
		DOM.Event.StopDefaultAction(evt);
	}

	_OnMouseUp(evt)
	{
		const mouse_state = new Mouse.State(evt);

		this.mouseDown = false;

		// Chain to on click event when released without movement
		if (!this.mouseMoved)
		{
			if (this.onClickHandler)
			{
				this.onClickHandler(mouse_state);
			}
		}
	}

	_OnMouseMove(evt)
	{
		const mouse_state = new Mouse.State(evt);

		if (this.mouseDown)
		{
			// Has the mouse moved while being held down?
			const move_offset_x = mouse_state.Position[0] - this.lastMouseState.Position[0]; 
			const move_offset_y = mouse_state.Position[1] - this.lastMouseState.Position[1]; 
			if (move_offset_x != 0 || move_offset_y != 0)
			{
				this.mouseMoved = true;

				// Chain to move handler
				if (this.onMoveHandler)
				{
					this.onMoveHandler(mouse_state, move_offset_x, move_offset_y);
				}
			}
		}

		// Chain to hover handler
		else if (this.onHoverHandler)
		{
			this.onHoverHandler(mouse_state);
		}

		this.lastMouseState = mouse_state;
	}

	_OnMouseLeave(evt)
	{
		// Cancel panning
		this.mouseDown = false;

		// Cancel hovering
		if (this.onHoverHandler)
		{
			this.onHoverHandler(null);
		}
	}

    _OnMouseScroll(evt)
    {
		const mouse_state = new Mouse.State(evt);
        if (this.onScrollHandler)
        {
            this.onScrollHandler(mouse_state);

			// Prevent vertical scrolling on mouse-wheel
			DOM.Event.StopDefaultAction(evt);
        }
    }
};
