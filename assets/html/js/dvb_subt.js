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
 * @file dvb_subt.js
 */

var dvb_subt_descrComponentTypes= 
[
 	{name: "", value: "null"},
 	{name: "normal; no monitor aspect ratio criticality", value: 16},
 	{name: "normal; 4:3 aspect ratio monitor", value: 17},
 	{name: "normal; 16:9 aspect ratio monitor", value: 18},
 	{name: "normal; 2.21:1 aspect ratio monitor", value: 19},
 	{name: "normal; high definition monitor", value: 20},
 	{name: "normal; plano-stereoscopic disparity for display " +
 			"on a high definition monitor", value: 21},
 	{name: "hard of hearing; no monitor aspect ratio criticality", 
 				value: 32},
 	{name: "hard of hearing; 4:3 aspect ratio monitor", value: 33},
 	{name: "hard of hearing; 16:9 aspect ratio monitor", value: 34},
 	{name: "hard of hearing; 2.21:1 aspect ratio monitor", value: 35},
 	{name: "hard of hearing; high definition monitor", value: 36},
 	{name: "hard of hearing; plano-stereoscopic disparity on a " +
 			"high definition monitor", value: 37},
];
var dvb_subt_languageCode= 
[
 	{name: "", value: "null"},
 	{name: "eng", value: 6647399},
 	{name: "spa", value: 7565409},
 	{name: "por", value: 7368562},
 	{name: "fre", value: 6713957}
];

function dvb_subt_settings(es_proc_json, parentNode, parentNodeId, url)
{
	var dvbSubtSettingsDivId= parentNodeId+ '_dvb_subt_settings';

//	var dvbSubtDescElemId= dvbSubtSettingsDivId+ '_desc';
	var descrTableSettingElemId= dvbSubtSettingsDivId+ 
			'_descr_table_setting';
	var submitDescrSettingsAlertModalDivId= dvbSubtSettingsDivId+ 
			'_alert_descr_modal_submit';

	var subtSettings= es_proc_json.settings;
	var subtDesc= subtSettings.subtitling_descriptor;

	// ****************************************************************** //
	// Create DVB-subtitling settings node
	// ****************************************************************** //
	var dvbSubtSettingsDiv= nodes_simpleDropDownCreate(dvbSubtSettingsDivId, 
			parentNodeId, esProcDetailsClass, "DVB-subtitling settings", true);

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
	var th3= document.createElement("th");
	th3.classList.add("settings");
	th3.classList.add("dvb-subt-proc-filters");
	th3.appendChild(document.createTextNode("Ancillary page Id."));
	tr.appendChild(th0);
	tr.appendChild(th1);
	tr.appendChild(th2);
	tr.appendChild(th3);
	table.appendChild(tr);
	dvb_subt_createOrUpdateDescrTableBody(subtDesc, table, parentNodeId);
	// - Append table
	dvbSubtSettingsDiv.appendChild(table);

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
	submitSettings.value= "Edit dvb-subtitling descriptor";
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
	dvbSubtSettingsDiv.appendChild(table);
}

function dvb_subt_descrSettingsModal(parentNodeId, url, 
		descrTableSettingElemId, submitDescrSettingsAlertModalDivId)
{
	var descMaxLen= 16;
	var rowTags= ["ISO_639_language_code", "subtitling_type", 
        "composition_page_id", "ancillary_page_id"];
	var modalIdSufix= "dvbSubtDescrSettings";
	var modalPrivateContentId= parentNodeId+ '_dvbSubtDescrSettingsPrivate';
	var selectDescLenId= modalPrivateContentId+ '_select_descriptor_len';
	var descSetsDivId= modalPrivateContentId+ '_descriptor_sets';

	var modalObj= modals_create_generic_modal(parentNodeId, modalIdSufix, 
			["settings", "es"]);
	if(modalObj== null) {
		// Modal already exist, update body content
		httpRequests_respJSON(url, 'GET', '', null, 
				{onResponse: reloadDvbSubtDescrSettingsSettings});
		// - Display the modal 
		modals_display_modal(parentNodeId, modalIdSufix);
	} else {
		// Create private modal content
		var divModalContentBody= modalObj.bodyDiv;
		var divModalContentHeader= modalObj.headerDiv;
		// Header
		divModalContentHeader.appendChild(document.createTextNode(
				"DVB subtitling processor settings: Add/edit descriptor."));
		// Body
		var privModalDiv= createDescrSettingsModal();
		privModalDiv.id= modalPrivateContentId;
		divModalContentBody.appendChild(privModalDiv);
		// - Format settings table 'tipsy's'
		$(function() {
		$('#'+ parentNodeId+ "_placeholder_ourl_href").tipsy(
				{html: true, gravity: 'w'});
		});
		// - Reload settings table values
		httpRequests_respJSON(url, 'GET', '', null, 
				{onResponse: reloadDvbSubtDescrSettingsSettings});
		// Display the modal 
		modals_display_modal(parentNodeId, modalIdSufix);
	}

	function createDescrSettingsModal()
	{
		// Modal private DIV
		var privModalDiv= document.createElement("div");

		// Settings edition table
		var table= document.createElement("table");
		table.classList.add("settings");
		table.classList.add("es");
		// Row: 'select' to choose the number of descriptor sets to draw
		var tr= document.createElement("tr");
		var td0_1= document.createElement("td");
		td0_1.colSpan= "2";
	    var selectDescLen= document.createElement("select");
	    selectDescLen.classList.add("settings");
	    selectDescLen.classList.add("es");
	    selectDescLen.classList.add("dvb-subt-proc-desc-num");
	    selectDescLen.id= selectDescLenId;
	    for(var i= 0; i<= descMaxLen; i++) {
	        var option= document.createElement("option");
	        if(i== 0) {
	        	option.setAttribute("selected", "selected");
	        	option.setAttribute("value", i);
	        	option.appendChild(document.createTextNode(
	        			"No services (do not add descriptor)"));
	        } else {
	        	option.setAttribute("value", i);
	        	option.appendChild(document.createTextNode(i));
	        }
	        selectDescLen.appendChild(option);
	    }
	    selectDescLen.addEventListener("change", function(event){
	    	drawDescrDataSetTable();
	    });
		var label= document.createElement("label");
		label.htmlFor= selectDescLen.id;
		label.appendChild(document.createTextNode(
				"Indicate number of DVB-subtitle services:   "));
		td0_1.appendChild(label);
		td0_1.appendChild(selectDescLen);
		tr.appendChild(td0_1);
		table.appendChild(tr);
		// Rows: Draw descriptor sets
		var tr= document.createElement("tr");
		var td0_1= document.createElement("td");
		td0_1.colSpan= "2";
    	var descSetsDiv= document.createElement("div");
    	descSetsDiv.classList.add("settings");
    	descSetsDiv.classList.add("dvb-subt-proc-descr-sets-div");
    	descSetsDiv.id= descSetsDivId;
	    td0_1.appendChild(descSetsDiv);
	    tr.appendChild(td0_1);
		table.appendChild(tr);
		// -- Row: Empty row (kind of "</br>")
		var tr= document.createElement("tr");
		var td0= document.createElement("td");
		td0.classList.add("settings");
		td0.appendChild(document.createElement("br"));
		var td1= document.createElement("td");
		td1.classList.add("settings");
		td1.appendChild(document.createElement("br"));
		tr.appendChild(td0);
		tr.appendChild(td1);
		table.appendChild(tr);
		// Append settings table
		privModalDiv.appendChild(table);

		// Submit new settings button
		var submitSettingsModalDiv= document.createElement("div");
		submitSettingsModalDiv.id= submitDescrSettingsAlertModalDivId;
		privModalDiv.appendChild(submitSettingsModalDiv);
		var submitSettings= document.createElement("input");
		submitSettings.classList.add("button");
		submitSettings.classList.add("settings");
		submitSettings.classList.add("es");
		submitSettings.classList.add("modal-button");
		submitSettings.type= "button";
		submitSettings.value= "Submit new descriptor";
		submitSettings.addEventListener('click', function(event)
		{
			/* Descriptor */
	    	var selectDescLen= document.getElementById(selectDescLenId);
	    	var N= selectDescLen.options[selectDescLen.selectedIndex].value;
			descriptorTxt= '[';
			for(var i= 0; i< N; i++) {
				descriptorTxt+= '[';
				for(var tag_idx= 0; tag_idx< rowTags.length; tag_idx++) {
					var rowTagsInputId= descSetsDivId+ '_'+ i+ '_'+ 
						rowTags[tag_idx];
					//console.log(rowTagsInputId); //comment-me
					var val= document.getElementById(rowTagsInputId).value;
					if(!val) val= 0;
					// Check value is numeric or alert
					if(val!= parseInt(val, 10)) {
						var modalTxt= rowTags[tag_idx]+ 
								": Not valid setting"
						//console.log(modalTxt); //comment-me
						modals_alert_modal(
								submitDescrSettingsAlertModalDivId, 
								modalTxt);
						return;
					}
					descriptorTxt+= val;
					if(tag_idx< rowTags.length- 1) descriptorTxt+=','
				}
				descriptorTxt+= ']';
				if(i< N-1) descriptorTxt+=','
			}
			descriptorTxt+= ']';
			var setDescr= (descriptorTxt== '[]')? 'null': descriptorTxt;
			var body_string= '{';
			body_string+= '\"subtitling_descriptor\":'+ setDescr;
			body_string+= '}';

			// Compose query-string
			var query_string= '';

			// Compose callback object
			var externalCallbackObj= { 
				onResponse: function() {
					modals_close_modal(parentNodeId, modalIdSufix);
				},
				onError: function(alertString) {
					modals_alert_modal(
							submitDescrSettingsAlertModalDivId, 
							alertString);
				}
			};

			// PUT new values
			httpRequests_respJSON(url, 'PUT', query_string, body_string, 
					externalCallbackObj);
		});
		privModalDiv.appendChild(submitSettings);

		return privModalDiv;
	}

	function reloadDvbSubtDescrSettingsSettings(es_proc_json)
	{
		// Descriptor
		drawDescrDataSetTable(es_proc_json.settings.subtitling_descriptor);
	}

	function drawDescrDataSetTable(descrJSON) {
		var selectDescLen= document.getElementById(selectDescLenId);
    	var descSetsDiv= document.getElementById(descSetsDivId);
    	var tableLen= (descrJSON && descrJSON.length)? descrJSON.length: 
    		selectDescLen.options[selectDescLen.selectedIndex].value;
    	//console.log("selectDescLen: "+ selectDescLen.length); //comment-me
		//console.log("tableLen: "+ tableLen); //comment-me

    	// Update descriptor length selector
    	if(descMaxLen< tableLen)
    		console.error("Bad descriptor length");
    	selectDescLen.options[tableLen]
    		.setAttribute("selected", "selected");
    	selectDescLen.options[tableLen].selected= true;

    	// Delete previous table (as we may change number of rows)
    	while(descSetsDiv.firstChild)
    		descSetsDiv.removeChild(descSetsDiv.firstChild);

    	// If table is length== 0; no table to draw
    	if(tableLen<= 0)
    		return;

    	var descrTableSettingElem= document.getElementById(
    			descrTableSettingElemId);
    	if(descrTableSettingElem.jsonrest && 
    			descrTableSettingElem.jsonrest!= null && 
    			!(descrJSON && descrJSON.length))
    		descrJSON= descrTableSettingElem.jsonrest;

		// Draw descriptor sets table
		var table= document.createElement("table");
		table.classList.add("settings");
		table.classList.add("es");
		descSetsDiv.appendChild(table);
		// Row: fields denomination
		var tr= document.createElement("tr");
		var td0= document.createElement("td");
		td0.classList.add("settings");
		td0.appendChild(document.createTextNode("ISO_639_language_code"));
		var td1= document.createElement("td");
		td1.classList.add("settings");
		td1.appendChild(document.createTextNode("subtitling_type"));
		var td2= document.createElement("td");
		td2.classList.add("settings");
		td2.appendChild(document.createTextNode("composition_page_id"));
		var td3= document.createElement("td");
		td3.classList.add("settings");
		td3.appendChild(document.createTextNode("ancillary_page_id"));
		tr.appendChild(td0);
		tr.appendChild(td1);
		tr.appendChild(td2);
		tr.appendChild(td3);
		table.appendChild(tr);
		// Add rows with field values 
    	for(var row_idx= 0; row_idx< tableLen; row_idx++) {
    		var rowIdPrefix= descSetsDivId+ '_'+ row_idx+ '_';
    		var dataSetJSON= (descrJSON && descrJSON[row_idx])? 
    				descrJSON[row_idx]: null;
    		var tr= drawDescrDataSetRow(rowIdPrefix, dataSetJSON);
			table.appendChild(tr);
    	}

    	function drawDescrDataSetRow(rowIdPrefix, dataSetJSON) {
			var tr= document.createElement("tr");
			// Draw 'select' for 'ISO_639_language_code' field
			var td0= document.createElement("td");
			td0.classList.add("settings");
		    var selectSubtLang= document.createElement("select");
		    selectSubtLang.classList.add("settings");
		    selectSubtLang.classList.add("es");
		    selectSubtLang.classList.add("dvb-subt-proc-desc");
		    selectSubtLang.id= rowIdPrefix+ rowTags[0];
		    for(var i= 0; i< dvb_subt_languageCode.length; i++) {
		        var option= document.createElement("option");
		        var selIdx= (dataSetJSON!= null && 
		        		dataSetJSON[0]== dvb_subt_languageCode[i].value) || 
		        		(i== 0);
		        if(selIdx)
		        	option.setAttribute("selected", "selected");
	        	option.setAttribute("value", 
	        			dvb_subt_languageCode[i].value);
	        	option.appendChild(document.createTextNode(
	        			dvb_subt_languageCode[i].name));
	        	selectSubtLang.appendChild(option);
		    }
			td0.appendChild(selectSubtLang);
			// Draw 'select' for 'subtitling_type' field
			var td1= document.createElement("td");
			td1.classList.add("settings");
		    var selectSubtType= document.createElement("select");
		    selectSubtType.classList.add("settings");
		    selectSubtType.classList.add("es");
		    selectSubtType.classList.add("dvb-subt-proc-desc");
		    selectSubtType.id= rowIdPrefix+ rowTags[1];
		    for(var i= 0; i< dvb_subt_descrComponentTypes.length; i++) {
		        var option= document.createElement("option");
		        var selIdx= (dataSetJSON!= null && dataSetJSON[1]== 
		        	dvb_subt_descrComponentTypes[i].value) || 
		        		(i== 0);
		        if(selIdx)
		        	option.setAttribute("selected", "selected");
	        	option.setAttribute("value", 
	        			dvb_subt_descrComponentTypes[i].value);
	        	option.appendChild(document.createTextNode(
	        			dvb_subt_descrComponentTypes[i].name));
	        	selectSubtType.appendChild(option);
		    }
		    td1.appendChild(selectSubtType);
		    tr.appendChild(td0);
			tr.appendChild(td1);
			// Draw 'input' for 'composition_page_id' and 
			//   'ancillary_page_id' fields.
			for(var tag_idx= 2; tag_idx< rowTags.length; tag_idx++) {
	    		var tdi= document.createElement("td");
	    		tdi.classList.add("settings");
	    		tdi.classList.add("es");
	    		var rowTagsInput= document.createElement("input");
	    		rowTagsInput.classList.add("settings");
	    		rowTagsInput.classList.add("es");
	    		rowTagsInput.classList.add("dvb-subt-proc-desc");
	    		rowTagsInput.id= rowIdPrefix+ rowTags[tag_idx];
	    		rowTagsInput.type= "text";
	    		if(dataSetJSON!= null && 
	    				!isNaN(parseFloat(dataSetJSON[tag_idx])))
	    			rowTagsInput.value= dataSetJSON[tag_idx];
	    		tdi.appendChild(rowTagsInput);
	    		tr.appendChild(tdi);
			}
			return tr;
    	}
	}
}

