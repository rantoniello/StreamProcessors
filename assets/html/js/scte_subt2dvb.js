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
 * @file scte_subt2dvb.js
 */

function scte_subt2dvb_settings(es_proc_json, parentNode, parentNodeId, url)
{
	var scteSubtSettingsDivId= parentNodeId+ '_scte_subt_settings';

	var descrTableSettingElemId= scteSubtSettingsDivId+ 
			'_descr_table_setting';
	var descrTable3SettingTypsiId= '_descr_table3_setting_typsi';
	var descrTable4SettingTypsiId= '_descr_table4_setting_typsi';
	var submitDescrSettingsAlertModalDivId= scteSubtSettingsDivId+ 
			'_alert_descr_modal_submit';
	var durationOffsetElemId= scteSubtSettingsDivId+ "_duration_offset";
	var durationOffsetTypsiId= durationOffsetElemId+ "_typsi";
	var vposOffsetElemId= scteSubtSettingsDivId+ "_vpos_offset";
	var vposOffsetTypsiId= vposOffsetElemId+ "_typsi";

	var subtSettings= es_proc_json.settings;
	var durationOffset= subtSettings.duration_offset_msec;
	var vposOffset= subtSettings.vpos_offset_pels;
	var subtDesc= subtSettings.subtitling_descriptor;

	// ****************************************************************** //
	// Create SCTE-subtitling settings node
	// ****************************************************************** //
	var scteSubtSettingsDiv= nodes_simpleDropDownCreate(scteSubtSettingsDivId, 
			parentNodeId, esProcDetailsClass, 
			"SCTE to DVB transcoder settings", true);
	var table= document.createElement("table");
	table.classList.add("settings");
	table.classList.add("es");
	// - Table caption: "Settings"
	var caption= document.createElement("caption");
	caption.classList.add("settings");
	table.appendChild(caption);
	// Row: "duration offset"
	var tr= document.createElement("tr");
	var td0= document.createElement("td");
	td0.classList.add("settings");
	td0.appendChild(document.createTextNode(
			"Subtitle duration offset [msecs]: "));
	var a= document.createElement('a');
	a.href= "#";
	a.id= durationOffsetTypsiId;
	a.appendChild(document.createTextNode("[?]"));
	a.title= ""
	+ " Specify an offset value, in milliseconds, to lengthen "
	+ "(positive offset) "
	+ "or shorten (negative offset) the subtitling duration in the screen. "
	+ "Accepted value ranges from -1000 (shorten in 1 second) to 5000 "
	+ "(lengthen in 5 seconds).<br/>";
	a.onclick = function() {return false;};
	td0.appendChild(a);
	var td1= document.createElement("td");
	td1.classList.add("settings");
	td1.classList.add("unselect");
	td1.id= durationOffsetElemId;
	td1.appendChild(document.createTextNode(durationOffset));
	tr.appendChild(td0);
	tr.appendChild(td1);
	table.appendChild(tr);
	// Row: "vertical position offset"
	var tr= document.createElement("tr");
	var td0= document.createElement("td");
	td0.classList.add("settings");
	td0.appendChild(document.createTextNode(
			"Subtitle vertical position offset [pixels]: "));
	var a= document.createElement('a');
	a.href= "#";
	a.id= vposOffsetTypsiId;
	a.appendChild(document.createTextNode("[?]"));
	a.title= ""
	+ " Specify an offset value, in pixels, to vertically move "
	+ "(positive value moves downwards, negative upwards) the subtitles "
	+ "position in the screen. "
	+ "New position is bounded to subtitle display size.<br/>";
	a.onclick = function() {return false;};
	td0.appendChild(a);
	var td1= document.createElement("td");
	td1.classList.add("settings");
	td1.classList.add("unselect");
	td1.id= vposOffsetElemId;
	td1.appendChild(document.createTextNode(vposOffset));
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
	submitSettings.classList.add("es");
	submitSettings.type= "button";
	submitSettings.value= "Edit transcoding settings";
	submitSettings.addEventListener('click', function(event){
		transcoderSettingsModal(parentNodeId);
	});
	td0.appendChild(submitSettings);
	var td1= document.createElement("td");
	td1.classList.add("settings");
	tr.appendChild(td0);
	tr.appendChild(td1);
	table.appendChild(tr);
	// - Append table
	scteSubtSettingsDiv.appendChild(table);
	// - Format table 'tipsy's'
	$(function() {
		$('#'+ durationOffsetTypsiId).tipsy(
				{html: true, gravity: 'w'});
	});
	$(function() {
		$('#'+ vposOffsetTypsiId).tipsy(
				{html: true, gravity: 'w'});
	});

	// Descriptor table
	var table= document.createElement("table");
	table.classList.add("settings");
	table.classList.add("dvb-subt-proc-display-set");
	table.id= descrTableSettingElemId;
	table.jsonrest= subtDesc;
	// - Table caption
	var caption= document.createElement("caption");
	caption.classList.add("settings");
	caption.classList.add("dvb-subt-proc-display-set");
	caption.classList.add("es-h2");
	caption.appendChild(document.createTextNode("DVB-subtitling descriptor"));
	table.appendChild(caption);
	// - Table header
	var tr= document.createElement("tr");
	var th0= document.createElement("th");
	th0.classList.add("settings");
	th0.classList.add("dvb-subt-proc-filters");
	th0.appendChild(document.createTextNode("Language code (ISO-639)"));
	var th1= document.createElement("th");
	th1.classList.add("settings");
	th1.classList.add("dvb-subt-proc-filters");
	th1.appendChild(document.createTextNode("Type"));
	var th2= document.createElement("th");
	th2.classList.add("settings");
	th2.classList.add("dvb-subt-proc-filters");
	th2.appendChild(document.createTextNode("Composition page Id."));
	var a= document.createElement('a');
	a.href= "#";
	a.id= descrTable3SettingTypsiId;
	a.appendChild(document.createTextNode("[?]"));
	a.title= ""
	+ " Set to 1 ('page_id'= 1 is used by default in transcoding).<br/>";
	a.onclick = function() {return false;};
	th2.appendChild(a);
	var th3= document.createElement("th");
	th3.classList.add("settings");
	th3.classList.add("dvb-subt-proc-filters");
	th3.appendChild(document.createTextNode("Ancillary page Id."));
	var a= document.createElement('a');
	a.href= "#";
	a.id= descrTable4SettingTypsiId;
	a.appendChild(document.createTextNode("[?]"));
	a.title= ""
	+ " Set to 1 ('page_id'= 1 is used by default in transcoding).<br/>";
	a.onclick = function() {return false;};
	th3.appendChild(a);
	tr.appendChild(th0);
	tr.appendChild(th1);
	tr.appendChild(th2);
	tr.appendChild(th3);
	table.appendChild(tr);
	dvb_subt_createOrUpdateDescrTableBody(subtDesc, table, parentNodeId);
	// - Append table
	scteSubtSettingsDiv.appendChild(table);
	$(function() {
		$('#'+ descrTable3SettingTypsiId).tipsy(
				{html: true, gravity: 'w'});
	});
	$(function() {
		$('#'+ descrTable4SettingTypsiId).tipsy(
				{html: true, gravity: 'w'});
	});

	// Edit Descriptor button table
	var table= document.createElement("table");
	table.classList.add("settings");
	table.classList.add("es");
	// - Row: Edit descriptor settings
	var tr= document.createElement("tr");
	var td0= document.createElement("td");
	td0.classList.add("settings");
	var submitSettings= document.createElement("input");
	submitSettings.classList.add("settings");
	submitSettings.classList.add("button");
	submitSettings.classList.add("es");
	submitSettings.type= "button";
	submitSettings.value= "Edit output dvb-subtitling descriptor";
	submitSettings.addEventListener('click', function(event){
		dvb_subt_descrSettingsModal(parentNodeId, url, descrTableSettingElemId, 
				submitDescrSettingsAlertModalDivId);
	});
	td0.appendChild(submitSettings);
	var td1= document.createElement("td");
	td1.classList.add("settings");
	tr.appendChild(td0);
	tr.appendChild(td1);
	table.appendChild(tr);
	// - Append table
	scteSubtSettingsDiv.appendChild(table);

	function transcoderSettingsModal(parentNodeId)
	{
		var modalIdSufix= "transcoderSettings";
		var modalPrivateContentId= parentNodeId+ '_transcoderSettingsPrivate';

		var modalObj= modals_create_generic_modal(parentNodeId, 
				modalIdSufix, ["settings", "es"]);
		if(modalObj== null) {
			// Modal already exist, update body content
			httpRequests_respJSON(url, 'GET', '', null, 
					{onResponse: reloadTranscoderSettings});
			// - Display the modal 
			modals_display_modal(parentNodeId, modalIdSufix);
		} else {
			// Create private modal content
			var divModalContentBody= modalObj.bodyDiv;
			var divModalContentHeader= modalObj.headerDiv;
			// Header
			divModalContentHeader.appendChild(document.createTextNode(
					"SCTE transcoder processor settings"));
			// Body
			var privModalDiv= createTranscoderSettingsModal();
			privModalDiv.id= modalPrivateContentId;
			divModalContentBody.appendChild(privModalDiv);
			// - Reload settings table values
			httpRequests_respJSON(url, 'GET', '', null, 
					{onResponse: reloadTranscoderSettings});
			// Display the modal 
			modals_display_modal(parentNodeId, modalIdSufix);
		}

		function createTranscoderSettingsModal()
		{
			// Modal private DIV
			var privModalDiv= document.createElement("div");

			// Settings table
			var table= document.createElement("table");
			table.classList.add("settings");
			table.classList.add("es");
			// Row: "duration offset"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("settings");
			td0.appendChild(document.createTextNode(
					"Subtitle duration offset [msecs]: "));
			var td1= document.createElement("td");
			td1.classList.add("settings");
			var input= document.createElement("input");
			input.classList.add("settings");
			input.classList.add("es");
			input.type= "text";
			input.id= durationOffsetElemId+ '_new';
			input.value= durationOffset;
			td1.appendChild(input);
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// Row: "vertical position offset"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("settings");
			td0.appendChild(document.createTextNode(
					"Subtitle vertical position offset [pixels]: "));
			var td1= document.createElement("td");
			td1.classList.add("settings");
			var input= document.createElement("input");
			input.classList.add("settings");
			input.classList.add("es");
			input.type= "text";
			input.id= vposOffsetElemId+ '_new';
			input.value= vposOffset;
			td1.appendChild(input);
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
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
				// - Duration offset
				var durationOffsetElem= document.getElementById(
						durationOffsetElemId+ '_new');
				var durationOffset= durationOffsetElem.value;
				// - Bound offset to limits
				if(durationOffset> 2147483647) {
					// Int32 maximum
					durationOffsetElem.value= durationOffset= 2147483647;
				} else if(durationOffset< -2147483648) {
					// Int32 minimum
					durationOffsetElem.value= durationOffset= -2147483648;
				}
				// - Vertical position offset
				var vposOffsetElem= document.getElementById(
						vposOffsetElemId+ '_new');
				var vposOffset= vposOffsetElem.value;
				// - Bound offset to limits
				if(vposOffset> 2147483647) {
					// Int32 maximum
					vposOffsetElem.value= vposOffset= 2147483647;
				} else if(vposOffset< -2147483648) {
					// Int32 minimum
					vposOffsetElem.value= vposOffset= -2147483648;
				}

				// Compose query-string
				var query_string= '?duration_offset_msec='+ durationOffset;
				query_string+= '&vpos_offset_pels='+ vposOffset;

				// Compose body-string
				var body_string= '';

				// Compose callback object
				var externalCallbackObj= { 
					onResponse: function() {
						modals_close_modal(parentNodeId, modalIdSufix);
					}
				};

				// PUT new values
				httpRequests_respJSON(url, 'PUT', query_string, 
						body_string, externalCallbackObj);
			});
			privModalDiv.appendChild(submitSettings);

			return privModalDiv;
		}

		function reloadTranscoderSettings(es_proc_json)
		{
			// "Duration offset"
			document.getElementById(durationOffsetElemId+ '_new').value= 
				es_proc_json.settings.duration_offset_msec;
			// "Vertical position offset"
			document.getElementById(vposOffsetElemId+ '_new').value= 
				es_proc_json.settings.vpos_offset_pels;
		}
	}
}

