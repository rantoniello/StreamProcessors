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
 * @file nodes.js
 */

/**
 * Create node link and division.
 * 
 * Discussion:
 * 
 * - This is HTML parent node with given id attribute 'parentNodeId' -
 * 
 * If 'linkClass' argument has the '-dropdown' key needle, HTML structure will 
 * be created with links and corresponding divisions within the same unordered 
 * list (UL) as follows:
 * 
 * <ul class="linkClass">
 * <!-- Attribute 'class="linkClass"' characterize full unordered list -->
 *     <li><a id="nodeId1_link" href="#nodeId1">nodeTxt1</a></li>
 *     <div class="linkClass-content" id="nodeId1">...node content ...</div>
 *     <li><a id="nodeId2_link" href="#nodeId2">nodeTxt2</a></li>
 *     <div class="linkClass-content" id="nodeId2">...node content ...</div>
 *     ... more links and divisions ...
 * </ul>
 * 
 * Otherwise, HTML structure will be created with divisions outside of the UL 
 * element as follows:
 * 
 * <ul class="linkClass">
 * <!-- Attribute 'class="linkClass"' characterize full unordered list -->
 *     <li><a id="nodeId1_link" href="#nodeId1">nodeTxt1</a></li>     
 *     <li><a id="nodeId2_link" href="#nodeId2">nodeTxt2</a></li>
 *     ...
 * </ul>
 * <div class="linkClass-content" id="nodeId1">...node content ...</div>
 * <div class="linkClass-content" id="nodeId2">...node content ...</div>
 * ...
 * 
 * 
 * To specify that a node is selected and visible (e.g. tab-like node, 
 * drop-down node, etc.), class 'selected' should be added to link class-list 
 * and class 'hide' should be removed from division class-list.
 * Inversely, to specify that a node is hide, class 'selected' should be 
 * removed from link class-list and class 'hide' should be added to division 
 * class-list.
 * Nodes are initialized to a "hide" state (class 'hide' is added to division 
 * class-list by default).
 *
 * To be able to interact with node links, the following events are added:
 * - link.onclick= nodes_clickTab(event);
 * - link.onfocus= function() {this.blur();}
 * ; where 'nodes_clickTab()' is an external function callback.
 * 
 * @param nodeId 
 * Node link will be created with the HTML attribute link.id= nodeId+ '_link'.
 * Node division will be created with the HTML attribute div.id= nodeId.
 * @param parentNodeId 
 * Refers to the HTML 'id' attribute of the existent parent node.
 * @param linkClass
 * Node link will be created with the HTML attribute link.className= linkClass.
 * Node division will be created with the HTML attribute 
 * div.className= linkClass+ '-content'.
 * @param nodeUrlSchemeTag
 * Each node is referred unambiguously by a URL. 
 * For example, node corresponding to the 'program' number '1' of 'DEMUXER' 
 * number '0' may be referred by the URL '.../demuxers/0/program/1/...'.
 * Parameter 'nodeUrlSchemeTag' is used to find any scheme tag id/number in 
 * the URL. For example, to find the program number (in this example with 
 * value '1') in the corresponding URL we would use the substring tag (scheme) 
 * 'program/' (nodeUrlSchemeTag should be set to 'program/').
 * @param nodeTxt
 * Text to be attached in the node link.
 * @returns Just created node division HTML object.
 */
function nodes_create(nodeId, parentNodeId, linkClass, nodeUrlSchemeTag, 
		nodeTxt, doInitSelected)
{
	var parentNode= document.getElementById(parentNodeId);
	if(!parentNode) {
		console.error("Not existent node Id.: "+ parentNodeId);
		return null;
	}
	var urlId= utils_findIDinUrl(utils_id2url(nodeId), nodeUrlSchemeTag);

	// Create unordered-list for nodes in case it does not exist
	var ul= parentNode.getElementsByClassName(linkClass)[0];
	if(ul== null) {
		ul= document.createElement("ul");
		ul.className= linkClass;
		p= document.createElement("p");
		p.appendChild(document.createTextNode("- Not Available -"));
		p.classList.add("unselect");
		p.classList.add("naTxtNodeClass");
		ul.appendChild(p);
		parentNode.appendChild(ul);
	}

	// Find place to insert new node
	// (link + division, sorted by alphabetical order)
	var childNodeAfter;
	var ulListItems= ul.childNodes;
	for(var i= 0; i< ulListItems.length; i++) {
		var ulItem= ulListItems[i];
		if(ulItem.nodeName== "LI") {
			var link= getFirstChildWithTagName(ulItem, 'A');
			var href= getHash(link.getAttribute('href'));
			var hrefId= utils_findIDinUrl(utils_id2url(href), nodeUrlSchemeTag);
			if(hrefId!= null && parseInt(hrefId, 10)> parseInt(urlId, 10)) {
				//console.debug("childNodeAfterId: "+ hrefId+ " urlId: "+ 
				//		urlId); //comment-me
				childNodeAfter= ulItem;
				break;
			}
		}
	}

    // Create and insert new node link
	var li= document.createElement('li');
	var a= document.createElement('a');
	a.id= nodeId+ '_link';
	a.href= "#"+ nodeId;
	a.appendChild(document.createTextNode(nodeTxt)); 
	a.onclick= nodes_clickTab;
	a.onfocus= function() {
        this.blur()
    };
    // Initialize as 'selected'/'not-selected'
    if(doInitSelected && doInitSelected== true)
    	a.classList.add('selected');
    else
    	a.classList.remove('selected');
    li.appendChild(a);
	ul.insertBefore(li, childNodeAfter);

	// Create and insert division
	var nodeDiv= document.createElement("div");
	nodeDiv.className= linkClass+ '-content';
	nodeDiv.id= nodeId;
	if(doInitSelected && doInitSelected== true)
		nodeDiv.classList.remove('hide');
	else
		nodeDiv.classList.add('hide');
	if(linkClass.indexOf('-dropdown')> -1) {
		ul.insertBefore(nodeDiv, childNodeAfter);
	} else {
		parentNode.appendChild(nodeDiv);
	}

	return nodeDiv;
}