function dvb_subt_update_node(es_proc_json, parentNodeId)
{
	var dvbSubtSettingsDivId= parentNodeId+ '_dvb_subt_settings';

	// Settings: descriptor
	var descrTableSettingElemId= dvbSubtSettingsDivId+ 
			'_descr_table_setting';
	dvb_subt_createOrUpdateDescrTableBody(
			es_proc_json.settings.subtitling_descriptor, 
			document.getElementById(descrTableSettingElemId), parentNodeId);
}

function dvb_subt_createOrUpdateDescrTableBody(subtDesc, table, parentNodeId)
{
	var subtDescLength= (subtDesc== null)? 0: subtDesc.length;
	// Update table REST
	table.jsonrest= subtDesc;
	//console.log(JSON.stringify(table.jsonrest)); //comment-me

	// Firstly, delete not used rows
	while(table.rows.length> subtDescLength+ 1)
		table.deleteRow(-1);

	if(subtDescLength<= 0) {
		var tr= document.createElement("tr");
		var td0= document.createElement("td");
		td0.colSpan = "4";
		td0.classList.add("settings");
		td0.classList.add("dvb-subt-proc-filters");
		td0.classList.add("unselect");
		td0.appendChild(document.createTextNode(
				"No descriptor defined"));
		tr.appendChild(td0);
		table.appendChild(tr);
	}

	for(var i= 1; i< subtDescLength+ 1; i++) {
		var subtDescService= subtDesc[i- 1];
		var langCode= subtDescService[0];
		var subtType= subtDescService[1];
		var compPageId= subtDescService[2];
		var ancillaryPageId= subtDescService[3];

		var tr= (table.rows[i]!= null)? table.rows[i]: table.insertRow(-1);
		createOrUpdateCell(tr, 0, langCode);
		createOrUpdateCell(tr, 1, subtType);
		createOrUpdateCell(tr, 2, compPageId);
		createOrUpdateCell(tr, 3, ancillaryPageId);
	}

	function createOrUpdateCell(tr, index, nodeTxt) {
		if(tr.cells[index]!= null) {
			// Update exitent cell
			var td= tr.cells[index];
			if(td.innerHTML!= nodeTxt)
				td.innerHTML= nodeTxt;
			if(td.colSpan!= "1")
				td.colSpan= "1"
		} else {
			// Create cell
			var td= tr.insertCell(index);
			td.classList.add("settings");
			td.classList.add("dvb-subt-proc-filters");
			td.classList.add("unselect");
			td.appendChild(document.createTextNode(nodeTxt));
		}
	}
}

function dvb_subt_draw_services_nodes(es_proc_json, parentNodeId)
{
	// IMPLEMENTATION NOTE: This function is a specific adaptation of function 
	// 'nodes_update()'; similar to performing the following call:
	//
	// function update_link(linkNode, currListJSON_nth) {}
	// nodes_update(parentNodeId, currListJSON, esProcDetailsClass, 
	//		dvb_subt_service_erase_node, dvb_subt_service_manager, update_link);

	// Get list of services nodes in JSON response
	var currListJSON= es_proc_json.services;

	// Get parent node (services place-holder)
	//console.debug("Parent node Id.: "+ parentNodeId); //comment-me
	var servicesPH= document.getElementById(parentNodeId+ 
			'-services_placeholder');

	// Only execute if parent node is not hidden
	if(servicesPH.classList.contains('hide'))
		return;

	var ul= servicesPH.getElementsByClassName(esProcDetailsClass)[0];
	if(!ul) {
		console.error("UL node not defined");
		return;
	}
	var ulListItems= ul.childNodes;

	// Iterate UL to check all currently drawn nodes.
	// Erase those nodes that have been deleted from representational state.
	// We go inverse way to avoid problems when deleting elements from array.
	for(var i= ulListItems.length- 1; i>= 0; i--) {
		var ulItem= ulListItems[i];
		if(ulItem.nodeName== "LI") {
			var link= getFirstChildWithTagName(ulItem, 'A');
			var href= getHash(link.getAttribute('href'));
			if(!href || href== null)
				continue;
			var mustDelete= true;
			// Iterate over representational state list
			for(var j= 0; j< currListJSON.length; j++) {
				var serviceId= currListJSON[j].page_id;
				var hrefJSON= parentNodeId+ '-services-'+ serviceId;
				if(href== hrefJSON) {
					mustDelete= false; // Node exist; do not delete
					break;
				}
			}
			if(mustDelete) {
				//console.log("Must delete: "+ href); // comment-me
				dvb_subt_service_erase_node(href);
			}
		}
	}

	// Draw those nodes that have been added to representational state.
	// Update selected (not hidden) nodes.
	for(var i= 0; i< currListJSON.length; i++) {
		var mustAddOrUpdate= true;
		var serviceId= currListJSON[i].page_id;
		var hrefJSON= parentNodeId+ '-services-'+ serviceId;
		// Iterate over all currently drawn nodes.
		for(var j= 0; j< ulListItems.length; j++) {
			var ulItem= ulListItems[j];
			if(ulItem.nodeName== "LI") {
				var link= getFirstChildWithTagName(ulItem, 'A');
				var href= getHash(link.getAttribute('href'));
				if(!href || href== null)
					continue;
				if(href== hrefJSON) {
					if(!link.classList.contains('selected')) {
						// Node exist but is not selected; do not draw
						// but only update link
						mustAddOrUpdate= false;
					}
					break;
				}
			}
		}
		if(mustAddOrUpdate) {
			// Attach service data and service settings together
			services_num= es_proc_json.settings.services!= null? 
					es_proc_json.settings.services.length: 0;
			for(var j= 0; j< services_num; j++) {
				service_setting_json= es_proc_json.settings.services[j];
				if(service_setting_json.id== serviceId) {
					currListJSON[i]["settings"]= service_setting_json;
					break;
				}
			}
			//console.log("Must add or update: "+ hrefJSON); // comment-me
			//console.log(JSON.stringify(currListJSON[i])); //comment-me
			dvb_subt_service_manager(utils_id2url(hrefJSON), currListJSON[i]);
		}
	} // for(var i= 0; i< currListJSON.length; i++)
}

