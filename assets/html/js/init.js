/*
 * Copyright (c) 2015 Rafael Antoniello
 *
 * This file is part of StreamProcessors.
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
 * @file init.js
 */

var mainTabsLinkClass= 'main-tabs';

function init()
{
	var getUrl= window.location;
	//console.log(getUrl.href); // comment-me
	var mainJSON= // TODO: Get *FULL* interface description from server
		'[\
		    {\
			    \"name\":\"system\",\
				\"content\":\"System\",\
		        \"links\":\
		        [\
		            {\"rel\":\"self\", \"href\":\"'+ getUrl.host+ '\/api/1.0/system.json\"}\
		        ]\
		    }\,\
		    {\
			    \"name\":\"stream_procs\",\
				\"content\":\"Stream processors\",\
		        \"links\":\
		        [\
		            {\"rel\":\"self\", \"href\":\"'+ getUrl.host+ '\/api/1.0/stream_procs.json\"}\
		        ]\
		    }\
		]';
	console.log(mainJSON); // comment-me

	// Create MAIN tabs
	var ul= document.createElement('ul');
	ul.className= mainTabsLinkClass;
	document.body.appendChild(ul);

	var initJSON= JSON.parse(mainJSON);
	//console.log(JSON.stringify(initJSON)); // comment-me

	for(var i= 0; i< initJSON.length; i++) {
		// Get node id.
		var nodeJSON= initJSON[i];
		var url= utils_getSelfHref(nodeJSON.links);
		var nodeId= utils_url2id(url);
		var name= nodeJSON.name;

		// Create node link
		var li= document.createElement('li');
		var a= document.createElement('a');
		a.id= nodeId+ '_link';
		a.href= "#"+ nodeId;
		a.appendChild(document.createTextNode(nodeJSON.content));
		// Initialize first link as 'selected' and rest as 'not-selected'
		a.onclick= nodes_clickTab;
		a.onfocus= function() {this.blur();};
		if(i== 0)
			a.classList.add('selected');
		li.appendChild(a);
		ul.appendChild(li);

		// Create node division
		var nodeDiv= document.createElement("div");
		nodeDiv.className= mainTabsLinkClass+ '-content';
		nodeDiv.id= nodeId;
		// Initialize first division as 'selected' and rest as 'not-selected'
		if(i!= 0)
			nodeDiv.classList.add('hide');
		document.body.appendChild(nodeDiv);

		// Draw main tabs
		console.log(window); // comment-me
		var drawFunction= window[name.toLowerCase()+ '_manager'];
		if(drawFunction) {
			//console.log("drawFunction"+ drawFunction); // comment-me
			drawFunction(url);
		}
	}

	//alert(utils_nodeToString(
	//		document.getElementsByTagName("body")[0])); // comment-me
}
