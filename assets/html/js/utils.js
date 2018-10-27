function utils_nodeToString(node)
{
   var tmpNode = document.createElement( "div" );
   tmpNode.appendChild( node.cloneNode( true ) );
   var str = tmpNode.innerHTML;
   tmpNode = node = null; // prevent memory leaks in IE
   return str;
}

function utils_url2id(url)
{
	var id= url.replace(/\//g, "-");
	id= id.substr(0, url.indexOf('.json'));
	//console.debug("Id: "+ id+ " ; url: "+ url); //comment-me
	return id;
}

function utils_id2url(id)
{
	var url= id.replace(/\-/g, "/");
	url+= '.json';
	//console.debug("Id: "+ id+ " ; url: "+ url); //comment-me
	return url;
}

function utils_getSelfHref(linksArrayJSON)
{
	/* 
	 * [{
	 *       "rel":	"self",
	 *       "href":	"/demuxers/0/program_processors/57/dvb_subt/60.json"
	 * }]
	 */
	for(var i= 0; i< linksArrayJSON.length; i++) {
		linkObject= linksArrayJSON[i];
		if(linkObject.rel== "self")
			return linkObject.href;
	}
	return null;
}

function utils_findIDinUrl(url, needle)
{
	var indexOfNeedle= url.indexOf(needle);
	if(indexOfNeedle< 0)
		return null;

	// Isolate Id.
	// Note: 
	// - ID. can be followed by '/' or '.'; the latter refers to string 
	// '.json' that appears at the end of a URL.
	var startIdx= indexOfNeedle+ needle.length;
	url= url.substr(startIdx);
	var stopIdx1= url.indexOf('.');
	var stopIdx2= url.indexOf('/');
	if(stopIdx1< 0 && stopIdx2< 0)
		return null;
	else if(stopIdx1< 0 && stopIdx2> -1)
		stopIdx= stopIdx2;
	else if(stopIdx1> -1 && stopIdx2< 0)
		stopIdx= stopIdx1;
	else if(stopIdx1> -1 && stopIdx2> -1)
		stopIdx= Math.min(stopIdx1, stopIdx2);
 
	var id= url.substr(0, stopIdx);
	//console.debug("Id. found: "+ id); // comment-me
	return id;
}