function dvb_subt_service_manager(url, dvb_subt_service_json)
{
	// Just draw node
	drawNode();

	function drawNode()
	{
		var nodeUrlSchemeTag= '/services/';
		var nodeId;
		var nodeDiv;
		var servicesPHId;

		// Do not create service node if we do not have yet a display set 
		// (just return)
		if(dvb_subt_service_json.display_set_in== null)
			return;

		// Compose HTML id for this node
		nodeId= utils_url2id(url);

		// Compose HTML id for the parent node
		// Note that we are attaching service nodes within the ES-processor 
		// node. Parent node format:
		// '-demuxers-ID1-program_processors-ID2-es_processors-ID3'
		var demuxerId= utils_findIDinUrl(url, '/demuxers/');
		var programId= utils_findIDinUrl(url, '/program_processors/');
		var esId= utils_findIDinUrl(url, '/es_processors/');
		var parentUrl= '/demuxers/'+ demuxerId+ '/program_processors/'+ 
				programId+ '/es_processors/'+ esId+ '.json';
		servicesPHId= '-demuxers-'+ demuxerId+ '-program_processors-'+ 
				programId+ '-es_processors-'+ esId+ '-services_placeholder';
		//console.log("nodeId: '"+ nodeId+ "'"); //comment-me
		//console.log("servicesPHId: '"+ servicesPHId+ "'"); //comment-me

		// Compose local elements Ids.
		var serviceSettingsDivId= nodeId+ '_settings';
		var displayWidthId= serviceSettingsDivId+ '_display_width';
		var displayWidthTypsiId= displayWidthId+ '_typsi';
		var displayHeightId= serviceSettingsDivId+ '_display_height';
		var displayHeightTypsiId= displayHeightId+ '_typsi';
		var scaleFactorId= serviceSettingsDivId+ '_scale_percentage';
		var scaleFactorTypsiId= scaleFactorId+ '_typsi';
		var durationMaxId= serviceSettingsDivId+ '_duration_max_seg';
		var durationMaxTypsiId= durationMaxId+ '_typsi';
		var durationMinId= serviceSettingsDivId+ '_duration_min_seg';
		var durationMinTypsiId= durationMinId+ '_typsi';
		var durationOffsetId= serviceSettingsDivId+ '_duration_offset_seg';
		var durationOffsetTypsiId= durationOffsetId+ '_typsi';
		var verticalOffTopId= serviceSettingsDivId+ '_vertical_off_top';
		var verticalOffBotId= serviceSettingsDivId+ '_vertical_off_bot';
		var verticalOffTopTypsiId= verticalOffTopId+ '_typsi';
		var verticalOffBotTypsiId= verticalOffBotId+ '_typsi';
		var horizontalOffId= serviceSettingsDivId+ '_horizontal_off';
		var horizontalOffTypsiId= horizontalOffId+ '_typsi';
		var mostUsedPelColorId= serviceSettingsDivId+ 
			'_most_used_pixel_color_rgb24b';
		var mostUsedPelColorTypsiId= mostUsedPelColorId+ '_typsi';
		var secondaryUsedPelColorId= serviceSettingsDivId+ 
			'_secondary_used_pixel_color_rgb24b';
		var secondaryUsedPelColorTypsiId= secondaryUsedPelColorId+ '_typsi';
		var backgroundPelColorId= serviceSettingsDivId+ 
			'_background_pixel_color_rgb24b';
		var backgroundPelColorTypsiId= backgroundPelColorId+ '_typsi';
		var submitServiceSettingsAlertModalDivId= serviceSettingsDivId+ 
				'_alert_modal_submit';
		var ddsTableElemId= nodeId+ '_ddsTable';
		var pcsTableElemId= nodeId+ '_pcsTable';
		var rcsSufixId= nodeId+ '_rcs';
		var rcsDivId= rcsSufixId+ '_div';
		var dvbSubtPreviewPlaceholderDivId= nodeId+ 
				'_dvbSubtPreviewPlaceholder';
		var dvbSubtInputDataDivId= nodeId+ '_inputData';
		var dvbSubtPreviewDivId= dvbSubtInputDataDivId+ '_dvbSubtPreview';

		// Some locals
		var serviceSettings= dvb_subt_service_json.settings;
		var displayWidth= serviceSettings.display_width;
		var displayHeight= serviceSettings.display_height;
		var scaleFactor= serviceSettings.scale_percentage;
		var durationMin= serviceSettings.duration_min_seg;
		var durationMax= serviceSettings.duration_max_seg;
		var durationOffset= serviceSettings.duration_offset_seg;
		var verticalOffTop= serviceSettings.vertical_off_top;
		var verticalOffBot= serviceSettings.vertical_off_bot;
		var horizontalOff= serviceSettings.horizontal_off;
		var mostUsedPelColor= serviceSettings.most_used_pixel_color_rgb24b;
		var secondaryUsedPelColor= 
			serviceSettings.secondary_used_pixel_color_rgb24b;
		var backgroundPelColor= serviceSettings.background_pixel_color_rgb24b;
		var serviceId= dvb_subt_service_json.page_id;
		var dds= dvb_subt_service_json.display_set_in.dds;
		var pcs= dvb_subt_service_json.display_set_in.pcs;
		var rcSegments= dvb_subt_service_json.display_set_in.rcss;
		var mostUsedPelColorDisplaySet= 
			dvb_subt_service_json.display_set_in.most_used_pixel_color_rgb24b;

		// Texts
		var displayWidthTypsiTxt= 
			"Display with. Left blank or set to zero to keep the input " +
			"default value. Range goes from 0 to 4096.";
		var displayHeightTypsiTxt= 
			"Display height. Left blank or set to zer to keep the input " +
			"default value. Range goes from 0 to 4096.";
		var durationMaxTypsiTxt= 
			"Maximum subtitle duration in screen [seconds]. " +
			"Range goes from 0 to 30 seconds.";
		var durationMinTypsiTxt= 
			"Minimum subtitle duration in screen [seconds]. " +
			"Range goes from 0 to 30 seconds. IMPORTANT: note that some " +
			"players/STBs internally define a fixed time-out.";
		var durationOffsetTypsiTxt= 
			"Subtitle duration offset in screen [seconds]. " +
			"Range goes from 0 to 10 seconds. IMPORTANT: note that some " +
			"players/STBs internally define a fixed time-out.";
		var scaleFactorTypsiTxt=
			"Scale factor in % units. Range goes from 50% to 300% " +
			"(Set to 100 to disable scaling).";
		var verticalOffTopTypsiTxt= 
			"Subtitle objects vertical offset, for objects " +
			"which are originally positioned in the top half of " +
			"the display (the original position of the first " +
			"line of the object is taken as the position " +
			"reference). " +
			"A positive value moves subtitle down, a negative " +
			"value moves it up.";
		var verticalOffBotTypsiTxt= 
			"Subtitle objects vertical offset, for objects " +
			"which are originally positioned in the bottom " +
			"half of the display (the original position of the " +
			"first line of the object is taken as the position " +
			"reference). " +
			"A positive value moves subtitle down, a negative " +
			"value moves it up."
		var horizontalOffTypsiTxt= 
			"Subtitle objects horizontal offset. " +
			"A positive value moves subtitle to the right, a " +
			"negative value to the left."
		var mostUsedPelColorTypsiTxt= 
			"Subtitle primary most used pixel color in RGB format.";
		var secondaryUsedPelColorTypsiTxt= 
			"Subtitle secondary most used pixel color in RGB format.";
		var backgroundPelColorTypsiTxt= 
			"Subtitle background pixel color in RGB format.";

		// Create or update node division
		nodeDiv= document.getElementById(nodeId);
		if(!nodeDiv) {
			// Create program node
			// IMPORTANT NOTE: we will always have a page composition 
			// segment (PCS) at this point.
			nodeDiv= createNode();
		} else {
			// Update program node
			updateNode();
		}

		function createNode()
		{
			// Create node link and division
			// Open node only if we have both page and region(s) information.
			var nodeTxt= "Service/page Id.: "+ serviceId+ 
					" (0x"+ serviceId.toString(16)+ ") ";
			nodeDiv= nodes_create(nodeId, servicesPHId, esProcDetailsClass, 
					nodeUrlSchemeTag, nodeTxt, 
					(!pcs || !rcSegments.length)? false: true);

			// ************************************************************** //
			// Create service settings node
			// ************************************************************** //
			var serviceSettingsDiv= nodes_simpleDropDownCreate(
					serviceSettingsDivId, nodeId, esProcDetailsClass, 
					"Service settings", true);
			var table= document.createElement("table");
			table.classList.add("settings");
			table.classList.add("es");
			// - Table caption: "Service settings"
			var caption= document.createElement("caption");
			caption.classList.add("settings");
			table.appendChild(caption);
			// Row: "display_width"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("settings");
			td0.appendChild(document.createTextNode(
					"Display width [pixels]: "));
			var td1= document.createElement("td");
			td1.classList.add("settings");
			td1.classList.add("unselect");
			td1.id= displayWidthId;
			td1.appendChild(document.createTextNode(
					(!displayWidth || displayWidth== 0)? "input default": 
						displayWidth));
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// Row: "display_height"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("settings");
			td0.appendChild(document.createTextNode(
					"Display height [pixels]: "));
			var td1= document.createElement("td");
			td1.classList.add("settings");
			td1.classList.add("unselect");
			td1.id= displayHeightId;
			td1.appendChild(document.createTextNode(
					(!displayHeight || displayHeight== 0)? "input default": 
						displayHeight));
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// Row: "scale_percentage"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("settings");
			td0.appendChild(document.createTextNode(
					"Scale factor [%]: "));
			var a= document.createElement('a');
			a.href= "#";
			a.id= scaleFactorTypsiId;
			a.appendChild(document.createTextNode("[?]"));
			a.title= scaleFactorTypsiTxt;
			a.onclick = function() {return false;};
			td0.appendChild(a);
			var td1= document.createElement("td");
			td1.classList.add("settings");
			td1.classList.add("unselect");
			td1.id= scaleFactorId;
			td1.appendChild(document.createTextNode(scaleFactor));
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// Row: "duration_max_seg"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("settings");
			td0.appendChild(document.createTextNode(
					"Maximum duration [seconds]: "));
			var a= document.createElement('a');
			a.href= "#";
			a.id= durationMaxTypsiId;
			a.appendChild(document.createTextNode("[?]"));
			a.title= durationMaxTypsiTxt;
			a.onclick = function() {return false;};
			td0.appendChild(a);
			var td1= document.createElement("td");
			td1.classList.add("settings");
			td1.classList.add("unselect");
			td1.id= durationMaxId;
			td1.appendChild(document.createTextNode(durationMax));
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// Row: "duration_min_seg"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("settings");
			td0.appendChild(document.createTextNode(
					"Minimum duration [seconds]: "));
			var a= document.createElement('a');
			a.href= "#";
			a.id= durationMinTypsiId;
			a.appendChild(document.createTextNode("[?]"));
			a.title= durationMinTypsiTxt;
			a.onclick = function() {return false;};
			td0.appendChild(a);
			var td1= document.createElement("td");
			td1.classList.add("settings");
			td1.classList.add("unselect");
			td1.id= durationMinId;
			td1.appendChild(document.createTextNode(durationMin));
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// Row: "duration_offset_seg"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("settings");
			td0.appendChild(document.createTextNode(
					"Current duration offset [seconds]: "));
			var a= document.createElement('a');
			a.href= "#";
			a.id= durationOffsetTypsiId;
			a.appendChild(document.createTextNode("[?]"));
			a.title= durationOffsetTypsiTxt;
			a.onclick = function() {return false;};
			td0.appendChild(a);
			var td1= document.createElement("td");
			td1.classList.add("settings");
			td1.classList.add("unselect");
			td1.id= durationOffsetId;
			td1.appendChild(document.createTextNode(durationOffset));
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// Row: "vertical_off_top"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("settings");
			td0.appendChild(document.createTextNode(
					"Vertical offset top-half [pixels]: "));
			var a= document.createElement('a');
			a.href= "#";
			a.id= verticalOffTopTypsiId;
			a.appendChild(document.createTextNode("[?]"));
			a.title= verticalOffTopTypsiTxt;
			a.onclick = function() {return false;};
			td0.appendChild(a);
			var td1= document.createElement("td");
			td1.classList.add("settings");
			td1.classList.add("unselect");
			td1.id= verticalOffTopId;
			td1.appendChild(document.createTextNode(verticalOffTop));
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// Row: "vertical_off_bot"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("settings");
			td0.appendChild(document.createTextNode(
					"Vertical offset bottom-half [pixels]: "));
			var a= document.createElement('a');
			a.href= "#";
			a.id= verticalOffBotTypsiId;
			a.appendChild(document.createTextNode("[?]"));
			a.title= verticalOffBotTypsiTxt;
			a.onclick = function() {return false;};
			td0.appendChild(a);
			var td1= document.createElement("td");
			td1.classList.add("settings");
			td1.classList.add("unselect");
			td1.id= verticalOffBotId;
			td1.appendChild(document.createTextNode(verticalOffBot));
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// Row: "horizontal_off"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("settings");
			td0.appendChild(document.createTextNode(
					"Horizontal offset [pixels]: "));
			var a= document.createElement('a');
			a.href= "#";
			a.id= horizontalOffTypsiId;
			a.appendChild(document.createTextNode("[?]"));
			a.title= horizontalOffTypsiTxt;
			a.onclick = function() {return false;};
			td0.appendChild(a);
			var td1= document.createElement("td");
			td1.classList.add("settings");
			td1.classList.add("unselect");
			td1.id= horizontalOffId;
			td1.appendChild(document.createTextNode(horizontalOff));
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// Row: "most_used_pixel_color_rgb24b"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("settings");
			td0.appendChild(document.createTextNode(
					"Subtitle primary used color [RGB]: "));
			var a= document.createElement('a');
			a.href= "#";
			a.id= mostUsedPelColorTypsiId;
			a.appendChild(document.createTextNode("[?]"));
			a.title= mostUsedPelColorTypsiTxt;
			a.onclick = function() {return false;};
			td0.appendChild(a);
			var td1= document.createElement("td");
			td1.classList.add("settings");
			td1.classList.add("unselect");
			td1.id= mostUsedPelColorId;
			if(mostUsedPelColor< 0) {
				td1.appendChild(document.createTextNode("input default"));
			} else {
				var div= document.createElement("div");
				div.style.color= 'rgb('+ 
				/*R*/(mostUsedPelColor>>> 16)& 0xFF+ ','+ 
				/*G*/(mostUsedPelColor>>>  8)& 0xFF+ ','+ 
				/*B*/(mostUsedPelColor      )& 0xFF+ ');'
				td1.appendChild(div);
			}
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// Row: "secondary_used_pixel_color_rgb24b"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("settings");
			td0.appendChild(document.createTextNode(
					"Subtitle secondary used color [RGB]: "));
			var a= document.createElement('a');
			a.href= "#";
			a.id= secondaryUsedPelColorTypsiId;
			a.appendChild(document.createTextNode("[?]"));
			a.title= secondaryUsedPelColorTypsiTxt;
			a.onclick = function() {return false;};
			td0.appendChild(a);
			var td1= document.createElement("td");
			td1.classList.add("settings");
			td1.classList.add("unselect");
			td1.id= secondaryUsedPelColorId;
			if(secondaryUsedPelColor< 0) {
				td1.appendChild(document.createTextNode("input default"));
			} else {
				var div= document.createElement("div");
				div.style.color= 'rgb('+ 
				/*R*/(secondaryUsedPelColor>>> 16)& 0xFF+ ','+ 
				/*G*/(secondaryUsedPelColor>>>  8)& 0xFF+ ','+ 
				/*B*/(secondaryUsedPelColor      )& 0xFF+ ');'
				td1.appendChild(div);
			}
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// Row: "background_pixel_color_rgb24b"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("settings");
			td0.appendChild(document.createTextNode(
					"Subtitle background color [RGB]: "));
			var a= document.createElement('a');
			a.href= "#";
			a.id= backgroundPelColorTypsiId;
			a.appendChild(document.createTextNode("[?]"));
			a.title= backgroundPelColorTypsiTxt;
			a.onclick = function() {return false;};
			td0.appendChild(a);
			var td1= document.createElement("td");
			td1.classList.add("settings");
			td1.classList.add("unselect");
			td1.id= backgroundPelColorId;
			if(backgroundPelColor< 0) {
				td1.appendChild(document.createTextNode("input default"));
			} else {
				var div= document.createElement("div");
				div.style.color= 'rgb('+ 
				/*R*/(backgroundPelColor>>> 16)& 0xFF+ ','+ 
				/*G*/(backgroundPelColor>>>  8)& 0xFF+ ','+ 
				/*B*/(backgroundPelColor      )& 0xFF+ ');'
				td1.appendChild(div);
			}
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
			submitSettings.value= "Edit service settings";
			submitSettings.addEventListener('click', function(event){
				serviceSettingsModal(nodeId);
			});
			td0.appendChild(submitSettings);
			var td1= document.createElement("td");
			td1.classList.add("settings");
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// - Append table
			serviceSettingsDiv.appendChild(table);
			// - Format table 'tipsy's'
			$(function() {
				$('#'+ scaleFactorTypsiId).tipsy({html: true, gravity: 'w'});
				$('#'+ durationMaxTypsiId).tipsy({html: true, gravity: 'w'});
				$('#'+ durationMinTypsiId).tipsy({html: true, gravity: 'w'});
				$('#'+ durationOffsetTypsiId).tipsy({html: true, gravity: 'w'});
				$('#'+ verticalOffTopTypsiId).tipsy({html: true, gravity: 'w'});
				$('#'+ verticalOffBotTypsiId).tipsy({html: true, gravity: 'w'});
				$('#'+ horizontalOffTypsiId).tipsy({html: true, gravity: 'w'});
				$('#'+ mostUsedPelColorTypsiId).tipsy(
						{html: true, gravity: 'w'});
				$('#'+ secondaryUsedPelColorTypsiId).tipsy(
						{html: true, gravity: 'w'});
				$('#'+ backgroundPelColorTypsiId).tipsy(
						{html: true, gravity: 'w'});
			});

			// ********************************************************** //
			// Create input data information simple node
			// ********************************************************** //
			dvbSubtInputDataDiv= nodes_simpleDropDownCreate(
					dvbSubtInputDataDivId, nodeId, esProcDetailsClass, 
					"Input information", false);
			createDisplaySetData(dvbSubtInputDataDivId, dvbSubtInputDataDiv, 
					dvbSubtPreviewPlaceholderDivId, ddsTableElemId, 
					pcsTableElemId, rcsDivId);

			return nodeDiv;
		}

		function serviceSettingsModal(servicesPHId)
		{
			var modalIdSufix= "serviceSettings";
			var modalPrivateContentId= servicesPHId+ '_serviceSettingsPrivate';

			var modalObj= modals_create_generic_modal(servicesPHId, 
					modalIdSufix, ["settings", "es"]);
			if(modalObj== null) {
				// Modal already exist, update body content
				httpRequests_respJSON(parentUrl, 'GET', '', null, 
						{onResponse: reloadServiceSettings});
				// - Display the modal 
				modals_display_modal(servicesPHId, modalIdSufix);
			} else {
				// Create private modal content
				var divModalContentBody= modalObj.bodyDiv;
				var divModalContentHeader= modalObj.headerDiv;
				// Header
				divModalContentHeader.appendChild(document.createTextNode(
						"DVB subtitling service processor settings"));
				// Body
				var privModalDiv= createServiceSettingsModal();
				privModalDiv.id= modalPrivateContentId;
				divModalContentBody.appendChild(privModalDiv);
				// - Reload settings table values
				httpRequests_respJSON(parentUrl, 'GET', '', null, 
						{onResponse: reloadServiceSettings});
				// Display the modal 
				modals_display_modal(servicesPHId, modalIdSufix);
				// - Format table 'tipsy's' //FIXME!! TODO!!
				$(function() {
					$('#'+ displayWidthTypsiId+ '_new').tipsy(
							{html: true, gravity: 'w'});
					$('#'+ displayHeightTypsiId+ '_new').tipsy(
							{html: true, gravity: 'w'});
					$('#'+ scaleFactorTypsiId+ '_new').tipsy(
							{html: true, gravity: 'w'});
					$('#'+ durationMaxTypsiId+ '_new').tipsy(
							{html: true, gravity: 'w'});
					$('#'+ durationMinTypsiId+ '_new').tipsy(
							{html: true, gravity: 'w'});
					$('#'+ durationOffsetTypsiId+ '_new').tipsy(
							{html: true, gravity: 'w'});
					$('#'+ verticalOffTopTypsiId+ '_new').tipsy(
							{html: true, gravity: 'w'});
					$('#'+ verticalOffBotTypsiId+ '_new').tipsy(
							{html: true, gravity: 'w'});
					$('#'+ horizontalOffTypsiId+ '_new').tipsy(
							{html: true, gravity: 'w'});
					$('#'+ mostUsedPelColorTypsiId+ '_new').tipsy(
							{html: true, gravity: 'w'});
					$('#'+ secondaryUsedPelColorTypsiId+ '_new').tipsy(
							{html: true, gravity: 'w'});
					$('#'+ backgroundPelColorTypsiId+ '_new').tipsy(
							{html: true, gravity: 'w'});
				});
			}

			function createServiceSettingsModal()
			{
				// Modal private DIV
				var privModalDiv= document.createElement("div");
				// Settings table
				var table= document.createElement("table");
				table.classList.add("settings");
				table.classList.add("es");
				// Row: "display_width"
				var tr= document.createElement("tr");
				var td0= document.createElement("td");
				td0.classList.add("settings");
				td0.appendChild(document.createTextNode(
						"Display width [pixels]: "));
				var a= document.createElement('a');
				a.href= "#";
				a.id= displayWidthTypsiId+ '_new';
				a.appendChild(document.createTextNode("[?]"));
				a.title= displayWidthTypsiTxt;
				a.onclick = function() {return false;};
				td0.appendChild(a);
				var td1= document.createElement("td");
				td1.classList.add("settings");
				var input= document.createElement("input");
				input.classList.add("settings");
				input.classList.add("es");
				input.type= "text";
				input.id= displayWidthId+ '_new';
				input.value= (!displayWidth || "input default")? "": 
					displayWidth;
				td1.appendChild(input);
				tr.appendChild(td0);
				tr.appendChild(td1);
				table.appendChild(tr);
				// Row: "display_height"
				var tr= document.createElement("tr");
				var td0= document.createElement("td");
				td0.classList.add("settings");
				td0.appendChild(document.createTextNode(
						"Display height [pixels]: "));
				var a= document.createElement('a');
				a.href= "#";
				a.id= displayHeightTypsiId+ '_new';
				a.appendChild(document.createTextNode("[?]"));
				a.title= displayHeightTypsiTxt;
				a.onclick = function() {return false;};
				td0.appendChild(a);
				var td1= document.createElement("td");
				td1.classList.add("settings");
				var input= document.createElement("input");
				input.classList.add("settings");
				input.classList.add("es");
				input.type= "text";
				input.id= displayHeightId+ '_new';
				input.value= (!displayHeight || "input default")? "": 
					displayHeight;
				td1.appendChild(input);
				tr.appendChild(td0);
				tr.appendChild(td1);
				table.appendChild(tr);
				// Row: "scale_percentage"
				var tr= document.createElement("tr");
				var td0= document.createElement("td");
				td0.classList.add("settings");
				td0.appendChild(document.createTextNode(
						"Scale factor [%]: "));
				var a= document.createElement('a');
				a.href= "#";
				a.id= scaleFactorTypsiId+ '_new';
				a.appendChild(document.createTextNode("[?]"));
				a.title= scaleFactorTypsiTxt;
				a.onclick = function() {return false;};
				td0.appendChild(a);
				var td1= document.createElement("td");
				td1.classList.add("settings");
				var input= document.createElement("input");
				input.classList.add("settings");
				input.classList.add("es");
				input.type= "text";
				input.id= scaleFactorId+ '_new';
				input.value= scaleFactor;
				td1.appendChild(input);
				tr.appendChild(td0);
				tr.appendChild(td1);
				table.appendChild(tr);
				// Row: "duration_max_seg"
				var tr= document.createElement("tr");
				var td0= document.createElement("td");
				td0.classList.add("settings");
				td0.appendChild(document.createTextNode(
						"Maximum duration [seconds]: "));
				var a= document.createElement('a');
				a.href= "#";
				a.id= durationMaxTypsiId+ '_new';
				a.appendChild(document.createTextNode("[?]"));
				a.title= durationMaxTypsiTxt;
				a.onclick = function() {return false;};
				td0.appendChild(a);
				var td1= document.createElement("td");
				td1.classList.add("settings");
				var input= document.createElement("input");
				input.classList.add("settings");
				input.classList.add("es");
				input.type= "text";
				input.id= durationMaxId+ '_new';
				input.value= durationMax;
				td1.appendChild(input);
				tr.appendChild(td0);
				tr.appendChild(td1);
				table.appendChild(tr);
				// Row: "duration_min_seg"
				var tr= document.createElement("tr");
				var td0= document.createElement("td");
				td0.classList.add("settings");
				td0.appendChild(document.createTextNode(
						"Minimum duration [seconds]: "));
				var a= document.createElement('a');
				a.href= "#";
				a.id= durationMinTypsiId+ '_new';
				a.appendChild(document.createTextNode("[?]"));
				a.title= durationMinTypsiTxt;
				a.onclick = function() {return false;};
				td0.appendChild(a);
				var td1= document.createElement("td");
				td1.classList.add("settings");
				var input= document.createElement("input");
				input.classList.add("settings");
				input.classList.add("es");
				input.type= "text";
				input.id= durationMinId+ '_new';
				input.value= durationMin;
				td1.appendChild(input);
				tr.appendChild(td0);
				tr.appendChild(td1);
				table.appendChild(tr);
				// Row: "duration_offset_seg"
				var tr= document.createElement("tr");
				var td0= document.createElement("td");
				td0.classList.add("settings");
				td0.appendChild(document.createTextNode(
						"Current duration offset [seconds]: "));
				var a= document.createElement('a');
				a.href= "#";
				a.id= durationOffsetTypsiId+ '_new';
				a.appendChild(document.createTextNode("[?]"));
				a.title= durationOffsetTypsiTxt;
				a.onclick = function() {return false;};
				td0.appendChild(a);
				var td1= document.createElement("td");
				td1.classList.add("settings");
				var input= document.createElement("input");
				input.classList.add("settings");
				input.classList.add("es");
				input.type= "text";
				input.id= durationOffsetId+ '_new';
				input.value= durationOffset;
				td1.appendChild(input);
				tr.appendChild(td0);
				tr.appendChild(td1);
				table.appendChild(tr);
				// Row: "vertical_off_top"
				var tr= document.createElement("tr");
				var td0= document.createElement("td");
				td0.classList.add("settings");
				td0.appendChild(document.createTextNode(
						"Vertical offset top-half [pixels]: "));
				var a= document.createElement('a');
				a.href= "#";
				a.id= verticalOffTopTypsiId+ '_new';
				a.appendChild(document.createTextNode("[?]"));
				a.title= verticalOffTopTypsiTxt;
				a.onclick = function() {return false;};
				td0.appendChild(a);
				var td1= document.createElement("td");
				td1.classList.add("settings");
				var input= document.createElement("input");
				input.classList.add("settings");
				input.classList.add("es");
				input.type= "text";
				input.id= verticalOffTopId+ '_new';
				input.value= verticalOffTop;
				td1.appendChild(input);
				tr.appendChild(td0);
				tr.appendChild(td1);
				table.appendChild(tr);
				// Row: "vertical_off_bot"
				var tr= document.createElement("tr");
				var td0= document.createElement("td");
				td0.classList.add("settings");
				td0.appendChild(document.createTextNode(
						"Vertical offset bottom-half [pixels]: "));
				var a= document.createElement('a');
				a.href= "#";
				a.id= verticalOffBotTypsiId+ '_new';
				a.appendChild(document.createTextNode("[?]"));
				a.title= verticalOffBotTypsiTxt;
				a.onclick = function() {return false;};
				td0.appendChild(a);
				var td1= document.createElement("td");
				td1.classList.add("settings");
				var input= document.createElement("input");
				input.classList.add("settings");
				input.classList.add("es");
				input.type= "text";
				input.id= verticalOffBotId+ '_new';
				input.value= verticalOffBot;
				td1.appendChild(input);
				tr.appendChild(td0);
				tr.appendChild(td1);
				table.appendChild(tr);
				// Row: "horizontal_off"
				var tr= document.createElement("tr");
				var td0= document.createElement("td");
				td0.classList.add("settings");
				td0.appendChild(document.createTextNode(
						"Horizontal offset [pixels]: "));
				var a= document.createElement('a');
				a.href= "#";
				a.id= horizontalOffTypsiId+ '_new';
				a.appendChild(document.createTextNode("[?]"));
				a.title= horizontalOffTypsiTxt;
				a.onclick = function() {return false;};
				td0.appendChild(a);
				var td1= document.createElement("td");
				td1.classList.add("settings");
				var input= document.createElement("input");
				input.classList.add("settings");
				input.classList.add("es");
				input.type= "text";
				input.id= horizontalOffId+ '_new';
				input.value= horizontalOff;
				td1.appendChild(input);
				tr.appendChild(td0);
				tr.appendChild(td1);
				table.appendChild(tr);
				// Row: "most_used_pixel_color_rgb24b"
				var tr= document.createElement("tr");
				var td0= document.createElement("td");
				td0.classList.add("settings");
				td0.appendChild(document.createTextNode(
						"Subtitle primary used color [RGB]: "));
				var a= document.createElement('a');
				a.href= "#";
				a.id= mostUsedPelColorTypsiId+ '_new';
				a.appendChild(document.createTextNode("[?]"));
				a.title= mostUsedPelColorTypsiTxt+ " "+ 
					"Left blank to use default input color.";
				a.onclick = function() {return false;};
				td0.appendChild(a);
				var td1= document.createElement("td");
				td1.classList.add("settings");
				var input= document.createElement("input");
				input.classList.add("settings");
				input.classList.add("es");
				input.style.width= "80%";
				input.type= "input";
				input.id= mostUsedPelColorId+ '_new';
				if(mostUsedPelColor< 0) {
					input.value= "";
				} else {
					mostUsedPelColor= mostUsedPelColor.toString(16);
					while(mostUsedPelColor.length< 6) {
						mostUsedPelColor= '0'+ mostUsedPelColor;
					}
					input.value= '#'+ mostUsedPelColor;
				}
				var colorPick= document.createElement("input");
				colorPick.id= mostUsedPelColorId+ '_new'+ '_colorPick';
				colorPick.style.width= "20%";
				colorPick.type= "color";
				colorPick.onclick= function() {
					if(input.value!= "") {
						colorPick.value= input.value;
					}
				}
				colorPick.onchange= function() {
					input.value= colorPick.value;
				}
				colorPick.oninput= function() {
					input.value= colorPick.value;
				}
				if(input.value!= "") {
					colorPick.value= input.value;
				}
				td1.appendChild(input);
				td1.appendChild(colorPick);
				tr.appendChild(td0);
				tr.appendChild(td1);
				table.appendChild(tr);
				// Row: "secondary_used_pixel_color_rgb24b"
				var tr= document.createElement("tr");
				var td0= document.createElement("td");
				td0.classList.add("settings");
				td0.appendChild(document.createTextNode(
						"Subtitle secondary used color [RGB]: "));
				var a= document.createElement('a');
				a.href= "#";
				a.id= secondaryUsedPelColorTypsiId+ '_new';
				a.appendChild(document.createTextNode("[?]"));
				a.title= secondaryUsedPelColorTypsiTxt+ " "+ 
					"Left blank to use default input color.";
				a.onclick = function() {return false;};
				td0.appendChild(a);
				var td1= document.createElement("td");
				td1.classList.add("settings");
				var input2= document.createElement("input");
				input2.classList.add("settings");
				input2.classList.add("es");
				input2.style.width= "80%";
				input2.type= "input";
				input2.id= secondaryUsedPelColorId+ '_new';
				if(secondaryUsedPelColor< 0) {
					input2.value= "";
				} else {
					secondaryUsedPelColor= secondaryUsedPelColor.toString(16);
					while(secondaryUsedPelColor.length< 6) {
						secondaryUsedPelColor= '0'+ secondaryUsedPelColor;
					}
					input2.value= '#'+ secondaryUsedPelColor;
				}
				var colorPick2= document.createElement("input");
				colorPick2.id= secondaryUsedPelColorId+ '_new'+ '_colorPick';
				colorPick2.style.width= "20%";
				colorPick2.type= "color";
				colorPick2.onclick= function() {
					if(input2.value!= "") {
						colorPick2.value= input2.value;
					}
				}
				colorPick2.onchange= function() {
					input2.value= colorPick2.value;
				}
				colorPick2.oninput= function() {
					input2.value= colorPick2.value;
				}
				if(input2.value!= "") {
					colorPick2.value= input2.value;
				}
				td1.appendChild(input2);
				td1.appendChild(colorPick2);
				tr.appendChild(td0);
				tr.appendChild(td1);
				table.appendChild(tr);
				// Row: "background_pixel_color_rgb24b"
				var tr= document.createElement("tr");
				var td0= document.createElement("td");
				td0.classList.add("settings");
				td0.appendChild(document.createTextNode(
						"Subtitle background color [RGB]: "));
				var a= document.createElement('a');
				a.href= "#";
				a.id= backgroundPelColorTypsiId+ '_new';
				a.appendChild(document.createTextNode("[?]"));
				a.title= backgroundPelColorTypsiTxt+ " "+ 
					"Left blank to use default input color.";
				a.onclick = function() {return false;};
				td0.appendChild(a);
				var td1= document.createElement("td");
				td1.classList.add("settings");
				var input3= document.createElement("input");
				input3.classList.add("settings");
				input3.classList.add("es");
				input3.style.width= "80%";
				input3.type= "input";
				input3.id= backgroundPelColorId+ '_new';
				if(backgroundPelColor< 0) {
					input3.value= "";
				} else {
					backgroundPelColor= backgroundPelColor.toString(16);
					while(backgroundPelColor.length< 6) {
						backgroundPelColor= '0'+ backgroundPelColor;
					}
					input3.value= '#'+ backgroundPelColor;
				}
				var colorPick3= document.createElement("input");
				colorPick3.id= backgroundPelColorId+ '_new'+ '_colorPick';
				colorPick3.style.width= "20%";
				colorPick3.type= "color";
				colorPick3.onclick= function() {
					if(input3.value!= "") {
						colorPick3.value= input3.value;
					}
				}
				colorPick3.onchange= function() {
					input3.value= colorPick3.value;
				}
				colorPick3.oninput= function() {
					input3.value= colorPick3.value;
				}
				if(input3.value!= "") {
					colorPick3.value= input3.value;
				}
				td1.appendChild(input3);
				td1.appendChild(colorPick3);
				tr.appendChild(td0);
				tr.appendChild(td1);
				table.appendChild(tr);

				// - Append table
				privModalDiv.appendChild(table);
				// Submit new settings button
				var submitServiceSettingsAlertModalDiv= 
					document.createElement("div");
				submitServiceSettingsAlertModalDiv.id= 
					submitServiceSettingsAlertModalDivId;
				privModalDiv.appendChild(submitServiceSettingsAlertModalDiv);
				var submitSettings= document.createElement("input");
				submitSettings.classList.add("button");
				submitSettings.classList.add("settings");
				submitSettings.classList.add("es");
				submitSettings.classList.add("modal-button");
				submitSettings.type= "button";
				submitSettings.value= "Submit new settings";
				submitSettings.addEventListener('click', function(event)
				{
					// - display_width
					var displayWidthElem= document.getElementById(
							displayWidthId+ '_new');
					var displayWidth= displayWidthElem.value;
					// - display_height
					var displayHeightElem= document.getElementById(
							displayHeightId+ '_new');
					var displayHeight= displayHeightElem.value;
					// - scale_percentage
					var scaleFactorElem= document.getElementById(
							scaleFactorId+ '_new');
					var scaleFactor= scaleFactorElem.value;
					// - duration_max_seg
					var durationMaxElem= document.getElementById(
							durationMaxId+ '_new');
					var durationMax= durationMaxElem.value;
					// - duration_min_seg
					var durationMinElem= document.getElementById(
							durationMinId+ '_new');
					var durationMin= durationMinElem.value;
					// - duration_offset_seg
					var durationOffsetElem= document.getElementById(
							durationOffsetId+ '_new');
					var durationOffset= durationOffsetElem.value;
					// - vertical_off_top
					var verticalOffTopElem= document.getElementById(
							verticalOffTopId+ '_new');
					var verticalOffTop= verticalOffTopElem.value;
					// - vertical_off_bot
					var verticalOffBotElem= document.getElementById(
							verticalOffBotId+ '_new');
					var verticalOffBot= verticalOffBotElem.value;
					// - horizontal_off
					var horizontalOffElem= document.getElementById(
							horizontalOffId+ '_new');
					var horizontalOff= horizontalOffElem.value;
					// "most_used_pixel_color_rgb24b"
					var mostUsedPelColorElem= 
						document.getElementById(mostUsedPelColorId+ '_new');
					var mostUsedPelColor= mostUsedPelColorElem.value;
					if(!mostUsedPelColor || mostUsedPelColor== "" || 
							mostUsedPelColor< 0) {
						mostUsedPelColor= null;
					} else {
						mostUsedPelColor= parseInt(
								'0x'+ mostUsedPelColor.substring(1));
					}
					// "secondary_used_pixel_color_rgb24b"
					var secondaryUsedPelColorElem=  document.getElementById(
							secondaryUsedPelColorId+ '_new');
					var secondaryUsedPelColor= secondaryUsedPelColorElem.value;
					if(!secondaryUsedPelColor || secondaryUsedPelColor== "" || 
							secondaryUsedPelColor< 0) {
						secondaryUsedPelColor= null;
					} else {
						secondaryUsedPelColor= parseInt(
								'0x'+ secondaryUsedPelColor.substring(1));
					}
					// "background_pixel_color_rgb24b"
					var backgroundPelColorElem=  document.getElementById(
							backgroundPelColorId+ '_new');
					var backgroundPelColor= backgroundPelColorElem.value;
					if(!backgroundPelColor || backgroundPelColor== "" || 
							backgroundPelColor< 0) {
						backgroundPelColor= null;
					} else {
						backgroundPelColor= parseInt(
								'0x'+ backgroundPelColor.substring(1));
					}

					var body_string= '{\"services\":[{';
					body_string+= '\"id\":'+ serviceId+ ','
					if(displayWidth)
						body_string+= '\"display_width\":'+ displayWidth+ ',';
					if(displayHeight)
						body_string+= '\"display_height\":'+ displayHeight+ ',';
					body_string+= '\"scale_percentage\":'+ scaleFactor+ ',';
					body_string+= '\"duration_max_seg\":'+ durationMax+ ',';
					body_string+= '\"duration_min_seg\":'+ durationMin+ ',';
					body_string+= '\"duration_offset_seg\":'+ durationOffset+ 
							',';
					body_string+= '\"vertical_off_top\":'+ verticalOffTop+ ',';
					body_string+= '\"vertical_off_bot\":'+ verticalOffBot+ ',';
					body_string+= '\"horizontal_off\":'+ horizontalOff+ ',';
					body_string+= '\"most_used_pixel_color_rgb24b\":'+ 
						mostUsedPelColor+ ',';
					body_string+= '\"secondary_used_pixel_color_rgb24b\":'+ 
						secondaryUsedPelColor+ ',';
					body_string+= '\"background_pixel_color_rgb24b\":'+ 
						backgroundPelColor;
					body_string+= '}]}';

					// Compose query-string
					var query_string= '';

					// Compose callback object
					var externalCallbackObj= { 
						onResponse: function() {
							modals_close_modal(servicesPHId, modalIdSufix);
						},
						onError: function(alertString) {
							modals_alert_modal(
									submitServiceSettingsAlertModalDivId, 
									alertString);
						}
					};

					// PUT new values
					//console.log('PUT-> '+ body_string); // comment-me
					httpRequests_respJSON(parentUrl, 'PUT', query_string, 
							body_string, externalCallbackObj);
				});
				privModalDiv.appendChild(submitSettings);

				return privModalDiv;
			}

			function reloadServiceSettings(es_proc_json)
			{
				var dvb_subt_services_json= es_proc_json.settings.services;
				if(!dvb_subt_services_json)
					return;
				//console.log(dvb_subt_services_json); // comment-me
				for(var i= 0; i< dvb_subt_services_json.length; i++) {
					var dvb_subt_service_json= dvb_subt_services_json[i];
					//console.log(dvb_subt_service_json); // comment-me
					if(dvb_subt_service_json.id!= serviceId)
						continue;
					// "display_width"
					displayWidth= dvb_subt_service_json.display_width;
					document.getElementById(displayWidthId+ '_new').value= 
						(!displayWidth || displayWidth== 0)? "": 
							displayWidth;
					// "display_height"
					displayHeight= dvb_subt_service_json.display_height;
					document.getElementById(displayHeightId+ '_new').value= 
						(!displayHeight || displayHeight== 0)? "": 
							displayHeight;
					// "scale_percentage"
					document.getElementById(scaleFactorId+ '_new').value= 
						dvb_subt_service_json.scale_percentage;
					// "duration_max_seg"
					document.getElementById(durationMaxId+ '_new').value= 
						dvb_subt_service_json.duration_max_seg;

					// "duration_min_seg"
					document.getElementById(durationMinId+ '_new').value= 
						dvb_subt_service_json.duration_min_seg;
					// "duration_offset_seg"
					document.getElementById(durationOffsetId+ '_new').value= 
						dvb_subt_service_json.duration_offset_seg;
					// "vertical_off_top"
					document.getElementById(verticalOffTopId+ '_new').value= 
						dvb_subt_service_json.vertical_off_top;
					// "vertical_off_bot"
					document.getElementById(verticalOffBotId+ '_new').value= 
						dvb_subt_service_json.vertical_off_bot;
					// "horizontal_off"
					document.getElementById(horizontalOffId+ '_new').value= 
						dvb_subt_service_json.horizontal_off;
					// "most_used_pixel_color_rgb24b"
					var mostUsedPelColor= 
						dvb_subt_service_json.most_used_pixel_color_rgb24b;
					var mostUsedPelColorElem= 
						document.getElementById(mostUsedPelColorId+ '_new');
					if(mostUsedPelColor< 0) {
						mostUsedPelColorElem.value= "";
					} else {
						mostUsedPelColor= mostUsedPelColor.toString(16);
						while(mostUsedPelColor.length< 6) {
							mostUsedPelColor= '0'+ mostUsedPelColor;
						}
						mostUsedPelColorElem.value= '#'+ mostUsedPelColor;
						document.getElementById(
								mostUsedPelColorId+ '_new_colorPick').value= 
									mostUsedPelColorElem.value;
					}
					// "secondary_used_pixel_color_rgb24b"
					var secondaryUsedPelColor= 
						dvb_subt_service_json.secondary_used_pixel_color_rgb24b;
					var secondaryUsedPelColorElem= document.getElementById(
							secondaryUsedPelColorId+ '_new');
					if(secondaryUsedPelColor< 0) {
						secondaryUsedPelColorElem.value= "";
					} else {
						secondaryUsedPelColor= 
							secondaryUsedPelColor.toString(16);
						while(secondaryUsedPelColor.length< 6) {
							secondaryUsedPelColor= '0'+ secondaryUsedPelColor;
						}
						secondaryUsedPelColorElem.value= '#'+ 
							secondaryUsedPelColor;
						document.getElementById(secondaryUsedPelColorId+ 
								'_new_colorPick').value= 
									secondaryUsedPelColorElem.value;
					}
					// "background_pixel_color_rgb24b"
					var backgroundPelColor= 
						dvb_subt_service_json.background_pixel_color_rgb24b;
					var backgroundPelColorElem= document.getElementById(
							backgroundPelColorId+ '_new');
					if(backgroundPelColor< 0) {
						backgroundPelColorElem.value= "";
					} else {
						backgroundPelColor= 
							backgroundPelColor.toString(16);
						while(backgroundPelColor.length< 6) {
							backgroundPelColor= '0'+ backgroundPelColor;
						}
						backgroundPelColorElem.value= '#'+ 
							backgroundPelColor;
						document.getElementById(backgroundPelColorId+ 
								'_new_colorPick').value= 
									backgroundPelColorElem.value;
					}
				}
			}
		}

		function updateNode()
		{
			// Only update if current node is not hidden
			if(nodeDiv.classList.contains('hide'))
				return;

			// Update service settings
			// "display_width"
			var displayWidthElem= document.getElementById(displayWidthId);
			var displayWidth= dvb_subt_service_json.settings.display_width;
			if(!displayWidth || displayWidth== 0)
				displayWidth= "input default";
			if(displayWidthElem.innerHTML!= displayWidth)
				displayWidthElem.innerHTML= displayWidth;
			// "display_height"
			var displayHeightElem= document.getElementById(displayHeightId);
			var displayHeight= dvb_subt_service_json.settings.display_height;
			if(!displayHeight || displayHeight== 0)
				displayHeight= "input default";
			if(displayHeightElem.innerHTML!= displayHeight)
				displayHeightElem.innerHTML= displayHeight;
			// "duration_max_seg"
			var durationMaxElem= document.getElementById(durationMaxId);
			var durationMax= dvb_subt_service_json.settings.duration_max_seg;
			if(durationMaxElem.innerHTML!= durationMax)
				durationMaxElem.innerHTML= durationMax;
			// "duration_min_seg"
			var durationMinElem= document.getElementById(durationMinId);
			var durationMin= dvb_subt_service_json.settings.duration_min_seg;
			if(durationMinElem.innerHTML!= durationMin)
				durationMinElem.innerHTML= durationMin;
			// "duration_offset_seg"
			var durationOffsetElem= document.getElementById(durationOffsetId);
			var durationOffset= 
				dvb_subt_service_json.settings.duration_offset_seg;
			if(durationOffsetElem.innerHTML!= durationOffset)
				durationOffsetElem.innerHTML= durationOffset;
			// - "scale_percentage"
			var scaleFactorElem= document.getElementById(scaleFactorId);
			var scaleFactor= dvb_subt_service_json.settings.scale_percentage;
			if(scaleFactorElem.innerHTML!= scaleFactor)
				scaleFactorElem.innerHTML= scaleFactor;
			// - "vertical_off_top"
			var verticalOffTopElem= document.getElementById(verticalOffTopId);
			var verticalOffTop= dvb_subt_service_json.settings.vertical_off_top;
			if(verticalOffTopElem.innerHTML!= verticalOffTop)
				verticalOffTopElem.innerHTML= verticalOffTop;
			// - "vertical_off_bot"
			var verticalOffBotElem= document.getElementById(verticalOffBotId);
			var verticalOffBot= dvb_subt_service_json.settings.vertical_off_bot;
			if(verticalOffBotElem.innerHTML!= verticalOffBot)
				verticalOffBotElem.innerHTML= verticalOffBot;
			// - "horizontal_off"
			var horizontalOffElem= document.getElementById(horizontalOffId);
			var horizontalOff= dvb_subt_service_json.settings.horizontal_off;
			if(horizontalOffElem.innerHTML!= horizontalOff)
				horizontalOffElem.innerHTML= horizontalOff;
			// "most_used_pixel_color_rgb24b"
			var mostUsedPelColorElem= 
				document.getElementById(mostUsedPelColorId);
			var mostUsedPelColor= 
				dvb_subt_service_json.settings.most_used_pixel_color_rgb24b;
			if(mostUsedPelColor< 0) {
				mostUsedPelColor= 'input default';
			} else {
				var style= window.getComputedStyle(mostUsedPelColorElem, null);
				mostUsedPelColor= '<div style="background-color:rgb('+ 
				/*R*/((mostUsedPelColor>>> 16)& 0xFF)+ ','+ 
				/*G*/((mostUsedPelColor>>>  8)& 0xFF)+ ','+ 
				/*B*/((mostUsedPelColor      )& 0xFF)+ '); width:20%;' + 
					'height:'+ style.getPropertyValue("height")+ '"></div>';
			}
			if(mostUsedPelColorElem.innerHTML!= mostUsedPelColor)
				mostUsedPelColorElem.innerHTML= mostUsedPelColor;
			// "secondary_used_pixel_color_rgb24b"
			var secondaryUsedPelColorElem= 
				document.getElementById(secondaryUsedPelColorId);
			var secondaryUsedPelColor= dvb_subt_service_json.settings.
				secondary_used_pixel_color_rgb24b;
			if(secondaryUsedPelColor< 0) {
				secondaryUsedPelColor= 'input default';
			} else {
				var style= window.getComputedStyle(secondaryUsedPelColorElem, 
						null);
				secondaryUsedPelColor= '<div style="background-color:rgb('+ 
				/*R*/((secondaryUsedPelColor>>> 16)& 0xFF)+ ','+ 
				/*G*/((secondaryUsedPelColor>>>  8)& 0xFF)+ ','+ 
				/*B*/((secondaryUsedPelColor      )& 0xFF)+ '); width:20%;' + 
					'height:'+ style.getPropertyValue("height")+ '"></div>';
			}
			if(secondaryUsedPelColorElem.innerHTML!= secondaryUsedPelColor)
				secondaryUsedPelColorElem.innerHTML= secondaryUsedPelColor;
			// "background_pixel_color_rgb24b"
			var backgroundPelColorElem= 
				document.getElementById(backgroundPelColorId);
			var backgroundPelColor= dvb_subt_service_json.settings.
				background_pixel_color_rgb24b;
			if(backgroundPelColor< 0) {
				backgroundPelColor= 'input default';
			} else {
				var style= window.getComputedStyle(backgroundPelColorElem, 
						null);
				backgroundPelColor= '<div style="background-color:rgb('+ 
				/*R*/((backgroundPelColor>>> 16)& 0xFF)+ ','+ 
				/*G*/((backgroundPelColor>>>  8)& 0xFF)+ ','+ 
				/*B*/((backgroundPelColor      )& 0xFF)+ '); width:20%;' + 
					'height:'+ style.getPropertyValue("height")+ '"></div>';
			}
			if(backgroundPelColorElem.innerHTML!= backgroundPelColor)
				backgroundPelColorElem.innerHTML= backgroundPelColor;

			// - Display definition segment
			dvb_subt_createOrUpdateDDSTableBody(ddsTableElemId, dds, false);

			// - Page composition segment
			createOrUpdatePCSTableBody(pcsTableElemId, pcs)

			// - Region composition segments tables
			var rcsDivElem= document.getElementById(rcsDivId);
			while(rcsDivElem.firstChild)
				rcsDivElem.removeChild(rcsDivElem.firstChild);
			appendRegionCompositionSegmentsTables(rcSegments, rcsDivElem);

			// Preview
			previewCanvas();
		}

		function createDisplaySetData(nodeId, nodeDiv, 
				dvbSubtPreviewPlaceholderDivId, ddsTableElemId, pcsTableElemId, 
				rcsDivId)
		{
			// ************************************************************** //
			// Create preview node placeholder division
			// ************************************************************** //
			var div= document.createElement("div");
			div.id= dvbSubtPreviewPlaceholderDivId;
			nodeDiv.appendChild(div);

			// ************************************************************** //
			// Create display definition segment (DDS) table
			// ************************************************************** //
			var table= document.createElement("table");
			table.classList.add("settings");
			table.classList.add("dvb-subt-proc-display-set");
			table.id= ddsTableElemId;
			// - Table caption
			var caption= document.createElement("caption");
			caption.classList.add("settings");
			caption.classList.add("dvb-subt-proc-display-set");
			caption.classList.add("es-h2");
			caption.appendChild(document.createTextNode(
					"Display definition segment"));
			table.appendChild(caption);
			// - Row: "No display definition segment"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("settings");
			td0.classList.add("dvb-subt-proc-display-set");
			td0.classList.add("dds-not-defined");
			td0.classList.add("es");
			td0.appendChild(document.createTextNode(
					"No display definition segment defined"));
			tr.appendChild(td0);
			table.appendChild(tr);
			// - Append table
			nodeDiv.appendChild(table);

			// ************************************************************** //
			// Create page composition segment (PCS) table
			// ************************************************************** //
			var table= document.createElement("table");
			table.classList.add("settings");
			table.classList.add("dvb-subt-proc-display-set");
			table.id= pcsTableElemId;
			// - Table caption
			var caption= document.createElement("caption");
			caption.classList.add("settings");
			caption.classList.add("dvb-subt-proc-display-set");
			caption.classList.add("es-h2");
			caption.appendChild(document.createTextNode(
					"Page composition segment"));
			table.appendChild(caption);
			// - Row: "No page composition segment"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("settings");
			td0.classList.add("dvb-subt-proc-display-set");
			td0.classList.add("dds-not-defined");
			td0.classList.add("es");
			td0.appendChild(document.createTextNode(
					"No page composition segment defined"));
			tr.appendChild(td0);
			table.appendChild(tr);
			// - Append table
			nodeDiv.appendChild(table);

			// ************************************************************** //
			// Create region composition segments tables
			// ************************************************************** //
			// Append region composition segments placeholder
			var rcsDiv= document.createElement("div");
			rcsDiv.id= rcsDivId;
			nodeDiv.appendChild(rcsDiv);
			// Append region compositopn segments tables if aplicable
			appendRegionCompositionSegmentsTables(rcSegments, rcsDiv);
		}

		function previewCanvas()
		{
			// Check if preview node exists
			var dvbSubtPreviewDiv= document.getElementById(dvbSubtPreviewDivId);

			// Draw preview only if we have page and regions to display.
			// I it was already drawn, delete if applicable.
			if(!pcs || !rcSegments.length) {
				if(dvbSubtPreviewDiv) {
					var placeHolder= document.getElementById(
							dvbSubtPreviewPlaceholderDivId);
					while(placeHolder.firstChild)
						placeHolder.removeChild(placeHolder.firstChild);
				}
				return;
			}

			if(!dvbSubtPreviewDiv) {
				// ********************************************************** //
				// Create preview node
				// ********************************************************** //
				dvbSubtPreviewDiv= nodes_simpleDropDownCreate(
						dvbSubtPreviewDivId, dvbSubtPreviewPlaceholderDivId, 
						esProcDetailsClass, "Display set preview", false);

				// Create preview table
				var table= document.createElement("table");
				table.classList.add("settings");
				table.classList.add("dvb-subt-proc-filters");
				// - Table caption
				var caption= document.createElement("caption");
				caption.classList.add("settings");
				caption.classList.add("dvb-subt-proc-filters");
				caption.classList.add("es-h2");
				table.appendChild(caption);
				// - Append table
				dvbSubtPreviewDiv.appendChild(table);
			}

			// Get CANVAS
			var canvasSide= 560; // We set 'width= height' for canvas
			var prevCanvas= document.getElementById(nodeId+ '_preview');
			if(!prevCanvas) {
				prevCanvas= document.createElement('canvas');
				prevCanvas.id= nodeId+ '_preview';
				prevCanvas.width= canvasSide;
				prevCanvas.height= canvasSide;
				dvbSubtPreviewDiv.appendChild(prevCanvas);
			}

			// Display composition (use DDS data)
			var displayWidth= dds? dds.display_width: 720- 1;
			var displayHeigth= dds? dds.display_height: 576- 1;
			var prevWidth= (displayWidth> displayHeigth)? canvasSide: 
					(displayWidth/displayHeigth)*canvasSide;
			var prevHeigth= (displayHeigth> displayWidth)? canvasSide: 
					(displayHeigth/displayWidth)*canvasSide;
			var factorW= prevWidth/displayWidth;
			var factorH= prevHeigth/displayHeigth;
			//console.log(factorW); //comment-me
			var ctx= prevCanvas.getContext("2d");

			// Clean and clip canvas
			ctx.beginPath();
			ctx.clearRect(0, 0, prevCanvas.width, prevCanvas.height);
			ctx.lineWidth= "1";
			ctx.strokeStyle= "black";
			ctx.rect(0, 0, prevCanvas.width, prevCanvas.height);
			ctx.clip();
			ctx.stroke();

			// Draw full display
			ctx.lineWidth= "1";
			ctx.strokeStyle= "black";
			ctx.setLineDash([6]);
			ctx.rect((canvasSide- prevWidth)/2, (canvasSide- prevHeigth)/2, 
					prevWidth, prevHeigth);
			ctx.stroke();
			ctx.setLineDash([]);

			// Draw display
			var w= prevWidth;
			var h= prevHeigth;
			var x= (canvasSide- prevWidth)/2;
			var y= (canvasSide- prevHeigth)/2;
			var display_window_flag= (dds!= null && 
					dds.display_window_flag== true)? true: false;
			if(display_window_flag== true) {
				var hPosMin= dds.display_window_horizontal_position_minimum;
				var hPosMax= dds.display_window_horizontal_position_maximum;
				var vPosMin= dds.display_window_vertical_position_minimum;
				var vPosMax= dds.display_window_vertical_position_maximum;
				w= (hPosMax- hPosMin)* factorW;
				h= (vPosMax- vPosMin)* factorH;
				x+= (hPosMin* factorW);
				y+= (vPosMin* factorH);
			}
			ctx.beginPath();
			ctx.lineWidth= "1";
			ctx.strokeStyle= "blue";
			ctx.rect(x, y, w, h);
			ctx.fontSize= "14px Arial";
			var txt= "(x= ";
			txt+= display_window_flag? hPosMin: 0;
			txt+= ", y= ";
			txt+= display_window_flag? vPosMin: 0;
			txt+= ", w= ";
			txt+= display_window_flag? (hPosMax- hPosMin+ 1): displayWidth+ 1;
			txt+= ", h= ";
			txt+= display_window_flag? (vPosMax- vPosMin+ 1): displayHeigth+ 1;
			txt+= ")";
			ctx.fillStyle="blue";
			ctx.fillText(txt, x, y+ 10);
			ctx.stroke();

			// Draw regions
			for(var i= 0; pcs && (i< pcs.page_regions.length); i++) {
				var pcsRegion= pcs.page_regions[i];
				var id= pcsRegion.region_id;
				var hAddr= pcsRegion.region_horizontal_address;
				var vAddr= pcsRegion.region_vertical_address;
				// Get region data
				for(var j= 0; j< rcSegments.length; j++) {
					var rcSegment= rcSegments[i];
					var regionId= rcSegment.region_id;
					if(regionId== id) {
						var w2= rcSegment.region_width* factorW;
						var h2= rcSegment.region_height* factorH;
						var x2= hAddr* factorW;
						var y2= vAddr* factorH;
						ctx.beginPath();
						ctx.lineWidth= "1";
						ctx.strokeStyle="green";
						ctx.rect(x+ x2, y+ y2, w2, h2);
						ctx.fontSize= "14px Arial";
						var txt= "("+ "x="+ hAddr+ ", y="+ vAddr
								+ ", w:"+ rcSegment.region_width
								+ ", h:"+ rcSegment.region_height+ ")";
						ctx.fillStyle="green";
						ctx.fillText(txt, x+ x2, y+ y2+ 10);
						ctx.stroke();
					}
				}
			}
		}

		function createOrUpdatePCSTableBody(pcsTableElemId, pcsJSON)
		{
			var pcsTimeOutElemId= pcsTableElemId+ "_timeout";
			var pcsRegionsElemId= pcsTableElemId+ '_page_regions';

			var pcsTableElem= document.getElementById(pcsTableElemId);
			var expectedRowsLength= (pcsJSON== null)? 1: 2;

			if(pcsJSON!= null) {
				var pcsTimeOut= pcsJSON.page_time_out;
				var pcsPageVersionNum= pcsJSON.page_version_number;
				var pcsRegions= pcsJSON.page_regions;
			}

			if(expectedRowsLength!= pcsTableElem.rows.length) {

				// Delete table
				while(pcsTableElem.rows.length> 0)
					pcsTableElem.deleteRow(-1);

				if(expectedRowsLength== 1) {
					var tr= document.createElement("tr");
					var td0= document.createElement("td");
					td0.classList.add("settings");
					td0.classList.add("dvb-subt-proc-display-set");
					td0.classList.add("dds-not-defined");
					td0.classList.add("es");
					td0.appendChild(document.createTextNode(
							"No page composition segment defined"));
					tr.appendChild(td0);
					pcsTableElem.appendChild(tr);
				} else {
					// - Row: "page_time_out"
					dvb_subt_createCSTableRow(pcsTableElem, 
							"Page time-out [secs]", 
							document.createTextNode(pcsTimeOut), 
							pcsTimeOutElemId);
					// Row: "page regions table"
					var regionsTable= createPageCompositionRegionsTable(
							pcsRegions);
					dvb_subt_createCSTableRow(pcsTableElem, "Page regions", 
							regionsTable, pcsRegionsElemId);
				}
			} else {
				// **** Update table if applicable ****

				if(pcsJSON== null)
					return;

		    	var pcsTimeOutElem= document.getElementById(pcsTimeOutElemId);
				if(pcsTimeOutElem.innerHTML!= pcsTimeOut)
					pcsTimeOutElem.innerHTML= pcsTimeOut;

				var regionsTable= createPageCompositionRegionsTable(pcsRegions);
				var pcsRegionsElem= document.getElementById(pcsRegionsElemId);
				while(pcsRegionsElem.firstChild)
					pcsRegionsElem.removeChild(pcsRegionsElem.firstChild);
				pcsRegionsElem.appendChild(regionsTable);
			}

			function createPageCompositionRegionsTable(pcsRegionsJSON)
			{
				if(pcsRegionsJSON.length<= 0)
					return document.createTextNode("none");

				var regions_table= document.createElement("table");
				regions_table.classList.add("settings");
				regions_table.classList.add("dvb-subt-proc-display-set");
				regions_table.classList.add("display-set-subtable");
				var region_tr= document.createElement("tr");
				var region_th0= document.createElement("th");
				region_th0.classList.add("settings");
				region_th0.classList.add("dvb-subt-proc-display-set");
				region_th0.classList.add("display-set-subtable");
				region_th0.appendChild(document.createTextNode("Region Id."));
				var region_th1= document.createElement("th");
				region_th1.classList.add("settings");
				region_th1.classList.add("dvb-subt-proc-display-set");
				region_th1.classList.add("display-set-subtable");
				region_th1.appendChild(document.createTextNode(
						"Horiz. addr."));
				var region_th2= document.createElement("th");
				region_th2.classList.add("settings");
				region_th2.classList.add("dvb-subt-proc-display-set");
				region_th2.classList.add("display-set-subtable");
				region_th2.appendChild(document.createTextNode(
						"Vert. addr."));
				var region_th3= document.createElement("th");
				region_th3.classList.add("settings");
				region_th3.classList.add("dvb-subt-proc-display-set");
				region_th3.classList.add("display-set-subtable");
				region_th3.appendChild(document.createTextNode("Visible"));
				region_tr.appendChild(region_th0);
				region_tr.appendChild(region_th1);
				region_tr.appendChild(region_th2);
				region_tr.appendChild(region_th3);
				regions_table.appendChild(region_tr);

				for(var i= 0; i< pcsRegionsJSON.length; i++) {
					var pcsRegion= pcsRegionsJSON[i];
					var id= pcsRegion.region_id;
					var h_addr= pcsRegion.region_horizontal_address;
					var v_addr= pcsRegion.region_vertical_address;
					var isActive= pcsRegion.flag_is_visible? true: false;
					var region_tr= regions_table.insertRow(-1);
					var region_td0= document.createElement("td");
					region_td0.classList.add("settings");
					region_td0.classList.add("dvb-subt-proc-display-set");
					region_td0.classList.add("display-set-subtable");
					region_td0.appendChild(document.createTextNode(id));
					var region_td1= document.createElement("td");
					region_td1.classList.add("settings");
					region_td1.classList.add("dvb-subt-proc-display-set");
					region_td1.classList.add("display-set-subtable");
					region_td1.appendChild(document.createTextNode(h_addr));
					var region_td2= document.createElement("td");
					region_td2.classList.add("settings");
					region_td2.classList.add("dvb-subt-proc-display-set");
					region_td2.classList.add("display-set-subtable");
					region_td2.appendChild(document.createTextNode(v_addr));
					var region_td3= document.createElement("td");
					region_td3.classList.add("settings");
					region_td3.classList.add("dvb-subt-proc-display-set");
					region_td3.classList.add("display-set-subtable");
					region_td3.appendChild(document.createTextNode(isActive));
					region_tr.appendChild(region_td0);
					region_tr.appendChild(region_td1);
					region_tr.appendChild(region_td2);
					region_tr.appendChild(region_td3);
				}
				return regions_table;
			}
		}

		function appendRegionCompositionSegmentsTables(rcSegmentsJSON, nodeDiv)
		{
			for(var i= 0; i< rcSegmentsJSON.length; i++) {
				var rcSegment= rcSegmentsJSON[i];
				var regionId= rcSegment.region_id;

				var table= document.createElement("table");
				table.classList.add("settings");
				table.classList.add("dvb-subt-proc-display-set");
				// - Table caption
				var caption= document.createElement("caption");
				caption.classList.add("settings");
				caption.classList.add("dvb-subt-proc-display-set");
				caption.classList.add("es-h2");
				caption.appendChild(document.createTextNode(
						"Region composition segment #"+ regionId));
				table.appendChild(caption);
				// - Row: "region_width"
				dvb_subt_createCSTableRow(table, "Region width", 
						document.createTextNode(rcSegment.region_width), 
						rcsSufixId+ '_'+ regionId+ '_region_width');
				// - Row: "region_height"
				dvb_subt_createCSTableRow(table, "Region height", 
						document.createTextNode(rcSegment.region_height),  
						rcsSufixId+ '_'+ regionId+ '_region_height');
				// - Row: "objects"
				var objects_table= createRegionCompositionObjectsTable(
						rcSegment.region_ojects);
				dvb_subt_createCSTableRow(table, 
						"Region objects", 
						objects_table,
						rcsSufixId+ '_'+ regionId+ '_objects');
				// - Append table
				nodeDiv.appendChild(table);
			}

			function createRegionCompositionObjectsTable(rcsObjectsJSON)
			{
				if(rcsObjectsJSON.length<= 0)
					return document.createTextNode("none");

				var objects_table= document.createElement("table");
				objects_table.classList.add("settings");
				objects_table.classList.add("dvb-subt-proc-display-set");
				objects_table.classList.add("display-set-subtable");
				var object_tr= document.createElement("tr");
				var object_th0= document.createElement("th");
				object_th0.classList.add("settings");
				object_th0.classList.add("dvb-subt-proc-display-set");
				object_th0.classList.add("display-set-subtable");
				object_th0.appendChild(document.createTextNode("Object Id."));
				var object_th1= document.createElement("th");
				object_th1.classList.add("settings");
				object_th1.classList.add("dvb-subt-proc-display-set");
				object_th1.classList.add("display-set-subtable");
				object_th1.appendChild(document.createTextNode("Type"));
				var object_th2= document.createElement("th");
				object_th2.classList.add("settings");
				object_th2.classList.add("dvb-subt-proc-display-set");
				object_th2.classList.add("display-set-subtable");
				object_th2.appendChild(document.createTextNode(
						"Horiz. pos."));
				var object_th3= document.createElement("th");
				object_th3.classList.add("settings");
				object_th3.classList.add("dvb-subt-proc-display-set");
				object_th3.classList.add("display-set-subtable");
				object_th3.appendChild(document.createTextNode(
						"Vert. pos."));
				object_tr.appendChild(object_th0);
				object_tr.appendChild(object_th1);
				object_tr.appendChild(object_th2);
				object_tr.appendChild(object_th3);
				objects_table.appendChild(object_tr);

				for(var i= 0; i< rcsObjectsJSON.length; i++) {
					var rcsObject= rcsObjectsJSON[i];
					var id= rcsObject.object_id;
					var type= rcsObject.object_type;
					var h_addr= rcsObject.object_horizontal_position;
					var v_addr= rcsObject.object_vertical_position;
					var object_tr= objects_table.insertRow(-1);
					var object_td0= document.createElement("td");
					object_td0.classList.add("settings");
					object_td0.classList.add("dvb-subt-proc-display-set");
					object_td0.classList.add("display-set-subtable");
					object_td0.appendChild(document.createTextNode(id));
					var object_td1= document.createElement("td");
					object_td1.classList.add("settings");
					object_td1.classList.add("dvb-subt-proc-display-set");
					object_td1.classList.add("display-set-subtable");
					object_td1.appendChild(document.createTextNode(type));
					var object_td2= document.createElement("td");
					object_td2.classList.add("settings");
					object_td2.classList.add("dvb-subt-proc-display-set");
					object_td2.classList.add("display-set-subtable");
					object_td2.appendChild(document.createTextNode(h_addr));
					var object_td3= document.createElement("td");
					object_td3.classList.add("settings");
					object_td3.classList.add("dvb-subt-proc-display-set");
					object_td3.classList.add("display-set-subtable");
					object_td3.appendChild(document.createTextNode(v_addr));
					object_tr.appendChild(object_td0);
					object_tr.appendChild(object_td1);
					object_tr.appendChild(object_td2);
					object_tr.appendChild(object_td3);
				}
				return objects_table;
			}
		}
	}
}

