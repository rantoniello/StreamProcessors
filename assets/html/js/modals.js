
/* CSS Classes */
var cssClassModal= "modal";
var cssClassModalHeader= "modal-header";
var cssClassModalBody= "modal-body";
var cssClassModalContent= "modal-content";
var cssClassModalCloseSpan= "modal-close-span";

function modals_alert_modal(parentNodeId, alertStr)
{
	var modalIdSufix= "alert_json";
	var modalPrivateContentId= parentNodeId+ '_alert_json_modal_txt';

	var modalObj= modals_create_generic_modal(parentNodeId, modalIdSufix, 
			["alert"]);
	if(modalObj== null) {
		// Modal already exist, modify body content
		// - Change modal text
		document.getElementById(modalPrivateContentId).innerHTML= alertStr;
		// - Display the modal 
		modals_display_modal(parentNodeId, modalIdSufix);
	} else {
		var divModalContentBody= modalObj.bodyDiv;
		// Create alert text paragraph
		var modalParag= document.createElement("p");
		modalParag.id= modalPrivateContentId;
		modalParag.appendChild(document.createTextNode(alertStr));
		divModalContentBody.appendChild(modalParag);
		// Display the modal 
		modals_display_modal(parentNodeId, modalIdSufix);
	}
}

function modals_confirmation_modal(parentNodeId, modalClassSufixList, 
		confirmationStr, confirmationEvent, confirmationEventObj, 
		cancelEvent, cancelEventObj)
{
	var modalIdSufix= "confirmation_json";
	var modalPrivateContentId= parentNodeId+ '_confirmation_json_modal_txt';
	var modalId= parentNodeId+ "_modal_"+ modalIdSufix;

	var conirmationInitialClass= ["confirmation"];
	var modalObj= modals_create_generic_modal(parentNodeId, modalIdSufix, 
			conirmationInitialClass.concat(modalClassSufixList), 
			cancelEvent, cancelEventObj);
	if(modalObj== null) {
		// Modal already exist, modify body content
		// - Change modal text
		document.getElementById(modalPrivateContentId).innerHTML= 
			confirmationStr;
		// - Update 'confirmationEventObj'
		var modalConfInput= document.getElementById(
				modalId+ '_confirm_button');
		modalConfInput.confirmationEventObj= confirmationEventObj
		// - Update 'cancelEventObj'
		var modalCancelInput= document.getElementById(
				modalId+ '_cancel_button');
		modalCancelInput.cancelEventObj= cancelEventObj
		// - Display the modal 
		modals_display_modal(parentNodeId, modalIdSufix);
	} else {
		var divModalContentBody= modalObj.bodyDiv;

		// Create confirmation text paragraph
		var modalParag= document.createElement("p");
		modalParag.id= modalPrivateContentId;
		modalParag.appendChild(document.createTextNode(confirmationStr));
		divModalContentBody.appendChild(modalParag);

		// Modal confirmation button
		var modalConfInput= document.createElement("input");
		modalConfInput.classList.add("button");
		modalConfInput.classList.add("modal-button");
		modalConfInput.type= "button";
		modalConfInput.value= "Confirm";
		modalConfInput.id= modalId+ '_confirm_button';
		modalConfInput.confirmationEventObj= confirmationEventObj;
		modalConfInput.addEventListener('click', function(event){
			confirmationEvent(event.target.confirmationEventObj);
			modals_close_modal(parentNodeId, modalIdSufix);
		});
		divModalContentBody.appendChild(modalConfInput);

		// Modal cancel button
		var modalCancelInput= document.createElement("input");
		modalCancelInput.classList.add("button");
		modalCancelInput.classList.add("modal-button");
		modalCancelInput.type= "button";
		modalCancelInput.value= "Cancel";
		modalCancelInput.id= modalId+ '_cancel_button';
		modalCancelInput.cancelEventObj= cancelEventObj;
		modalCancelInput.addEventListener('click', function(event){
			cancelEvent(event.target.cancelEventObj);
			modals_close_modal(parentNodeId, modalIdSufix);
		});
		divModalContentBody.appendChild(modalCancelInput);

		// Display the modal 
		modals_display_modal(parentNodeId, modalIdSufix);
	}
}

