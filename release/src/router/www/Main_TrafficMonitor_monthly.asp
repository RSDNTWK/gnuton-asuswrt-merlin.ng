﻿<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="X-UA-Compatible" content="IE=Edge" />
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">

<title><#Web_Title#> - <#menu5_8_3#></title>
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="tmmenu.css">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<script language="JavaScript" type="text/javascript" src="/js/jquery.js"></script>
<script language="JavaScript" type="text/javascript" src="/js/chart.min.js"></script>
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/tmmenu.js"></script>
<script language="JavaScript" type="text/javascript" src="/tmhist.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/js/httpApi.js"></script>

<script type='text/javascript'>
var nvram = httpApi.nvramGet(["wan_ifname", "lan_ifname", "rstats_enable", "http_id", "cstats_enable"])

try {
	<% bandwidth("monthly"); %>
}
catch (ex) {
	monthly_history = [];
}
rstats_busy = 0;
if (typeof(monthly_history) == 'undefined') {
	monthly_history = [];
	rstats_busy = 1;
}

var barDataUl, barDataDl, barLabels;
var myBarChart;

Chart.defaults.color = "#CCC";

function save()
{
	cookie.set('monthly', scale, 31);
}

function genData()
{
	var w, i, h;

	w = window.open('', 'tomato_data_d');
	w.document.writeln('<pre>');
	for (i = 0; i < monthly_history.length-1; ++i) {
		h = monthly_history[i];
		h = monthly_history[i];
		w.document.writeln([(((h[0] >> 16) & 0xFF) + 1900), (((h[0] >>> 8) & 0xFF) + 1), h[1], h[2]].join(','));
	}
	w.document.writeln('</pre>');
	w.document.close();
}

function getYMD(n)
{
	// [y,m,d]
	return [(((n >> 16) & 0xFF) + 1900), ((n >>> 8) & 0xFF), (n & 0xFF)];
}

function redraw()
{
	var h;
	var grid;
	var rows;
	var yr, mo, da;

	rows = 0;
	block = '';
	gn = 0;

	barDataUl = [];
	barDataDl = [];
	barLabels = [];

	grid = '<table width="730px" class="FormTable_NWM">';
	grid += "<tr><th style=\"height:30px;\"><#Date#></th>";
	grid += "<th><#tm_reception#></th>";
	grid += "<th><#tm_transmission#></th>";
	grid += "<th><#Total#></th></tr>";

	for (i = 0; i < monthly_history.length-1; ++i) {
		h = monthly_history[i];
		yr = (((h[0] >> 16) & 0xFF) + 1900);
		mo = ((h[0] >>> 8) & 0xFF);

		grid += makeRow(((rows & 1) ? 'odd' : 'even'), ymText(yr, mo), rescale(h[1]), rescale(h[2]), rescale(h[1] + h[2]));
		++rows;

		barDataDl.unshift(h[1] / ((scale == 2) ?  1048576 : ((scale == 1) ? 1024 : 1)));
		barDataUl.unshift(h[2] / ((scale == 2) ?  1048576 : ((scale == 1) ? 1024 : 1)));
		barLabels.unshift(months[mo] + ' ' + yr);
	}

	if(rows == 0)
		grid +='<tr><td style="color:#FFCC00;" colspan="4"><#IPConnection_VSList_Norule#></td></tr>';

	E('bwm-monthly-grid').innerHTML = grid + '</table>';

	draw_chart();
}

function init()
{
	var s;

	if (nvram.cstats_enable == '1') {
		selGroup = E('page_select');

		optGroup = document.createElement('OPTGROUP');
		optGroup.label = "Per device";

		opt = document.createElement('option');
		opt.innerHTML = "<#menu4_2_1#>";
		opt.value = "5";
		optGroup.appendChild(opt);
		opt = document.createElement('option');
		opt.innerHTML = "<#menu4_2_3#>";
		opt.value = "6";
		optGroup.appendChild(opt);
		opt = document.createElement('option');
		opt.innerHTML = "Monthly";
		opt.value = "7";
		optGroup.appendChild(opt);

		selGroup.appendChild(optGroup);
	}

	if (nvram.rstats_enable != '1') return;

	if ((s = cookie.get('monthly')) != null) {
		if (s.match(/^([0-2])$/)) {
			E('scale').value = scale = RegExp.$1 * 1;
		}
	}

	initDate('ym');
	monthly_history.sort(cmpHist);
	redraw();
	if(bwdpi_support){
		document.getElementById('content_title').innerHTML = "<#traffic_monitor#>";
	}
}

function switchPage(page){
	if(page == "1")
		location.href = "/Main_TrafficMonitor_realtime.asp";
	else if(page == "2")
		location.href = "/Main_TrafficMonitor_last24.asp";
	else if(page == "3")
		location.href = "/Main_TrafficMonitor_daily.asp";
	else if(page == "5")
		location.href = "/Main_TrafficMonitor_devrealtime.asp";
	else if(page == "6")
		location.href = "/Main_TrafficMonitor_devdaily.asp";
	else if(page == "7")
		location.href = "/Main_TrafficMonitor_devmonthly.asp";
	else
		return false;
}