function dvb_subt_createOrUpdateDDSTableBody(ddsTableElemId, ddsJSON, isSetting)
{
	var ddsVersionElemId= ddsTableElemId+ "_version_number";
	var ddsWidthElemId= ddsTableElemId+ "_width";
	var ddsHeigthElemId= ddsTableElemId+ "_heigth";
	var ddsDWFlagElemId= ddsTableElemId+ "_dwFlag";
	var ddsDWHPosMinElemId= ddsTableElemId+ "_dwHPosMin";
	var ddsDWHPosMaxElemId= ddsTableElemId+ "_dwHPosMax";
	var ddsDWVPosMinElemId= ddsTableElemId+ "_dwVPosMin";
	var ddsDWVPosMaxElemId= ddsTableElemId+ "_dwVPosMax";

	var ddsTableElem= document.getElementById(ddsTableElemId);

	if(ddsJSON!= null) {
		var ddsVersion= ddsJSON.dds_version_number;
		var ddsWidth= ddsJSON.display_width;
		var ddsHeigth= ddsJSON.display_height;
		var ddsDWFlag= ddsJSON.display_window_flag;
		var ddsDWHPosMin= ddsJSON.display_window_horizontal_position_minimum;
		var ddsDWHPosMax= ddsJSON.display_window_horizontal_position_maximum;
		var ddsDWVPosMin= ddsJSON.display_window_vertical_position_minimum;
		var ddsDWVPosMax= ddsJSON.display_window_vertical_position_maximum;		
	}

	var basicDDSLen= (isSetting== true)? 3: 4;
	var expectedRowsLength= (ddsJSON== null)? 1:
			(ddsDWFlag== false)? basicDDSLen: basicDDSLen+ 4;

	if(expectedRowsLength!= ddsTableElem.rows.length) {

		// Delete table
		while(ddsTableElem.rows.length> 0)
			ddsTableElem.deleteRow(-1);

		if(expectedRowsLength== 1) {

			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("settings");
			td0.classList.add("dvb-subt-proc-display-set");
			td0.classList.add("dds-not-defined");
			td0.classList.add("es");
			td0.appendChild(document.createTextNode(
					"No display definition segment defined"));
			tr.appendChild(td0);
			ddsTableElem.appendChild(tr);

		} else {

			if(isSetting== false) {
				// - Row: Version number"
				dvb_subt_createCSTableRow(ddsTableElem, "Version number", 
						document.createTextNode(ddsVersion), 
						ddsVersionElemId);
			}
			// - Row: "Display width"
			dvb_subt_createCSTableRow(ddsTableElem, "Display width", 
					document.createTextNode(ddsWidth), 
					ddsWidthElemId);
			// - Row: "Display heigth"
			dvb_subt_createCSTableRow(ddsTableElem, "Display heigth", 
					document.createTextNode(ddsHeigth), 
					ddsHeigthElemId);
			// - Row: "Display window flag"
			dvb_subt_createCSTableRow(ddsTableElem, "Display window flag", 
					document.createTextNode(ddsDWFlag), 
					ddsDWFlagElemId);

			if(ddsDWFlag== true) {
				// - Row: "Display window horizontal position minimum"
				dvb_subt_createCSTableRow(ddsTableElem, 
						"Display window horizontal position minimum", 
						document.createTextNode(ddsDWHPosMin), 
						ddsDWHPosMinElemId);
				// - Row: "Display window horizontal position maximum"
				dvb_subt_createCSTableRow(ddsTableElem, 
						"Display window horizontal position maximum", 
						document.createTextNode(ddsDWHPosMax), 
						ddsDWHPosMaxElemId);
				// - Row: "Display window vertical position minimum"
				dvb_subt_createCSTableRow(ddsTableElem, 
						"Display window vertical position minimum", 
						document.createTextNode(ddsDWVPosMin), 
						ddsDWVPosMinElemId);
				// - Row: "Display window vertical position maximum"
				dvb_subt_createCSTableRow(ddsTableElem, 
						"Display window vertical position maximum", 
						document.createTextNode(ddsDWVPosMax), 
						ddsDWVPosMaxElemId);
			}
		}
	} else {
		// **** Update table if applicable ****

		if(ddsJSON== null)
			return;

		if(isSetting== false) {
			// - Display definition segment "version number"
	    	var ddsVersionElem= document.getElementById(
	    			ddsVersionElemId);
			if(ddsVersionElem.innerHTML!= ddsVersion)
				ddsVersionElem.innerHTML= ddsVersion;			
		}

		// - Display definition segment "width"
    	var ddsWidthElem= document.getElementById(ddsWidthElemId);
		if(ddsWidthElem.innerHTML!= ddsWidth)
			ddsWidthElem.innerHTML= ddsWidth;
		// - Display definition segment "heigth"
    	var ddsHeigthElem= document.getElementById(ddsHeigthElemId);
		if(ddsHeigthElem.innerHTML!= ddsHeigth)
			ddsHeigthElem.innerHTML= ddsHeigth;
		// - Display definition segment "dis. win. flag"
    	var ddsDWFlagElem= document.getElementById(ddsDWFlagElemId);
		if(ddsDWFlagElem.innerHTML!= ddsDWFlag)
			ddsDWFlagElem.innerHTML= ddsDWFlag;

		if(ddsDWFlag== true) {
			// - Display definition segment "dis. win. horiz. pos. min."
	    	var ddsDWHPosMinElem= document.getElementById(
	    			ddsDWHPosMinElemId);
			if(ddsDWHPosMinElem.innerHTML!= ddsDWHPosMin)
				ddsDWHPosMinElem.innerHTML= ddsDWHPosMin;
			// - Display definition segment "dis. win. horiz. pos. max."
	    	var ddsDWHPosMaxElem= document.getElementById(
	    			ddsDWHPosMaxElemId);
			if(ddsDWHPosMaxElem.innerHTML!= ddsDWHPosMax)
				ddsDWHPosMaxElem.innerHTML= ddsDWHPosMax;
			// - Display definition segment "dis. win. vert. pos. min."
	    	var ddsDWVPosMinElem= document.getElementById(
	    			ddsDWVPosMinElemId);
			if(ddsDWVPosMinElem.innerHTML!= ddsDWVPosMin)
				ddsDWVPosMinElem.innerHTML= ddsDWVPosMin;
			// - Display definition segment "dis. win. vert. pos. max."
	    	var ddsDWVPosMaxElem= document.getElementById(
	    			ddsDWVPosMaxElemId);
			if(ddsDWVPosMaxElem.innerHTML!= ddsDWVPosMax)
				ddsDWVPosMaxElem.innerHTML= ddsDWVPosMax;
		}
	}
}

// For creating composition segment table rows.
function dvb_subt_createCSTableRow(table, td0Txt, td1Elem, td1Id) {
	var tr= document.createElement("tr");
	var td0= document.createElement("td");
	td0.classList.add("settings");
	td0.classList.add("dvb-subt-proc-display-set");
	td0.classList.add("es");
	td0.appendChild(document.createTextNode(td0Txt));
	var td1= document.createElement("td");
	td1.classList.add("settings");
	td1.classList.add("dvb-subt-proc-display-set");
	td1.classList.add("unselect");
	td1.appendChild(td1Elem);
	td1.id= td1Id;
	tr.appendChild(td0);
	tr.appendChild(td1);
	table.appendChild(tr);
}

function dvb_subt_service_erase_node(nodeId)
{
	//console.log('dvb_subt_service_erase_node('+ nodeId+ ')');
	nodes_erase(nodeId);
}