function scte_subt2dvb_update_node(es_proc_json, parentNodeId)
{
	var scteSubtSettingsDivId= parentNodeId+ '_scte_subt_settings';
	var durationOffsetElemId= scteSubtSettingsDivId+ "_duration_offset";
	var vposOffsetElemId= scteSubtSettingsDivId+ "_vpos_offset";

	var subtSettings= es_proc_json.settings;
	var durationOffset= subtSettings.duration_offset_msec;
	var vposOffset= subtSettings.vpos_offset_pels;

	// Settings: "Duration offset"
	var durationOffsetElem= document.getElementById(durationOffsetElemId);
	if(durationOffsetElem.innerHTML!= durationOffset)
		durationOffsetElem.innerHTML= durationOffset;
	// Settings: "Vertical position offset"
	var vposOffsetElem= document.getElementById(vposOffsetElemId);
	if(vposOffsetElem.innerHTML!= vposOffset)
		vposOffsetElem.innerHTML= vposOffset;
	// Settings: descriptor
	var descrTableSettingElemId= scteSubtSettingsDivId+ 
			'_descr_table_setting';
	dvb_subt_createOrUpdateDescrTableBody(
			es_proc_json.settings.subtitling_descriptor, 
			document.getElementById(descrTableSettingElemId), parentNodeId);
}