/**
 * This function is called every time a node link item is clicked.
 * @param event
 */
function nodes_clickTab(event)
{
    event= window.event || event;
    //console.debug("nodes_clickTab(): this: "+ this+ 
    //		" event.target: "+ event.target); // comment-me
    if(this=== event.target) {
        // This code won't be executed in any child node
    	// (similar to "event.stopPropagation();")

        // Show clicked link node
    	var nodeId= getHash(this.getAttribute('href'));
        nodes_showLink(nodeId);

        // Return false to prevent scrolling to selected link
        return false;
    }
}

/**
 * Show selected node.
 * In the case of a drop-down style node, the node's content division can be 
 * 'slide-down' or 'slide-up' on consecutive click events.
 * @param nodedId
 * Node link has the HTML attribute link.id= nodeId+ '_link'.
 * Node division has the HTML attribute div.id= nodeId.
 */
function nodes_showLink(nodeId)
{
	//console.debug("Showing node with Id.: "+ nodeId); // comment-me

	// Get link's parent-parent node (that is, unordered list of nodes)
	var ul= document.getElementById(nodeId+ '_link').parentNode.parentNode;
	if(!ul) {
		console.error("Could not show node; error in node list element.")
		return;
	}

	// Iterate UL to select ONLY the given node (hiding the rest)
	var ulListItems= ul.childNodes;
	for(var i= 0; i< ulListItems.length; i++) {
		var ulItem= ulListItems[i];
		if(ulItem.nodeName== "LI") {
			var link= getFirstChildWithTagName(ulItem, 'A');
			var href= getHash(link.getAttribute('href'));
			if(!href || href== null)
				continue;
			var nodeDiv= document.getElementById(href);
			//console.debug("UL href: "+ href); //comment-me
			if(href== nodeId) {
				// Select/hide this node
				if(link.classList.contains('selected')) {
					if(nodeDiv.className.indexOf('-dropdown')> -1) {
						link.classList.remove('selected');
						nodeDiv.classList.add('hide');	
					}
				} else {
					link.classList.add('selected');
					nodeDiv.classList.remove('hide');
				}
			} else {
				// Hide this node
				link.classList.remove('selected');
				nodeDiv.classList.add('hide');
			}
		}
	}
}

/**
 * Erase node link and corresponding division.
 * @param nodeId
 * Node link has the HTML attribute link.id= nodeId+ '_link'.
 * Node division has the HTML attribute div.id= nodeId.
 */
function nodes_erase(nodeId)
{
	// Check if node exist; if not, we're done.
	var nodeDiv= document.getElementById(nodeId);
	if(!nodeDiv) return;
	//console.debug("erasing node with Id.: "+ nodeId); // comment-me

	var nodeLink= document.getElementById(nodeId+ '_link');
	var isDropdownStyle= (nodeDiv.className.indexOf('-dropdown')> -1);

	// Get link's parent-parent node (that is, unordered list of nodes)
	var ul= nodeLink.parentNode.parentNode;

	// Get link's parent node (that is, 'li' tag)
	var ulItem= nodeLink.parentNode;

	// Get node before ('previousSibling' node)
	var ulItemPrev= ulItem.previousSibling;

	// Remove node link and division
	ul.removeChild(ulItem);
    if(isDropdownStyle) {
    	ul.removeChild(nodeDiv);
    } else {
    	// Before removing, check if this node is selected. If it is, 
    	// after deleting, we shall select a new one.
    	var isNodeSelected= nodeDiv.classList.contains('hide')? false: true;

    	// Remove node division
    	var ulParentNode= ul.parentNode;
    	ulParentNode.removeChild(nodeDiv);

    	if(isNodeSelected) {
        	// Select previous node
            var linkPrev= getFirstChildWithTagName(ulItemPrev, 'A');
            var nodeIdPrev= getHash(linkPrev.getAttribute('href'));
            linkPrev.classList.add('selected');
            document.getElementById(nodeIdPrev).classList.remove('hide');
    	}
    }
}

