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
 * @file elementary_stream.js
 */

var elementaryStreamLinkClass= 'es-dropdown';

function elementary_stream_manager(url)
{
	// GET node REST JSON
	httpRequests_respJSON(url, 'GET', '', null, {onResponse: drawNode});

	function drawNode(elementary_stream_json)
	{
		var nodeUrlSchemeTag= '/elementary_streams/';
		var es_PID= elementary_stream_json.es_PID;
		var demuxerId= utils_findIDinUrl(url, 'demuxers/');
		var progId= utils_findIDinUrl(url, 'programs/');
		var nodeId;
		var nodeDiv;
		var parentNodeId;

		// Compose HTML id for this node and for parent node.
		// Note that we are attaching elementary stream nodes within the program 
		// nodes.
		// Parent node format: '-demuxers-ID1-programs-ID2'
		nodeId= utils_url2id(url);
		parentNodeId= '-demuxers-'+ demuxerId+ '-programs-'+ progId;
		//console.log("nodeId: "+ nodeId); //comment-me
		//console.log("parentNodeId: "+ parentNodeId); //comment-me

		// Create or update node division 
		nodeDiv= document.getElementById(nodeId);
		if(!nodeDiv) {
			// Create ES-processor node
			nodeDiv= createNode();
		} else {
			// Update ES-processor node
			updateNode();
		}

		// Draw elementary stream processors ("ES-processor") nodes
		drawEsProcNodes();

		function createNode()
		{
			var streamType= elementary_stream_json.stream_type;

			// Create node link and division
			var nodeTxt= "PID "+ es_PID+ "(0x"+ es_PID.toString(16)+ ") ";
			nodeTxt+= streamType? "; stream type: "+ streamType: "N/A";
			nodeDiv= nodes_create(nodeId, parentNodeId, 
					elementaryStreamLinkClass, nodeUrlSchemeTag, nodeTxt);

			// Create ES basic information table
			var table= document.createElement("table");
			table.classList.add("es");
			// -- Row: "Stream type"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("es");
			td0.appendChild(document.createTextNode("Stream type: "));
			var td1= document.createElement("td");
			td1.classList.add("es");
			td1.classList.add("unselect");
			td1.appendChild(document.createTextNode(streamType));
			td1.id= nodeId+ "stream_type";
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// -- Row: "ES PID"
			var tr= document.createElement("tr");
			var td0= document.createElement("td");
			td0.classList.add("es");
			td0.appendChild(document.createTextNode("Elementary stream PID: "));
			var td1= document.createElement("td");
			td1.classList.add("es");
			td1.classList.add("unselect");
			td1.appendChild(document.createTextNode(es_PID));
			td1.id= nodeId+ "es_PID";
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			// -- Row: "Descriptors"
			var tr= document.createElement("tr");
			tr.id= nodeId+ "es_descriptors"
			var td0= document.createElement("td");
			td0.classList.add("es");
			td0.appendChild(document.createTextNode("Descriptors: "));
			var td1= document.createElement("td");
			td1.classList.add("es");
			td1.classList.add("unselect");
			tr.appendChild(td0);
			tr.appendChild(td1);
			table.appendChild(tr);
			var es_descr_json= elementary_stream_json.descriptors;
			if(es_descr_json.length> 0) {
				// Descriptors (one per row)
				for(var descCnt= 0; descCnt< es_descr_json.length; descCnt++) {
					var tr= document.createElement("tr");
					tr.id= nodeId+ "es_descriptors-"+ descCnt;
					var td0= document.createElement("td");
					td0.classList.add("es");
					var td1= document.createElement("td");
					td1.classList.add("es");
					td1.classList.add("unselect");
					var es_desc_txt= es_descr_json[descCnt].type+ 
							" (tag: "+ es_descr_json[descCnt].tag+ ")";
					td1.appendChild(document.createTextNode(es_desc_txt));
					tr.appendChild(td0);
					tr.appendChild(td1);
					table.appendChild(tr);
				}
			} else {
				var tr= document.createElement("tr");
				tr.id= nodeId+ "es_descriptors-0";
				var td0= document.createElement("td");
				td0.classList.add("es");
				var td1= document.createElement("td");
				td1.classList.add("es");
				td1.classList.add("unselect");
				td1.appendChild(document.createTextNode("- Not available -"));
				tr.appendChild(td0);
				tr.appendChild(td1);
				table.appendChild(tr);
			}
			// - Append table
			nodeDiv.appendChild(table);

			// Create list of ES-processors links
			ul= document.createElement("ul");
			ul.classList.add(esProcLinkClass);
			ul.classList.add("es-h2");
			nodeDiv.appendChild(ul);

			return nodeDiv;
		}

		function updateNode()
		{
			var streamTypeJSON= elementary_stream_json.stream_type;

			// Only update if current node is not hidden
			if(nodeDiv.classList.contains('hide'))
				return;

			// Update "Stream type"
			var streamType= document.getElementById(nodeId+ "stream_type");
			if(streamType.innerHTML!= streamTypeJSON)
				streamType.innerHTML= streamTypeJSON;

			// Update "elementary stream (ES) PID"
			var esPIDElem= document.getElementById(nodeId+ "es_PID");
			if(esPIDElem.innerHTML!= es_PID)
				esPIDElem.innerHTML= es_PID;

			// Update "descriptors"
			// - Get descriptor "block" initial row index
			var descFirstRow= document.getElementById(nodeId+ "es_descriptors");
			var descTable= descFirstRow.parentNode;
			//- Iterate and update each descriptor row
			var es_descr_json= elementary_stream_json.descriptors;
			if(es_descr_json.length> 0) {
				// - Update each descriptor node
				for(var descCnt= 0; descCnt< es_descr_json.length; descCnt++){
					//console.log("Descriptor type: "+ 
					//		es_descr_json[descCnt].type); //comment-me
					var rowNthId= nodeId+ "es_descriptors-"+ descCnt;
					var rowNth= document.getElementById(rowNthId);
					var es_desc_txt= es_descr_json[descCnt].type+ 
						" (tag: "+ es_descr_json[descCnt].tag+ ")";
					if(!rowNth) {
						// - Create new descriptor node
						//console.log("Creating new descritor idx="+ 
						//		descCnt); // comment-me
						var tr= document.createElement("tr");
						tr.id= rowNthId;
						var td0= document.createElement("td");
						td0.className="normal";
						var td1= document.createElement("td");
						td1.appendChild(document.createTextNode(es_desc_txt));
						td1.className="unselect";
						td1.style.paddingLeft= "2em";
						tr.appendChild(td0);
						tr.appendChild(td1);
						descTable.appendChild(tr);
					} else {
						// - Update 'td1' of 'Nth-row'
						var cols1= rowNth.getElementsByTagName("td")[1];
						if(cols1.innerHTML!= es_desc_txt) {
							//console.log("updating descritor text idx="+ 
							//		descCnt); // comment-me
							cols1.innerHTML= es_desc_txt;
						}
					}
				}
				// - Delete rest of rows if applicable
				for(var i= es_descr_json.length;; i++){
					var rowNth= 
						document.getElementById(nodeId+ "es_descriptors-"+ i);
					if(rowNth) {
						rowNth.parentNode.remoceChild(rowNth);
					} else {
						break;
					}
				}
			} else {
				var rowNth= document.getElementById(nodeId+ "es_descriptors-0");
				// - Update 'td1' of first row
				var cols1= rowNth.getElementsByTagName("td")[1];
				if(cols1.innerHTML!= "- Not available -")
					cols1.innerHTML= "- Not available -";
				// - Delete rest of rows if applicable
				for(var i= 1;; i++){
					var rowNth= 
						document.getElementById(nodeId+ "es_descriptors-"+ i);
					if(rowNth) {
						rowNth.parentNode.remoceChild(rowNth);
					} else {
						break;
					}
				}
			}
		}

		function drawEsProcNodes()
		{
			var currList= [];

	    	// Check if this elementary stream is associated to a program 
			// being processed
			// (Iterate over list of program-processor nodes in JSON response)
	    	var progProcNodeUrlSchemeTag= '/program_processors/';
				for(var j= 0; j< demuxerCurrProgProcListJSON.length; j++) {
					var progProcJSON= demuxerCurrProgProcListJSON[j];
					var progProcUrl= utils_getSelfHref(progProcJSON.links);
					var progProcId= utils_findIDinUrl(progProcUrl, 
							progProcNodeUrlSchemeTag);
					if(progId== progProcId) {
						// This elementary stream is being processed.
					currListTXT= "[{" +
						"\"links\":[{" +
						"\"rel\":\"self\"," +
						"\"href\":\"/demuxers/"+ demuxerId+ 
						"/program_processors/"+ progId+ "/es_processors/"+ 
						es_PID+ ".json\"}]" +
						"}]";
					//console.log(currListTXT); //comment-me
					currList= JSON.parse(currListTXT);
					break;
				}
			}
			// Update elementary-stream processor if applicable
			function update_link(linkNode, currListJSON_nth) {}
			nodes_update(nodeId, currList, esProcLinkClass, 
					es_proc_erase_node, es_proc_manager, update_link);
		}
	}
}

function elementary_stream_erase_node(nodeId)
{
	nodes_erase(nodeId);
}
