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
 * @file System.js
 */

function system_manager(url)
{
	var nodeId= utils_url2id(url);
	var nodeDiv= document.getElementById(nodeId);

	// Statistics information
	var h4= document.createElement("h4");
	h4.appendChild(document.createTextNode("Statistics: "));
	h4.style.marginLeft= "10%";
	nodeDiv.appendChild(h4);

	// CPU/Network/Rss stats table
	var table= document.createElement("table");
	table.style.marginLeft= "calc(45% - 480px)"; // 480= (400+ 400+ 160)/2
	nodeDiv.appendChild(table);

	// titles row
	var tr= document.createElement("tr");
	var td0= document.createElement("td");
	td0.style.paddingLeft= "16px";
	td0.style.paddingRight= "16px";
	var h5= document.createElement("h5");
	h5.appendChild(document.createTextNode(" CPU History [%/secs] "));
	td0.appendChild(document.createElement("br"));
	td0.appendChild(h5);
	var td1= document.createElement("td");
	td1.style.paddingLeft= "16px";
	td1.style.paddingRight= "16px";
	var h5= document.createElement("h5");
	h5.appendChild(document.createTextNode(" Network History [Kbps] "));
	td1.appendChild(document.createElement("br"));
	td1.appendChild(h5);
	var td2= document.createElement("td");
	td2.style.paddingLeft= "16px";
	td2.style.paddingRight= "16px";
	var h5= document.createElement("h5");
	h5.appendChild(document.createTextNode(" Resident set size [KBytes]"));
	h5.appendChild(document.createElement("br"));
	h5.appendChild(document.createTextNode(" (physical memory use) "));
	td2.appendChild(document.createElement("br"));
	td2.appendChild(h5);
	tr.appendChild(td0);
	tr.appendChild(td1);
	tr.appendChild(td2);
	table.appendChild(tr);

	// CPU/Network/Rss plots placeholder row
	var tr= document.createElement("tr");
	var td0= document.createElement("td");
	td0.style.paddingLeft= "16px";
	td0.style.paddingRight= "16px";
	var div= document.createElement("div");
	div.id= "placeholder_cpu_usage";
	div.style.width= "400px";
	div.style.height= "320px";
	td0.appendChild(div);
	var td1= document.createElement("td");
	td1.style.paddingLeft= "16px";
	td1.style.paddingRight= "16px";
	var div= document.createElement("div");
	div.id= "placeholder_net_stats";
	div.style.width= "400px";
	div.style.height= "320px";
	td1.appendChild(div);
	var td2= document.createElement("td");
	td2.style.paddingLeft= "16px";
	td2.style.paddingRight= "16px";
	var div= document.createElement("div");
	div.id= "placeholder_rss_stats";
	div.style.width= "160px";
	div.style.height= "320px";
	td2.appendChild(div);
	tr.appendChild(td0);
	tr.appendChild(td1);
	tr.appendChild(td2);
	table.appendChild(tr);

	// CPU/Network/Rss usage placeholder row
	var tr= document.createElement("tr");
	var td0= document.createElement("td");
	var h5= document.createElement("h5");
	h5.id= "placeholder_cpu_usage_avg";
	td0.appendChild(h5);
	var td1= document.createElement("td");
	// TODO
	var td2= document.createElement("td");
	// TODO
	tr.appendChild(td0);
	tr.appendChild(td1);
	tr.appendChild(td2);
	table.appendChild(tr);

	// System configuration backup/restore
	var h4= document.createElement("h4");
	h4.appendChild(document.createTextNode("System configuration "
			+ "backup/restore: "));
	h4.style.marginLeft= "10%";
	nodeDiv.appendChild(h4);

	// Download configuration form
	var form= document.createElement("form");
	form.setAttribute("method", "get");
	form.setAttribute("action", "/api/1.0/download/configuration.zip");
	var button= document.createElement("button");
	button.setAttribute("type", "submit");
	button.appendChild(document.createTextNode("Download configuration"));
	button.className="normal";
	button.style.width= "22em";
	button.style.marginLeft= "10%";
	form.appendChild(button);
	nodeDiv.appendChild(form);

	// Upload configuration form
	var form= document.createElement("form");
	form.setAttribute("id", "upload-configuration-form");
	form.addEventListener('submit', function(){
		uploadConfigurationForm();
	});
	var button= document.createElement("button");
	button.setAttribute("type", "submit");
	button.setAttribute("id", "upload-configuration-button");
	button.appendChild(document.createTextNode("Upload configuration"));
	button.className="normal";
	button.style.width= "22em";
	button.style.marginLeft= "10%";
	form.appendChild(button);
	var input= document.createElement("input");
	input.setAttribute("id", "upload-configuration-input");
	input.setAttribute("type", "file");
	input.className="normal";
	input.style.marginLeft= "2.5%";
	form.appendChild(input);
	nodeDiv.appendChild(form);

	// Plot graphs
	system_plot_cpu_usage();
	system_plot_net_stats();
	system_plot_rss_stats();

	function system_plot_cpu_usage() {

		var mustSetupPlotArea= true;
		var plot;

		/* Update interval */
		var updateInterval= 1000; // [milliseconds]

		/* Fetch plot data from server */
	    function onDataReceived(stats_cpu_json) {
	    	var stats_cpu_json_obj= JSON.parse(stats_cpu_json);
	    	var data= stats_cpu_json_obj.data;
	    	var cpu_stats= data.cpu_stats;
	    	var max_xaxis= data.time_window;
	    	//alert(JSON.stringify(stats)); // comment-me

	    	/* Present CPU usage average value */
	    	var cpu_usage_average= cpu_stats[0].data[max_xaxis- 1][1];
	    	document.getElementById("placeholder_cpu_usage_avg").innerHTML= 
	    			"CPU average: "+ cpu_usage_average;

	    	/* setup plot */
	    	var options= {
	    			series: {shadowSize: 0}, // drawing faster without shadows
	    			yaxis: {min: 0, max: 100},
	    			xaxis: {max: max_xaxis, min: 0}
	    	};
	    	//$.plot($("#placeholder_cpu_usage"), cpu_stats, options);
	   
	    	if(mustSetupPlotArea== true) {
	    		mustSetupPlotArea= false;
	    		plot= $.plot($("#placeholder_cpu_usage"), cpu_stats, options);
	    		plot.setupGrid(options);
	    	}

	        plot.setData(cpu_stats);
	        // since the axes don't change, we don't need to call 
	        // plot.setupGrid()
	        plot.draw(options);
	    }

		function update()
		{
			// Only draw if current tab is not hidden
			if(!nodeDiv.classList.contains('hide')) {
				// Request, via API, CPU statistics
				loadXMLDoc('GET', '/api/1.0/stats/cpu_stats.json', null, 
						onDataReceived);			
			}

			// Update
			setTimeout(update, updateInterval);
		}
		update();
	}

	function system_plot_net_stats() {

		/* Update interval */
		var updateInterval= 1000; // [milliseconds]

		/* Fetch plot data from server */
	    function onDataReceived(stats_net_json) {
	    	var stats_net_json_obj= JSON.parse(stats_net_json);
	    	var data= stats_net_json_obj.data;
	    	var net_stats= data.net_stats;
	    	var max_xaxis= data.time_window;
	    	//alert(JSON.stringify(stats)); // comment-me

	    	var maxyaxis= 0;
	    	for(var i= 0; i< data.net_stats.length; i++) {
	        	for(var j= 0; j< max_xaxis; j++) {
	            	var maxyaxis_i= data.net_stats[i].data[j][1];
	            	//console.debug("maxyaxis: "+ maxyaxis_i); // comment-me
	            	if(maxyaxis_i> maxyaxis)
	            		maxyaxis= maxyaxis_i;
	        	}
	    	}
	    	maxyaxis= maxyaxis*1.5;

	    	/* setup plot */
	    	var options= {
	    			series: {shadowSize: 0}, // drawing is faster without shadows
	    			yaxis: {min: 0, max: maxyaxis},
	    			xaxis: {max: max_xaxis, min: 0}
	    	};
	    	$.plot($("#placeholder_net_stats"), net_stats, options);
	    }

		function update()
		{
			if(!nodeDiv.classList.contains('hide')) {
				loadXMLDoc('GET', '/api/1.0/stats/net_stats.json', null, 
						onDataReceived);			
			}
			setTimeout(update, updateInterval);
		}
		update();
	}

	function system_plot_rss_stats() {

		/* Update interval */
		var updateInterval= 1000; // [milliseconds]

		/* Fetch plot data from server */
	    function onDataReceived(stats_net_json) {
	    	var stats_rss_json_obj= JSON.parse(stats_net_json);
	    	var data= stats_rss_json_obj.data;
	    	var maxrss= Math.floor(data.maxrss/1000);
	    	var currss= Math.floor(data.currss/1000);
	    	document.getElementById("placeholder_rss_stats").innerHTML= 
				"Peak Rss: "+ maxrss+ "<br/>"+ "Current Rss: "+ currss;
	    }

		function update()
		{
			if(!nodeDiv.classList.contains('hide')) {
				loadXMLDoc('GET', '/api/1.0/stats/rss_stats.json', null, 
						onDataReceived);			
			}
			setTimeout(update, updateInterval);
		}
		update();
	}

	function uploadConfigurationForm()
	{
		var form= document.getElementById('upload-configuration-form');
		var fileSelect= document.getElementById('upload-configuration-input');
		var uploadButton= document.getElementById('upload-configuration-button');

		// Update button text.
		uploadButton.innerHTML= 'Uploading...';

		// Get the selected files from the input.
		var files= fileSelect.files;

		// Create a new FormData object.
		var formData= new FormData();

		// Request PUT settings
		loadXMLDoc('POST', '/api/1.0/upload/configuration.zip', files[0], null);
	}
}