/**
 * @param parentNodeId
 * @param currListJSON
 * @param linkClass
 * @param eraseNodeFxn
 * @param drawNodeFxn
 * @param updateNodeLinkTxtFxn
 */
function nodes_update(parentNodeId, currListJSON, linkClass, 
		eraseNodeFxn, drawNodeFxn, updateNodeLinkTxtFxn)
{
	// Get parent node
	console.debug("Parent node Id.: "+ parentNodeId); //comment-me
	var parentNode= document.getElementById(parentNodeId);

	// Only execute if parent node is not hidden
	if(parentNode.classList.contains('hide'))
		return;

	var ul= parentNode.getElementsByClassName(linkClass)[0];
	if(!ul) {
		console.error("UL node not defined");
		return;
	}
	var ulListItems= ul.childNodes;

	var p= getFirstChildWithTagName(ul, 'P');
	if(p && p.classList.contains("naTxtNodeClass")) {
		if(ulListItems.length> 1 || currListJSON.length> 0)
			p.style.display= "none";
		else
			p.removeAttribute("style");
	}

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
				var url= utils_getSelfHref(currListJSON[j].links);
				var hrefJSON= utils_url2id(url);
				if(href== hrefJSON) {
					mustDelete= false; // Node exist; do not delete
					break;
				}
			}
			if(mustDelete) {
				//console.log("Must delete: "+ href); // comment-me
				eraseNodeFxn(href);
			}
		}
	}

	// Draw those nodes that have been added to representational state.
	// Update selected (not hidden) nodes.
	for(var i= 0; i< currListJSON.length; i++) {
		var mustAddOrUpdate= true;
		var url= utils_getSelfHref(currListJSON[i].links);
		var hrefJSON= utils_url2id(url);
		console.log(hrefJSON); // comment-me
		console.log(ulListItems); // comment-me
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
					if(updateNodeLinkTxtFxn!= null)
						updateNodeLinkTxtFxn(link, currListJSON[i]);
					break;
				}
			}
		}
		if(mustAddOrUpdate) {
			console.log("Must add or update: "+ hrefJSON); // comment-me
			if(drawNodeFxn!= null)
				drawNodeFxn(url);
		}
	}

	// In the case nodes are not of 'dropdown style', at least one node must 
	// be selected
	if(linkClass.indexOf('-dropdown')< 0) {
		var isAnyNodeSelected= false;
		var firstNodeId= null;
		for(var i= 0; i< ulListItems.length; i++) {
			var ulItem= ulListItems[i];
			if(ulItem.nodeName== "LI") {
				var link= getFirstChildWithTagName(ulItem, 'A');
				if(firstNodeId== null)
					firstNodeId= getHash(link.getAttribute('href'));
				if(link.classList.contains('selected')) {
					isAnyNodeSelected= true;
					break;
				}
			}
		}
		if(isAnyNodeSelected== false) {
			if(firstNodeId!= null) {
				document.getElementById(firstNodeId+ "_link").classList.
					add('selected');
				document.getElementById(firstNodeId).classList.remove('hide');
			}
		}
	}
}

function getFirstChildWithTagName(element, tagName) {
    for(var i= 0; i< element.childNodes.length; i++) {
    	//console.log(element.childNodes[i]); // comment-me
        if(element.childNodes[i].nodeName== tagName) 
        	return element.childNodes[i]
    }
}

function getHash(url) {
    var hashPos= url.lastIndexOf('#');
    return url.substring(hashPos+ 1);
}

/**
 * 
 */
function nodes_simpleDropDownCreate(nodeId, parentNodeId, linkClass, nodeTxt, 
		doInitSelected)
{
	var parentNode= document.getElementById(parentNodeId);
	if(!parentNode) {
		console.error("Not existent node Id.: "+ parentNodeId);
		return null;
	}

	var ul= document.createElement("ul");
	ul.className= linkClass;

	// Node link
	var li= document.createElement('li');
	var a= document.createElement('a');
	a.id= nodeId+ '_link';
	a.href= "#"+ nodeId;
	a.appendChild(document.createTextNode(nodeTxt)); 
	a.onclick= nodes_clickTab;
	a.onfocus= function() {
        this.blur()
    };
    li.appendChild(a);
	ul.appendChild(li);

	// Node division
	var nodeDiv= document.createElement("div");
	nodeDiv.id= nodeId;
	nodeDiv.className= linkClass+ '-content';
	ul.appendChild(nodeDiv);

    // Initialize as 'selected'/'not-selected'
    if(doInitSelected && doInitSelected== true) {
    	a.classList.add('selected');
    	nodeDiv.classList.remove('hide');
    } else {
    	a.classList.remove('selected');
    	nodeDiv.classList.add('hide');
    }

	// Attach UL
    parentNode.appendChild(ul);

    return nodeDiv;
}
