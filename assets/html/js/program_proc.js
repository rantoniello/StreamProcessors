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
 * @file program_proc.js
 */

var programProcLinkClass= 'program-proc-dropdown';

function program_proc_manager(url)
{
	// GET node REST JSON
	httpRequests_respJSON(url, 'GET', '', null, {onResponse: drawNode});

	function drawNode(program_proc_json)
	{
		var nodeUrlSchemeTag= '/program_processors/';
		var programProcId= program_proc_json.id;
		var nodeId;
		var nodeDiv;
		var parentNodeId;
		var parentNode;

		// Get list of elementary stream (ES) processors supported
		// Note that 'esProcTagList' is defined globally at 'es_proc.js'
		esProcTagList= program_proc_json.es_processor_extension_types;

		// Compose HTML id for this node
		nodeId= utils_url2id(url);

		// Compose HTML id for the parent node
		// Note that we are attaching program processor node within the program 
		// node.
		// Parent node format: '-demuxers-ID1-programs-ID2'
		var demuxerId= utils_findIDinUrl(url, '/demuxers/');
		var programId= utils_findIDinUrl(url, '/program_processors/');
		parentNodeId= '-demuxers-'+ demuxerId+ '-programs-'+ programId;
		parentNode= document.getElementById(parentNodeId);
		// If parent node not defined yet we have nothing to do
		if(!parentNode)
			return;

		// Compose local elements Ids.
		var iBitrateElemId= nodeId+ "_input_bitrate";
		var iBitratePeakElemId= nodeId+ "_input_bitrate_peak";
		var oBitrateElemId= nodeId+ "_output_bitrate";
		var progProcTotalDelayElemId= nodeId+ "_prog_proc_total_delay";
		var streamingSettingsDivId= nodeId+ '_streaming_settings';
		var settingsBrctrlTypeId= nodeId+ 
				"_settings_selected_brctrl_type_value";
		var settingsBrctrlTargetValId= nodeId+ "_settings_brctrl_target_value";
		var oURLElemId= nodeId+ "_settings_ourl";
		var oURLElemTypsiId= nodeId+ "_settings_ourl_typsi";
		var submitStreamingSettingsAlertModalDivId= streamingSettingsDivId+ 
				"_alert_modal_submit";
		var advSettingsDivId= nodeId+ '_adv_settings';
		var settingsTSPCRGuardId= nodeId+ "_settings_ts_pcr_guard_msec";
		var settingsMinProgProcBufId= nodeId+ 
				"_settings_min_progproc_buffering_msec";
		var submitAdvSettingsAlertModalDivId= advSettingsDivId+ 
				"_alert_modal_submit";
		var tsPCRGuardElemTypsiId= settingsTSPCRGuardId+ "_typsi";
		var minProgProcBufElemTypsiId= settingsMinProgProcBufId+ "_typsi";

		// Some locals
		var iBitrate= program_proc_json.input_bitrate;
		var iBitratePeak= program_proc_json.input_bitrate_peak;
		var oBitrate= program_proc_json.output_bitrate;
		var progProcTotalDelay= program_proc_json.delay_offset;
		var settingsBrctrlType= program_proc_json.settings.brctrl_type_selector[
			program_proc_json.settings.selected_brctrl_type_value].name;
		var settingsBrctrlTargetVal= program_proc_json.settings.cbr;
		var oURL= program_proc_json.settings.output_url;
		var settingsTSPCRGuard= 
			program_proc_json.settings.max_ts_pcr_guard_msec;
		var settingsMinProgProcBuf= 
			program_proc_json.settings.min_stc_delay_output_msec;

		// Create or update node division
		nodeDiv= document.getElementById(nodeId);
		if(!nodeDiv) {
			// Create program processor node
			nodeDiv= createNode();
		} else {
			// Update node
			updateNode();
		}

		function createNode()
		{
			// Create node link and division
			var nodeTxt= "Program processor";
			nodeDiv= nodes_create(nodeId, parentNodeId, programProcLinkClass, 
					nodeUrlSchemeTag, nodeTxt, true);
			nodeDiv.classList.add("program");

			// Create program processor basic information table
			var table= document.createElement("table");
			table.classList.add("program");
			// - Append table
			nodeDiv.appendChild(table);
			// - Row: "Input bit-rate"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("program");
			td0.appendChild(document.createTextNode("Input bitrate [Kbps]: "));
			var td1= document.createElement("td");
			td1.classList.add("program");
			td1.classList.add("unselect");
			td1.appendChild(document.createTextNode(iBitrate));
			td1.id= iBitrateElemId;
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// - Row: "Maximum bit-rate peak"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("program");
			td0.appendChild(document.createTextNode(
					"Input bitrate peak [Kbps]: "));
			var td1= document.createElement("td");
			td1.classList.add("program");
			td1.classList.add("unselect");
			td1.appendChild(document.createTextNode(iBitratePeak));
			td1.id= iBitratePeakElemId;
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// - Row: "Output bit-rate"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("program");
			td0.appendChild(document.createTextNode("Output bitrate [Kbps]: "));
			var td1= document.createElement("td");
			td1.classList.add("program");
			td1.classList.add("unselect");
			td1.appendChild(document.createTextNode(oBitrate));
			td1.id= oBitrateElemId;
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// - Row "Delay offset"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("program");
			td0.appendChild(document.createTextNode("Total delay introduced " +
					"by processor [msecs]: "));
			var td1= document.createElement("td");
			td1.classList.add("program");
			td1.classList.add("unselect");
			td1.appendChild(document.createTextNode(progProcTotalDelay));
			td1.id= progProcTotalDelayElemId;
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// - Row: Reset input bit-rate peak register button
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("settings"); 
			var submitEraseSetting= document.createElement("input");
			submitEraseSetting.classList.add("settings");
			submitEraseSetting.classList.add("button");
			submitEraseSetting.classList.add("program");
			submitEraseSetting.type= "button";
			submitEraseSetting.value= "Reset input bitrate peak register";
			submitEraseSetting.url= url;
			submitEraseSetting.addEventListener('click', function(event){
				httpRequests_respJSON(url, 'PUT', 
						'?flag_clear_input_bitrate_peak=true', null, null);
			});
			td0.appendChild(submitEraseSetting);
			var td1= document.createElement("td");
			td1.classList.add("settings");
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);

			// ************************************************************** //
			// Create streaming-settings node
			// ************************************************************** //
			var streamingSettingsDiv= nodes_simpleDropDownCreate(
					streamingSettingsDivId, nodeId, 'program-details-dropdown', 
					"Streaming settings", true);
			var table= document.createElement("table");
			table.classList.add("settings");
			table.classList.add("program");
			// - Table caption: "Settings"
			var caption= document.createElement("caption");
			caption.classList.add("settings");
			table.appendChild(caption);
			// Row: "Bit-rate control type"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("settings");
			td0.appendChild(document.createTextNode("Bitrate control type: "));
			var td1= document.createElement("td");
			td1.classList.add("settings");
			td1.classList.add("unselect");
			td1.id= settingsBrctrlTypeId;
			td1.appendChild(document.createTextNode(settingsBrctrlType));
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// Row: "Bit-rate control target value"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("settings");
			td0.appendChild(document.createTextNode(
					"CBR / CVBR-maximum value [Kbps]: "));
			var td1= document.createElement("td");
			td1.classList.add("settings");
			td1.classList.add("unselect");
			td1.id= settingsBrctrlTargetValId;
			td1.appendChild(document.createTextNode(settingsBrctrlTargetVal));
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// - Row: "Output URL"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("settings");
			td0.appendChild(document.createTextNode("Output URL: "));
			var td1= document.createElement("td");
			td1.classList.add("settings");
			td1.classList.add("unselect");
			td1.id= oURLElemId;
			td1.appendChild(document.createTextNode((!oURL)? "N/A": oURL));
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
			submitSettings.classList.add("program");
			submitSettings.type= "button";
			submitSettings.value= "Edit streaming settings";
			submitSettings.addEventListener('click', function(event){
				streamingSettingsModal(nodeId);
			});
			td0.appendChild(submitSettings);
			var td1= document.createElement("td");
			td1.classList.add("settings");
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// - Append table
			streamingSettingsDiv.appendChild(table);

			// ************************************************************** //
			// Create advanced-settings node
			// ************************************************************** //
			var advSettingsDiv= nodes_simpleDropDownCreate(advSettingsDivId, 
					nodeId, 'program-details-dropdown', "Advanced settings", 
					false);
			var table= document.createElement("table");
			table.classList.add('settings');
			table.classList.add('program');
			// Table caption: "Advanced Settings"
			var caption= document.createElement("caption");
			caption.classList.add("settings");
			table.appendChild(caption);
			// Row: "TS-PCR guard"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("settings");
			td0.appendChild(document.createTextNode("Time-stamps over PCR " +
					"guard max. [msec]: "));
			var td1= document.createElement("td");
			td1.classList.add("settings");
			td1.classList.add("unselect");
			td1.id= settingsTSPCRGuardId;
			td1.appendChild(document.createTextNode(settingsTSPCRGuard));
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// - Row: Minimum program-processor buffering
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("settings");
			td0.appendChild(document.createTextNode("Minimum " +
					"program-processor buffering [msec]: "));
			var td1= document.createElement("td");
			td1.classList.add("settings");
			td1.classList.add("unselect");
			td1.id= settingsMinProgProcBufId;
			td1.appendChild(document.createTextNode(settingsMinProgProcBuf));
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
			submitSettings.classList.add("program");
			submitSettings.type= "button";
			submitSettings.value= "Edit advanced settings";
			submitSettings.addEventListener('click', function(event){
				advSettingsModal(nodeId);
			});
			td0.appendChild(submitSettings);
			var td1= document.createElement("td");
			td1.classList.add("settings");
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// - Row: Reset to defaults button
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("settings");
			var submitDefaults= document.createElement("input");
			submitDefaults.classList.add("settings");
			submitDefaults.classList.add("button");
			submitDefaults.classList.add("program");
			submitDefaults.type= "button";
			submitDefaults.value= "Reset to defaults";
			submitDefaults.addEventListener('click', function(event){
				var pcrGuardVal= 300;
				var progProcMinBuf= 600;
				var query_string= '?max_ts_pcr_guard_msec='+ pcrGuardVal;
				query_string+= '&min_stc_delay_output_msec='+ progProcMinBuf;
				// PUT default values
				httpRequests_respJSON(url, 'PUT', query_string, null, null);
			});
			td0.appendChild(submitDefaults);
			var td1= document.createElement("td");
			td1.classList.add("settings");
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// - Append table
			advSettingsDiv.appendChild(table);

			return nodeDiv;
		}

		function streamingSettingsModal(parentNodeId)
		{
			var modalIdSufix= "streamingSettings";
			var modalPrivateContentId= parentNodeId+ 
					'_streamingSettingsPrivate';

			var modalObj= modals_create_generic_modal(parentNodeId, 
					modalIdSufix, ["settings", "program"]);
			if(modalObj== null) {
				// Modal already exist, update body content
				httpRequests_respJSON(url, 'GET', '', null, 
						{onResponse: reloadStreamingSettings});
				// - Display the modal 
				modals_display_modal(parentNodeId, modalIdSufix);
			} else {
				// Create private modal content
				var divModalContentBody= modalObj.bodyDiv;
				var divModalContentHeader= modalObj.headerDiv;
				// Header
				divModalContentHeader.appendChild(document.createTextNode(
						"Program processor streaming settings"));
				// Body
				var privModalDiv= createStreamingSettingsModal();
				privModalDiv.id= modalPrivateContentId;
				divModalContentBody.appendChild(privModalDiv);
				// - Format settings table 'tipsy's'
				$(function() {
					$('#'+ oURLElemTypsiId).tipsy({html: true, gravity: 'w'});
				});
				// - Reload settings table values
				httpRequests_respJSON(url, 'GET', '', null, 
						{onResponse: reloadStreamingSettings});
				// Display the modal 
				modals_display_modal(parentNodeId, modalIdSufix);
			}

			function createStreamingSettingsModal()
			{
				// Modal private DIV
				var privModalDiv= document.createElement("div");

				// Settings edition table
				var table= document.createElement("table");
				table.classList.add("settings");
				table.classList.add("program");
				// Row: "Bit-rate control type" selector
				var tr= document.createElement("tr");
				var td0= document.createElement("td");
				td0.classList.add("settings");
				td0.appendChild(document.createTextNode(
						"Bitrate control type: "));
				var td1= document.createElement("td");
				td1.classList.add("settings");
			    var brctrlTypeSelect= document.createElement("select");
			    brctrlTypeSelect.classList.add("settings");
			    brctrlTypeSelect.classList.add("program");
			    brctrlTypeSelect.id= settingsBrctrlTypeId+ '_new';
			    brctrlTypeSelect.addEventListener("change", function(event){
					// If type is "none", we disable target bit-rate input box
					var brctrlTargetInputElem= document.getElementById(
							settingsBrctrlTargetValId+ '_new');
					if(brctrlTypeSelect.options[brctrlTypeSelect.selectedIndex].
							innerHTML== "None") {
						brctrlTargetInputElem.value= "";
						brctrlTargetInputElem.disabled= true;
					} else {
						brctrlTargetInputElem.value= document.getElementById(
								settingsBrctrlTargetValId).innerHTML;
						brctrlTargetInputElem.disabled= false;
					}
				});
			    var brctrlTypeSelectorJSON= 
			    		program_proc_json.settings.brctrl_type_selector;
			    var selBrctrlTypeValue= 
			    		program_proc_json.settings.selected_brctrl_type_value;
			    for(var i= 0; i< brctrlTypeSelectorJSON.length; i++) {
			    	var optName= brctrlTypeSelectorJSON[i].name;
			    	var optValue= brctrlTypeSelectorJSON[i].value;
			        var option= document.createElement("option");
			        option.setAttribute("value", optValue);
			        option.appendChild(document.createTextNode(optName));
			        if(optValue== selBrctrlTypeValue) {
			        	option.setAttribute("selected", "selected");
			        }
			        brctrlTypeSelect.appendChild(option);
			    }
			    td1.appendChild(brctrlTypeSelect);
				tr.appendChild(td0);
				tr.appendChild(td1);
				table.appendChild(tr);
				// Row: "Bit-rate control target value"
				var tr= document.createElement("tr");
				var td0= document.createElement("td");
				td0.classList.add("settings");
				td0.appendChild(document.createTextNode(
						"CBR / CVBR-maximum value [Kbps]: "));
				var td1= document.createElement("td");
				td1.classList.add("settings");
				var brctrlTargetInput= document.createElement("input");
				brctrlTargetInput.classList.add("input");
				brctrlTargetInput.classList.add("settings");
				brctrlTargetInput.classList.add("program");
				brctrlTargetInput.type= "text";
				brctrlTargetInput.id= settingsBrctrlTargetValId+ '_new';
				if(brctrlTypeSelect.options[brctrlTypeSelect.selectedIndex].
						innerHTML== "None") {
					brctrlTargetInput.value= "";
					brctrlTargetInput.disabled= true;
				} else {
					brctrlTargetInput.value= settingsBrctrlTargetVal;
				}
				td1.appendChild(brctrlTargetInput);
				tr.appendChild(td0);
				tr.appendChild(td1);
				table.appendChild(tr);
				// - Row: "Output URL"
				var tr= document.createElement("tr");
				var td0= document.createElement("td");
				td0.classList.add("settings");
				td0.appendChild(document.createTextNode("Output URL: "));
				var a= document.createElement('a');
				a.href= "#";
				a.id= oURLElemTypsiId;
				a.appendChild(document.createTextNode("[?]"));
				a.title= ""
					+ "Set to a valid URL or left blank to close output stream " 
					+ "interface";
				a.onclick = function() {return false;};
				td0.appendChild(a);
				var td1= document.createElement("td");
				td1.classList.add("settings");
				// Create "output URL" embedded table
				var tableOURL= document.createElement("table");
				tableOURL.classList.add('settings');
				tableOURL.classList.add('embedded');
				var trOURL= document.createElement("tr");
				// Create sub-table row "URL scheme selector"
				var td0OURL= document.createElement("td");
				td0OURL.classList.add("settings");
				td0OURL.classList.add('embedded');
				td0OURL.classList.add("program-proc-ourl-scheme");
			    var select= document.createElement("select");
			    select.classList.add("settings");
			    select.classList.add("program");
			    select.id= oURLElemId+ '_scheme_new';
			    var opt1= document.createElement("option");
			    opt1.setAttribute("value", "udp://");
			    opt1.appendChild(document.createTextNode("udp"));
			    opt1.setAttribute("selected", "selected");
			    select.appendChild(opt1);
			    td0OURL.appendChild(select);
			    // Create sub-table row "URL input"
				var td1OURL= document.createElement("td");
				td1OURL.classList.add("settings");
				td1OURL.classList.add('embedded');
				td1OURL.classList.add("program-proc-ourl-input");
				var outputURL= document.createElement("input");
				outputURL.classList.add("settings");
				outputURL.classList.add("program");
				outputURL.type= "text";
				outputURL.id= oURLElemId+ '_new';
				outputURL.placeholder= "(e.g.: 234.5.5.5:2001)";
				td1OURL.appendChild(outputURL);
				trOURL.appendChild(td0OURL);
				trOURL.appendChild(td1OURL);
				tableOURL.appendChild(trOURL);
				td1.appendChild(tableOURL);
				tr.appendChild(td0);
				tr.appendChild(td1);
				table.appendChild(tr);
				// - Append table
				privModalDiv.appendChild(table);

				// Submit new settings button
				var submitSettingsModalDiv= document.createElement("div");
				submitSettingsModalDiv.id= 
					submitStreamingSettingsAlertModalDivId;
				privModalDiv.appendChild(submitSettingsModalDiv);
				var submitSettings= document.createElement("input");
				submitSettings.classList.add("button");
				submitSettings.classList.add("settings");
				submitSettings.classList.add("program");
				submitSettings.classList.add("modal-button");
				submitSettings.type= "button";
				submitSettings.value= "Submit new settings";
				submitSettings.addEventListener('click', function(event)
				{
					// - Get new URL scheme
					var oURLSchemeSelect= document.getElementById(oURLElemId+ 
							'_scheme_new');
					var oURLScheme= oURLSchemeSelect.options[
							oURLSchemeSelect.selectedIndex].value;
					// - Output URL
					var outputUrl= oURLScheme+ document.getElementById(
							oURLElemId+ '_new').value;
					if(outputUrl=== oURLScheme)
						outputUrl= ""; // 'close interface' option
					// - Get bit-rate control type
					var brctrlTypeSelect= document.getElementById(
							settingsBrctrlTypeId+ '_new');
					var brctrlTypeSelectVal= brctrlTypeSelect.options[
							brctrlTypeSelect.selectedIndex].value;
					// - Get bit-rate control target value
					var brctrlTargetInput= document.getElementById(
							settingsBrctrlTargetValId+ '_new');
					var brctrlTargetInputVal= brctrlTargetInput.value;
					if(brctrlTargetInputVal> 2147483647) { // Bound to limits
						// Int32 maximum
						brctrlTargetInput.value= brctrlTargetInputVal= 
								2147483647;
					} else if(brctrlTargetInputVal< -2147483648) {
						// Int32 minimum
						brctrlTargetInput.value= brctrlTargetInputVal= 
								-2147483648;
					}

					// Compose query-string
					var query_string= '?output_url='+ outputUrl;
					query_string+= '&selected_brctrl_type_value='+ 
							brctrlTypeSelectVal;
					query_string+= '&cbr='+ brctrlTargetInputVal;

					// Compose callback object
					var externalCallbackObj= { 
						onResponse: function() {
							modals_close_modal(nodeId, modalIdSufix);
						},
						onError: function(alertString) {
							modals_alert_modal(
									submitStreamingSettingsAlertModalDivId, 
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

			function reloadStreamingSettings(program_proc_json)
			{
				// - "Bit-rate control type"
				var brctrlType= 
					program_proc_json.settings.selected_brctrl_type_value;
				var selBrctrlTypeElem= document.getElementById(
						settingsBrctrlTypeId+ '_new');
			    for(var opt, j= 0; opt= selBrctrlTypeElem.options[j]; j++) {
			        if(opt.value== brctrlType) {
			        	selBrctrlTypeElem.selectedIndex= j;
			            break;
			        }
			    }
				// - "CBR offset"
				var cbrOffsetVal= program_proc_json.settings.cbr;
				cbrOffsetElem= document.getElementById(
						settingsBrctrlTargetValId+ '_new');
				// If bit-rate control type is "none", we disable target 
				// bit-rate box; otherwise we set input box value
				if(selBrctrlTypeElem.options[selBrctrlTypeElem.selectedIndex].
						innerHTML== "None") {
					cbrOffsetElem.value= "";
					cbrOffsetElem.disabled= true;
				} else {
					cbrOffsetElem.value= cbrOffsetVal? cbrOffsetVal: 0;
					cbrOffsetElem.disabled= false;
				}
				// - "Output URL"
				var oURLJSON= program_proc_json.settings.output_url;
				oURLJSON= oURLJSON.substring(oURLJSON.indexOf("://")+ 3);
				document.getElementById(oURLElemId+ '_new').value= 
					oURLJSON? oURLJSON: "N/A";
			}
		}

		function advSettingsModal(parentNodeId)
		{
			var modalIdSufix= "advSettings";
			var modalPrivateContentId= parentNodeId+ '_advSettingsPrivate';

			var modalObj= modals_create_generic_modal(parentNodeId, 
					modalIdSufix, ["settings", "program"]);
			if(modalObj== null) {
				// Modal already exist, update body content
				httpRequests_respJSON(url, 'GET', '', null, 
						{onResponse: reloadAdvSettings});
				// - Display the modal 
				modals_display_modal(parentNodeId, modalIdSufix);
			} else {
				// Create private modal content
				var divModalContentBody= modalObj.bodyDiv;
				var divModalContentHeader= modalObj.headerDiv;
				// Header
				divModalContentHeader.appendChild(document.createTextNode(
						"Program processor advanced settings"));
				// Body
				var privModalDiv= createAdvSettingsModal();
				privModalDiv.id= modalPrivateContentId;
				divModalContentBody.appendChild(privModalDiv);
				// - Format settings table 'tipsy's'
				$(function() {
				$('#'+ tsPCRGuardElemTypsiId).tipsy({html: true, gravity: 'w'});
				$('#'+ minProgProcBufElemTypsiId).
						tipsy({html: true, gravity: 'w'});
				});
				// - Reload settings table values
				httpRequests_respJSON(url, 'GET', '', null, 
						{onResponse: reloadAdvSettings});
				// Display the modal 
				modals_display_modal(parentNodeId, modalIdSufix);
			}

			function createAdvSettingsModal()
			{
				// Modal private DIV
				var privModalDiv= document.createElement("div");

				// Settings edition table
				var table= document.createElement("table");
				table.classList.add("settings");
				table.classList.add("program");
				// Row: "TS-PCR guard"
				var tr= document.createElement("tr");
				var td0= document.createElement("td");
				td0.classList.add("settings");
				td0.appendChild(document.createTextNode("Time-stamps over " +
						"PCR guard max. [msec]: "));
				var a= document.createElement('a');
				a.href= "#";
				a.id= tsPCRGuardElemTypsiId;
				a.appendChild(document.createTextNode("[?]"));
				a.title= "Offset value, in milliseconds, to be applied to " +
				"the time-stamps of the packetized elementary stream (PES) " +
				"in reference to its associated program clock reference " +
				"(PCR). Valid settable range (milliseconds): [50-2000]. " +
				"Default: 300 milliseconds.";
				a.onclick = function() {return false;};
				td0.appendChild(a);
				var td1= document.createElement("td");
				td1.classList.add("settings");
				var pcrGuardInput= document.createElement("input");
				pcrGuardInput.classList.add("input");
				pcrGuardInput.classList.add("settings");
				pcrGuardInput.classList.add("program");
				pcrGuardInput.type= "text";
				pcrGuardInput.id= settingsTSPCRGuardId+ '_new';
				td1.appendChild(pcrGuardInput);
				tr.appendChild(td0);
				tr.appendChild(td1);
				table.appendChild(tr);
				// Row: "Minimum program-processor buffering"
				var tr= document.createElement("tr");
				var td0= document.createElement("td");
				td0.classList.add("settings");
				td0.appendChild(document.createTextNode("Minimum " +
						"program-processor buffering [msec]: "));
				var a= document.createElement('a');
				a.href= "#";
				a.id= minProgProcBufElemTypsiId;
				a.appendChild(document.createTextNode("[?]"));
				a.title= "Program-processor minimum buffering level, in " +
				"milliseconds. The minimum settable value is \"Time-stamps " +
				"over PCR guard value + 300\" [msec]. The maximum settable " +
				"value is only limited by the overflow of any of the " +
				"elementary-stream interleaving output buffers. " +
				"Default value: \"Time-stamps over PCR guard value + 300\" " +
				"[msec].";
				a.onclick = function() {return false;};
				td0.appendChild(a);
				var td1= document.createElement("td");
				td1.classList.add("settings");
				var progProcMinBuf= document.createElement("input");
				progProcMinBuf.classList.add("input");
				progProcMinBuf.classList.add("settings");
				progProcMinBuf.classList.add("program");
				progProcMinBuf.type= "text";
				progProcMinBuf.id= settingsMinProgProcBufId+ '_new';
				td1.appendChild(progProcMinBuf);
				tr.appendChild(td0);
				tr.appendChild(td1);
				table.appendChild(tr);
				// - Append table
				privModalDiv.appendChild(table);

				// Submit new settings button
				var submitSettingsModalDiv= document.createElement("div");
				submitSettingsModalDiv.id= submitAdvSettingsAlertModalDivId;
				privModalDiv.appendChild(submitSettingsModalDiv);
				var submitSettings= document.createElement("input");
				submitSettings.classList.add("button");
				submitSettings.classList.add("settings");
				submitSettings.classList.add("program");
				submitSettings.classList.add("modal-button");
				submitSettings.type= "button";
				submitSettings.value= "Submit new settings";
				submitSettings.addEventListener('click', function(event)
				{
					// Compose query-string
					var query_string= '?max_ts_pcr_guard_msec='+ 
							document.getElementById(settingsTSPCRGuardId+ 
									'_new').value;
					query_string+= '&min_stc_delay_output_msec='+ 
							document.getElementById(settingsMinProgProcBufId+ 
									'_new').value;

					// Compose callback object
					var externalCallbackObj= { 
						onResponse: function() {
							modals_close_modal(nodeId, modalIdSufix);
						},
						onError: function(alertString) {
							modals_alert_modal(
									submitAdvSettingsAlertModalDivId, 
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

			function reloadAdvSettings(program_proc_json)
			{
				// - "TS-PCR guard"
				document.getElementById(settingsTSPCRGuardId+ '_new').value= 
					program_proc_json.settings.max_ts_pcr_guard_msec;
				// - Minimum program-processor buffering level
				document.getElementById(settingsMinProgProcBufId+ '_new').value= 
					program_proc_json.settings.min_stc_delay_output_msec;
			}
		}

		function updateNode()
		{
			// Only update if current program node is not hidden
			if(nodeDiv.classList.contains('hide'))
				return;

			// Update "Input bit-rate"
			var iBitrateElem= document.getElementById(iBitrateElemId);
			if(iBitrateElem.innerHTML!= iBitrate)
				iBitrateElem.innerHTML= iBitrate;
			// Update "Bit-rate maximum peak"
			var iBitratePeakElem= document.getElementById(iBitratePeakElemId);
			if(iBitratePeakElem.innerHTML!= iBitratePeak)
				iBitratePeakElem.innerHTML= iBitratePeak;
			// Update "Output bit-rate"
			var oBitrateElem= document.getElementById(oBitrateElemId);
			if(oBitrateElem.innerHTML!= oBitrate)
				oBitrateElem.innerHTML= oBitrate;
			// Update "Delay offset"
			var progProcTotalDelayElem= document.getElementById(
					progProcTotalDelayElemId);
			if(progProcTotalDelayElem.innerHTML!= progProcTotalDelay)
				progProcTotalDelayElem.innerHTML= progProcTotalDelay;

			// **** Update program processor streaming settings ****
			var streamingSettingsDivId= nodeId+ '_streaming_settings';
			if(!document.getElementById(streamingSettingsDivId).classList.
					contains('hide')) {

				// Update "Bit-rate control type"
				var settingsBrctrlTypeElem= document.getElementById(
						settingsBrctrlTypeId);
				if(settingsBrctrlTypeElem.innerHTML!= settingsBrctrlType)
					settingsBrctrlTypeElem.innerHTML= 
						!(typeof settingsBrctrlType=== 'undefined')? 
								settingsBrctrlType: "N/A";
				// Update "Bit-rate control target value"
				var settingsBrctrlTargetValElem= document.getElementById(
						settingsBrctrlTargetValId);
				if(settingsBrctrlTargetValElem.innerHTML!= 
						settingsBrctrlTargetVal)
					settingsBrctrlTargetValElem.innerHTML= 
						settingsBrctrlTargetVal;
				// - "Output URL"
				var oURLElem= document.getElementById(oURLElemId);
				if(oURLElem.innerHTML!= oURL)
					oURLElem.innerHTML= oURL? oURL: "N/A";
			}

			// **** Update program processor adavnced settings ****
			var streamingAdvDivId= nodeId+ '_adv_settings';
			if(!document.getElementById(streamingAdvDivId).classList.contains(
					'hide')) {

				// - "TS-PCR guard"
				var settingsTSPCRGuardElem= document.getElementById(
						settingsTSPCRGuardId);
				if(settingsTSPCRGuardElem.innerHTML!= settingsTSPCRGuard)
					settingsTSPCRGuardElem.innerHTML= settingsTSPCRGuard;
				// - "Minimum program-processor buffering level"
				var settingsMinProgProcBufElem= document.getElementById(
						settingsMinProgProcBufId);
				if(settingsMinProgProcBufElem.innerHTML!= 
							settingsMinProgProcBuf)
					settingsMinProgProcBufElem.innerHTML= 
						settingsMinProgProcBuf;
			}
		}
	}
}

function program_proc_erase_node(nodeId)
{
	nodes_erase(nodeId);
}