function draw_chart(){
	if (barLabels.length == 0) return;

	if (myBarChart != undefined) myBarChart.destroy();

	var ctx = document.getElementById("chart").getContext("2d");

	var barOptions = {
		segmentShowStroke : false,
		segmentStrokeColor : "#000",
		animationEasing : "easeOutQuart",
		animationSteps : 100,
		animateScale : true,
		plugins: {
			tooltip: {
				callbacks: {
					label: function (context) { return comma(context.parsed.y.toFixed(2)) + " " + snames[scale]; },
				}
			},
		},
		scales: {
			x: {
				grid: { display: false },
			},
			y: {
				grid: { color: "#282828" },
				ticks: {
					callback: function(value, index, values) {
						return comma(value);
					}
				}
			}
		}
	};

	var barDataset = {
		labels: barLabels,
		datasets: [
			{data: barDataDl,
			label: "<#tm_reception#> (" + snames[scale].trim() + ")",
			borderWidth: 1,
			backgroundColor: "#4C8FC0",
			borderColor: "#000000"
		},
			{data: barDataUl,
			label: "<#tm_transmission#> (" + snames[scale].trim() +")",
			borderWidth: 1,
			backgroundColor: "#2B6692",
			borderColor: "#000000"
		}]
	};

	myBarChart = new Chart(ctx, {
		type: 'bar',
		options: barOptions,
		data: barDataset
	});
}

</script>
</head>

<body onload="show_menu();init();" class="bg" >

<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" action="apply.cgi" target="hidden_frame">
<input type="hidden" name="current_page" value="Main_TrafficMonitor_monthly.asp">
<input type="hidden" name="next_page" value="Main_TrafficMonitor_monthly.asp">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_wait" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">

<table class="content" align="center" cellpadding="0" cellspacing="0">
<tr>
	<td width="23">&nbsp;</td>

<!--=====Beginning of Main Menu=====-->
	<td valign="top" width="202">
		<div id="mainMenu"></div>
		<div id="subMenu"></div>
	</td>

	<td valign="top">
		<div id="tabMenu" class="submenuBlock"></div>
<!--===================================Beginning of Main Content===========================================-->
	<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
		<tr>
		<td align="left"  valign="top">
			<table width="100%" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle">
				<tbody>
				<!--===================================Beginning of QoS Content===========================================-->
			<tr>
				<td bgcolor="#4D595D" valign="top">
					<table width="740px" border="0" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3">
						<tr><td><table width="100%" >
						<tr>
							<td  class="formfonttitle" align="left">
								<div id="content_title" style="margin-top:5px;"><#Menu_TrafficManager#> - <#traffic_monitor#></div>
							</td>
							<td>
								<div align="right">
									<select id="page_select" class="input_option" style="width:120px" onchange="switchPage(this.options[this.selectedIndex].value)">
										<!--option><#switchpage#></option-->
										<optgroup label="Global">
											<option value="1"><#menu4_2_1#></option>
											<option value="2"><#menu4_2_2#></option>
											<option value="3"><#menu4_2_3#></option>
											<option value="4" selected>Monthly</option>
										</optgroup>
									</select>
								</div>
							</td>
						</tr>
					</table></td></tr>

					<tr>
						<td height="5"><div class="splitLine"></div></td>
					</tr>
						<tr>
							<td bgcolor="#4D595D">
								<table width="730"  border="1" align="left" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
									<thead>
										<tr>
											<td colspan="2"><#t2BC#></td>
										</tr>
									</thead>
									<tbody>
										<tr class='even'>
											<th width="40%"><#Date#></th>
											<td>
												<select class="input_option" style="width:130px" onchange='changeDate(this, "ymd")' id='dafm'>
													<option value=0>yyyy-mm-dd</option>
													<option value=1>mm-dd-yyyy</option>
													<option value=2>mm, dd, yyyy</option>
													<option value=3>dd.mm.yyyy</option>
												</select>
											</td>
										</tr>
										<tr class='even'>
											<th width="40%"><#Scale#></th>
											<td>
												<select style="width:70px" class="input_option" onchange='changeScale(this)' id='scale'>
													<option value=0>KB</option>
													<option value=1>MB</option>
													<option value=2 selected>GB</option>
												</select>
											</td>
										</tr>
									</tbody>
								</table>
							</td>
						</tr>
						<tr>
							<td>
								<div style="background-color:#2f3e44;border-radius:10px;width:730px;padding-left:5px;"><canvas id="chart" height="120"></div>
							</td>
						</tr>
						<tr>
							<td>
								<div id='bwm-monthly-grid' style='float:left'></div>
							</td>
						</tr>
					</table>
     				</td>
	     		</tr>
				</tbody>
				</table>
			</td>
		</tr>
		</table>
		</div>
	</td>

    	<td width="10" align="center" valign="top">&nbsp;</td>
</tr>
</table>
<div id="footer"></div>
</body>
</html>


