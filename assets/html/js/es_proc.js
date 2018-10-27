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
 * @file es_proc.js
 */

var esProcLinkClass= 'es-proc-dropdown';
var esProcDetailsClass= 'es-proc-details-dropdown';
var esProcTagList= []; // Filled (redefined) in execution time

/**
 * @param url URL of the ES-processor link.
 * Nomenclature for the URL:
 * /demuxers/ID1/program_processors/ID2/es_bypass/ID3.json'
 */
function es_proc_manager(url)
{
	// GET node REST JSON
	httpRequests_respJSON(url, 'GET', '', null, {onResponse: drawNode});

	function drawNode(es_proc_json)
	{
		var esProcTag= es_proc_json.settings.extension_type;
		var nodeUrlSchemeTag= '/es_processors/';
		var nodeId;
		var nodeDiv;
		var parentNodeId;

		// Compose HTML id for this node
		nodeId= utils_url2id(url);

		// Compose HTML id for the parent node
		// Note that we are attaching ES-processors nodes within the 
		// elementary stream nodes. Parent node format: 
		// '-demuxers-ID1-programs-ID2-elementary_streams-ID3'
		var demuxerId= utils_findIDinUrl(url, '/demuxers/');
		var programId= utils_findIDinUrl(url, '/program_processors/');
		var esProcId= es_proc_json.id;
		parentNodeId= '-demuxers-'+ demuxerId+ '-programs-'+ programId+ 
			'-elementary_streams-'+ esProcId;

		// Compose local elements Ids.
		var esProcExtensionTypeElemId= nodeId+ '_settings_extension_type';
		var submitExtensionTypeConfModalDivId= esProcExtensionTypeElemId+ 
				"_conf_modal_submit";
		var iBitrateElemId= nodeId+ "_input_bitrate";
		var oBitrateElemId= nodeId+ "_output_bitrate";
		var oBufLevelElemId= nodeId+ "_output_buf_level";
		var timeStampsPlotTypsiId= nodeId+ "_time_stamps_plot_typsi";
		var timeStampsPlotDivId= nodeId+ "_time_stamps_plot_div";
		var generalSettingsDivId= nodeId+ '_general_settings';
		var enableOutputElemId= generalSettingsDivId+ "_enable_output";
		var timeShiftOffsetElemId= generalSettingsDivId+ "_time_shift_offset";
		var tsPCRGuardElemId= generalSettingsDivId+ "_ts_pcr_guard_msec";
		var tsPCRGuardTypsiId= tsPCRGuardElemId+ "_typsi";
		var tsPCRGuardTypsiTxt= ""
			+ "Guard distance, in milliseconds, to be applied between the "
			+ "time-stamps of the elementary stream and its associated PCR. "
			+ "This value sets the specific guard applicable to this "
			+ "elementary stream (maximum guard limit value is set at the "
			+ "program-processor level advanced settings). "
			+ "Valid settable range (milliseconds): "
			+ "-2000 to program's maximum setting. "
			+ "Default: depends on stream type.<br/>";
		var enablePESRestampingElemId= generalSettingsDivId+ 
				"_enable_pes_restamping";
		var servicesDivNodeIdnodeId= nodeId+ '_services_div';

		// Some locals
		var iBitrate= es_proc_json.input_bitrate;
		var oBitrate= es_proc_json.output_bitrate;
		var oBufLevel= es_proc_json.output_buf_level;
		var timeStampsStats= es_proc_json.time_stamp_stats;
		var enableOutput= es_proc_json.settings.flag_enable_interl_output;
		var timeShiftOffset= es_proc_json.settings.time_shift_offset_msec;
		var tsPCRGuard= es_proc_json.settings.ts_pcr_guard_msec;
		var enablePESRestamping= es_proc_json.settings.restamping;

		// Create or update node division 
		nodeDiv= document.getElementById(nodeId);
		if(!nodeDiv) {
			// Create ES-processor node
			nodeDiv= createNode();
		} else {
			// Update ES-processor node
			updateNode();
		}

		// Draw specific processor services nodes
		if(esProcTag== 'dvb_subt') {
			dvb_subt_draw_services_nodes(es_proc_json, nodeId);
		}

		function createNode()
		{
			// Create node link and division
			var nodeTxt= "Elementary stream processor";
			nodeDiv= nodes_create(nodeId, parentNodeId, esProcLinkClass, 
					nodeUrlSchemeTag, nodeTxt, true);

			// Create "Force processor type" selector table
			nodeDiv.appendChild(document.createElement("br"));
			var submitModalDiv= document.createElement("div");
			submitModalDiv.id= submitExtensionTypeConfModalDivId;
			nodeDiv.appendChild(submitModalDiv);
			var table= document.createElement("table");
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("es");
			td0.classList.add("es-proc-type-selection");
			td0.appendChild(document.createTextNode("Processor type: "));
			var td1= document.createElement("td");
			td1.classList.add("es");
			td1.classList.add("es-proc-type-selection");
		    var select= document.createElement("select");
		    select.classList.add("es");
		    select.id= esProcExtensionTypeElemId;
            // Property 'extensionTypeSet' it is used to avoid updating to
            // interfere changing selection. Note that this property is set
            // only once in "creation" and never updated until a new creation
            // is executed.
		    select.extensionTypeSet= 
		    	esProcTag; // Used to avoid update while changing!
		    select.addEventListener("change", function(event){
		    	// Confirm operation
		    	var selectedExtensionType= event.target.options[
		    			event.target.selectedIndex].value;
				var confirmationEventObj= {};
				var cancelEventObj= {};
				var modalTxt= 
		    		"You are about to force the elementary stream processor " +
		    		"type (from '"+ esProcTag+ "' to '"+ 
		    		selectedExtensionType+ "'). " +
		    		"All the current processing settings for this elementary " +
		    		"stream will be lost.";
		    	modals_confirmation_modal(submitExtensionTypeConfModalDivId, 
		    			["alert"], modalTxt, 
		    		function(confirmationEventObj) {
		    			// Erase node as we are modifying extension type
		    			nodes_erase(nodeId);
		    			// PUT new extension
		    			httpRequests_respJSON(url, 'PUT', '?extension_type='+ 
		    					selectedExtensionType, null, null);
		    		}, confirmationEventObj,
		    		function(cacelEventOb){
		    			for(var i= 0; i< event.target.options.length; i++) {
		    		        var option= event.target.options[i];
		    		        if(option.value== esProcTag) {
		    		        	option.setAttribute("selected", "selected");
		    		        	option.selected= "true";
		    		        	break;
		    		        }
		    		    }
		    		}, cancelEventObj);
			});
		    for(var i= 0; i< esProcTagList.length; i++) {
		    	var optValue= esProcTagList[i];
		        var option= document.createElement("option");
		        option.setAttribute("value", optValue);
		        option.appendChild(document.createTextNode(optValue));
		        if(optValue== esProcTag) {
		        	option.setAttribute("selected", "selected");
		        }
		        select.appendChild(option);
		    }
			td1.appendChild(select);
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// - Append table
			nodeDiv.appendChild(table);
			nodeDiv.appendChild(document.createElement("br"));

			// Create ES-processor basic information table
			var table= document.createElement("table");
			// - Row: "Input bit-rate"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("es");
			td0.appendChild(document.createTextNode("Input bitrate [bps]: "));
			var td1= document.createElement("td");
			td1.classList.add("es");
			td1.classList.add("unselect");
			td1.appendChild(document.createTextNode(iBitrate));
			td1.id= iBitrateElemId;
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// - Row: "Output bit-rate"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("es");
			td0.appendChild(document.createTextNode("Output bitrate [bps]: "));
			var td1= document.createElement("td");
			td1.classList.add("es");
			td1.classList.add("unselect");
			td1.appendChild(document.createTextNode(oBitrate));
			td1.id= oBitrateElemId;
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// - Row: "Output buffer level (interleaving buffer)"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("es");
			td0.appendChild(document.createTextNode("Output buffer level " +
					"(interleaving buffer) [%]: "));
			var td1= document.createElement("td");
			td1.classList.add("es");
			td1.classList.add("unselect");
			td1.appendChild(document.createTextNode(oBufLevel));
			td1.id= oBufLevelElemId;
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// - Append table
			nodeDiv.appendChild(table);
			nodeDiv.appendChild(document.createElement("br"));

			// Create "TS plot" table if applicable
			if(esProcTag!= 'es_bypass') {
				var table= document.createElement("table");
				// - Append table (Do it before ploting)
				nodeDiv.appendChild(table);
				nodeDiv.appendChild(document.createElement("br"));
				// Table title row
				var tr= document.createElement("tr");
				var td0= document.createElement("td");
				td0.classList.add("es");
				td0.appendChild(document.createTextNode("Input/output time " +
						"stamps [msecs/sec]: "));
				var a= document.createElement('a');
				a.href= "#";
				a.id= timeStampsPlotTypsiId;
				a.appendChild(document.createTextNode("[?]"));
				a.title= ""
				+ "The dots in the graphic depict the time stamp (TS) values "
				+ "evolution for this elementary stream (ES), at the input "
				+ "and output of the processor.<br/>"
				+ "TS values are normalized by substracting the system time "
				+ "clock (STC) value (the horizontal zero axis represents "
				+ "the STC).<br/>";
				a.onclick = function() {return false;};
				td0.appendChild(a);
				tr.appendChild(td0);
				table.appendChild(tr);
				// TSs plot placeholder row
				var plotWidth= "360px";
				var plotHeigth= "240px";
				var tr= document.createElement("tr");
				var td0= document.createElement("td");
				td0.classList.add("es");
				var div= document.createElement("div");
				div.id= nodeId+ "_placeholder_time_stamps";
				div.style.width= plotWidth;
				div.style.height= plotHeigth;
				td0.appendChild(div);
				var td1= document.createElement("td");
				td1.classList.add("es");
				td1.classList.add("es-proc-placeholder-time-stamps-selection");
				var div= document.createElement("div");
				div.appendChild(document.createTextNode("Curve selection: "));
				div.id= timeStampsPlotDivId;
				// Erase log register button
				var submitEraseTSRegisters= document.createElement("input");
				submitEraseTSRegisters.classList.add("es");
				submitEraseTSRegisters.type= "button";
				submitEraseTSRegisters.value= "Reset TS graphs";
				submitEraseTSRegisters.addEventListener('click', 
				function(event){
					httpRequests_respJSON(url, 'PUT', 
							'?flag_clear_ts_registers=true', null, null);
				});
				td1.appendChild(div);
				var div= document.createElement("div");
				div.appendChild(submitEraseTSRegisters);
				td1.appendChild(div);
				tr.appendChild(td0);
				tr.appendChild(td1);
				table.appendChild(tr);
				// - Insert checkboxes to select which plot curves to plot
				var choiceContainer= $("#"+ timeStampsPlotDivId);
				$.each(timeStampsStats, function(key, val) {
					choiceContainer.append("<br/><input type='checkbox' name='" 
					+ key + "' checked='checked' id='id" + key + "'></input>"
					+ "<label for='id" + key + "'>"
					+ val.label + "</label>");
				});
				// - Format table 'tipsy's'
				$(function() {
					$('#'+ timeStampsPlotTypsiId).tipsy(
							{html: true, gravity: 'w'});
				});
			}

			// ************************************************************** //
			// Create general settings node
			// ************************************************************** //
			var generalSettingsDiv= nodes_simpleDropDownCreate(
					generalSettingsDivId, nodeId, esProcDetailsClass, 
					"General settings", true);
			var table= document.createElement("table");
			table.classList.add("settings");
			table.classList.add("es");
			// - Table caption: "Settings"
			var caption= document.createElement("caption");
			caption.classList.add("settings");
			table.appendChild(caption);
			// -- Row: "enable/disable" output
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("settings");
			td0.appendChild(document.createTextNode("Enable elementary " +
					"stream output: "));
			var td1= document.createElement("td");
			td1.classList.add("settings");
			td1.classList.add("unselect");
			td1.id= enableOutputElemId;
			td1.appendChild(document.createTextNode(enableOutput? "enabled": 
				"disabled"));
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// Row: "Time shift offset"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("settings");
			td0.appendChild(document.createTextNode(
					"Time shift offset [msecs]: "));
			var td1= document.createElement("td");
			td1.classList.add("settings");
			td1.classList.add("unselect");
			td1.id= timeShiftOffsetElemId;
			td1.appendChild(document.createTextNode(timeShiftOffset));
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// Row: "TS-PCR guard"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("settings");
			td0.appendChild(document.createTextNode(
					"Time-stamps over PCR guard [msec]: "));
			var a= document.createElement('a');
			a.href= "#";
			a.id= tsPCRGuardTypsiId;
			a.appendChild(document.createTextNode("[?]"));
			a.title= tsPCRGuardTypsiTxt;
			a.onclick = function() {return false;};
			td0.appendChild(a);
			var td1= document.createElement("td");
			td1.classList.add("settings");
			td1.classList.add("unselect");
			td1.id= tsPCRGuardElemId;
			td1.appendChild(document.createTextNode(tsPCRGuard));
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// Specific processor's rows
			if(esProcTag== 'dvb_subt') { //FIXME!!
				//dvb_subt_generalSettings(es_proc_json, generalSettingsDiv, 
				//		generalSettingsDivId, url);
				// -- Row: "enable/disable" restamping
				var tr= document.createElement("tr");
				var td0= document.createElement("td");
				td0.classList.add("settings");
				td0.appendChild(document.createTextNode(
						"Enable PES restamping"));
				var td1= document.createElement("td");
				td1.classList.add("settings");
				td1.classList.add("unselect");
				td1.id= enablePESRestampingElemId;
				td1.appendChild(document.createTextNode(enablePESRestamping));
				tr.appendChild(td0);
				tr.appendChild(td1);
				table.appendChild(tr);
			}
			// - Row: Edit settings
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("settings");
			var submitSettings= document.createElement("input");
			submitSettings.classList.add("settings");
			submitSettings.classList.add("button");
			submitSettings.classList.add("es");
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
			// - Format table 'tipsy's'
			$(function() {
				$('#'+ tsPCRGuardTypsiId).tipsy({html: true, gravity: 'w'});
			});
	 
			// ************************************************************** //
			// Append processor-specific data
			// ************************************************************** //
			if(esProcTag== 'dvb_subt') {
				dvb_subt_settings(es_proc_json, nodeDiv, nodeId, url);
			} else if(esProcTag== 'scte_subt2dvb') {
				scte_subt2dvb_settings(es_proc_json, nodeDiv, nodeId, url);
			}

			// Create ES-processor services placeholder
			var servicesPH= document.createElement("div");
			servicesPH.id= nodeId+ '-services_placeholder';
			nodeDiv.appendChild(servicesPH);
			// Create list of services links inside specific div to be able to 
			// search by class
			ul= document.createElement("ul");
			ul.className= esProcDetailsClass;
			servicesPH.appendChild(ul);

			return nodeDiv;
		}

		function generalSettingsModal(parentNodeId)
		{
			var modalIdSufix= "generalSettings";
			var modalPrivateContentId= parentNodeId+ '_generalSettingsPrivate';

			var modalObj= modals_create_generic_modal(parentNodeId, 
					modalIdSufix, ["settings", "es"]);
			if(modalObj== null) {
				// Modal already exist, update body content
				httpRequests_respJSON(url, 'GET', '', null, 
						{onResponse: reloadGeneralSettings});
				// - Display the modal 
				modals_display_modal(parentNodeId, modalIdSufix);
			} else {
				// Create private modal content
				var divModalContentBody= modalObj.bodyDiv;
				var divModalContentHeader= modalObj.headerDiv;
				// Header
				divModalContentHeader.appendChild(document.createTextNode(
						"Elementary stream processor general settings"));
				// Body
				var privModalDiv= createGeneralSettingsModal();
				privModalDiv.id= modalPrivateContentId;
				divModalContentBody.appendChild(privModalDiv);
				// - Reload settings table values
				httpRequests_respJSON(url, 'GET', '', null, 
						{onResponse: reloadGeneralSettings});
				// Display the modal 
				modals_display_modal(parentNodeId, modalIdSufix);
				// - Format table 'tipsy's'
				$(function() {
					$('#'+ tsPCRGuardTypsiId+ '_new').tipsy(
							{html: true, gravity: 'w'});
				});
			}

			function createGeneralSettingsModal()
			{
				// Modal private DIV
				var privModalDiv= document.createElement("div");

				// Settings table
				var table= document.createElement("table");
				table.classList.add("settings");
				table.classList.add("es");
				// -- Row: "enable/disable" output
				var tr= document.createElement("tr");
				var td0= document.createElement("td");
				td0.classList.add("settings");
				td0.appendChild(document.createTextNode(
						"Enable elementary stream output: "));
				var td1= document.createElement("td");
				td1.classList.add("settings");
				var checkbox= document.createElement("input");
				checkbox.classList.add("settings");
				checkbox.classList.add("es");
				checkbox.classList.add("es-proc-enable-output");
				checkbox.setAttribute("type", "checkbox");
				checkbox.id= enableOutputElemId+ '_new';
				checkbox.checked= enableOutput? true: false;
				td1.appendChild(checkbox);
				tr.appendChild(td0);
				tr.appendChild(td1);
				table.appendChild(tr);
				// Row: "Time shift offset"
				var tr= document.createElement("tr");
				var td0= document.createElement("td");
				td0.classList.add("settings");
				td0.appendChild(document.createTextNode(
						"Time shift offset [msecs]: "));
				var td1= document.createElement("td");
				td1.classList.add("settings");
				var timeShiftInput= document.createElement("input");
				timeShiftInput.classList.add("settings");
				timeShiftInput.classList.add("es");
				timeShiftInput.type= "text";
				timeShiftInput.id= timeShiftOffsetElemId+ '_new';
				timeShiftInput.value= timeShiftOffset;
				td1.appendChild(timeShiftInput);
				tr.appendChild(td0);
				tr.appendChild(td1);
				table.appendChild(tr);
				// Row: "TS-PCR guard"
				var tr= document.createElement("tr");
				var td0= document.createElement("td");
				td0.classList.add("settings");
				td0.appendChild(document.createTextNode(
						"Time-stamps over PCR guard [msec]: "));
				var a= document.createElement('a');
				a.href= "#";
				a.id= tsPCRGuardTypsiId+ '_new';
				a.appendChild(document.createTextNode("[?]"));
				a.title= tsPCRGuardTypsiTxt;
				a.onclick = function() {return false;};
				td0.appendChild(a);
				var td1= document.createElement("td");
				td1.classList.add("settings");
				var input= document.createElement("input");
				input.classList.add("settings");
				input.classList.add("es");
				input.type= "text";
				input.id= tsPCRGuardElemId+ '_new';
				input.value= tsPCRGuard;
				td1.appendChild(input);
				tr.appendChild(td0);
				tr.appendChild(td1);
				table.appendChild(tr);
				if(esProcTag== 'dvb_subt') {
					//FIXME!!
					// -- Row: "enable/disable" PES restamping
					var tr= document.createElement("tr");
					var td0= document.createElement("td");
					td0.classList.add("settings");
					td0.appendChild(document.createTextNode(
							"Enable PES restamping"));
					var td1= document.createElement("td");
					td1.classList.add("settings");
					var select= document.createElement("select");
					select.classList.add("es");
					select.classList.add("settings");
					select.id= enablePESRestampingElemId+ '_new';
					var option= document.createElement("option");
					option.appendChild(document.createTextNode("enable"));
					option.setAttribute("value", "enable");
					select.appendChild(option);
					var option= document.createElement("option");
					option.appendChild(document.createTextNode("disable"));
					option.setAttribute("value", "disable");
					select.appendChild(option);
					var option= document.createElement("option");
					option.appendChild(document.createTextNode("auto"));
					option.setAttribute("value", "auto");
					select.appendChild(option);
					td1.appendChild(select);
					select.value= "disable"; // Set default
					tr.appendChild(td0);
					tr.appendChild(td1);
					table.appendChild(tr);
				}
				// - Append table
				privModalDiv.appendChild(table);

				// Submit new settings button
				var submitSettings= document.createElement("input");
				submitSettings.classList.add("button");
				submitSettings.classList.add("settings");
				submitSettings.classList.add("es");
				submitSettings.classList.add("modal-button");
				submitSettings.type= "button";
				submitSettings.value= "Submit new settings";
				submitSettings.addEventListener('click', function(event)
				{
					// - Output-enable flag
					var enableOutputElem= document.getElementById(
							enableOutputElemId+ '_new');
					var enableOutput= enableOutputElem.checked? true: false;
					// - Time-shift offset
					var timeShiftOffsetElem= document.getElementById(
							timeShiftOffsetElemId+ '_new');
					var timeShiftOffset= timeShiftOffsetElem.value;
					// - Bound time-shift offset to limits
					if(timeShiftOffset> 2147483647) {
						// Int32 maximum
						timeShiftOffsetElem.value= timeShiftOffset= 2147483647;
					} else if(timeShiftOffset< -2147483648) {
						// Int32 minimum
						timeShiftOffsetElem.value= timeShiftOffset= -2147483648;
					}
					// - TS-PCR guard
					var tsPCRGuardElem= document.getElementById(
							tsPCRGuardElemId+ '_new');
					var tsPCRGuard= tsPCRGuardElem.value;
					// - Bound value to limits
					if(tsPCRGuard> 32767) {
						// Int32 maximum
						tsPCRGuardElem.value= tsPCRGuard= 32767;
					} else if(tsPCRGuard< -32767) {
						// Int32 minimum
						tsPCRGuardElem.value= tsPCRGuard= -32767;
					}

					// Compose query-string
					var query_string= '?flag_enable_interl_output='+ 
							enableOutput;
					query_string+= '&time_shift_offset_msec='+ timeShiftOffset;
					query_string+= '&ts_pcr_guard_msec='+ tsPCRGuard;

					// Compose body-string
					var body_string= '';

					if(esProcTag== 'dvb_subt') {
						//FIXME!!
						// - PES-restamping-enable flag
						var enablePESRestampingElem= document.getElementById(
								enablePESRestampingElemId+ '_new');
						query_string+= '&restamping='+ 
							enablePESRestampingElem.value;
					}

					// Compose callback object
					var externalCallbackObj= { 
						onResponse: function() {
							modals_close_modal(nodeId, modalIdSufix);
						}
					};

					// PUT new values
					httpRequests_respJSON(url, 'PUT', query_string, 
							body_string, externalCallbackObj);
				});
				privModalDiv.appendChild(submitSettings);

				return privModalDiv;
			}

			function reloadGeneralSettings(es_proc_json)
			{
				// "Output enabling/disabling flag"
				document.getElementById(enableOutputElemId+ '_new').checked= 
						es_proc_json.settings.flag_enable_interl_output? true: 
							false;
				// "Time shift offset"
				document.getElementById(timeShiftOffsetElemId+ '_new').value= 
					es_proc_json.settings.time_shift_offset_msec;
				// "TS-PCR guard"
				document.getElementById(tsPCRGuardElemId+ '_new').value= 
					es_proc_json.settings.ts_pcr_guard_msec;

				if(esProcTag== 'dvb_subt') {
					//FIXME!!
					// - PES-restamping-enable flag
					var val= es_proc_json.settings.restamping;
					if(val== "disabled") val= "disable";
					if(val== "enabled") val= "enable";
					document.getElementById(enablePESRestampingElemId+ '_new').
						value= val;
				}
			}
		}

		function updateNode()
		{
			// Only update if current node is not hidden
			if(nodeDiv.classList.contains('hide'))
				return;

			// Update node variable values
			// - "Processor type"
	    	var esProcExtensionTypeElem= document.getElementById(
	    			esProcExtensionTypeElemId);
	    	var extensionType= esProcExtensionTypeElem.extensionTypeSet;
	    	if(extensionType!= es_proc_json.settings.extension_type)
	    		nodes_erase(nodeId);
			// -"Input bit-rate"
			var iBitrateElem= document.getElementById(iBitrateElemId);
			if(iBitrateElem.innerHTML!= iBitrate)
				iBitrateElem.innerHTML= iBitrate;
			// -"Output bit-rate"
			var oBitrateElem= document.getElementById(oBitrateElemId);
			if(oBitrateElem.innerHTML!= oBitrate)
				oBitrateElem.innerHTML= oBitrate;
			// - "Output buffer level (interleaving buffer)"
			var oBufLevelElem= document.getElementById(oBufLevelElemId);
			if(oBufLevelElem.innerHTML!= oBufLevel)
				oBufLevelElem.innerHTML= oBufLevel;
			// - Time-stamp plots
			if(esProcTag!= 'es_bypass')
				es_proc_plot_time_stamp();

			if(!document.getElementById(generalSettingsDivId).classList.
					contains('hide')) {
				//console.log("updating general settings..."); //comment me
				// - "Enable/disable output"
				var enableOutputElem= document.getElementById(
						enableOutputElemId);
				var val= enableOutput? "enabled": "disabled";
				if(enableOutputElem.innerHTML!= val)
					enableOutputElem.innerHTML= val;
				// - "Time shift offset"
				var timeShiftOffsetElem= document.getElementById(
						timeShiftOffsetElemId);
				if(timeShiftOffsetElem.innerHTML!= timeShiftOffset)
					timeShiftOffsetElem.innerHTML= timeShiftOffset;
				// - "TS-PCR guard"
				var tsPCRGuardElem= document.getElementById(tsPCRGuardElemId);
				if(tsPCRGuardElem.innerHTML!= tsPCRGuard)
					tsPCRGuardElem.innerHTML= tsPCRGuard;

				if(esProcTag== 'dvb_subt') {
					//FIXME!!
					// - PES-restamping-enable flag
					var enablePESRestampingElem= document.getElementById(
							enablePESRestampingElemId);
					if(enablePESRestampingElem.innerHTML!= enablePESRestamping)
						enablePESRestampingElem.innerHTML= enablePESRestamping;
				}
			}

			if(esProcTag== 'dvb_subt') {
				dvb_subt_update_node(es_proc_json, nodeId);
			} else if(esProcTag== 'scte_subt2dvb') {
				scte_subt2dvb_update_node(es_proc_json, nodeId);
			}
		}

		function es_proc_plot_time_stamp()
		{
			var data= [];
			var choiceContainer= $("#"+ timeStampsPlotDivId);

			// - Hard-code color indices to prevent curves from shifting as
			//   these are turned on/off
			var i= 0;
			$.each(timeStampsStats, function(key, val) {
				val.color= i;
				++i;
			});

			// Check which curve is to be drawn
			if(!choiceContainer.find("input:checked").length){
				data.length= 0; // empty array
			} else {
				choiceContainer.find("input:checked").each(function () {
					var key = $(this).attr("name");
					if (key && timeStampsStats[key]) {
						data.push(timeStampsStats[key]);
					}
				});		
			}

			// Draw data
			// - Correct vertical axis scale
			var max_xaxis= timeStampsStats.input.data.length;
			var maxyaxis= 1;
			var minyaxis= -1;
			var currMaxDistance= 0;
			for(var i= 0; i< data.length; i++) {
				var yaxis_0= Math.abs(data[i].data[0][1]);
				if(yaxis_0> currMaxDistance)
					currMaxDistance= yaxis_0;

				for(var j= 0; j< max_xaxis; j++) {
					var yaxis_i= data[i].data[j][1];
					//console.debug("maxyaxis: "+ maxyaxis_i); // comment-me
					if(yaxis_i> maxyaxis)
						maxyaxis= yaxis_i;
					else if(yaxis_i< minyaxis)
						minyaxis= yaxis_i;
		    	}
			}
			maxyaxis= maxyaxis*1.5;
			minyaxis= minyaxis*1.5;
			// - Setup plot options
			var options= {
					points: { show: true/*, radius: 10, lineWidth: 4*/, 
						fill: false },
					lines: { show: false},
					series: {shadowSize: 0}, // faster without shadows
					yaxis: {min: minyaxis, max: maxyaxis},
					xaxis: {max: max_xaxis, min: 0}
			};
			// - Draw
			//if(data.length> 0) { // better to plot empty graph if applicable
				$.plot("#"+ nodeId+ "_placeholder_time_stamps", data, options);
			//}
		}
	}
}

function es_proc_erase_node(nodeId)
{
	nodes_erase(nodeId);
}
