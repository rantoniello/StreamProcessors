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
 * 
 */
function loadXMLDoc(method, uri, msg, callback)
{
	var xmlhttp;

	//alert(method);
	//alert(uri);
	//alert(msg);
	//alert("callback exist?: "+ callback!= null);

	if(window.XMLHttpRequest) {
		// Code for IE7+, Firefox, Chrome, Opera, Safari
		xmlhttp=new XMLHttpRequest();
	} else { 
		// Code for IE6, IE5
		xmlhttp=new ActiveXObject("Microsoft.XMLHTTP");
	}

	// Open connection and send message body if apply
	xmlhttp.open(method, uri, true); // `false` makes the request synchronous
	xmlhttp.overrideMimeType("application/json");
	//if(msg!= null) {
		//alert("sending message: '"+ msg+"'");
		xmlhttp.send(msg);
	//}
	xmlhttp.onreadystatechange= function() {
		var response= xmlhttp.responseText;
		/* 'xmlhttp.readyState'  Description
		 * 0      The request is not initialized
		 * 1      The request has been set up
		 * 2      The request has been sent
		 * 3      The request is in process
		 * 4      The request is complete
		 */

		if(xmlhttp.readyState== 4 && 
				xmlhttp.status>= 200 && xmlhttp.status< 300) {
			if(response && callback!= null)
				callback(response);
		}
		else if(xmlhttp.readyState== 4 && xmlhttp.status>= 400) {
			//alert("Response :"+ response+ "\nState: "+ xmlhttp.readyState+ 
			//		"\nStatus: "+ xmlhttp.status); //comment-me
			if(response)
				alert(response);
		}
		
	}
	xmlhttp.onerror= function() { }
	xmlhttp.onload= function() { }
}

/**
 * @param url
 * @param method
 * @param query_string
 * @param body_string
 * @param externalCallbackObj
 * Example: 
 * var externalCallbackObj= { 
 *     onResponse: function(dataJSON) {...},
 *     onError: function(alertString) {...}
 * };
 */
function httpRequests_respJSON(url, method, query_string, body_string, 
		externalCallbackObj)
{
	console.log(method+ "->"+ url+ query_string); // comment-me
	loadXMLDoc(method, "http://"+ url+ query_string, body_string, 
			respJSONCallback);

	function respJSONCallback(response)
	{
		// Check message status
		console.log(response); // comment-me
		var responseJSON= JSON.parse(response);
		if(responseJSON.code!= '200' && 
		   !(responseJSON.code== '201' && method== 'POST') 
		) {
			console.error("Error on request "+ method+ url+ query_string);
			var alertStr= "Error: ";
			if(responseJSON.message.length> 0)
				alertStr+= responseJSON.message;
			else
				alertStr+= responseJSON.status+ ".";
			if(externalCallbackObj && externalCallbackObj!= null && 
					externalCallbackObj.onError)
				externalCallbackObj.onError(alertStr);
		} else {
			// Response is O.K. (code 200)
			if(externalCallbackObj && externalCallbackObj!= null && 
					externalCallbackObj.onResponse) {
				externalCallbackObj.onResponse(responseJSON.data);
			}
		}
	}
}
