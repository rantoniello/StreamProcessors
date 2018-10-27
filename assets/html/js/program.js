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
 * @file program.js
 */

var programLinkClass= 'program-dropdown';

function program_manager(url)
{
	// GET node REST JSON
	httpRequests_respJSON(url, 'GET', '', null, {onResponse: drawNode});

	function drawNode(program_json)
	{
		var nodeUrlSchemeTag= '/programs/';
		var nodeId;
		var nodeDiv;
		var parentNodeId;

		// Compose HTML id for this node
		nodeId= utils_url2id(url);

		// Compose HTML id for the parent node
		// Note that we are attaching program nodes within the DEMUXER node.
		// Parent node format: '-demuxers-ID1'
		var demuxerId= utils_findIDinUrl(url, '/demuxers/');
		parentNodeId= '-demuxers-'+ demuxerId;

		// Compose local elements Ids.
		var progProcCheckBoxId= nodeId+ '_settings_enableProgProc';
		var progNumId= nodeId+ "program_number";
		var servNameId= nodeId+ "service_name";
		var pmsPIDId= nodeId+ "program_map_section_PID";
		var pcrPIDId= nodeId+ "pcr_pid";
		var confirmEnablingProgProcModalDivId= progProcCheckBoxId+ 
				"_confirm_modal";
		var confirmPurgeESModalDivId= nodeId+ "_confirm_purge_es_modal";

		// Some locals
		var progId= program_json.program_map_section_PID;
		var serviceName= program_json.service_name;
		var programNum= program_json.program_number;
		var pcrPID= program_json.pcr_pid;

		// Create or update node division 
		nodeDiv= document.getElementById(nodeId);
		if(!nodeDiv) {
			// Create program node
			nodeDiv= createNode();
		} else {
			// Update program node
			updateNode();
		}

		// Draw program processor node
		drawProgProcNodeLink();

		// Draw elementary stream nodes
		drawElementaryStreamNodes();

		function createNode()
		{
			// Create node link and division
			var nodeTxt= "program "+ programNum;
			nodeTxt+= serviceName? ": "+ serviceName: 
				" (no service name available)";
			nodeDiv= nodes_create(nodeId, parentNodeId, programLinkClass, 
					nodeUrlSchemeTag, nodeTxt);
			nodeDiv.classList.add("program");

			// Create "Enable/Disable" program processor check-box
			var confirmEnablingProgProcModalDiv= document.createElement("div");
			confirmEnablingProgProcModalDiv.id= 
					confirmEnablingProgProcModalDivId;
			nodeDiv.appendChild(confirmEnablingProgProcModalDiv);
			var checkbox= document.createElement("input");
			checkbox.setAttribute("type", "checkbox");
			checkbox.className= "normal right";
			checkbox.id= progProcCheckBoxId;
			checkbox.checked= false;
			// Note that program processor URL is of the form: 
			// '/demuxers/ID1/program_processors/ID2.json'
			checkbox.url= '/demuxers/'+ demuxerId+ '/program_processors/'+ 
					progId+ '.json';
			checkbox.addEventListener('click', function(event){
				// Get request method.
				// Note that the target event property returns the element that 
				// triggered the event.
				var requestMethod= event.target.checked? 'POST': 'DELETE';
				//console.log('requestMethod: '+ requestMethod); // comment-me
				if(requestMethod== 'DELETE') {
					// Confirm operation
					var confirmationEventObj= {};
					var cancelEventObj= {};
					var modalTxt= "You are about to disable program " +
					"processing; all current processing configurations will " +
					"be deleted.";
					modals_confirmation_modal(
							confirmEnablingProgProcModalDivId, ["alert"], 
							modalTxt, 
							function(confirmationEventObj) {
								httpRequests_respJSON(event.target.url, 
										requestMethod, '', null, null);
							}, confirmationEventObj,
							function(cacelEventOb){
								event.target.checked= true;
							}, cancelEventObj);
				} else {
					// Method is 'POST'
					httpRequests_respJSON(event.target.url, requestMethod, '', 
							null, null);
				}
			});
			var label= document.createElement("label");
			label.htmlFor= checkbox.id;
			label.appendChild(document.createTextNode("Enable/disable " +
					"program processing"));
			label.className= "normal right";
			nodeDiv.appendChild(checkbox);
			nodeDiv.appendChild(label);

			// Create program information table
			var table= document.createElement("table");
			table.classList.add("program");
			// - Row: "Program number"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("program");
			td0.appendChild(document.createTextNode("Program number: "));
			var td1= document.createElement("td");
			td1.classList.add("program");
			td1.classList.add("unselect");
			td1.appendChild(document.createTextNode(programNum));
			td1.id= progNumId;
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// - Row: "Service name"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("program");
			td0.appendChild(document.createTextNode("Service name: "));
			var td1= document.createElement("td");
			td1.classList.add("program");
			td1.classList.add("unselect");
			td1.appendChild(document.createTextNode(serviceName));
			td1.id= servNameId;
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// - Row: "Program Map Section PID"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("program");
			td0.appendChild(document.createTextNode(
					"Program Map Section PID: "));
			var td1= document.createElement("td");
			td1.classList.add("program");
			td1.classList.add("unselect");
			td1.appendChild(document.createTextNode(progId));
			td1.id= pmsPIDId;
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// - Row: "Program PCR PID"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("program");
			td0.appendChild(document.createTextNode("Program PCR PID: "));
			var td1= document.createElement("td");
			td1.classList.add("program");
			td1.classList.add("unselect");
			td1.appendChild(document.createTextNode(pcrPID));
			td1.id= pcrPIDId;
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// - Append table
			nodeDiv.appendChild(table);

			// Create unliked list (UL) for the program processor
			ul= document.createElement("ul");
			ul.className= programProcLinkClass;
			ul.classList.add("program-h2");
			nodeDiv.appendChild(ul);

			// Create elementary streams table
			var table= document.createElement("table");
			// - Row: "Elementary Streams"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.appendChild(document.createElement("br"));
			td0.appendChild(document.createTextNode("Elementary streams: "));
			td0.classList.add("es-h1");
			tr.appendChild(td0);
			table.appendChild(tr);
			// - Append table
			nodeDiv.appendChild(table);

			// Create unliked list (UL) of elementary streams
			ul= document.createElement("ul");
			ul.className= elementaryStreamLinkClass;
			p= document.createElement("p");
			p.appendChild(document.createTextNode("- Not Available -"));
			p.classList.add("unselect");
			p.classList.add("naTxtNodeClass");
			ul.appendChild(p);
			nodeDiv.appendChild(ul);

			// Button to purge disassociated programs
			var confirmPurgeESModalDiv= document.createElement("div");
			confirmPurgeESModalDiv.id= confirmPurgeESModalDivId;
			nodeDiv.appendChild(confirmPurgeESModalDiv);
			var purge_url= "/demuxers/"+ demuxerId+ "/program_processors/"+ 
				progId+ ".json";
			var submitPurgeDisassociated= document.createElement("input");
			submitPurgeDisassociated.classList.add("program");
			submitPurgeDisassociated.classList.add("button");
			submitPurgeDisassociated.classList.add("traces");
			submitPurgeDisassociated.type= "button";
			submitPurgeDisassociated.value= "Purge disassociated ES";
			submitPurgeDisassociated.url= purge_url;
			submitPurgeDisassociated.addEventListener('click', function(event){
				//console.log('PUT'+ '-> /api/1.0'+ event.target.url+ 
				//	'?flag_purge_disassociated_processors=true'); // comment-me

				// Confirm operation
				var confirmationEventObj= {};
				var cancelEventObj= {};
				var modalTxt= "You are about to purge the ES-processors " +
						"list; all the disassociated ES-processors " +
						"configurations will be deleted.";
				modals_confirmation_modal(
						confirmPurgeESModalDivId, ["alert"], 
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

		function updateNode()
		{
			// Only update DIV if current node is not hidden
			if(nodeDiv.classList.contains('hide'))
				return;

			// Update "program processing indicator"
			var progProcCheckbox= 
				document.getElementById(progProcCheckBoxId);
			var progLink= document.getElementById(nodeId+ "_link");
			var isProgProcSet= (progLink.classList.contains('processing') || 
					progLink.classList.contains('disassociated'))? 
					true: false;
			if(isProgProcSet!= progProcCheckbox.checked) {
				progProcCheckbox.checked= isProgProcSet;
			}

			// Update "Program number"
			var programNumberElem= document.getElementById(progNumId);
			if(programNumberElem.innerHTML!= programNum)
				programNumberElem.innerHTML= programNum;
			// Update "service name"
			var serviceNameElem= document.getElementById(servNameId);
			if(serviceNameElem.innerHTML!= serviceName)
				serviceNameElem.innerHTML= serviceName;
			// Update "program map section (PMS) PID"
			var pmsPIDElem= document.getElementById(pmsPIDId);
			if(pmsPIDElem.innerHTML!= progId)
				pmsPIDElem.innerHTML= progId;
			// Update "program clock reference (PCR) PID"
			var pcrPIDElem= document.getElementById(pcrPIDId);
			if(pcrPIDElem.innerHTML!= pcrPID)
				pcrPIDElem.innerHTML= pcrPID;
		}

		function drawProgProcNodeLink()
		{
			nodes_update(nodeId, demuxerCurrProgProcListJSON, 
					programProcLinkClass, program_proc_erase_node, 
					program_proc_manager, null);
		}

		function drawElementaryStreamNodes()
		{
			// Get list of elementary stream (ES) nodes in JSON response
			var currListJSON= program_json.elementary_streams;

			nodes_update(nodeId, currListJSON, elementaryStreamLinkClass, 
					elementary_stream_erase_node, elementary_stream_manager, 
					update_link);

			function update_link(linkNode, currListJSON_nth)
			{
				// **** Update text in link ****
				var es_PID= currListJSON_nth.es_PID;
				var streamType= currListJSON_nth.stream_type;
				var linkTxt= "PID "+ es_PID+ "(0x"+ es_PID.toString(16)+ ") ";
				linkTxt+= streamType? "; stream type: "+ streamType: "N/A";
		    	//console.log("linkTxt: "+ linkTxt); // comment-me
		    	if(linkNode.innerHTML!= linkTxt)
		    		linkNode.innerHTML= linkTxt;

		    	// **** Update indications in link ****
				// Add or remove CSS class to highlight as "being processed."
				if(currListJSON_nth.hasBeenDisassociated)
					linkNode.classList.add('disassociated_es');
				else
					linkNode.classList.remove('disassociated_es');
			}
		}
	}
}

function program_erase_node(nodeId)
{
	nodes_erase(nodeId);
}
