/*
 * Copyright (c) 2015 Rafael Antoniello
 *
 * This file is part of StreamProcessor.
 *
 * StreamProcessor is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * StreamProcessor is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with StreamProcessor.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file stream_procs.js
 */

function stream_procs_manager(url)
{
	var nodeId= utils_url2id(url);
	var nodeDiv= document.getElementById(nodeId);

	// Demuxer creation button
	var table= document.createElement("table");
	var tr= document.createElement("tr");
	var td= document.createElement("td");
	var inputElemCreateDemultiplexer= document.createElement("input");
	inputElemCreateDemultiplexer.type= "button";
	inputElemCreateDemultiplexer.value= "Add demultiplexer";
	inputElemCreateDemultiplexer.className= "normal";
	inputElemCreateDemultiplexer.url= url;
	inputElemCreateDemultiplexer.addEventListener('click', function(event){
		streamProcs_post(event.target.url);
	});
	td.appendChild(inputElemCreateDemultiplexer);
	tr.appendChild(td);
	table.appendChild(tr);
	nodeDiv.appendChild(table);

	// Create list of DEMUXER links
	ul= document.createElement("ul");
	ul.className= streamProcLinkClass;
	nodeDiv.appendChild(ul);

	streamProcs_update();

	function streamProcs_update()
	{
		// Only update DIV if current node is not hidden
		if(!nodeDiv.classList.contains('hide')) {
			streamProcs_get(url);
		}
		setTimeout(streamProcs_update, 700);
	}

	function streamProcs_get()
	{
		// Request list of available streamProcs
		//console.log("Updating DEMUXERS main tab"); // comment-me
		httpRequests_respJSON(url, 'GET', '', null, 
				{onResponse: streamProcs_draw});

		function streamProcs_draw(streamProcs_json)
		{
			// Draw DEMUXER nodes
			draw_streamProc_nodes();

			function draw_streamProc_nodes()
			{
				// Get list of DEMUXER nodes in JSON response
				var currListJSON= streamProcs_json.stream_procs;

				nodes_update(nodeId, currListJSON, streamProcLinkClass, 
						streamProc_erase_node, streamProc_manager, update_link);

				function update_link(linkNode, currListJSON_nth) {
					var tagJSON= currListJSON_nth.tag;
					var linkTxtJSON= "#"+ window.location.host+ '-'+ currListJSON_nth.proc_id+ "<br>&nbsp;"+ tagJSON;
console.error(linkTxtJSON);
					if(linkNode.innerHTML!= linkTxtJSON)
						linkNode.innerHTML= linkTxtJSON;
				}
			}
		}
	}
}

function streamProcs_post(url)
{
	// Request creation of new DEMUXER node
	loadXMLDoc('POST', "http://"+ url+ '?proc_name=mpeg2_sp', null, 
			streamProcs_post_callback);

	function streamProcs_post_callback(response)
	{
		// Check message status
		var response_json= JSON.parse(response);
		if(response_json.code!= '200' && response_json.code!= '201') {
			// Alert user on error
			var alertStr= "Error: ";
			if(response_json.message.length> 0)
				alertStr+= response_json.message;
			else
				alertStr+= response_json.status+ ".";
			alert(alertStr);
		}
	}
}
