
WebSocketConnection = (function()
{
	function WebSocketConnection()
	{
		this.MessageHandlers = { };
		this.Socket = null;
		this.Console = null;
	}


	WebSocketConnection.prototype.SetConsole = function(console)
	{
		this.Console = console;
	}


	WebSocketConnection.prototype.Connecting = function()
	{
		return this.Socket != null && this.Socket.readyState == WebSocket.CONNECTING;
	}

	WebSocketConnection.prototype.Connected = function()
	{
		return this.Socket != null && this.Socket.readyState == WebSocket.OPEN;
	}


	WebSocketConnection.prototype.AddConnectHandler = function(handler)
	{
		this.AddMessageHandler("__OnConnect__", handler);
	}


	WebSocketConnection.prototype.AddDisconnectHandler = function(handler)
	{
		this.AddMessageHandler("__OnDisconnect__", handler);
	}


	WebSocketConnection.prototype.AddMessageHandler = function(message_name, handler)
	{
		// Create the message handler array on-demand
		if (!(message_name in this.MessageHandlers))
			this.MessageHandlers[message_name] = [ ];
		this.MessageHandlers[message_name].push(handler);
	}


	WebSocketConnection.prototype.Connect = function(address)
	{
		// Abandon previous connection attempt
		this.Disconnect();

		Log(this, "Connecting to " + address);

		this.Socket = new WebSocket(address);
		this.Socket.binaryType = "arraybuffer";
		this.Socket.onopen = Bind(OnOpen, this);
		this.Socket.onmessage = Bind(OnMessage, this);
		this.Socket.onclose = Bind(OnClose, this);
		this.Socket.onerror = Bind(OnError, this);
	}


	WebSocketConnection.prototype.Disconnect = function()
	{
		Log(this, "Disconnecting");
		if (this.Socket != null)
		{
			this.Socket.close();
			this.Socket = null;
		}
	}


	WebSocketConnection.prototype.Send = function(msg)
	{
		if (this.Connected())
			this.Socket.send(msg);
	}


	function Log(self, message)
	{
		self.Console.Log(message);
	}


	function CallMessageHandlers(self, message_name, data_view, length)
	{
		if (message_name in self.MessageHandlers)
		{
			var handlers = self.MessageHandlers[message_name];
			for (var i in handlers)
			    handlers[i](self, data_view, length);
		}
	}


	function OnOpen(self, event)
	{
		Log(self, "Connected");
		CallMessageHandlers(self, "__OnConnect__");
	}


	function OnClose(self, event)
	{
		// Clear all references
		self.Socket.onopen = null;
		self.Socket.onmessage = null;
		self.Socket.onclose = null;
		self.Socket.onerror = null;
		self.Socket = null;

		Log(self, "Disconnected");
		CallMessageHandlers(self, "__OnDisconnect__");
	}


	function OnError(self, event)
	{
		Log(self, "Connection Error ");
	}


	function OnMessage(self, event)
	{
	    let data_view = new DataView(event.data);
		let data_view_reader = new DataViewReader(data_view, 0);
		self.CallMessageHandlers(data_view_reader);
	}

	WebSocketConnection.prototype.CallMessageHandlers = function(data_view_reader)
	{
		// Decode standard message header
		const id = data_view_reader.GetStringOfLength(4);
		const length = data_view_reader.GetUInt32();

		// Pass the length of the message left to parse
		CallMessageHandlers(this, id, data_view_reader, length - 8);

		return [ id, length ];
	}


	return WebSocketConnection;
})();
