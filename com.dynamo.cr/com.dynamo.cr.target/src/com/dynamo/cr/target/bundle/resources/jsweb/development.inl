	<!-- DEVELOPMENT BLOCK -->
	<div class="dev_panel dev_depth">
		<h1>Memory Info</h1>
		<div class="dev_checkbox_row">
			<div class="dev_checkbox">
				<span>HEAP</span>
		  		<input type="checkbox" value="1" id="chk_heap" name="" />
			  	<label for="chk_heap"></label>
	  		</div>
	  		<div class="dev_checkbox">
				<span>DYNAMIC</span>
		  		<input type="checkbox" value="1" id="chk_dynamic" name="" />
			  	<label for="chk_dynamic"></label>
  			</div>
  		</div>
		<div class="dev_checkbox_row">
			<div class="dev_checkbox">
				<span>STATIC</span>
		  		<input type="checkbox" value="1" id="chk_static" name="" />
			  	<label for="chk_static"></label>
			</div>
			<div class="dev_checkbox">
				<span>STACK</span>
		  		<input type="checkbox" value="1" id="chk_stack" name="" />
			  	<label for="chk_stack"></label>
			</div>
		</div>
		<div>
			<textarea type="text" readonly id="memory_summary"></textarea>
		</div>
	</div>
    <script type="text/javascript" src="development.js"></script>
    <!-- END DEVELOPMENT BLOCK -->