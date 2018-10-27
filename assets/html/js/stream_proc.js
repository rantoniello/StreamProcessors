/*
 * Copyright (c) 2015, 2016, 2017 Rafael Antoniello
 *
 * This file is part of StreamProcessor.
 *
 * StreamProcessor is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * StreamProcessor is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with StreamProcessor.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file stream_proc.js
 */

var streamProcLinkClass= 'streamProc';

// JSON formatted list of program-processors enabled at backend
var streamProcCurrProgProcListJSON= [];

function streamProc_manager(url)
{
	// GET node REST JSON
	httpRequests_respJSON(url, 'GET', '', null, {onResponse: drawNode});

	function drawNode(streamProc_json)
	{
		var streamProcId= streamProc_json.id;
		var streamProcIdStr= streamProc_json.id_str;
		var nodeUrlSchemeTag= '/streamProcs/';
		var nodeId;
		var parentNodeId;
		var nodeDiv;

		// Update globlas
		streamProcCurrProgProcListJSON= streamProc_json.program_processors;

		// Compose HTML id for this node and for parent node
		nodeId= utils_url2id(url);
		parentNodeId= nodeId.substring(0, nodeId.lastIndexOf("-"));
		//console.log("nodeId: "+ nodeId); //comment-me
		//console.log("parentNodeId: "+ parentNodeId); //comment-me

		// Compose local elements Ids.
		var iBitrateElemId= nodeId+ "_input_bitrate";
		var iBufLevelElemId= nodeId+ "_input_buf_level";
		var tagElemId= nodeId+ "_settings_tag";
		var iURLElemId= nodeId+ "_settings_iurl";
		var iURLElemTypsiId= nodeId+ "_settings_iurl_typsi";
		var generalSettingsDivId= nodeId+ '_general_settings';
		var tracesNodeId= nodeId+ '_traces';
		var deleteDemultiplexerAlertModalDivId= nodeId+ 
				"_alert_modal_delDemuxer";
		var submitGeneralSettingsAlertModalDivId= generalSettingsDivId+ 
				"_alert_modal_submit";
		var confirmPurgeProgModalDivId= nodeId+ "_confirm_purge_prog_modal";

		// Create or update node division 
		nodeDiv= document.getElementById(nodeId);
		if(!nodeDiv)
			nodeDiv= createNode();
		else
			updateNode();

		// Draw programs nodes links
		drawProgramNodesLinks();

		function createNode()
		{
			// Create node link and division
			var nodeTxt= "#"+ streamProcIdStr;
			nodeDiv= nodes_create(nodeId, parentNodeId, streamProcLinkClass, 
					nodeUrlSchemeTag, nodeTxt);

			// Demultiplexer delete button
			var deleteDemultiplexerModalDiv= document.createElement("div");
			deleteDemultiplexerModalDiv.id= deleteDemultiplexerAlertModalDivId;
			nodeDiv.appendChild(deleteDemultiplexerModalDiv);
			var inputElemDeleteDemultiplexer= document.createElement("input");
			inputElemDeleteDemultiplexer.type= "button";
			inputElemDeleteDemultiplexer.value= "Delete demultiplexer";
			inputElemDeleteDemultiplexer.className= "normal";
			inputElemDeleteDemultiplexer.addEventListener('click', 
			function(event){
		    	// Confirm operation
				var confirmationEventObj= {};
				var cancelEventObj= {};
				var modalTxt= "You are about to delete a demultiplexer " +
						"element. All settings will be lost.";
		    	modals_confirmation_modal(deleteDemultiplexerAlertModalDivId, 
		    			["alert"], modalTxt, 
		    		function(confirmationEventObj) {
		    			streamProc_delete(url);
		    		}, 
		    		confirmationEventObj,
		    		function(cacelEventOb){}, 
		    		cancelEventObj);
			});
			inputElemDeleteDemultiplexer.className= "normal right";
			nodeDiv.appendChild(inputElemDeleteDemultiplexer);

			// Create demultiplexer title table
			var table= document.createElement("table");
			// - Table caption
			var caption= document.createElement("caption");
			table.appendChild(caption);
			// - Row: "Title"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.appendChild(document.createTextNode("Demultiplexer: "));
			td0.classList.add("streamProc-h1");
			tr.appendChild(td0);
			table.appendChild(tr);
			// - Append table
			nodeDiv.appendChild(table);
			nodeDiv.appendChild(document.createElement("br"));

			// Create streamProc information table
			var table= document.createElement("table");
			table.classList.add("streamProc");
			// - Create row: "Processor unambiguous id"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("streamProc");
			td0.appendChild(document.createTextNode("Unambiguous id.: "));
			var td1= document.createElement("td");
			td1.classList.add("streamProc");
			td1.classList.add("unselect");
			td1.appendChild(document.createTextNode("Demultiplexer #"+ 
					streamProcId));
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// - Create row: "bit-rate"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("streamProc");
			td0.appendChild(document.createTextNode("Input bitrate [kbps]: "));
			var td1= document.createElement("td");
			td1.classList.add("streamProc");
			td1.classList.add("unselect");
			td1.appendChild(document.createTextNode(
					streamProc_json.input_bitrate));
			td1.id= iBitrateElemId;
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// - Create row: "Input buffer level"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("streamProc");
			td0.appendChild(document.createTextNode(
					"Input buffer level [%]: "));
			var ibufLevel_typsi_id= nodeId+ "_placeholder_input_buf_level_href";
			var a= document.createElement('a');
			a.href= "#";
			a.id= ibufLevel_typsi_id;
			a.appendChild(document.createTextNode("[?]"));
			a.title= ""
				+ "This "
				+ "value represent the level (0% to 100%) of the demultiplexer "
				+ "input FIFO buffer; in correct operation it should be ~ 0% " 
				+ "(input packets are immediately consumed).<br/>"
				+ "If the buffer reaches ~ 100% of its level (for example, due " 
				+ "to a very high input bitrate exceeding demultiplexer " 
				+ "maximum throughput), packet losses may occur due to " 
				+ "overflow.";
			a.onclick = function() {return false;};
			td0.appendChild(a);
			var td1= document.createElement("td");
			td1.classList.add("streamProc");
			td1.classList.add("unselect");
			td1.appendChild(document.createTextNode(
					streamProc_json.input_buf_level));
			td1.id= iBufLevelElemId;
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// - Append table
			nodeDiv.appendChild(table);
			// - Format table 'tipsy's'
			$(function() {
				$('#'+ ibufLevel_typsi_id).tipsy({html: true, gravity: 'w'});
			});

			// ************************************************************** //
			// Create general-settings node
			// ************************************************************** //
			generalSettingsDiv= nodes_simpleDropDownCreate(
					generalSettingsDivId, nodeId, 'streamProc-details-dropdown', 
					"General settings", true);
			var table= document.createElement("table");
			table.classList.add("settings");
			table.classList.add("streamProc");
			// - Table caption: "Settings"
			var caption= document.createElement("caption");
			caption.classList.add("settings");
			table.appendChild(caption);
			// Row: "Tag"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("settings");
			td0.appendChild(document.createTextNode("Tag: "));
			var td1= document.createElement("td");
			td1.classList.add("settings");
			td1.classList.add("unselect");
			td1.id= tagElemId;
			var tagVal= streamProc_json.settings.tag;
			if(!tagVal) tagVal= "";
			td1.appendChild(document.createTextNode(tagVal));
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// - Row: "Input URL"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("settings");
			td0.appendChild(document.createTextNode("Input URL: "));
			var td1= document.createElement("td");
			td1.classList.add("settings");
			td1.classList.add("unselect");
			td1.id= iURLElemId;
			var iUrlVal= streamProc_json.settings.input_url;
			if(!iUrlVal) iUrlVal= "N/A";
			td1.appendChild(document.createTextNode(iUrlVal));
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// - Row: Edit settings
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("settings");
			var submitSettings= document.createElement("input");
			submitSettings.classList.add("settings");
			submitSettings.classList.add("button");
			submitSettings.classList.add("streamProc");
			submitSettings.type= "button";
			submitSettings.value= "Edit general settings";
			submitSettings.addEventListener('click', function(event){
				generalSettingsModal(nodeId);
			});
			td0.appendChild(submitSettings);
			var td1= document.createElement("td");
			td1.classList.add("settings");
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// - Append table
			generalSettingsDiv.appendChild(table);

			// ************************************************************** //
			// Create traces node
			// ************************************************************** //
			tracesNodeDiv= nodes_simpleDropDownCreate(
					tracesNodeId, nodeId, 'streamProc-details-dropdown', 
					"Messages", true);
			// Create traces table
			var table= document.createElement("table");
			table.classList.add("streamProc");
			table.classList.add("streamProc-traces");
			table.id= tracesNodeId+ '_traces_table';
			// - Table caption: "Settings"
			var caption= document.createElement("caption");
			caption.classList.add("settings");
			table.appendChild(caption);
			// - Header row: "Description | Counter | Date | Code"
			var tr= document.createElement("tr");
			var th0= document.createElement("th");
			th0.classList.add("streamProc-traces");
			th0.style.width= "65%";
			th0.appendChild(document.createTextNode("Description: "));
			var th1= document.createElement("th");
			th1.classList.add("streamProc-traces");
			th1.style.width= "5%";
			th1.appendChild(document.createTextNode("Counter: "));
			var th2= document.createElement("th");
			th2.classList.add("streamProc-traces");
			th2.style.width= "15%";
			th2.appendChild(document.createTextNode("Date: "));
			var th3= document.createElement("th");
			th3.classList.add("streamProc-traces");
			th3.style.width= "15%";
			th3.appendChild(document.createTextNode("Code: "));
			tr.appendChild(th0);
			tr.appendChild(th1);
			tr.appendChild(th2);
			tr.appendChild(th3);
			table.appendChild(tr);
			// - Append table to traces division
			tracesNodeDiv.appendChild(table);

			// - Body-Table (Created as separate table!)
			var tbodyDiv= document.createElement('div');
			tbodyDiv.classList.add("traces");
			tbodyDiv.classList.add("tbody-traces-div");
			tbodyDiv.id= tracesNodeId+ '_traces_table_tbody_div';
			var tbody= document.createElement('table'/*'tbody'*/);
			tbody.id= tracesNodeId+ '_traces_table_tbody';
			tbodyDiv.appendChild(tbody);
			// - Append body-table to traces division
			tracesNodeDiv.appendChild(tbodyDiv);

			// Erase log register button
			var submitEraseLogsSetting= document.createElement("input");
			submitEraseLogsSetting.classList.add("traces");
			submitEraseLogsSetting.classList.add("button");
			submitEraseLogsSetting.type= "button";
			submitEraseLogsSetting.value= "Erase messages";
			submitEraseLogsSetting.url= url;
			submitEraseLogsSetting.fieldIdString= '_settings_erase_logs_new';
			submitEraseLogsSetting.addEventListener('click', function(event){
				httpRequests_respJSON(url, 'PUT', '?flag_clear_logs=true', 
						null, null);
			});
			tracesNodeDiv.appendChild(submitEraseLogsSetting);

			// Create programs table
			var table= document.createElement("table");
			// - Table caption
			var caption= document.createElement("caption");
			table.appendChild(caption);
			// - Row: "Programs"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.appendChild(document.createTextNode("Programs: "));
			td0.classList.add("program-h1");
			tr.appendChild(td0);
			table.appendChild(tr);
			// - Append table
			nodeDiv.appendChild(document.createElement("br"));
			nodeDiv.appendChild(table);

			// Create list of program links
			var ul= document.createElement("ul");
			ul.className= programLinkClass;
			p= document.createElement("p");
			p.appendChild(document.createTextNode("- Not Available -"));
			p.classList.add("unselect");
			p.classList.add("naTxtNodeClass");
			ul.appendChild(p);
			nodeDiv.appendChild(ul);

			// Button to purge disassociated programs
			var confirmPurgeProgModalDiv= document.createElement("div");
			confirmPurgeProgModalDiv.id= confirmPurgeProgModalDivId;
			nodeDiv.appendChild(confirmPurgeProgModalDiv);
			var submitPurgeDisassociated= document.createElement("input");
			submitPurgeDisassociated.classList.add("traces");
			submitPurgeDisassociated.classList.add("button");
			submitPurgeDisassociated.type= "button";
			submitPurgeDisassociated.value= "Purge disassociated programs";
			submitPurgeDisassociated.url= url;
			submitPurgeDisassociated.addEventListener('click', function(event){
				// Confirm operation
				var confirmationEventObj= {};
				var cancelEventObj= {};
				var modalTxt= "You are about to purge the program " +
						"list; all the disassociated program-processors " +
						"configurations will be deleted.";
				modals_confirmation_modal(
						confirmPurgeProgModalDivId, ["alert"], 
						modalTxt, 
						function(confirmationEventObj) {
							httpRequests_respJSON(event.target.url, 'PUT', 
								'?flag_purge_disassociated_processors=true', 
								null, null);
						}, confirmationEventObj,
						function(cacelEventOb){}, cancelEventObj);
			});
			nodeDiv.appendChild(submitPurgeDisassociated);

			return nodeDiv;
		}

		function generalSettingsModal(parentNodeId)
		{
			var modalIdSufix= "genSettings";
			var modalPrivateContentId= parentNodeId+ '_genSettingsPrivate';

			var modalObj= modals_create_generic_modal(parentNodeId, 
					modalIdSufix, ["settings", "streamProc"]);
			if(modalObj== null) {
				// Modal already exist, update body content
				streamProc_reloadSettings(url, reloadGenSettings);
				// - Display the modal 
				modals_display_modal(parentNodeId, modalIdSufix);
			} else {
				// Create private modal content
				var divModalContentBody= modalObj.bodyDiv;
				var divModalContentHeader= modalObj.headerDiv;
				// Header
				divModalContentHeader.appendChild(document.createTextNode(
						"Demultiplexer general settings"));
				// Body
				var privModalDiv= createGeneralSettingsModal();
				privModalDiv.id= modalPrivateContentId;
				divModalContentBody.appendChild(privModalDiv);
				// - Format settings table 'tipsy's'
				$(function() {
				$('#'+ iURLElemTypsiId).tipsy({html: true, gravity: 'w'});
				});
				// - Reload settings table values
				streamProc_reloadSettings(url, reloadGenSettings);
				// Display the modal 
				modals_display_modal(parentNodeId, modalIdSufix);
			}

			function createGeneralSettingsModal()
			{
				// Modal private DIV
				var privModalDiv= document.createElement("div");

				var table= document.createElement("table");
				table.classList.add("settings");
				table.classList.add("streamProc");
				// Row: "Tag"
				var tr= document.createElement("tr");
				var td0= document.createElement("td");
				td0.classList.add("settings");
				td0.appendChild(document.createTextNode("Tag: "));
				var td1= document.createElement("td");
				td1.classList.add("settings");
				var tagInput= document.createElement("input");
				tagInput.classList.add("settings");
				tagInput.classList.add("streamProc");
				tagInput.type= "text";
				tagInput.size= 30;
				tagInput.id= tagElemId+ '_new';
				tagInput.maxLength= 16;
				tagInput.placeholder= "(max. 16 characters)";
				td1.appendChild(tagInput);
				tr.appendChild(td0);
				tr.appendChild(td1);
				table.appendChild(tr);
				// Row: "Input URL"
				var tr= document.createElement("tr");
				var td0= document.createElement("td");
				td0.classList.add("settings");
				td0.appendChild(document.createTextNode("Input URL: "));
				var a= document.createElement('a');
				a.href= "#";
				a.id= iURLElemTypsiId;
				a.appendChild(document.createTextNode("[?]"));
				a.title= ""
					+ "Set to a valid URL or left blank to close input stream " 
					+ "interface";
				a.onclick = function() {return false;};
				td0.appendChild(a);
				var td1= document.createElement("td");
				td1.classList.add("settings");
				// Create "input URL" embedded table
				var tableIURL= document.createElement("table");
				tableIURL.classList.add('settings');
				tableIURL.classList.add('embedded');
				var trIURL= document.createElement("tr");
				// Create sub-table row "URL scheme selector"
				var td0IURL= document.createElement("td");
				td0IURL.classList.add("settings");
				td0IURL.classList.add('embedded');
				td0IURL.classList.add("streamProc-iurl-scheme");
			    var select= document.createElement("select");
			    select.classList.add("settings");
			    select.classList.add("streamProc");
			    select.id= iURLElemId+ '_scheme_new';
			    var opt1= document.createElement("option");
			    opt1.setAttribute("value", "udp://");
			    opt1.appendChild(document.createTextNode("udp"));
			    opt1.setAttribute("selected", "selected");
			    select.appendChild(opt1);
			    td0IURL.appendChild(select);
			    // Create sub-table row "URL input"
				var td1IURL= document.createElement("td");
				td1IURL.classList.add("settings");
				td1IURL.classList.add('embedded');
				td1IURL.classList.add("streamProc-iurl-input");
				var iURL= document.createElement("input");
				iURL.classList.add("settings");
				iURL.classList.add("streamProc");
				iURL.type= "text";
				iURL.id= iURLElemId+ '_new';
				iURL.placeholder= "(e.g.: 234.5.5.5:2000)";
				td1IURL.appendChild(iURL);
				trIURL.appendChild(td0IURL);
				trIURL.appendChild(td1IURL);
				tableIURL.appendChild(trIURL);
				td1.appendChild(tableIURL);
				tr.appendChild(td0);
				tr.appendChild(td1);
				table.appendChild(tr);
				// - Append table
				privModalDiv.appendChild(table);

				// Submit new settings button
				var submitSettingsModalDiv= document.createElement("div");
				submitSettingsModalDiv.id= submitGeneralSettingsAlertModalDivId;
				privModalDiv.appendChild(submitSettingsModalDiv);
				var submitSettings= document.createElement("input");
				submitSettings.classList.add("button");
				submitSettings.classList.add("settings");
				submitSettings.classList.add("streamProc");
				submitSettings.classList.add("modal-button");
				submitSettings.type= "button";
				submitSettings.value= "Submit new settings";
				submitSettings.url= url;
				submitSettings.addEventListener('click', function(event)
				{
					// Get query-string values
					// - DEMUXER tag
					var tagValNew= 
						document.getElementById(tagElemId+ '_new').value;
					// - Input URL scheme
					var iURLSchemeSelElem= document.getElementById(iURLElemId+ 
							'_scheme_new');
					var iURLSchemeNew= iURLSchemeSelElem.options[
							iURLSchemeSelElem.selectedIndex].value;
					// - Input URL
					var iUrlValNew= 
						document.getElementById(iURLElemId+ '_new').value;
					if(iUrlValNew && iUrlValNew!= "")
						iUrlValNew= iURLSchemeNew+ iUrlValNew;

					// Compose query-string
					 var query_string= '?tag='+ tagValNew;
					 query_string+= '&input_url='+ iUrlValNew;

					// Compose callback object
					var externalCallbackObj= { 
						onResponse: function() {
							modals_close_modal(nodeId, modalIdSufix);
						},
						onError: function(alertString) {
							modals_alert_modal(
									submitGeneralSettingsAlertModalDivId, 
									alertString);
						}
					};

					// PUT new values
					httpRequests_respJSON(url, 'PUT', query_string, null, 
							externalCallbackObj);
				});
				privModalDiv.appendChild(submitSettings);

				return privModalDiv;
			}

			function reloadGenSettings(streamProc_json)
			{
				// - "Input URL"
				var iUrlValJSON= streamProc_json.settings.input_url;
				iUrlValJSON= 
					iUrlValJSON.substring(iUrlValJSON.indexOf("://")+ 3);
				if(!iUrlValJSON) iUrlValJSON= "";
				document.getElementById(iURLElemId+ '_new').value= iUrlValJSON;
				// - "Tag"
				var tagValJSON= streamProc_json.settings.tag;
				if(!tagValJSON) tagValJSON= "";
				document.getElementById(tagElemId+ '_new').value= tagValJSON;
			}
		}

		function updateNode()
		{
			// Only update if current node is not hidden
			if(nodeDiv.classList.contains('hide'))
				return;

			// Update streamProc node variable values
			// - "Bit-rate"
			var currIBitrateElem= document.getElementById(iBitrateElemId);
			var iBitrateValJSON= streamProc_json.input_bitrate;
			if(currIBitrateElem.innerHTML!= iBitrateValJSON)
				currIBitrateElem.innerHTML= iBitrateValJSON;
			// - "Input buffer level [%]"
			var currIBufLevelElem= document.getElementById(iBufLevelElemId);
			var iBufLevelJSON= streamProc_json.input_buf_level;
			if(currIBufLevelElem.innerHTML!= iBufLevelJSON)
				currIBufLevelElem.innerHTML= iBufLevelJSON;
			// - "Input URL"
			var currinputURLElem= document.getElementById(iURLElemId);
			var iUrlValJSON= streamProc_json.settings.input_url;
			if(currinputURLElem.innerHTML!= iUrlValJSON)
				currinputURLElem.innerHTML= iUrlValJSON? iUrlValJSON: "";
			// - "Tag"
			var currTagElem= document.getElementById(tagElemId);
			var tagValJSON= streamProc_json.settings.tag;
			if(currTagElem.innerHTML!= tagValJSON)
				currTagElem.innerHTML= tagValJSON;
			// -Traces
			drawTracesRows();
		}

		function drawTracesRows()
		{
			var nodeDiv= document.getElementById(tracesNodeId);

			// Only update DIV if current node is not hidden
			if(nodeDiv.classList.contains('hide'))
				return;

			//console.log("Updating traces!"); //comment-me
			var tbody= document.getElementById(tracesNodeId+ 
					'_traces_table_tbody');
			var tbody_new= document.createElement('table'/*'tbody'*/);
			tbody_new.classList.add("streamProc-traces");
			tbody_new.classList.add("tbody-traces-table");
			tbody_new.id= tracesNodeId+ '_traces_table_tbody';
			// - Append trace lines to table
			var logTraces= streamProc_json.log_traces;
			for(var i= 0; i< logTraces.length; i++) {
				var trace= logTraces[i];
				var tr= document.createElement("tr");
				var td0= document.createElement("td");
				td0.classList.add("streamProc-traces");
				td0.classList.add("unselect");
				td0.style.width= "65%";
				var desc= trace.log_trace_desc;
				if(desc=== "Assertion failed.\n" || 
						desc=== "Check point failed.\n")
					desc= "Description not available (please report code)";
				td0.appendChild(document.createTextNode(desc));
				var td1= document.createElement("td");
				td1.classList.add("streamProc-traces");
				td1.classList.add("unselect");
				td1.style.width= "5%";
				td1.appendChild(document.createTextNode(
						trace.log_trace_counter));
				var td2= document.createElement("td");
				td2.classList.add("streamProc-traces");
				td2.classList.add("unselect");
				td2.style.width= "15%";
				td2.appendChild(document.createTextNode(trace.log_trace_date));
				var td3= document.createElement("td");
				td3.classList.add("streamProc-traces");
				td3.classList.add("unselect");
				td3.style.width= "15%";
				td3.appendChild(document.createTextNode(trace.log_trace_code));
				tr.appendChild(td0);
				tr.appendChild(td1);
				tr.appendChild(td2);
				tr.appendChild(td3);
				tbody_new.appendChild(tr);
			}
			tbody.parentNode.replaceChild(tbody_new, tbody);

			// Adjust header width to avoid scroll-bar
			document.getElementById(tracesNodeId+ '_traces_table').style.width= 
				tbody_new.clientWidth;
		}

		function drawProgramNodesLinks()
		{
			nodes_update(nodeId, streamProc_json.programs, programLinkClass, 
					program_erase_node, program_manager, update_link);

			function update_link(linkNode, currListJSON_nth)
			{
				// **** Update text in link ****
		    	var progNumJSON= currListJSON_nth.program_number;
		    	var serviceNameJSON= currListJSON_nth.service_name;
		    	var linkTxtJSON= "program "+ progNumJSON;
		    	linkTxtJSON+= serviceNameJSON? ": "+ serviceNameJSON: 
		    		" (No service name available)";
		    	//console.log("linkTxtJSON: "+ linkTxtJSON); // comment-me
		    	if(linkNode.innerHTML!= linkTxtJSON)
		    		linkNode.innerHTML= linkTxtJSON;

		    	// **** Update indications in link ****
				// Add or remove CSS class to highlight as "being processed."
				if(currListJSON_nth.hasBeenDisassociated) {
					linkNode.classList.remove('processing');
					linkNode.classList.add('disassociated');
				} else {
					linkNode.classList.remove('disassociated');
					if(currListJSON_nth.isBeingProcessed)
						linkNode.classList.add('processing');
					else
						linkNode.classList.remove('processing');
				}
			}
		}
	}
}

function streamProc_delete(url)
{
	// Request deletion of existent DEMUXER
	loadXMLDoc('DELETE', '/api/1.0'+ url, null, streamProc_delete_callback);

	function streamProc_delete_callback(response)
	{
		// Check message status
		var response_json= JSON.parse(response);
		if(response_json.code!= '200' && response_json.code!= '404') {
			console.error("Unexpected error while deleting streamProc");
			return;
		}
	}
}

function streamProc_reloadSettings(url, reloadSettings2)
{
	// Request representational state
	httpRequests_respJSON(url, 'GET', '', null, {onResponse: reloadSettings2});
}

function streamProc_erase_node(nodeId) {
	nodes_erase(nodeId);
}