function modals_create_generic_modal(parentNodeId, modalIdSufix, 
		modalClassSufixList, onCloseEvent, onCloseEventObj)
{
	var modalId= parentNodeId+ "_modal_"+ modalIdSufix;

	// Only create alert modal if it does not exist yet
	if(document.getElementById(modalId)) {
		//console.log("Modal '"+ modalId+ "' already exists."); //comment-me
		return null; // Modal already exists
	}

	// Modal general DIV
	var divModal= document.createElement("div");
	divModal.id= modalId;
	divModal.className= cssClassModal;
	if(modalClassSufixList)
		for(var i= 0; i< modalClassSufixList.length; i++)
			divModal.classList.add(modalClassSufixList[i]);

	// Modal header
	var divModalContentHdr= document.createElement("div");
	divModalContentHdr.className= cssClassModalHeader;
	if(modalClassSufixList)
		for(var i= 0; i< modalClassSufixList.length; i++)
			divModalContentHdr.classList.add(modalClassSufixList[i]);
	divModalContentHdr.appendChild(modals_create_close_span(divModal));

	// Modal body
	var divModalContentBody= document.createElement("div"); 
	divModalContentBody.className= cssClassModalBody;
	if(modalClassSufixList)
		for(var i= 0; i< modalClassSufixList.length; i++)
			divModalContentBody.classList.add(modalClassSufixList[i]);

	// Modal content DIV
	var divModalContent= document.createElement("div");
	divModalContent.className= cssClassModalContent;
	if(modalClassSufixList)
		for(var i= 0; i< modalClassSufixList.length; i++)
			divModalContent.classList.add(modalClassSufixList[i]);
	divModalContent.appendChild(divModalContentHdr);
	divModalContent.appendChild(divModalContentBody);

	// Append all modal parts
	divModal.appendChild(divModalContent);
	document.getElementById(parentNodeId).appendChild(divModal);

//	// Close when the user clicks anywhere outside of the modal // Do not use
//	window.onclick= function(event) {
//	    if(event.target== divModal) {
//	    	divModal.style.display= "none";
//	    	if(onCloseEvent)
//	    		onCloseEvent(onCloseEventObj);
//	    }
//	}

	function modals_create_close_span(divModal)
	{
		// Modal close span ("X")
		var modalCloseSpan= document.createElement("span");
		modalCloseSpan.className= cssClassModalCloseSpan;
		if(modalClassSufixList)
			for(var i= 0; i< modalClassSufixList.length; i++)
				modalCloseSpan.classList.add(modalClassSufixList[i]);
		modalCloseSpan.appendChild(document.createTextNode('X'));

		// When the user clicks on <span> (x), close the modal
		modalCloseSpan.onclick = function() {
			divModal.style.display= "none";
	    	if(onCloseEvent)
	    		onCloseEvent(onCloseEventObj);
		}

		return modalCloseSpan;
	}

	//console.log("Modal '"+ modalId+ "' created."); //comment-me
	var modalObj= {
		headerDiv: divModalContentHdr,
		bodyDiv: divModalContentBody
	}
	return modalObj;
}

function modals_display_modal(parentNodeId, modalIdSufix)
{
	var modalId= parentNodeId+ "_modal_"+ modalIdSufix;
	document.getElementById(modalId).style.display= "block";
}

function modals_close_modal(parentNodeId, modalIdSufix)
{
	var modalId= parentNodeId+ "_modal_"+ modalIdSufix;
	var modalElem= document.getElementById(modalId);
	if(modalElem)
		modalElem.style.display= "none";
}
